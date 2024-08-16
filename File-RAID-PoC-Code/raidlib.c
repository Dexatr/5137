#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <assert.h>

#include "raidlib.h" // Include the custom RAID library

#ifdef RAID64
#include "raidlib64.h" // Include 64-bit RAID library if defined
#define PTR_CAST (unsigned long long *)
#else
#include "raidlib.h" // Default to the regular RAID library
#define PTR_CAST (unsigned char *)
#endif

// RAID-5 encoding
//
// This provides 80% capacity with 1/5 LBAs (Logical Block Addresses) used for parity.
// It handles only single faults.
//
// PRECONDITIONS:
// 1) LBA pointers must have memory allocated for them externally.
// 2) Blocks pointed to by LBAs are initialized with data.
//
// POST-CONDITIONS:
// 1) Contents of PLBA (Parity LBA) are modified and contain the computed parity using XOR.
void xorLBA(unsigned char *LBA1,
            unsigned char *LBA2,
            unsigned char *LBA3,
            unsigned char *LBA4,
            unsigned char *PLBA)
{
    int idx;

    // Compute XOR for each byte in the sector to generate the parity LBA
    for(idx=0; idx < SECTOR_SIZE; idx++)
        *(PLBA + idx) = (*(LBA1 + idx)) ^ (*(LBA2 + idx)) ^ (*(LBA3 + idx)) ^ (*(LBA4 + idx));
}

// RAID-5 Rebuild
//
// Provide any 3 of the original LBAs and the Parity LBA to rebuild the RLBA (Rebuilt LBA).
//
// If the Parity LBA was lost, it can be rebuilt by simply re-encoding.
void rebuildLBA(unsigned char *LBA1,
                unsigned char *LBA2,
                unsigned char *LBA3,
                unsigned char *PLBA,
                unsigned char *RLBA)
{
    int idx;
    unsigned char checkParity;

    // Rebuild the missing LBA (RLBA) using the remaining LBAs and parity LBA
    for(idx = 0; idx < SECTOR_SIZE; idx++)
    {
        // Parity check word is simply XOR of remaining good LBAs
        checkParity = (*(LBA1 + idx)) ^ (*(LBA2 + idx)) ^ (*(LBA3 + idx));

        // Rebuilt LBA is XOR of original parity and parity check word
        *(RLBA + idx) = (*(PLBA + idx)) ^ (checkParity);
    }
}

// Check if two LBAs are equivalent
int checkEquivLBA(unsigned char *LBA1,
                  unsigned char *LBA2)
{
    int idx;

    // Compare each byte in the LBAs
    for(idx = 0; idx < SECTOR_SIZE; idx++)
    {
        if((*(LBA1 + idx)) != (*(LBA2 + idx)))
        {
            // Print mismatch details if LBAs are not equivalent
            printf("EQUIV CHECK MISMATCH @ byte %d: LBA1=0x%x, LBA2=0x%x\n", idx, (*LBA1 + idx), (*LBA2 + idx));
            return ERROR;
        }
    }

    return OK;
}

// Stripes the input file across multiple chunks and returns the number of bytes written
int stripeFile(char *inputFileName, int offsetSectors)
{
    int fd[5], idx;
    FILE *fdin;
    unsigned char stripe[5 * 512]; // Buffer to hold a stripe (4 data chunks + 1 XOR parity)
    int offset = 0, bread = 0, btoread = (4 * 512), bwritten = 0, btowrite = (512), sectorCnt = 0, byteCnt = 0;

    // Open the input file for reading
    fdin = fopen(inputFileName, "r");

    // Open files for each stripe chunk and the XOR parity chunk
    fd[0] = open("StripeChunk1.bin", O_RDWR | O_CREAT, 00644);
    fd[1] = open("StripeChunk2.bin", O_RDWR | O_CREAT, 00644);
    fd[2] = open("StripeChunk3.bin", O_RDWR | O_CREAT, 00644);
    fd[3] = open("StripeChunk4.bin", O_RDWR | O_CREAT, 00644);
    fd[4] = open("StripeChunkXOR.bin", O_RDWR | O_CREAT, 00644);

    do
    {
        // Read a stripe or until the end of the file
        offset = 0, bread = 0, btoread = (4 * 512);
        do
        {
            bread = fread(&stripe[offset], 1, btoread, fdin); 
            offset += bread;
            btoread = (4 * 512) - bread;
        }
        while (!(feof(fdin)) && (btoread > 0));

        if((offset < (4 * 512)) && (feof(fdin)))
        {
            // Handle the case when the end of file is reached before a full stripe is read
            printf("hit end of file\n");
            bzero(&stripe[offset], btoread); // Zero-fill the remaining stripe
            byteCnt += offset;
        }
        else
        {
            printf("read full stripe\n");
            assert(offset == (4 * 512)); // Ensure a full stripe was read
            byteCnt += (4 * 512);
        };

        // Compute XOR parity for the stripe
        xorLBA(PTR_CAST &stripe[0],
               PTR_CAST &stripe[512],
               PTR_CAST &stripe[1024],
               PTR_CAST &stripe[1536],
               PTR_CAST &stripe[2048]);

        // Write out the stripe chunks and XOR parity chunk to their respective files
        offset = 0, bwritten = 0, btowrite = (512);
        do
        {
            bwritten = write(fd[0], &stripe[offset], 512); 
            offset += bwritten;
            btowrite = (512) - bwritten;
        }
        while (btowrite > 0);

        offset = 512, bwritten = 0, btowrite = (512);
        do
        {
            bwritten = write(fd[1], &stripe[offset], 512); 
            offset += bwritten;
            btowrite = (512) - bwritten;
        }
        while (btowrite > 0);

        offset = 1024, bwritten = 0, btowrite = (512);
        do
        {
            bwritten = write(fd[2], &stripe[offset], 512); 
            offset += bwritten;
            btowrite = (512) - bwritten;
        }
        while (btowrite > 0);

        offset = 1536, bwritten = 0, btowrite = (512);
        do
        {
            bwritten = write(fd[3], &stripe[offset], 512); 
            offset += bwritten;
            btowrite = (512) - bwritten;
        }
        while (btowrite > 0);

        offset = 2048, bwritten = 0, btowrite = (512);
        do
        {
            bwritten = write(fd[4], &stripe[offset], 512); 
            offset += bwritten;
            btowrite = (512) - bwritten;
        }
        while (btowrite > 0);

        sectorCnt += 4;

    }
    while (!(feof(fdin))); // Continue until the end of file is reached

    // Close all file descriptors
    fclose(fdin);
    for(idx = 0; idx < 5; idx++) close(fd[idx]);

    return(byteCnt); // Return the total number of bytes written
}

// Restores the original file from the striped chunks
//
// missingChunk = 0 for no missing chunk
//              = 1 ... 4 for missing data chunk
//              = 5 for missing XOR chunk
int restoreFile(char *outputFileName, int offsetSectors, int fileLength, int missingChunk)
{
    int fd[5], idx;
    FILE *fdout;
    unsigned char stripe[5 * 512];
    int offset = 0, bread = 0, btoread = (4 * 512), bwritten = 0, btowrite = (512), sectorCnt = fileLength / 512;
    int stripeCnt = fileLength / (4 * 512);
    int lastStripeBytes = fileLength % (4 * 512);

    // Open the output file for writing the restored data
    fdout = fopen(outputFileName, "w");

    // Open files for each stripe chunk and the XOR parity chunk
    fd[0] = open("StripeChunk1.bin", O_RDWR | O_CREAT, 00644);
    fd[1] = open("StripeChunk2.bin", O_RDWR | O_CREAT, 00644);
    fd[2] = open("StripeChunk3.bin", O_RDWR | O_CREAT, 00644);
    fd[3] = open("StripeChunk4.bin", O_RDWR | O_CREAT, 00644);
    fd[4] = open("StripeChunkXOR.bin", O_RDWR | O_CREAT, 00644);

    for(idx = 0; idx < stripeCnt; idx++)
    {
        // Read in the stripe chunks and XOR code

        if(missingChunk == 1)
        {
            printf("will rebuild chunk 1\n");
        }
        else
        {
            offset = 0, bread = 0, btoread = (512);
            do
            {
                bread = read(fd[0], &stripe[offset], 512); 
                offset += bread;
                btoread = (512) - bread;
            }
            while (btoread > 0);
        }

        if(missingChunk == 2)
        {
            printf("will rebuild chunk 2\n");
        }
        else
        {
            offset = 512, bread = 0, btoread = (512);
            do
            {
                bread = read(fd[1], &stripe[offset], 512); 
                offset += bread;
                btoread = (512) - bread;
            }
            while (btoread > 0);
        }

        if(missingChunk == 3)
        {
            printf("will rebuild chunk 3\n");
        }
        else
        {
            offset = 1024, bread = 0, btoread = (512);
            do
            {
                bread = read(fd[2], &stripe[offset], 512); 
                offset += bread;
                btoread = (512) - bread;
            }
            while (btoread > 0);
        }

        if(missingChunk == 4)
        {
            printf("will rebuild chunk 4\n");
        }
        else
        {
            offset = 1536, bread = 0, btoread = (512);
            do
            {
                bread = read(fd[3], &stripe[offset], 512); 
                offset += bread;
                btoread = (512) - bread;
            }
            while (btoread > 0);
        }

        if(missingChunk == 5)
        {
            printf("will rebuild chunk 5\n");
        }
        else
        {
            offset = 2048, bread = 0, btoread = (512);
            do
            {
                bread = read(fd[4], &stripe[offset], 512); 
                offset += bread;
                btoread = (512) - bread;
            }
            while (btoread > 0);
        }

        // Rebuild the missing chunk if necessary
        if(missingChunk == 1)
        {
            rebuildLBA(PTR_CAST &stripe[512], 
                       PTR_CAST &stripe[1024], 
                       PTR_CAST &stripe[1536], 
                       PTR_CAST &stripe[2048], 
                       PTR_CAST &stripe[0]);
        }

        if(missingChunk == 2)
        {
            rebuildLBA(PTR_CAST &stripe[0], 
                       PTR_CAST &stripe[1024], 
                       PTR_CAST &stripe[1536], 
                       PTR_CAST &stripe[2048], 
                       PTR_CAST &stripe[512]);
        }

        if(missingChunk == 3)
        {
            rebuildLBA(PTR_CAST &stripe[0],
                       PTR_CAST &stripe[512],
                       PTR_CAST &stripe[1536],
                       PTR_CAST &stripe[2048],
                       PTR_CAST &stripe[1024]);
        }

        if(missingChunk == 4)
        {
            rebuildLBA(PTR_CAST &stripe[0],
                       PTR_CAST &stripe[512],
                       PTR_CAST &stripe[1024],
                       PTR_CAST &stripe[2048],
                       PTR_CAST &stripe[1536]);
        }

        if(missingChunk == 5)
        {
            rebuildLBA(PTR_CAST &stripe[0],
                       PTR_CAST &stripe[512],
                       PTR_CAST &stripe[1024],
                       PTR_CAST &stripe[1536],
                       PTR_CAST &stripe[2048]);
        }

        // Write the restored stripe to the output file
        offset = 0, bwritten = 0, btowrite = (4 * 512);

        do
        {
            bwritten = fwrite(&stripe[offset], 1, btowrite, fdout); 
            offset += bwritten;
            btowrite = (4 * 512) - bwritten;
        }
        while ((btowrite > 0));

    }

    // Handle the last stripe if it's not a full stripe
    if(lastStripeBytes)
    {
        if(missingChunk == 1)
        {
            printf("will rebuild chunk 1\n");
        }
        else
        {
            offset = 0, bread = 0, btoread = (512);
            do
            {
                bread = read(fd[0], &stripe[offset], 512); 
                offset += bread;
                btoread = (512) - bread;
            }
            while (btoread > 0);
        }

        if(missingChunk == 2)
        {
            printf("will rebuild chunk 2\n");
        }
        else
        {
            offset = 512, bread = 0, btoread = (512);
            do
            {
                bread = read(fd[1], &stripe[offset], 512); 
                offset += bread;
                btoread = (512) - bread;
            }
            while (btoread > 0);
        }

        if(missingChunk == 3)
        {
            printf("will rebuild chunk 3\n");
        }
        else
        {
            offset = 1024, bread = 0, btoread = (512);
            do
            {
                bread = read(fd[2], &stripe[offset], 512); 
                offset += bread;
                btoread = (512) - bread;
            }
            while (btoread > 0);
        }

        if(missingChunk == 4)
        {
            printf("will rebuild chunk 4\n");
        }
        else
        {
            offset = 1536, bread = 0, btoread = (512);
            do
            {
                bread = read(fd[3], &stripe[offset], 512); 
                offset += bread;
                btoread = (512) - bread;
            }
            while (btoread > 0);
        }

        if(missingChunk == 5)
        {
            printf("will rebuild chunk 5\n");
        }
        else
        {
            offset = 2048, bread = 0, btoread = (512);
            do
            {
                bread = read(fd[4], &stripe[offset], 512); 
                offset += bread;
                btoread = (512) - bread;
            }
            while (btoread > 0);
        }

        // Rebuild the missing chunk if necessary
        if(missingChunk == 1)
        {
            rebuildLBA(PTR_CAST &stripe[512], 
                       PTR_CAST &stripe[1024], 
                       PTR_CAST &stripe[1536], 
                       PTR_CAST &stripe[2048], 
                       PTR_CAST &stripe[0]);
        }

        if(missingChunk == 2)
        {
            rebuildLBA(PTR_CAST &stripe[0], 
                       PTR_CAST &stripe[1024], 
                       PTR_CAST &stripe[1536], 
                       PTR_CAST &stripe[2048], 
                       PTR_CAST &stripe[512]);
        }

        if(missingChunk == 3)
        {
            rebuildLBA(PTR_CAST &stripe[0],
                       PTR_CAST &stripe[512],
                       PTR_CAST &stripe[1536],
                       PTR_CAST &stripe[2048],
                       PTR_CAST &stripe[1024]);
        }

        if(missingChunk == 4)
        {
            rebuildLBA(PTR_CAST &stripe[0],
                       PTR_CAST &stripe[512],
                       PTR_CAST &stripe[1024],
                       PTR_CAST &stripe[2048],
                       PTR_CAST &stripe[1536]);
        }

        if(missingChunk == 5)
        {
            rebuildLBA(PTR_CAST &stripe[0],
                       PTR_CAST &stripe[512],
                       PTR_CAST &stripe[1024],
                       PTR_CAST &stripe[1536],
                       PTR_CAST &stripe[2048]);
        }

        // Write the last partial stripe to the output file
        offset = 0, bwritten = 0, btowrite = (lastStripeBytes);

        do
        {
            bwritten = fwrite(&stripe[offset], 1, btowrite, fdout); 
            offset += bwritten;
            btowrite = lastStripeBytes - bwritten;
        }
        while ((btowrite > 0));
    }

    // Close the output file and all chunk files
    fclose(fdout);
    for(idx = 0; idx < 5; idx++) close(fd[idx]);

    return(fileLength); // Return the total file length restored
}
