#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shell.h"
#include "shellmemory.h"
#include "pcb.h"

int pcb_has_next_instruction(struct PCB *pcb) {
    return pcb->pc < pcb->image->line_count;
}

int pcb_current_page_is_loaded(struct PCB *pcb) {
    int page = (int)(pcb->pc / PAGE_SIZE);
    return pcb->image->page_table[page] != INVALID_FRAME;
}

size_t pcb_next_instruction(struct PCB *pcb) {
    int page   = (int)(pcb->pc / PAGE_SIZE);
    int offset = (int)(pcb->pc % PAGE_SIZE);
    int frame  = pcb->image->page_table[page];
    size_t physical = frame_line_index(frame, offset);
    pcb->pc++;
    return physical;
}

static int read_one_page(FILE *f, char page_buffer[PAGE_SIZE][MAX_USER_INPUT]) {
    int n = 0;
    for (int i = 0; i < PAGE_SIZE; ++i) {
        if (fgets(page_buffer[i], MAX_USER_INPUT, f)) {
            n++;
        } else {
            memset(page_buffer[i], 0, MAX_USER_INPUT);
        }
    }
    return n;
}

int pcb_load_next_page(struct PCB *pcb) {
    struct ProgramImage *img = pcb->image;
    int page_index = (int)img->pages_loaded;
    if (page_index >= (int)img->num_pages) return -1;
    if (!img->backing_file) return -1;

    char page_buffer[PAGE_SIZE][MAX_USER_INPUT];
    if (read_one_page(img->backing_file, page_buffer) == 0) return -1;

    int frame = demand_load_page(page_buffer, img->page_table);
    img->page_table[page_index] = frame;
    frame_set_owner(frame, img->page_table, page_index);
    img->pages_loaded++;
    return frame;
}

static void count_lines_and_pages(FILE *f, size_t *lc, size_t *np) {
    char buf[MAX_USER_INPUT];
    *lc = 0;
    while (fgets(buf, MAX_USER_INPUT, f)) (*lc)++;
    *np = (*lc + PAGE_SIZE - 1) / PAGE_SIZE;
    rewind(f);
}

static void load_initial_pages(struct ProgramImage *img, int n) {
    char page_buffer[PAGE_SIZE][MAX_USER_INPUT];
    for (int p = 0; p < n && (size_t)p < img->num_pages; ++p) {
        if (read_one_page(img->backing_file, page_buffer) == 0) break;
        int frame = allocate_frame(page_buffer);
        img->page_table[p] = frame;
        frame_set_owner(frame, img->page_table, p);
        img->pages_loaded++;
    }
}

struct PCB *create_process_from_image(struct ProgramImage *image) {
    static pid fresh_pid = 1;
    struct PCB *pcb = malloc(sizeof(struct PCB));
    if (!pcb) return NULL;
    pcb->pid      = fresh_pid++;
    pcb->pc       = 0;
    pcb->duration = image->line_count;
    pcb->image    = image;
    pcb->next     = NULL;
    image->ref_count++;
    return pcb;
}

struct PCB *create_process(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) { perror("failed to open file"); return NULL; }

    struct ProgramImage *img = malloc(sizeof(struct ProgramImage));
    if (!img) { fclose(f); return NULL; }

    img->name         = strdup(filename);
    img->ref_count    = 0;
    img->backing_file = f;
    img->pages_loaded = 0;
    for (int i = 0; i < MAX_PAGES; ++i) img->page_table[i] = INVALID_FRAME;

    count_lines_and_pages(f, &img->line_count, &img->num_pages);

    /* Load first 2 pages eagerly (or 1 if the script is very short) */
    int pages_to_load = (img->num_pages >= 2) ? 2 : (int)img->num_pages;
    load_initial_pages(img, pages_to_load);

    return create_process_from_image(img);
}

struct PCB *create_process_from_FILE(FILE *script, const char *name) {
    /* stdin can't be rewound so load everything eagerly */
    struct ProgramImage *img = malloc(sizeof(struct ProgramImage));
    if (!img) return NULL;

    img->name         = strdup(name);
    img->ref_count    = 0;
    img->backing_file = NULL;
    img->line_count   = 0;
    img->num_pages    = 0;
    img->pages_loaded = 0;
    for (int i = 0; i < MAX_PAGES; ++i) img->page_table[i] = INVALID_FRAME;

    char linebuf[MAX_USER_INPUT];
    char page_buffer[PAGE_SIZE][MAX_USER_INPUT];
    int line_in_page = 0;

    while (fgets(linebuf, MAX_USER_INPUT, script)) {
        strcpy(page_buffer[line_in_page++], linebuf);
        img->line_count++;
        if (line_in_page == PAGE_SIZE) {
            int frame = allocate_frame(page_buffer);
            img->page_table[img->num_pages] = frame;
            frame_set_owner(frame, img->page_table, (int)img->num_pages);
            img->num_pages++;
            img->pages_loaded++;
            line_in_page = 0;
        }
    }
    if (line_in_page > 0) {
        for (int i = line_in_page; i < PAGE_SIZE; i++)
            memset(page_buffer[i], 0, MAX_USER_INPUT);
        int frame = allocate_frame(page_buffer);
        img->page_table[img->num_pages] = frame;
        frame_set_owner(frame, img->page_table, (int)img->num_pages);
        img->num_pages++;
        img->pages_loaded++;
    }

    fclose(script);
    return create_process_from_image(img);
}

void free_pcb(struct PCB *pcb) {
    struct ProgramImage *img = pcb->image;
    img->ref_count--;
    if (img->ref_count == 0) {
        /* per spec: do NOT evict pages on termination */
        if (img->backing_file) fclose(img->backing_file);
        free(img->name);
        free(img);
    }
    free(pcb);
}