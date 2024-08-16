#!/bin/bash
#
# RAID Unit level regression script
#

# Function to log messages with timestamps
log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $1"
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $1" >> testresults.log
}

# Function to log and display error messages
error_log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - ERROR: $1" | tee -a testresults.log
    exit 1
}

# Function to ensure RAID is fully unmounted and cleaned up before tests
cleanup_raid() {
    log "Unmounting and cleaning up RAID devices..."
    sudo umount /mnt/raid 2>/dev/null || true
    sudo mdadm --stop /dev/md0 2>/dev/null || true
    sudo losetup -d /dev/loop99 2>/dev/null || true
    sudo losetup -d /dev/loop100 2>/dev/null || true
    sudo losetup -d /dev/loop101 2>/dev/null || true
    sudo losetup -d /dev/loop102 2>/dev/null || true
    sudo losetup -d /dev/loop103 2>/dev/null || true
    log "RAID cleanup completed."
}

# Add the current directory to the PATH to ensure binaries are found
export PATH=.:$PATH

# Cleanup RAID before starting tests
cleanup_raid

log "RAID UNIT REGRESSION TEST"

log "TEST SET 1: Checkout and build test"
make clean >> testresults.log 2>&1 || error_log "make clean failed."

git pull >> testresults.log 2>&1 || error_log "git pull failed."

make -j$(nproc) >> testresults.log 2>&1 || error_log "make build failed."

log "TEST SET 2: Simple RAID encode, erase, and rebuild test"
./raidtest 1000 >> testresults.log 2>&1 || error_log "raidtest failed."

log "TEST SET 3: Rebuild diff check for Chunk4 rebuild"

# Log file sizes and checksums for debugging
for chunk in Chunk1 Chunk2 Chunk3 Chunk4; do
    log "Checksum and size for $chunk:"
    sha256sum ${chunk}.bin >> testresults.log 2>&1
    wc -c ${chunk}.bin >> testresults.log 2>&1
done

log "Checksum and size for Chunk4_Rebuilt.bin:"
sha256sum Chunk4_Rebuilt.bin >> testresults.log 2>&1
wc -c Chunk4_Rebuilt.bin >> testresults.log 2>&1

log "Performing diff check..."
diff Chunk4.bin Chunk4_Rebuilt.bin >> testresults.log 2>&1
if [ $? -ne 0 ]; then
    error_log "Diff check failed for Chunk4."
fi

log "TEST SET 4: RAID performance test"
./raid_perftest 1000 >> testresults.log 2>&1 || error_log "raid_perftest failed."

# Re-run cleanup after tests
cleanup_raid

log "RAID UNIT REGRESSION TEST COMPLETED SUCCESSFULLY"
