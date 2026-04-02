#pragma once
#include <stddef.h>
#include <stdio.h>

#define PAGE_SIZE 3
#define MAX_PAGES 10000
#define INVALID_FRAME (-1)

typedef size_t pid;

/*
 * ProgramImage: shared, ref-counted representation of a script on disk.
 * Multiple PCBs (e.g. from "exec prog prog RR") point to the same image.
 *
 * Demand-paging additions:
 *   - backing_file: kept open so we can load pages on demand.
 *   - pages_loaded: how many pages have been brought into the frame store.
 *     page_table[0..pages_loaded-1] are valid frames;
 *     page_table[pages_loaded..num_pages-1] == INVALID_FRAME.
 */
struct ProgramImage {
    char  *name;
    size_t line_count;
    size_t num_pages;
    int    page_table[MAX_PAGES]; /* page index -> frame number */
    int    ref_count;

    /* Demand-paging fields */
    FILE  *backing_file;   /* open file handle; NULL for stdin-sourced images */
    size_t pages_loaded;   /* number of pages currently in the frame store */
};

struct PCB {
    pid    pid;
    size_t pc;          /* logical instruction index (0-based) */
    size_t line_base;
    size_t duration;
    struct ProgramImage *image;
    struct PCB *next;
};

/* ---- Instruction helpers ---- */
int    pcb_has_next_instruction(struct PCB *pcb);
size_t pcb_next_instruction(struct PCB *pcb);

/* Returns 1 if the page containing pcb->pc is currently loaded. */
int    pcb_current_page_is_loaded(struct PCB *pcb);

/*
 * Load the next unloaded page from the image's backing file.
 * Prints page-fault messages; performs eviction if necessary.
 * Returns the frame number, or -1 on error.
 */
int    pcb_load_next_page(struct PCB *pcb);

/* ---- Process creation ---- */

/*
 * Open filename, count its lines, load the first 2 pages (or 1 if short),
 * leave the file open for demand paging, and return a new PCB.
 */
struct PCB *create_process(const char *filename);

/*
 * Build a PCB from an already-open FILE (e.g. stdin).
 * Loads ALL pages eagerly because stdin cannot be re-read.
 */
struct PCB *create_process_from_FILE(FILE *f, const char *name);

/*
 * Create an additional PCB that shares an existing ProgramImage.
 * Used when the same filename appears twice in an exec call.
 */
struct PCB *create_process_from_image(struct ProgramImage *image);

/* ---- Cleanup ---- */

/*
 * Decrement ref_count. If it reaches zero, close the backing file.
 * Per spec, pages are NOT evicted from the frame store on termination.
 */
void free_pcb(struct PCB *pcb);


