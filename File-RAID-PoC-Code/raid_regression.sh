#!/bin/bash
#
# RAID Unit level regression script
# This script runs a series of tests to validate the RAID implementation.
#

# Log the start of the RAID regression test
echo "RAID UNIT REGRESSSION TEST"
echo "RAID UNIT REGRESSSION TEST" >> testresults.log
echo "" >> testresults.log

# TEST SET 1: Checkout and Build Test
# This section cleans the build environment, updates the code from version control, and rebuilds the project.
echo "TEST SET 1: checkout and build test"
echo "TEST SET 1: checkout and build test" >> testresults.log
make clean >> testresults.log  # Clean the build environment
git pull >> testresults.log    # Update the code from the version control repository
make >> testresults.log        # Rebuild the project
echo "" >> testresults.log

# TEST SET 2: Simple RAID Encode, Erase, and Rebuild Test
# This test runs the 'raidtest' executable with 1000 iterations to validate the RAID encode, erase, and rebuild functionality.
echo "TEST SET 2: simple RAID encode, erase and rebuild test"
echo "TEST SET 2: simple RAID encode, erase and rebuild test" >> testresults.log
./raidtest 1000 >> testresults.log  # Run the RAID test and log the output
echo "" >> testresults.log

# TEST SET 3: Rebuild Diff Check for Chunk4 Rebuild
# This test compares the original data chunks with the rebuilt chunk to ensure that the rebuild process is accurate.
echo "TEST SET 3: REBUILD DIFF CHECK for Chunk4 rebuild"
echo "TEST SET 3: REBUILD DIFF CHECK for Chunk4 rebuild" >> testresults.log
diff Chunk1.bin Chunk4_Rebuilt.bin >> testresults.log  # Compare Chunk1 with the rebuilt Chunk4
diff Chunk2.bin Chunk4_Rebuilt.bin >> testresults.log  # Compare Chunk2 with the rebuilt Chunk4
diff Chunk3.bin Chunk4_Rebuilt.bin >> testresults.log  # Compare Chunk3 with the rebuilt Chunk4
diff Chunk4.bin Chunk4_Rebuilt.bin >> testresults.log  # Compare the original Chunk4 with the rebuilt Chunk4
echo "" >> testresults.log

# TEST SET 4: RAID Performance Test
# This test measures the performance of the RAID operations by running the 'raid_perftest' executable.
echo "TEST SET 4: RAID performance test"
echo "TEST SET 4: RAID performance test" >> testresults.log
./raid_perftest 1000 >> testresults.log  # Run the RAID performance test and log the output
echo "" >> testresults.log
