CC=gcc
#CFLAGS=-g -O0 -Wall #-DNDEBUG
CFLAGS=-DNDEBUG -Wall

# Compile-time memory sizes: make mysh framesize=X varmemsize=Y
framesize  ?= 18
varmemsize ?= 10

mysh: shell.c interpreter.c shellmemory.c pcb.c queue.c schedule_policy.c
	$(CC) $(CFLAGS) -DFRAME_STORE_SIZE=$(framesize) -DVAR_MEM_SIZE=$(varmemsize) \
		-c shell.c interpreter.c shellmemory.c pcb.c queue.c schedule_policy.c
	$(CC) $(CFLAGS) -DFRAME_STORE_SIZE=$(framesize) -DVAR_MEM_SIZE=$(varmemsize) \
		-o mysh shell.o interpreter.o shellmemory.o pcb.o queue.o schedule_policy.o -lpthread

clean:
	rm -f mysh *.o
