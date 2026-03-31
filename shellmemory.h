#pragma once
#include <stdio.h>
#include <stddef.h>

#define PAGE_SIZE 3

/* Compile-time memory sizes, set by Makefile via -D flags.
   Fallback defaults make the file self-contained for IDE tooling. */
#ifndef FRAME_STORE_SIZE
#define FRAME_STORE_SIZE 18
#endif
#ifndef VAR_MEM_SIZE
#define VAR_MEM_SIZE 10
#endif

/* MAX_USER_INPUT is defined in shell.h, but shellmemory needs it too.
   We reproduce it here rather than create a circular include. */
#ifndef MAX_USER_INPUT
#define MAX_USER_INPUT 1000
#endif

/* Total number of frames in the frame store */
#define NUM_FRAMES (FRAME_STORE_SIZE / PAGE_SIZE)

/* ---- Variable store ---- */
void mem_init(void);
char *mem_get_value(char *var);
void mem_set_value(char *var, char *value);

/* ---- Frame store ---- */

/* Allocate one frame, writing the PAGE_SIZE lines from page_buffer.
   Returns the frame number (0-based), or -1 if the store is full. */
int allocate_frame(char page_buffer[PAGE_SIZE][MAX_USER_INPUT]);

/* Evict whatever is in 'frame' and write page_buffer into it.
   Returns frame (the same value passed in). */
int evict_and_replace_frame(int frame, char page_buffer[PAGE_SIZE][MAX_USER_INPUT]);

/* Return the physical line index for frame f, offset o. */
size_t frame_line_index(int frame, int offset);

/* Is the frame store full? */
int framestore_is_full(void);

/* Pick a random victim frame number. */
int pick_random_victim_frame(void);
/* Pick a victim frame, avoiding frames belonging to requesting_pcb if possible.
   Pass NULL to pick any frame. Uses void* to avoid circular header dependency. */
int pick_random_victim_frame_for(void *requesting_pcb);

/* Print the contents of a frame (for page-fault eviction message). */
void print_frame_contents(int frame);

/* Raw line access */
const char *get_line(size_t index);

/* Forward declaration so shellmemory.h doesn't need to include pcb.h */
struct PCB;

/* Inverse page table bookkeeping:
   tell the frame store which (pcb, page) owns a given frame. */
void frame_set_owner(int frame, struct PCB *pcb, int page);
void frame_clear_owner(int frame);
void frame_get_owner(int frame, struct PCB **pcb_out, int *page_out);

/* Reset for a fresh exec (only valid when frame store is empty). */
void reset_linememory_allocator(void);
void assert_linememory_is_empty(void);