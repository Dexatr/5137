#include "raidtest.h"
#include <omp.h> // Include OpenMP for parallel processing
#include <time.h> // Include for high-precision timing

int main(int argc, char *argv[])
{
    int idx, LBAidx, numTestIterations;
    double rate = 0.0;
    struct timespec StartTime, StopTime; // Structure to store start and stop times
    long microsecs; // Variable to store elapsed time in microseconds

    // Check if the number of test iterations is provided as an argument
    if(argc < 2)
    {
        numTestIterations = TEST_ITERATIONS; // Default to a predefined number of iterations
        printf("Will default to %d iterations\n", TEST_ITERATIONS);
    }
    else
    {
        sscanf(argv[1], "%d", &numTestIterations); // Parse the number of iterations from the command line argument
        printf("Will start %d test iterations\n", numTestIterations);
    }

    // Initialize all test buffers in parallel using OpenMP
    #pragma omp parallel for
    for(idx = 0; idx < MAX_LBAS; idx++)
    {
        memcpy(&testLBA1[idx], TEST_RAID_STRING, SECTOR_SIZE);
        memcpy(&testLBA2[idx], TEST_RAID_STRING, SECTOR_SIZE);
        memcpy(&testLBA3[idx], TEST_RAID_STRING, SECTOR_SIZE);
        memcpy(&testLBA4[idx], TEST_RAID_STRING, SECTOR_SIZE);
        memcpy(&testRebuild[idx], NULL_RAID_STRING, SECTOR_SIZE);
    }

    // TEST CASE #1: RAID Operations Performance Test
    printf("\nRAID Operations Performance Test\n");

    clock_gettime(CLOCK_MONOTONIC, &StartTime); // Record the start time

    // Perform the RAID operations in parallel using OpenMP
    #pragma omp parallel for private(LBAidx) 
    for(idx = 0; idx < numTestIterations; idx++)
    {
        LBAidx = idx % MAX_LBAS;

        // Compute XOR from 4 LBAs for RAID-5 encoding
        xorLBA(PTR_CAST &testLBA1[LBAidx],
               PTR_CAST &testLBA2[LBAidx],
               PTR_CAST &testLBA3[LBAidx],
               PTR_CAST &testLBA4[LBAidx],
               PTR_CAST &testPLBA[LBAidx]);

        // Rebuild the LBA to verify the correctness of the RAID operation
        rebuildLBA(PTR_CAST &testLBA1[LBAidx],
                   PTR_CAST &testLBA2[LBAidx],
                   PTR_CAST &testLBA3[LBAidx],
                   PTR_CAST &testPLBA[LBAidx],
                   PTR_CAST &testRebuild[LBAidx]);
    }

    clock_gettime(CLOCK_MONOTONIC, &StopTime); // Record the stop time

    // Calculate the elapsed time in microseconds
    microsecs = (StopTime.tv_sec - StartTime.tv_sec) * 1000000L + 
                (StopTime.tv_nsec - StartTime.tv_nsec) / 1000;

    // Display the elapsed time and performance metrics
    printf("Test Done in %ld microsecs for %d iterations\n", microsecs, numTestIterations);

    rate = ((double)numTestIterations) / (((double)microsecs) / 1000000.0); // Calculate the RAID operations per second
    printf("%lf RAID ops computed per second\n", rate);
    printf("Average time per RAID operation: %lf microsecs\n", (double)microsecs / numTestIterations);

    // END TEST CASE #1
}
