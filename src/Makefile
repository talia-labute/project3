CC=gcc
CFLAGS=-DNDEBUG -Wall

framesize  ?= 18
varmemsize ?= 10
CFLAGS += -DFRAME_STORE_SIZE=$(framesize) -DVAR_MEM_SIZE=$(varmemsize)

mysh: shell.c interpreter.c shellmemory.c pcb.c queue.c schedule_policy.c
	$(CC) $(CFLAGS) -c shell.c interpreter.c shellmemory.c pcb.c queue.c schedule_policy.c
	$(CC) $(CFLAGS) -o mysh shell.o interpreter.o shellmemory.o pcb.o queue.o schedule_policy.o -lpthread

clean:
	rm -f mysh *.o