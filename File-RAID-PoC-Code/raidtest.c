#include "raidtest.h"

// Function to modify a buffer by adding an offset to each byte and wrapping around at 100
void modifyBuffer(unsigned char *bufferToModify, int offset)
{
    int idx;

    for(idx = 0; idx < SECTOR_SIZE; idx++)
        bufferToModify[idx] = (bufferToModify[idx] + offset) % 100;
}

// Function to print the contents of a buffer as characters
void printBuffer(char *bufferToPrint)
{
    int idx;

    for(idx = 0; idx < SECTOR_SIZE; idx++)
        printf("%c ", bufferToPrint[idx]);

    printf("\n");
}

// Function to dump the contents of a buffer in hexadecimal format
void dumpBuffer(unsigned char *bufferToDump)
{
    int idx;

    for(idx = 0; idx < SECTOR_SIZE; idx++)
        printf("%x ", bufferToDump[idx]);

    printf("\n");
}

int main(int argc, char *argv[])
{
    int idx, LBAidx, numTestIterations, written = 0;
    int fd[5], fdrebuild;
    double rate = 0.0;
    struct timeval StartTime, StopTime;
    unsigned int microsecs;

    // Check if the number of test iterations is provided as a command-line argument
    if(argc < 2)
    {
        numTestIterations = TEST_ITERATIONS;  // Default to a predefined number of iterations
        printf("Will default to %d iterations\n", TEST_ITERATIONS);
    }
    else
    {
        sscanf(argv[1], "%d", &numTestIterations);  // Parse the number of iterations from the argument
        printf("Will start %d test iterations\n", numTestIterations);
    }

    // Initialize all test buffers with a predefined RAID string
    for(idx = 0; idx < MAX_LBAS; idx++)
    {
        memcpy(&testLBA1[idx], TEST_RAID_STRING, SECTOR_SIZE);
        memcpy(&testLBA2[idx], TEST_RAID_STRING, SECTOR_SIZE);
        memcpy(&testLBA3[idx], TEST_RAID_STRING, SECTOR_SIZE);
        memcpy(&testLBA4[idx], TEST_RAID_STRING, SECTOR_SIZE);
        memcpy(&testRebuild[idx], NULL_RAID_STRING, SECTOR_SIZE);
    }

    // TEST CASE #1: Architecture validation
    printf("Architecture validation:\n");
    printf("sizeof(unsigned long long)=%zu\n", sizeof(unsigned long long));  // Verify size of data type
    printf("\n");

    // TEST CASE #2: Compute XOR from 4 LBAs for RAID-5 and rebuild
    printf("TEST CASE 1:\n");
    modifyBuffer(&(testLBA1[0][0]), 7);   // Modify buffers with different offsets
    modifyBuffer(&(testLBA2[0][0]), 11);
    modifyBuffer(&(testLBA3[0][0]), 13);
    modifyBuffer(&(testLBA4[0][0]), 23);

    // Compute the XOR parity for RAID-5
    xorLBA(PTR_CAST &testLBA1[0],
           PTR_CAST &testLBA2[0],
           PTR_CAST &testLBA3[0],
           PTR_CAST &testLBA4[0],
           PTR_CAST &testPLBA[0]);

    // Rebuild one of the LBAs using the remaining LBAs and the parity
    rebuildLBA(PTR_CAST &testLBA1[0],
               PTR_CAST &testLBA2[0],
               PTR_CAST &testLBA3[0],
               PTR_CAST &testPLBA[0],
               PTR_CAST &testRebuild[0]);

    // Dump the contents of the original and rebuilt buffers to compare
    dumpBuffer((char *)&testLBA4[0]);
    printf("\n");

    dumpBuffer((char *)&testRebuild[0]);
    printf("\n");

    // Ensure that the rebuilt buffer matches the original
    assert(memcmp(testRebuild, testLBA4, SECTOR_SIZE) == 0);

    // Save the chunks to binary files for further inspection
    fd[0] = open("Chunk1.bin", O_RDWR | O_CREAT, 00644);
    fd[1] = open("Chunk2.bin", O_RDWR | O_CREAT, 00644);
    fd[2] = open("Chunk3.bin", O_RDWR | O_CREAT, 00644);
    fd[3] = open("Chunk4.bin", O_RDWR | O_CREAT, 00644);
    fd[4] = open("ChunkXOR.bin", O_RDWR | O_CREAT, 00644);

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
    for(idx = 0; idx < 5; idx++) close(fd[idx]);

    // Save the rebuilt 4th chunk to a binary file
    fdrebuild = open("Chunk4_Rebuilt.bin", O_RDWR | O_CREAT, 00644);
    written = write(fdrebuild, &testRebuild[0], SECTOR_SIZE);
    assert(written == SECTOR_SIZE);
    close(fdrebuild);

    // TEST CASE #3: Verify rebuild across multiple iterations
    printf("TEST CASE 2 (randomized sectors and rebuild):\n");

    // Loop over the number of test iterations
    for(idx = 0; idx < numTestIterations; idx++)
    {
        LBAidx = idx % MAX_LBAS;  // Use modulo to cycle through LBAs
        printf("%d ", LBAidx);

        // Compute XOR and rebuild for the current LBA
        xorLBA(PTR_CAST &testLBA1[LBAidx],
               PTR_CAST &testLBA2[LBAidx],
               PTR_CAST &testLBA3[LBAidx],
               PTR_CAST &testLBA4[LBAidx],
               PTR_CAST &testPLBA[LBAidx]);

        rebuildLBA(PTR_CAST &testLBA1[LBAidx],
                   PTR_CAST &testLBA2[LBAidx],
                   PTR_CAST &testLBA3[LBAidx],
                   PTR_CAST &testPLBA[LBAidx],
                   PTR_CAST &testRebuild[LBAidx]);

        // Ensure that the rebuilt LBA matches the original
        assert(memcmp(&testRebuild[LBAidx], &testLBA4[LBAidx], SECTOR_SIZE) == 0);

        // Modify the contents of testLBA4 for the next iteration
        modifyBuffer(&(testLBA4[LBAidx][0]), 17);
    }
    printf("\n");

    // End of tests
    printf("FINISHED\n");
}
