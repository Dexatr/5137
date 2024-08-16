# Directories for include files and libraries
INCLUDE_DIRS = 
LIB_DIRS = 

# Compiler definitions and flags
CDEFS = 
# CFLAGS specifies the compiler options:
# -O0: No optimization (useful for debugging)
# -g: Generate debug information
# $(INCLUDE_DIRS): Include directories specified
# $(CDEFS): Compiler definitions specified
CFLAGS = -O0 -g $(INCLUDE_DIRS) $(CDEFS)
# Alternative CFLAGS (commented out) for optimization:
# -O3: High-level optimization
# -msse3: Use SSE3 instructions
# -malign-double: Align double variables on a double word boundary
# CFLAGS = -O3 -msse3 -malign-double -g $(INCLUDE_DIRS) $(CDEFS)
LIBS = 

# Names of the driver programs to be created
DRIVER = raidtest raid_perftest stripetest

# Header and source files
HFILES = raidlib.h
CFILES = raidlib.c

# Source files and object files
SRCS = ${HFILES} ${CFILES}
OBJS = ${CFILES:.c=.o}  # Converts .c file names to .o (object files)

# The default target, which will build all driver programs
all: ${DRIVER}

# Clean target to remove compiled files and binaries
clean:
	-rm -f *.o *.NEW *~ *Chunk*.bin  # Remove object files, temporary files, and chunk files
	-rm -f ${DRIVER} ${DERIVED} ${GARBAGE}  # Remove driver binaries and other derived/garbage files

# Rules to build the driver programs
# Link the object files and specific source files to create the raidtest binary
raidtest: ${OBJS} raidtest.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(OBJS) raidtest.o $(LIBS)

# Link the object files and specific source files to create the stripetest binary
stripetest: ${OBJS} stripetest.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(OBJS) stripetest.o $(LIBS)

# Link the object files and specific source files to create the raid_perftest binary
raid_perftest: ${OBJS} raid_perftest.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(OBJS) raid_perftest.o $(LIBS)

# Placeholder for a dependency rule (not used in this Makefile)
depend:

# Generic rule to compile .c files to .o files
.c.o:
	$(CC) $(CFLAGS) -c $<
