#include "raidtest.h"

int main(int argc, char *argv[])
{
    int idx, LBAidx, numTestIterations, rc;
    double rate = 0.0;
    double totalRate = 0.0, aveRate = 0.0;
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

    // TEST CASE #1: RAID Operations Performance Test
    //
    // This test case computes the XOR parity from 4 test buffers (512 bytes each) and then
    // rebuilds a missing buffer (buffer 4 in the test case). The time taken for these operations
    // is measured to evaluate the performance.
    //
    printf("\nRAID Operations Performance Test\n");

    // Start timing the RAID operations
    gettimeofday(&StartTime, 0);

    for(idx = 0; idx < numTestIterations; idx++)
    {
        LBAidx = idx % MAX_LBAS;  // Use modulo to cycle through LBAs

        // Compute XOR parity for RAID-5 from 4 LBAs
        xorLBA(PTR_CAST &testLBA1[LBAidx],
               PTR_CAST &testLBA2[LBAidx],
               PTR_CAST &testLBA3[LBAidx],
               PTR_CAST &testLBA4[LBAidx],
               PTR_CAST &testPLBA[LBAidx]);

        // Rebuild one of the LBAs using the remaining LBAs and the parity
        rebuildLBA(PTR_CAST &testLBA1[LBAidx],
                   PTR_CAST &testLBA2[LBAidx],
                   PTR_CAST &testLBA3[LBAidx],
                   PTR_CAST &testPLBA[LBAidx],
                   PTR_CAST &testRebuild[LBAidx]);
    }

    // Stop timing the RAID operations
    gettimeofday(&StopTime, 0);

    // Calculate the total time taken in microseconds
    microsecs = ((StopTime.tv_sec - StartTime.tv_sec) * 1000000);

    if(StopTime.tv_usec > StartTime.tv_usec)
        microsecs += (StopTime.tv_usec - StartTime.tv_usec);
    else
        microsecs -= (StartTime.tv_usec - StopTime.tv_usec);

    // Output the total time taken and the rate of RAID operations per second
    printf("Test Done in %u microsecs for %d iterations\n", microsecs, numTestIterations);

    rate = ((double)numTestIterations) / (((double)microsecs) / 1000000.0);
    printf("%lf RAID ops computed per second\n", rate);

    // END TEST CASE #1
}
