#pragma once
#include <stddef.h>

/* shell.h defines MAX_USER_INPUT; include it so callers don't need to. */
#include "shell.h"

#define PAGE_SIZE 3

/* Sizes set at compile time via Makefile: make mysh framesize=X varmemsize=Y */
#ifndef FRAME_STORE_SIZE
#define FRAME_STORE_SIZE 18
#endif
#ifndef VAR_MEM_SIZE
#define VAR_MEM_SIZE 10
#endif

/* Total number of frames in the frame store. */
#define NUM_FRAMES (FRAME_STORE_SIZE / PAGE_SIZE)

/* ---- Variable store ---- */
void mem_init(void);
char *mem_get_value(char *var);
void mem_set_value(char *var, char *value);

/* ---- Frame store ---- */

/*
 * Allocate a free frame and write page_buffer into it.
 * Returns the frame number (0-based), or -1 if the store is full.
 * Silent: no page-fault messages printed (used for initial eager loading).
 */
int allocate_frame(char page_buffer[PAGE_SIZE][MAX_USER_INPUT]);

/*
 * Demand-load page_buffer into the frame store with page-fault messaging.
 * requesting_pt: the page_table pointer of the requesting image, used to
 *   avoid evicting its own frames. Pass NULL if unavailable.
 * Returns the frame number used.
 */
int demand_load_page(char page_buffer[PAGE_SIZE][MAX_USER_INPUT],
                     void *requesting_pt);

/* Physical line index for a given frame + offset. */
size_t frame_line_index(int frame, int offset);

/* Is the frame store completely full? */
int framestore_is_full(void);

/* Print the PAGE_SIZE lines stored in frame (for eviction message). */
void print_frame_contents(int frame);

/* Raw line access — caller must know the physical index. */
const char *get_line(size_t index);

/* Free a single line (used when a process cleans up its pages). */
void free_line(size_t index);

/*
 * Inverse page table: each frame records which (image page_table, page_index)
 * owns it so we can invalidate the entry on eviction.
 * Both pointers use void* / int* to avoid a circular dependency.
 */
void frame_set_owner(int frame, int *page_table, int page_index);
void frame_set_owner_with_pc(int frame, int *page_table, int page_index, size_t *pc_ptr);
void frame_clear_owner(int frame);
void frame_get_owner(int frame, int **page_table_out, int *page_index_out);

/* Reset frame store (called at start of each non-background exec). */
void reset_linememory_allocator(void);
void assert_linememory_is_empty(void);