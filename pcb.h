#pragma once
#include <stddef.h>
#include <stdio.h> 

#define PAGE_SIZE 3
#define MAX_PAGES 1000
#define INVALID_FRAME (-1)

typedef size_t pid;

struct ProgramImage {
    char *name;
    size_t line_count;
    size_t num_pages;
    int page_table[MAX_PAGES]; // page -> frame
    int ref_count;
    int exec_order; /* order in exec batch (0,1,2,...) for victim selection */

    FILE  *backing_file;   
    size_t pages_loaded;   /* # pages currently in the frame store */
};

struct PCB {
    pid pid;

    size_t pc; //logical instruction index
    size_t line_base;

    size_t duration;

    struct ProgramImage *image;
    struct PCB *next;
};


int pcb_has_next_instruction(struct PCB *pcb);
size_t pcb_next_instruction(struct PCB *pcb);
struct PCB *create_process(const char *filename);
struct PCB *create_process_from_FILE(FILE *f, const char *name);
struct PCB *create_process_from_image(struct ProgramImage *image);
void pcb_reset_exec_order(void);
void free_pcb(struct PCB *pcb);
int pcb_current_page_is_loaded(struct PCB *pcb);
int pcb_load_next_page(struct PCB *pcb);

