#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <mqueue.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <time.h>
#include <syslog.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define HRES 1280
#define VRES 720
#define HRES_STR "1280"
#define VRES_STR "720"

#define SNDRCV_MQ "/frame_mq"
#define MAX_MSG_SIZE sizeof(void *)
#define ERROR (-1)
#define TERMINATION_SIGNAL (void *)(-1)

// Structs and global variables
static struct v4l2_format fmt;
static enum io_method { IO_METHOD_READ, IO_METHOD_MMAP, IO_METHOD_USERPTR } io = IO_METHOD_MMAP;
static int fd = -1;
struct buffer { void *start; size_t length; };
static struct buffer *buffers;
static unsigned int n_buffers;
static unsigned int frame_count = 180;
static mqd_t mq;
struct mq_attr mq_attr;

static const char *log_file_path = "./debug_log.txt";
static const char *frames_dir = "./frames";

// Function prototypes
static void errno_exit(const char *s);
static int xioctl(int fh, int request, void *arg);
static void process_image(const void *p, int size, unsigned int frame_number);
static int read_frame(unsigned int frame_number);
static void *capture_frames(void *arg);
static void *write_frames(void *arg);
static void open_device(void);
static void init_device(void);
static void start_capturing(void);
static void stop_capturing(void);
static void uninit_device(void);
static void close_device(void);
static void init_mmap(void);
static void log_message(const char *message);
static int clamp(int value);

int main(int argc, char **argv) {
    pthread_t th_capture, th_write;
    pthread_attr_t attr_capture, attr_write;
    struct sched_param param_capture, param_write;
    int rc;

    // Unlink the message queue if it exists
    mq_unlink(SNDRCV_MQ);

    // Create the directory for saving frames
    if (mkdir(frames_dir, 0755) && errno != EEXIST) {
        perror("mkdir");
        exit(EXIT_FAILURE);
    }

    // Create and set permissions for debug_log.txt
    int log_fd = open(log_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log_fd == -1) {
        perror("Failed to create debug_log.txt");
        exit(EXIT_FAILURE);
    }
    close(log_fd);

    // Initialize syslog for logging
    openlog("camera_mq_app", LOG_PID | LOG_CONS, LOG_USER);

    // Set message queue attributes
    mq_attr.mq_maxmsg = 10;
    mq_attr.mq_msgsize = MAX_MSG_SIZE;
    mq_attr.mq_flags = 0;

    // Open the message queue
    mq = mq_open(SNDRCV_MQ, O_CREAT | O_RDWR, 0666, &mq_attr);
    if (mq == (mqd_t) ERROR) {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }

    // Open and initialize the video device
    open_device();
    init_device();
    start_capturing();

    // Set thread attributes for capture and write threads
    pthread_attr_init(&attr_capture);
    pthread_attr_init(&attr_write);

    pthread_attr_setinheritsched(&attr_capture, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr_capture, SCHED_FIFO);
    param_capture.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_attr_setschedparam(&attr_capture, &param_capture);

    pthread_attr_setinheritsched(&attr_write, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr_write, SCHED_FIFO);
    param_write.sched_priority = sched_get_priority_min(SCHED_FIFO);
    pthread_attr_setschedparam(&attr_write, &param_write);

    // Create the capture and write threads
    rc = pthread_create(&th_capture, &attr_capture, capture_frames, NULL);
    if (rc != 0) {
        perror("pthread_create capture");
        exit(EXIT_FAILURE);
    }
    rc = pthread_create(&th_write, &attr_write, write_frames, NULL);
    if (rc != 0) {
        perror("pthread_create write");
        exit(EXIT_FAILURE);
    }

    // Wait for the capture thread to finish
    pthread_join(th_capture, NULL);

    // Send termination signal to write thread
    void *termination_signal = TERMINATION_SIGNAL;
    mq_send(mq, (char *)&termination_signal, sizeof(void *), 0);

    // Wait for the write thread to finish
    pthread_join(th_write, NULL);

    // Clean up and close resources
    stop_capturing();
    uninit_device();
    close_device();
    mq_close(mq);
    mq_unlink(SNDRCV_MQ);
    closelog();

    return 0;
}

// Function to handle errors and exit the program
static void errno_exit(const char *s) {
    perror(s);
    exit(EXIT_FAILURE);
}

// Wrapper for ioctl with error handling
static int xioctl(int fh, int request, void *arg) {
    int r;
    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);
    return r;
}

// Clamp function to ensure RGB values are within the 0-255 range
static int clamp(int value) {
    return value < 0 ? 0 : (value > 255 ? 255 : value);
}

// Process the captured image, convert YUYV to RGB, and save it as a PPM file
static void process_image(const void *p, int size, unsigned int frame_number) {
    unsigned char *rgb_image = malloc(HRES * VRES * 3); // 3 bytes per pixel for RGB

    if (rgb_image == NULL) {
        perror("Unable to allocate memory for RGB image");
        return;
    }

    // Convert YUYV to RGB manually
    const unsigned char *yuyv = p;
    for (int i = 0; i < HRES * VRES * 2; i += 4) {
        int y0 = yuyv[i];
        int u = yuyv[i + 1];
        int y1 = yuyv[i + 2];
        int v = yuyv[i + 3];

        int c = y0 - 16;
        int d = u - 128;
        int e = v - 128;

        rgb_image[3 * i / 2 + 0] = clamp((298 * c + 409 * e + 128) >> 8);
        rgb_image[3 * i / 2 + 1] = clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
        rgb_image[3 * i / 2 + 2] = clamp((298 * c + 516 * d + 128) >> 8);

        c = y1 - 16;
        rgb_image[3 * i / 2 + 3] = clamp((298 * c + 409 * e + 128) >> 8);
        rgb_image[3 * i / 2 + 4] = clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
        rgb_image[3 * i / 2 + 5] = clamp((298 * c + 516 * d + 128) >> 8);
    }

    // Save the image as a PPM file
    char filename[256];
    snprintf(filename, sizeof(filename), "%s/frame-%08d.ppm", frames_dir, frame_number);

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("Cannot open file to dump frame");
        free(rgb_image);
        return;
    }

    fprintf(fp, "P6\n%s %s\n255\n", HRES_STR, VRES_STR);
    fwrite(rgb_image, 3, HRES * VRES, fp);
    fclose(fp);
    free(rgb_image);
}

// Read a frame from the video device and pass it to the processing function
static int read_frame(unsigned int frame_number) {
    struct v4l2_buffer buf;
    CLEAR(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
        switch (errno) {
            case EAGAIN:
                return 0;
            case EIO:
            default:
                errno_exit("VIDIOC_DQBUF");
        }
    }

    assert(buf.index < n_buffers);
    printf("Reading frame: buffer index = %d, bytes used = %d\n", buf.index, buf.bytesused);

    process_image(buffers[buf.index].start, buf.bytesused, frame_number); // Pass frame_number

    if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
        errno_exit("VIDIOC_QBUF");
    }

    return 1;
}

// Open the video device
static void open_device(void) {
    fd = open("/dev/video2", O_RDWR | O_NONBLOCK, 0);

    if (-1 == fd) {
        fprintf(stderr, "Cannot open camera device: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    printf("Opened device: /dev/video2\n");
}

// Initialize the video device settings
static void init_device(void) {
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    unsigned int min;

    if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
        errno_exit("VIDIOC_QUERYCAP");
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "No video capture device\n");
        exit(EXIT_FAILURE);
    }

    printf("Initialized device: capabilities = %08x\n", cap.capabilities);

    switch (io) {
        case IO_METHOD_READ:
            if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
                fprintf(stderr, "Device does not support read i/o\n");
                exit(EXIT_FAILURE);
            }
            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
                fprintf(stderr, "Device does not support streaming i/o\n");
                exit(EXIT_FAILURE);
            }
            break;
    }

    CLEAR(cropcap);
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect;

        if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
            switch (errno) {
                case EINVAL:
                    break;
                default:
                    break;
            }
        }
    }

    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = HRES;
    fmt.fmt.pix.height = VRES;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;  // Using YUYV format
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt)) {
        errno_exit("VIDIOC_S_FMT");
    }

    printf("Set format: width = %d, height = %d, pixelformat = %d\n", fmt.fmt.pix.width, fmt.fmt.pix.height, fmt.fmt.pix.pixelformat);

    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min) fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min) fmt.fmt.pix.sizeimage = min;

    init_mmap();
}

// Start capturing frames from the video device
static void start_capturing(void) {
    unsigned int i;
    enum v4l2_buf_type type;

    switch (io) {
        case IO_METHOD_READ:
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < n_buffers; ++i) {
                struct v4l2_buffer buf;

                CLEAR(buf);
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;

                if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
                    errno_exit("VIDIOC_QBUF");
                }
            }
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            if (-1 == xioctl(fd, VIDIOC_STREAMON, &type)) {
                errno_exit("VIDIOC_STREAMON");
            }
            printf("Started capturing on /dev/video2\n");
            break;
    }
}

// Stop capturing frames from the video device
static void stop_capturing(void) {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type)) {
        errno_exit("VIDIOC_STREAMOFF");
    }
    printf("Stopped capturing on /dev/video2\n");
}

// Uninitialize the video device
static void uninit_device(void) {
    unsigned int i;

    switch (io) {
        case IO_METHOD_READ:
            free(buffers[0].start);
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < n_buffers; ++i) {
                if (-1 == munmap(buffers[i].start, buffers[i].length)) {
                    errno_exit("munmap");
                }
            }
            break;

        case IO_METHOD_USERPTR:
            for (i = 0; i < n_buffers; ++i) {
                free(buffers[i].start);
            }
            break;
    }

    free(buffers);
    printf("Uninitialized device and freed buffers\n");
}

// Close the video device
static void close_device(void) {
    if (-1 == close(fd)) {
        errno_exit("close");
    }

    fd = -1;
    printf("Closed device /dev/video2\n");
}

// Initialize memory-mapped buffers
static void init_mmap(void) {
    struct v4l2_requestbuffers req;

    CLEAR(req);
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf(stderr, "Device does not support memory mapping\n");
            exit(EXIT_FAILURE);
        } else {
            errno_exit("VIDIOC_REQBUFS");
        }
    }

    if (req.count < 2) {
        fprintf(stderr, "Insufficient buffer memory\n");
        exit(EXIT_FAILURE);
    }

    buffers = calloc(req.count, sizeof(*buffers));
    if (!buffers) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;

        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;

        if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf)) {
            errno_exit("VIDIOC_QUERYBUF");
        }

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

        if (MAP_FAILED == buffers[n_buffers].start) {
            errno_exit("mmap");
        }

        printf("Mapped buffer %d: start = %p, length = %zu\n", n_buffers, buffers[n_buffers].start, buffers[n_buffers].length);
    }
}

// Capture frames in the capture thread
static void *capture_frames(void *arg) {
    unsigned int count = frame_count;
    unsigned int frame_number = 1;
    
    while (count > 0) {
        // Add delay before capturing each frame (3 Hz acquisition rate)
        usleep(333333); // 333 milliseconds (3 Hz)

        if (read_frame(frame_number)) {
            log_message("Captured frame");

            void *frame_ptr = buffers[0].start;
            printf("Sending frame %u to message queue, address: %p\n", frame_number, frame_ptr);

            if (mq_send(mq, (char *)&frame_ptr, sizeof(frame_ptr), 0) == -1) {
                perror("mq_send");
            }

            frame_number++;
            count--;
        }
    }
    return NULL;
}

// Write frames in the write thread
static void *write_frames(void *arg) {
    unsigned int frame_num = 0;
    void *frame_ptr;

    while (1) {  // Infinite loop for processing frames
        if (mq_receive(mq, (char *)&frame_ptr, sizeof(frame_ptr), NULL) == -1) {
            perror("mq_receive");
        } else {
            if (frame_ptr == TERMINATION_SIGNAL) {
                break;  // Exit the loop if the termination signal is received
            }

            frame_num++;

            // Write back every third frame (1/3rd the acquisition rate)
            if (frame_num % 3 == 0) {
                struct timespec frame_time;
                clock_gettime(CLOCK_REALTIME, &frame_time);

                printf("Received frame %u from message queue, address: %p\n", frame_num, frame_ptr);

                // Call process_image here to save the frame
                process_image(frame_ptr, HRES * VRES * 2, frame_num);

                log_message("Wrote back frame");
            }
        }
    }
    return NULL;
}

// Log messages to syslog and debug_log.txt
static void log_message(const char *message) {
    syslog(LOG_INFO, "%s", message);

    FILE *log_file = fopen(log_file_path, "a");
    if (log_file != NULL) {
        // Set file permissions to 0644
        int fd = fileno(log_file);
        if (fchmod(fd, 0644) == -1) {
            perror("fchmod");
        }

        fprintf(log_file, "%s\n", message);
        fclose(log_file);
    } else {
        syslog(LOG_ERR, "Failed to open log file: %s", log_file_path);
    }
}
