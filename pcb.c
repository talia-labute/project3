#include <stdio.h>
#include <stdlib.h>
#include <string.h> // memset
#include "shell.h" // MAX_USER_INPUT
#include "shellmemory.h"
#include "pcb.h"

int pcb_has_next_instruction(struct PCB *pcb) {
    return pcb->pc < pcb->line_count;
}

size_t pcb_next_instruction(struct PCB *pcb) {
    int page = pcb->pc / PAGE_SIZE;
    int offset = pcb->pc % PAGE_SIZE;
    
    int frame = pcb->page_table[page];
    
    size_t physical_index = frame * PAGE_SIZE + offset;

    pcb->pc++;
    return physical_index;
}

struct PCB *create_process(const char *filename) {
    FILE *script = fopen(filename, "rt");
    if (!script) {
        perror("failed to open file for create_process");
        return NULL;
    }
    struct PCB *pcb = create_process_from_FILE(script);
    pcb->name = strdup(filename);
    return pcb;
}


struct PCB *create_process_from_FILE(FILE *script) {
    struct PCB *pcb = malloc(sizeof(struct PCB));
    static pid fresh_pid = 1;

    pcb->pid = fresh_pid++;
    pcb->name = "";
    pcb->next = NULL;
    pcb->pc = 0;
    pcb->line_count = 0;
    pcb->num_pages = 0;

    char linebuf[MAX_USER_INPUT];
    char page_buffer[PAGE_SIZE][MAX_USER_INPUT];
    int line_in_page = 0;

    while (fgets(linebuf, MAX_USER_INPUT, script)){
        strcpy(page_buffer[line_in_page], linebuf);
        line_in_page++;
        pcb->line_count++;

        //when page full
        if (line_in_page == PAGE_SIZE){
            int frame_start = allocate_frame(page_buffer);
            pcb->page_table[pcb->num_pages++] = frame_start;
            line_in_page = 0;
        }
    }

    //leftover lines = partial page
    if (line_in_page > 0){
        // zero out unused slots in the partial page
        for (int i = line_in_page; i < PAGE_SIZE; i++) {
            memset(page_buffer[i], 0, MAX_USER_INPUT);
        }
        int frame_start = allocate_frame(page_buffer);
        pcb->page_table[pcb->num_pages++] = frame_start;
    }

    fclose(script);
    pcb->duration = pcb->line_count;
    return pcb;
}

void free_pcb(struct PCB *pcb) {
    for (int p = 0; p < pcb->num_pages; ++p) {
        int frame = pcb->page_table[p];
        for (int i = 0; i < PAGE_SIZE; ++i) {
            free_line(frame * PAGE_SIZE + i);
        }
    }

    if (strcmp("", pcb->name)) {
        free(pcb->name);
    }
    free(pcb);
}
