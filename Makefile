CC=gcc
CFLAGS=-DNDEBUG -Wall

# Default values if not provided on command line
framesize ?= 18
varmemsize ?= 10

DFLAGS=-DFRAME_STORE_SIZE=$(framesize) -DVAR_MEM_SIZE=$(varmemsize)
SRCS=shell.c interpreter.c shellmemory.c pcb.c queue.c schedule_policy.c
OBJS=shell.o interpreter.o shellmemory.o pcb.o queue.o schedule_policy.o

mysh: $(SRCS)
	$(CC) $(CFLAGS) $(DFLAGS) -c $(SRCS)
	$(CC) $(CFLAGS) $(DFLAGS) -o mysh $(OBJS) -lpthread

clean:
	rm -f mysh *.o
