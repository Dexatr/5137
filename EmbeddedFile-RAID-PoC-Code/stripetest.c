#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <time.h> // Include for high-precision timing
#include <sys/sysinfo.h>
#include <errno.h>
#include <string.h>
#include "raidlib.h"

int main(int argc, char *argv[])
{
    int bytesWritten, bytesRestored;
    char rc;
    int chunkToRebuild = 0; // The chunk number to be restored
    struct timespec beginning_time_val; // Start time for measuring performance
    struct timespec end_time_val; // End time for measuring performance
    long diff_time_nsec; // Time difference in nanoseconds
    
    int average_sec = 0; // Time taken in seconds
    int average_msec = 0; // Time taken in milliseconds

    // Ensure proper usage of the program
    if(argc < 3)
    {
        printf("usage: stripetest inputfile outputfile <sector to restore>\n");
        exit(-1);
    }
    
    // If a chunk to rebuild is provided, parse it from the command line arguments
    if(argc >= 4)
    {
        sscanf(argv[3], "%d", &chunkToRebuild);
        printf("chunk to restore = %d\n", chunkToRebuild);
    }
    
    // Log the start of the operation without optimizations
    syslog(LOG_CRIT, "************** Without any optimizations **************");

    // Note the beginning time for the striping operation
    clock_gettime(CLOCK_MONOTONIC, &beginning_time_val);
    bytesWritten = stripeFile(argv[1], 0); // Perform the striping operation
    // Note the end time for the striping operation
    clock_gettime(CLOCK_MONOTONIC, &end_time_val);

    // Calculate time difference in nanoseconds
    diff_time_nsec = (end_time_val.tv_sec - beginning_time_val.tv_sec) * 1000000000L + (end_time_val.tv_nsec - beginning_time_val.tv_nsec);
    
    // Convert time difference to seconds and milliseconds for easier readability
    average_sec = (int)(diff_time_nsec / 1000000000L);
    average_msec = (int)((diff_time_nsec % 1000000000L) / 1000000L);

    // Log the time taken to complete the striping operation
    syslog(LOG_CRIT, "Time to complete stripeFile = %d sec: %d msec", average_sec, average_msec);
    
    printf("input file was written as 4 data chunks + 1 XOR parity - could have been on 5 devices\n");
    printf("Remove chunk %d and enter g for go - could have been on 5 devices\n", chunkToRebuild);
    printf("Hit return to start rebuild:");

    rc = getchar(); // Wait for user input before starting the rebuild

    printf("working on restoring file ...\n");

    // Note the beginning time for the restoration operation
    clock_gettime(CLOCK_MONOTONIC, &beginning_time_val);
    bytesRestored = restoreFile(argv[2], 0, bytesWritten, chunkToRebuild); // Perform the restoration operation
    // Note the end time for the restoration operation
    clock_gettime(CLOCK_MONOTONIC, &end_time_val);

    // Calculate time difference in nanoseconds
    diff_time_nsec = (end_time_val.tv_sec - beginning_time_val.tv_sec) * 1000000000L + (end_time_val.tv_nsec - beginning_time_val.tv_nsec);

    // Convert time difference to seconds and milliseconds for easier readability
    average_sec = (int)(diff_time_nsec / 1000000000L);
    average_msec = (int)((diff_time_nsec % 1000000000L) / 1000000L);

    // Log the time taken to complete the restoration operation
    syslog(LOG_CRIT, "Time to complete restoreFile = %d sec: %d msec", average_sec, average_msec);
    
    // Log the end of the operation
    syslog(LOG_CRIT, "************** END **************");
    printf("FINISHED\n");
}
