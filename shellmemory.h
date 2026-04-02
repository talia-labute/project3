#pragma once
#include <stddef.h>
#include "shell.h"

#define PAGE_SIZE 3
#define INVALID_FRAME (-1)
#ifndef FRAME_STORE_SIZE
#define FRAME_STORE_SIZE 18
#endif
#ifndef VAR_MEM_SIZE
#define VAR_MEM_SIZE 10
#endif

#define NUM_FRAMES (FRAME_STORE_SIZE / PAGE_SIZE)

/* Variable store */
void mem_init(void);
char *mem_get_value(char *var);
void mem_set_value(char *var, char *value);

/* Frame store */
int allocate_frame(char page_buffer[PAGE_SIZE][MAX_USER_INPUT]);
int demand_load_page(char page_buffer[PAGE_SIZE][MAX_USER_INPUT], int *requesting_page_table);
int framestore_is_full(void);
void print_frame_contents(int frame);
size_t frame_line_index(int frame, int offset);

/* Inverse page table */
void frame_set_owner(int frame, int *page_table, int page_index);
void frame_clear_owner(int frame);
void frame_get_owner(int frame, int **page_table_out, int *page_index_out);

/* Line access */
const char *get_line(size_t index);
void free_line(size_t index);
void reset_linememory_allocator(void);
void assert_linememory_is_empty(void);