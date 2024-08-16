#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <assert.h>
#include <omp.h> // Include OpenMP for parallel processing

#include "raidlib.h"

#ifdef RAID64
#include "raidlib64.h"
#define PTR_CAST (unsigned long long *)
#else
#include "raidlib.h"
#define PTR_CAST (unsigned char *)
#endif

// Handle O_DIRECT compatibility
#ifndef O_DIRECT
#define O_DIRECT 0 // Fallback to normal file I/O if O_DIRECT is unavailable
#endif

// RAID-5 encoding function
// This function takes in four logical block addresses (LBAs) and computes their XOR to produce parity (PLBA)
void xorLBA(unsigned char *LBA1,
            unsigned char *LBA2,
            unsigned char *LBA3,
            unsigned char *LBA4,
            unsigned char *PLBA)
{
    int idx;

    #pragma omp parallel for // Parallelize loop for better performance
    for(idx=0; idx<SECTOR_SIZE; idx++)
    {
        // Prefetch data to improve cache performance
        __builtin_prefetch(&LBA1[idx + 16], 0, 1);
        __builtin_prefetch(&LBA2[idx + 16], 0, 1);
        __builtin_prefetch(&LBA3[idx + 16], 0, 1);
        __builtin_prefetch(&LBA4[idx + 16], 0, 1);
        // Compute XOR across the four LBAs to generate parity
        *(PLBA+idx) = (*(LBA1+idx))^(*(LBA2+idx))^(*(LBA3+idx))^(*(LBA4+idx));
    }
}

// RAID-5 Rebuild function
// This function takes in three LBAs and the parity LBA to rebuild the missing LBA (RLBA)
void rebuildLBA(unsigned char *LBA1,
                unsigned char *LBA2,
                unsigned char *LBA3,
                unsigned char *PLBA,
                unsigned char *RLBA)
{
    int idx;
    unsigned char checkParity;

    #pragma omp parallel for // Parallelize loop for better performance
    for(idx=0; idx<SECTOR_SIZE; idx++)
    {
        // Prefetch data to improve cache performance
        __builtin_prefetch(&LBA1[idx + 16], 0, 1);
        __builtin_prefetch(&LBA2[idx + 16], 0, 1);
        __builtin_prefetch(&LBA3[idx + 16], 0, 1);
        // Compute checkParity by XORing the available LBAs
        checkParity = (*(LBA1+idx))^(*(LBA2+idx))^(*(LBA3+idx));

        // Rebuild RLBA by XORing the parity LBA with the computed checkParity
        *(RLBA+idx) = (*(PLBA+idx))^(checkParity);
    }
}

// Function to stripe a file across multiple RAID chunks
// It takes an input file and stripes its contents across four chunks, then computes the XOR parity chunk
// Returns the number of bytes written or an error code
int stripeFile(char *inputFileName, int offsetSectors)
{
    int fd[5], idx;
    FILE *fdin;
    unsigned char stripe[5*512];
    int offset=0, bread=0, btoread=(4*512), bwritten=0, btowrite=(512), sectorCnt=0, byteCnt=0;

    // Open the input file and create/open the RAID chunks for writing
    fdin = fopen(inputFileName, "r");
    fd[0] = open("StripeChunk1.bin", O_RDWR | O_CREAT | O_DIRECT, 00644);
    fd[1] = open("StripeChunk2.bin", O_RDWR | O_CREAT | O_DIRECT, 00644);
    fd[2] = open("StripeChunk3.bin", O_RDWR | O_CREAT | O_DIRECT, 00644);
    fd[3] = open("StripeChunk4.bin", O_RDWR | O_CREAT | O_DIRECT, 00644);
    fd[4] = open("StripeChunkXOR.bin", O_RDWR | O_CREAT | O_DIRECT, 00644);

    do
    {
        // Read a stripe (four chunks) or until the end of file
        offset=0, bread=0, btoread=(4*512);
        do
        {
            bread=fread(&stripe[offset], 1, btoread, fdin); 
            offset+=bread;
            btoread=(4*512)-bread;
        }
        while (!(feof(fdin)) && (btoread > 0));

        // Zero-fill the remaining space if we reach the end of file with a partial stripe
        if((offset < (4*512)) && (feof(fdin)))
        {
            bzero(&stripe[offset], btoread);
            byteCnt+=offset;
        }
        else
        {
            assert(offset == (4*512));
            byteCnt+=(4*512);
        };

        // Compute XOR parity for the stripe
        xorLBA(PTR_CAST &stripe[0],
               PTR_CAST &stripe[512],
               PTR_CAST &stripe[1024],
               PTR_CAST &stripe[1536],
               PTR_CAST &stripe[2048]);

        // Write out the stripe and its XOR parity chunk to the respective files
        for (int i = 0; i < 5; i++)
        {
            offset = i * 512;
            bwritten = 0;
            btowrite = 512;
            do
            {
                bwritten=write(fd[i], &stripe[offset], 512); 
                offset+=bwritten;
                btowrite=(512)-bwritten;
            }
            while (btowrite > 0);
        }

        sectorCnt+=4;

    } while (!(feof(fdin)));

    // Close all file descriptors
    fclose(fdin);
    for(idx=0; idx < 5; idx++) close(fd[idx]);

    return(byteCnt); // Return the total number of bytes written
}

// Function to restore a file from RAID chunks
// It reads the striped data from the chunks and writes it back into the output file
// If a chunk is missing, it is rebuilt using the other chunks and the XOR parity
// Returns the number of bytes read or an error code
int restoreFile(char *outputFileName, int offsetSectors, int fileLength, int missingChunk)
{
    int fd[5], idx;
    FILE *fdout;
    unsigned char stripe[5*512];
    int offset=0, bread=0, btoread=(4*512), bwritten=0, btowrite=(512), sectorCnt=fileLength/512;
    int stripeCnt=fileLength/(4*512);
    int lastStripeBytes = fileLength % (4*512);

    // Open the output file for writing and the RAID chunks for reading
    fdout = fopen(outputFileName, "w");

    fd[0] = open("StripeChunk1.bin", O_RDWR | O_CREAT | O_DIRECT, 00644);
    fd[1] = open("StripeChunk2.bin", O_RDWR | O_CREAT | O_DIRECT, 00644);
    fd[2] = open("StripeChunk3.bin", O_RDWR | O_CREAT | O_DIRECT, 00644);
    fd[3] = open("StripeChunk4.bin", O_RDWR | O_CREAT | O_DIRECT, 00644);
    fd[4] = open("StripeChunkXOR.bin", O_RDWR | O_CREAT | O_DIRECT, 00644);

    for(idx=0; idx < stripeCnt; idx++)
    {
        // Read in the stripe and its XOR parity chunk
        for (int i = 0; i < 5; i++)
        {
            if (i+1 == missingChunk)
            {
                continue; // Skip reading if this chunk is missing
            }

            offset = i * 512;
            bread = 0;
            btoread = 512;
            do
            {
                bread = read(fd[i], &stripe[offset], 512); 
                offset += bread;
                btoread = 512 - bread;
            }
            while (btoread > 0);
        }

        // Rebuild the missing chunk using the remaining chunks and the XOR parity
        switch(missingChunk)
        {
            case 1:
                rebuildLBA(PTR_CAST &stripe[512], 
                           PTR_CAST &stripe[1024], 
                           PTR_CAST &stripe[1536], 
                           PTR_CAST &stripe[2048], 
                           PTR_CAST &stripe[0]);
                break;
            case 2:
                rebuildLBA(PTR_CAST &stripe[0], 
                           PTR_CAST &stripe[1024], 
                           PTR_CAST &stripe[1536], 
                           PTR_CAST &stripe[2048], 
                           PTR_CAST &stripe[512]);
                break;
            case 3:
                rebuildLBA(PTR_CAST &stripe[0],
                           PTR_CAST &stripe[512],
                           PTR_CAST &stripe[1536],
                           PTR_CAST &stripe[2048],
                           PTR_CAST &stripe[1024]);
                break;
            case 4:
                rebuildLBA(PTR_CAST &stripe[0],
                           PTR_CAST &stripe[512],
                           PTR_CAST &stripe[1024],
                           PTR_CAST &stripe[2048],
                           PTR_CAST &stripe[1536]);
                break;
            case 5:
                rebuildLBA(PTR_CAST &stripe[0],
                           PTR_CAST &stripe[512],
                           PTR_CAST &stripe[1024],
                           PTR_CAST &stripe[1536],
                           PTR_CAST &stripe[2048]);
                break;
        }

        // Write the restored stripe to the output file
        offset=0, bwritten=0, btowrite=(4*512);
        do
        {
            bwritten=fwrite(&stripe[offset], 1, btowrite, fdout); 
            offset+=bwritten;
            btowrite=(4*512)-bwritten;
        }
        while ((btowrite > 0));
    }

    if(lastStripeBytes)
    {
        // Read in the partial stripe and its XOR parity chunk
        for (int i = 0; i < 5; i++)
        {
            if (i+1 == missingChunk)
            {
                continue; // Skip reading if this chunk is missing
            }

            offset = i * 512;
            bread = 0;
            btoread = 512;
            do
            {
                bread = read(fd[i], &stripe[offset], 512); 
                offset += bread;
                btoread = 512 - bread;
            }
            while (btoread > 0);
        }

        // Rebuild the missing chunk for the partial stripe
        switch(missingChunk)
        {
            case 1:
                rebuildLBA(PTR_CAST &stripe[512], 
                           PTR_CAST &stripe[1024], 
                           PTR_CAST &stripe[1536], 
                           PTR_CAST &stripe[2048], 
                           PTR_CAST &stripe[0]);
                break;
            case 2:
                rebuildLBA(PTR_CAST &stripe[0], 
                           PTR_CAST &stripe[1024], 
                           PTR_CAST &stripe[1536], 
                           PTR_CAST &stripe[2048], 
                           PTR_CAST &stripe[512]);
                break;
            case 3:
                rebuildLBA(PTR_CAST &stripe[0],
                           PTR_CAST &stripe[512],
                           PTR_CAST &stripe[1536],
                           PTR_CAST &stripe[2048],
                           PTR_CAST &stripe[1024]);
                break;
            case 4:
                rebuildLBA(PTR_CAST &stripe[0],
                           PTR_CAST &stripe[512],
                           PTR_CAST &stripe[1024],
                           PTR_CAST &stripe[2048],
                           PTR_CAST &stripe[1536]);
                break;
            case 5:
                rebuildLBA(PTR_CAST &stripe[0],
                           PTR_CAST &stripe[512],
                           PTR_CAST &stripe[1024],
                           PTR_CAST &stripe[1536],
                           PTR_CAST &stripe[2048]);
                break;
        }

        // Write the restored partial stripe to the output file
        offset=0, bwritten=0, btowrite=(lastStripeBytes);
        do
        {
            bwritten=fwrite(&stripe[offset], 1, btowrite, fdout); 
            offset+=bwritten;
            btowrite=lastStripeBytes-bwritten;
        }
        while ((btowrite > 0));
    }

    // Close all file descriptors
    fclose(fdout);
    for(idx=0; idx < 5; idx++) close(fd[idx]);

    return(fileLength); // Return the total number of bytes read
}
