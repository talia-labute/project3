#pragma once
#include <stddef.h>
#include <stdio.h>

#define PAGE_SIZE 3
#define MAX_PAGES 10000
#define INVALID_FRAME (-1)

typedef size_t pid;

struct PCB {
    pid   pid;
    char *name;

    /* Logical program counter (instruction index, 0-based). */
    size_t pc;
    size_t line_count;   /* total lines in the script */

    /* Page table: page -> frame number, or INVALID_FRAME if not loaded. */
    int    page_table[MAX_PAGES];
    size_t num_pages;    /* total pages the program has */
    size_t pages_loaded; /* how many pages have been brought in so far */

    /* Backing file — kept open so we can load pages on demand. */
    FILE *backing_file;

    size_t duration; /* for scheduling */

    struct PCB *next;
};

/* Returns 1 if the PCB still has an instruction to execute. */
int pcb_has_next_instruction(struct PCB *pcb);

/* Returns the physical line index of the next instruction and advances pc.
   Does NOT handle page faults — caller must check first. */
size_t pcb_next_instruction(struct PCB *pcb);

/* Returns 1 if the page that contains pc is currently loaded. */
int pcb_current_page_is_loaded(struct PCB *pcb);

/* Load the next not-yet-loaded page from the backing file.
   Handles eviction (random) if the frame store is full.
   Returns the frame number allocated, or -1 on error. */
int pcb_load_next_page(struct PCB *pcb);

/* Create a process from a file path.
   Loads only the first 2 pages (or 1 if the file is short). */
struct PCB *create_process(const char *filename);

/* Create a process from an already-open FILE (e.g. stdin).
   Loads all pages eagerly (used for background/source-from-stdin). */
struct PCB *create_process_from_FILE(FILE *f);

/* Free the PCB. Does NOT evict pages from the frame store
   (per assignment spec: when a process terminates, don't clean up pages). */
void free_pcb(struct PCB *pcb);

