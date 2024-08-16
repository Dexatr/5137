#include <stdio.h>
#include <stdlib.h>

#include "raidlib.h" // Include the custom RAID library header

int main(int argc, char *argv[])
{
    int bytesWritten, bytesRestored;
    char rc;
    int chunkToRebuild = 0; // Variable to store the chunk number to rebuild

    // Check if the correct number of arguments are provided
    if(argc < 3)
    {
        printf("usage: stripetest inputfile outputfile <sector to restore>\n");
        exit(-1); // Exit with an error code if insufficient arguments
    }
    
    // If the fourth argument is provided, parse it to determine the chunk to rebuild
    if(argc >= 4)
    {
        sscanf(argv[3], "%d", &chunkToRebuild); // Convert the argument to an integer
        printf("chunk to restore = %d\n", chunkToRebuild);
    }
   
    // Stripe the input file across 4 data chunks + 1 XOR parity chunk
    bytesWritten = stripeFile(argv[1], 0); 

    // Inform the user that the input file has been written into chunks
    printf("input file was written as 4 data chunks + 1 XOR parity - could have been on 5 devices\n");
    printf("Remove chunk %d and enter g for go - could have been on 5 devices\n", chunkToRebuild);
    printf("Hit return to start rebuild:");

    // Wait for the user to press 'g' and then hit return to start the rebuild process
    rc = getchar();

    // Start the file restoration process
    printf("working on restoring file ...\n");

    // Restore the file from the available chunks and rebuild the specified chunk if needed
    bytesRestored = restoreFile(argv[2], 0, bytesWritten, chunkToRebuild); 

    // Indicate that the restoration process is complete
    printf("FINISHED\n");
}
