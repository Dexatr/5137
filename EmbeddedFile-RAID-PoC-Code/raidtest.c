#include "raidtest.h"

// Function to modify the contents of a buffer by adding an offset to each byte
void modifyBuffer(unsigned char *bufferToModify, int offset)
{
    int idx;

    for(idx=0; idx < SECTOR_SIZE; idx++)
        bufferToModify[idx] = (bufferToModify[idx]+offset) % 100;
}

// Function to print the contents of a buffer as characters
void printBuffer(char *bufferToPrint)
{
    int idx;

    for(idx=0; idx < SECTOR_SIZE; idx++)
        printf("%c ", bufferToPrint[idx]);

    printf("\n");
}

// Function to dump the contents of a buffer in hexadecimal format
void dumpBuffer(unsigned char *bufferToDump)
{
    int idx;

    for(idx=0; idx < SECTOR_SIZE; idx++)
        printf("%x ", bufferToDump[idx]);

    printf("\n");
}

int main(int argc, char *argv[])
{
    int idx, LBAidx, numTestIterations, rc;
    int written=0, fd[5];
    int fdrebuild;
    double rate=0.0;
    double totalRate=0.0, aveRate=0.0;
    struct timeval StartTime, StopTime;
    unsigned int microsecs;

    // Check for the number of test iterations passed as a command-line argument
    if(argc < 2)
    {
        numTestIterations=TEST_ITERATIONS;
        printf("Will default to %d iterations\n", TEST_ITERATIONS);
    }
    else
    {
        sscanf(argv[1], "%d", &numTestIterations);
        printf("Will start %d test iterations\n", numTestIterations);
    }

    // Initialize test buffers with predefined RAID strings
    for(idx=0; idx<MAX_LBAS; idx++)
    {
        memcpy(&testLBA1[idx], TEST_RAID_STRING, SECTOR_SIZE);
        memcpy(&testLBA2[idx], TEST_RAID_STRING, SECTOR_SIZE);
        memcpy(&testLBA3[idx], TEST_RAID_STRING, SECTOR_SIZE);
        memcpy(&testLBA4[idx], TEST_RAID_STRING, SECTOR_SIZE);
        memcpy(&testRebuild[idx], NULL_RAID_STRING, SECTOR_SIZE);
    }

    // TEST CASE #1: Architecture validation and simple RAID-5 XOR and rebuild
    printf("Architecture validation:\n");
    printf("sizeof(unsigned long long)=%d\n", (int)sizeof(unsigned long long));
    printf("\n");

    printf("TEST CASE 1:\n");
    // Modify test buffers with different offsets
    modifyBuffer(&(testLBA1[0][0]), 7);
    modifyBuffer(&(testLBA2[0][0]), 11);
    modifyBuffer(&(testLBA3[0][0]), 13);
    modifyBuffer(&(testLBA4[0][0]), 23);

    // Compute XOR for RAID-5 and store in parity LBA
    xorLBA(PTR_CAST &testLBA1[0],
           PTR_CAST &testLBA2[0],
           PTR_CAST &testLBA3[0],
           PTR_CAST &testLBA4[0],
           PTR_CAST &testPLBA[0]);

    // Rebuild the fourth LBA using the parity and the other three LBAs
    rebuildLBA(PTR_CAST &testLBA1[0],
               PTR_CAST &testLBA2[0],
               PTR_CAST &testLBA3[0],
               PTR_CAST &testPLBA[0],
               PTR_CAST &testRebuild[0]);

    // Display the original and rebuilt LBAs
    dumpBuffer((unsigned char *)&testLBA4[0]);
    printf("\n");
    dumpBuffer((unsigned char *)&testRebuild[0]);
    printf("\n");

    // Ensure that the rebuilt LBA matches the original LBA4
    assert(memcmp(testRebuild, testLBA4, SECTOR_SIZE) == 0);

    // Write LBAs to binary files for further analysis
    fd[0] = open("Chunk1.bin", O_RDWR | O_CREAT, 00644);
    fd[1] = open("Chunk2.bin", O_RDWR | O_CREAT, 00644);
    fd[2] = open("Chunk3.bin", O_RDWR | O_CREAT, 00644);
    fd[3] = open("Chunk4.bin", O_RDWR | O_CREAT, 00644);
    fd[4] = open("ChunkXOR.bin", O_RDWR | O_CREAT, 00644);

    // Write the LBAs to their respective files
    written = write(fd[0], &testLBA1[0], SECTOR_SIZE);
    assert(written == SECTOR_SIZE);
    written = write(fd[1], &testLBA2[0], SECTOR_SIZE);
    assert(written == SECTOR_SIZE);
    written = write(fd[2], &testLBA3[0], SECTOR_SIZE);
    assert(written == SECTOR_SIZE);
    written = write(fd[3], &testLBA4[0], SECTOR_SIZE);
    assert(written == SECTOR_SIZE);
    written = write(fd[4], &testPLBA[0], SECTOR_SIZE);
    assert(written == SECTOR_SIZE);

    // Close all file descriptors
    for(idx=0; idx < 5; idx++) close(fd[idx]);

    // Write the rebuilt LBA to a binary file
    fdrebuild = open("Chunk4_Rebuilt.bin", O_RDWR | O_CREAT, 00644);
    written = write(fdrebuild, &testRebuild[0], SECTOR_SIZE);
    assert(written == SECTOR_SIZE);
    close(fdrebuild);

    // TEST CASE #2: Randomized sector testing and rebuild
    printf("TEST CASE 2 (randomized sectors and rebuild):\n");

    for(idx=0; idx < numTestIterations; idx++)
    {
        LBAidx = idx % MAX_LBAS;
        printf("%d ", LBAidx);

        // Compute XOR for RAID-5 for the current LBA
        xorLBA(PTR_CAST &testLBA1[LBAidx],
               PTR_CAST &testLBA2[LBAidx],
               PTR_CAST &testLBA3[LBAidx],
               PTR_CAST &testLBA4[LBAidx],
               PTR_CAST &testPLBA[LBAidx]);

        // Rebuild the LBA and compare with the original
        rebuildLBA(PTR_CAST &testLBA1[LBAidx],
                   PTR_CAST &testLBA2[LBAidx],
                   PTR_CAST &testLBA3[LBAidx],
                   PTR_CAST &testPLBA[LBAidx],
                   PTR_CAST &testRebuild[LBAidx]);

        // Ensure the rebuilt LBA matches the original LBA4
        assert(memcmp(&testRebuild[LBAidx], &testLBA4[LBAidx], SECTOR_SIZE) == 0);

        // Modify the contents of testLBA4 for the next iteration
        modifyBuffer(&(testLBA4[LBAidx][0]), 17);
    }
    printf("\n");

    printf("FINISHED\n");
}
