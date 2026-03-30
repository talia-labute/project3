#pragma once
#include <stddef.h>
#include <stdio.h> 

#define PAGE_SIZE 3
#define MAX_PAGES 1000
#define INVALID_FRAME ((size_t)-1)

typedef size_t pid;

struct PCB {
    pid pid;
    char *name;

    size_t pc; //logical instruction index
    size_t line_count;
    size_t line_base;

    int page_table[MAX_PAGES]; // page -> frame
    size_t num_pages;

    size_t duration;

    struct PCB *next;
};


int pcb_has_next_instruction(struct PCB *pcb);
size_t pcb_next_instruction(struct PCB *pcb);
struct PCB *create_process(const char *filename);
struct PCB *create_process_from_FILE(FILE *f);
void free_pcb(struct PCB *pcb);

