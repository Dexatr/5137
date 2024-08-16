#ifndef RAIDLIB_H
#define RAIDLIB_H

#include <unistd.h>
#include <omp.h>  // Include OpenMP for parallelization if needed

#define OK (0)
#define ERROR (-1)
#define TRUE (1)
#define FALSE (0)

#define SECTOR_SIZE (512)

// Function to compute XOR for RAID-5 encoding
void xorLBA(unsigned char *LBA1,
            unsigned char *LBA2,
            unsigned char *LBA3,
            unsigned char *LBA4,
            unsigned char *PLBA);

// Function to rebuild a lost LBA using the remaining LBAs and the parity LBA
void rebuildLBA(unsigned char *LBA1,
                unsigned char *LBA2,
                unsigned char *LBA3,
                unsigned char *PLBA,
                unsigned char *RLBA);

// Function to check if two LBAs are equivalent
int checkEquivLBA(unsigned char *LBA1,
                  unsigned char *LBA2);

// Function to stripe a file across multiple chunks
int stripeFile(char *inputFileName, int offsetSectors);

// Function to restore a file from its striped chunks
int restoreFile(char *outputFileName, int offsetSectors, int fileLength, int missingChunk);

#endif
