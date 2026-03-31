#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shell.h"        /* MAX_USER_INPUT */
#include "shellmemory.h"
#include "pcb.h"

/* ------------------------------------------------------------------ */
/*  Low-level helpers                                                  */
/* ------------------------------------------------------------------ */

int pcb_has_next_instruction(struct PCB *pcb) {
    return pcb->pc < pcb->line_count;
}

/* The page that contains logical instruction index pc. */
static int logical_page(struct PCB *pcb) {
    return (int)(pcb->pc / PAGE_SIZE);
}

int pcb_current_page_is_loaded(struct PCB *pcb) {
    int page = logical_page(pcb);
    return pcb->page_table[page] != INVALID_FRAME;
}

size_t pcb_next_instruction(struct PCB *pcb) {
    int page   = (int)(pcb->pc / PAGE_SIZE);
    int offset = (int)(pcb->pc % PAGE_SIZE);
    int frame  = pcb->page_table[page];
    size_t physical = frame_line_index(frame, offset);
    pcb->pc++;
    return physical;
}

/* ------------------------------------------------------------------ */
/*  Page loading                                                       */
/* ------------------------------------------------------------------ */

/*
 * Read one page worth of lines from the backing file into page_buffer.
 * Returns the number of lines actually read (0 means EOF was already here).
 */
static int read_one_page(FILE *f, char page_buffer[PAGE_SIZE][MAX_USER_INPUT]) {
    int lines_read = 0;
    for (int i = 0; i < PAGE_SIZE; ++i) {
        if (fgets(page_buffer[i], MAX_USER_INPUT, f)) {
            lines_read++;
        } else {
            /* zero-pad remaining slots */
            memset(page_buffer[i], 0, MAX_USER_INPUT);
        }
    }
    return lines_read;
}

/*
 * Internal: load next page.
 * If silent=1, no page-fault messages are printed
 * (used during initial process creation).
 * Evicts a random victim (preferring other processes) if frame store is full.
 * Returns the frame number allocated, or -1 on error.
 */
static int load_next_page_internal(struct PCB *pcb, int silent) {
    int page_index = (int)pcb->pages_loaded;
    if (page_index >= (int)pcb->num_pages) return -1;

    char page_buffer[PAGE_SIZE][MAX_USER_INPUT];
    int lines_read = read_one_page(pcb->backing_file, page_buffer);
    if (lines_read == 0) return -1;

    int frame;
    if (!framestore_is_full()) {
        if (!silent) printf("Page fault!\n");
        frame = allocate_frame(page_buffer);
    } else {
        frame = pick_random_victim_frame_for((void *)pcb);

        if (!silent) {
            printf("Page fault!\n");
            printf("Victim page contents:\n");
            print_frame_contents(frame);
            printf("End of victim page contents.\n");
        }

        struct PCB *victim_pcb  = NULL;
        int         victim_page = -1;
        frame_get_owner(frame, &victim_pcb, &victim_page);
        if (victim_pcb && victim_page >= 0) {
            victim_pcb->page_table[victim_page] = INVALID_FRAME;
        }

        evict_and_replace_frame(frame, page_buffer);
    }

    pcb->page_table[page_index] = frame;
    frame_set_owner(frame, pcb, page_index);
    pcb->pages_loaded++;
    return frame;
}

int pcb_load_next_page(struct PCB *pcb) {
    return load_next_page_internal(pcb, 0);  /* loud: prints page fault messages */
}

/* ------------------------------------------------------------------ */
/*  Process creation                                                   */
/* ------------------------------------------------------------------ */

/*
 * Count lines and pages without keeping the file data.
 * Rewinds the file afterwards.
 */
static void count_lines_and_pages(FILE *f, size_t *line_count, size_t *num_pages) {
    char buf[MAX_USER_INPUT];
    *line_count = 0;
    while (fgets(buf, MAX_USER_INPUT, f)) (*line_count)++;
    *num_pages = (*line_count + PAGE_SIZE - 1) / PAGE_SIZE;
    rewind(f);
}

/*
 * Load the first N pages of pcb from its backing file.
 * Leaves the file cursor positioned at the start of page N.
 */
static void load_initial_pages(struct PCB *pcb, int n) {
    for (int p = 0; p < n && (size_t)p < pcb->num_pages; ++p) {
        load_next_page_internal(pcb, 1);  /* silent */
    }
}

struct PCB *create_process(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("failed to open file for create_process");
        return NULL;
    }

    struct PCB *pcb = malloc(sizeof(struct PCB));
    static pid fresh_pid = 1;

    pcb->pid          = fresh_pid++;
    pcb->name         = strdup(filename);
    pcb->next         = NULL;
    pcb->pc           = 0;
    pcb->pages_loaded = 0;
    pcb->backing_file = f;

    /* Initialise all page table entries to INVALID */
    for (int i = 0; i < MAX_PAGES; ++i) pcb->page_table[i] = INVALID_FRAME;

    count_lines_and_pages(f, &pcb->line_count, &pcb->num_pages);
    pcb->duration = pcb->line_count;

    /* Load first 2 pages (or just 1 if program < PAGE_SIZE lines) */
    int pages_to_load = (pcb->num_pages >= 2) ? 2 : (int)pcb->num_pages;
    load_initial_pages(pcb, pages_to_load);

    return pcb;
}

/*
 * create_process_from_FILE: used for stdin / background mode.
 * Loads ALL pages eagerly (we can't re-read stdin).
 */
struct PCB *create_process_from_FILE(FILE *script) {
    struct PCB *pcb = malloc(sizeof(struct PCB));
    static pid fresh_pid_f = 10000; /* separate namespace */

    pcb->pid          = fresh_pid_f++;
    pcb->name         = strdup("");
    pcb->next         = NULL;
    pcb->pc           = 0;
    pcb->line_count   = 0;
    pcb->num_pages    = 0;
    pcb->pages_loaded = 0;
    pcb->backing_file = NULL; /* can't seek on stdin */

    for (int i = 0; i < MAX_PAGES; ++i) pcb->page_table[i] = INVALID_FRAME;

    char linebuf[MAX_USER_INPUT];
    char page_buffer[PAGE_SIZE][MAX_USER_INPUT];
    int line_in_page = 0;

    while (fgets(linebuf, MAX_USER_INPUT, script)) {
        strcpy(page_buffer[line_in_page], linebuf);
        line_in_page++;
        pcb->line_count++;

        if (line_in_page == PAGE_SIZE) {
            int frame = allocate_frame(page_buffer);
            pcb->page_table[pcb->num_pages] = frame;
            frame_set_owner(frame, pcb, (int)pcb->num_pages);
            pcb->num_pages++;
            pcb->pages_loaded++;
            line_in_page = 0;
        }
    }

    if (line_in_page > 0) {
        for (int i = line_in_page; i < PAGE_SIZE; i++)
            memset(page_buffer[i], 0, MAX_USER_INPUT);
        int frame = allocate_frame(page_buffer);
        pcb->page_table[pcb->num_pages] = frame;
        frame_set_owner(frame, pcb, (int)pcb->num_pages);
        pcb->num_pages++;
        pcb->pages_loaded++;
    }

    pcb->duration = pcb->line_count;
    return pcb;
}

void free_pcb(struct PCB *pcb) {
    /* Per assignment spec: do NOT evict pages when a process terminates.
       Just close the backing file and free the PCB struct. */
    if (pcb->backing_file) {
        fclose(pcb->backing_file);
        pcb->backing_file = NULL;
    }
    if (pcb->name && strcmp(pcb->name, "") != 0) {
        free(pcb->name);
    }
    free(pcb);
}
