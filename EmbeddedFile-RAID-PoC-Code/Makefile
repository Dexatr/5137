INCLUDE_DIRS = 
LIB_DIRS = 

CDEFS=
# Optimization Flags
CFLAGS= -O3 -march=native -flto -funroll-loops -g $(INCLUDE_DIRS) $(CDEFS)
# Advanced Optimization Flags (commented out by default, can be used if supported)
#CFLAGS += -msse3 -malign-double -fstrict-aliasing -ffast-math

LIBS=

DRIVER=raidtest raid_perftest stripetest

HFILES= raidlib.h raidtest.h
CFILES= raidlib.c raid_shared.c

SRCS= ${HFILES} ${CFILES}
OBJS= raidlib.o raid_shared.o

all:	${DRIVER}

clean:
	-rm -f *.o *.NEW *~ *Chunk*.bin
	-rm -f ${DRIVER} ${DERIVED} ${GARBAGE}
	-rm -f output.ppm  # Remove the output PPM file

raidtest:	${OBJS} raidtest.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ ${OBJS} raidtest.o $(LIBS)

stripetest:	${OBJS} stripetest.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ ${OBJS} stripetest.o $(LIBS)

raid_perftest:	${OBJS} raid_perftest.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ ${OBJS} raid_perftest.o $(LIBS)

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
