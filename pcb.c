#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shell.h"
#include "shellmemory.h"
#include "pcb.h"

int pcb_has_next_instruction(struct PCB *pcb) {
    return pcb->pc < pcb->image->line_count;
}

size_t pcb_next_instruction(struct PCB *pcb) {
    int page = pcb->pc / PAGE_SIZE;
    int offset = pcb->pc % PAGE_SIZE;

    int frame = pcb->image->page_table[page];
    size_t physical_index = frame * PAGE_SIZE + offset;

    pcb->pc++;
    return physical_index;
}

static struct ProgramImage *create_program_image_from_FILE(FILE *script, const char *name) {
    struct ProgramImage *image = malloc(sizeof(struct ProgramImage));
    if (!image) return NULL;

    image->name = strdup(name);
    image->line_count = 0;
    image->num_pages = 0;
    image->ref_count = 0;

    for (int i = 0; i < MAX_PAGES; i++) {
        image->page_table[i] = INVALID_FRAME;
    }

    char linebuf[MAX_USER_INPUT];
    char page_buffer[PAGE_SIZE][MAX_USER_INPUT];
    int line_in_page = 0;

    while (fgets(linebuf, MAX_USER_INPUT, script)) {
        strcpy(page_buffer[line_in_page], linebuf);
        line_in_page++;
        image->line_count++;

        if (line_in_page == PAGE_SIZE) {
            int frame = allocate_frame(page_buffer);
            if (frame == -1) {
                free(image->name);
                free(image);
                return NULL;
            }
            image->page_table[image->num_pages++] = frame;
            line_in_page = 0;
        }
    }

    if (line_in_page > 0) {
        for (int i = line_in_page; i < PAGE_SIZE; i++) {
            memset(page_buffer[i], 0, MAX_USER_INPUT);
        }

        int frame = allocate_frame(page_buffer);
        if (frame == -1) {
            free(image->name);
            free(image);
            return NULL;
        }
        image->page_table[image->num_pages++] = frame;
    }

    return image;
}

struct PCB *create_process_from_image(struct ProgramImage *image) {
    static pid fresh_pid = 1;

    struct PCB *pcb = malloc(sizeof(struct PCB));
    if (!pcb) return NULL;

    pcb->pid = fresh_pid++;
    pcb->pc = 0;
    pcb->duration = image->line_count;
    pcb->image = image;
    pcb->next = NULL;

    image->ref_count++;

    return pcb;
}

struct PCB *create_process_from_FILE(FILE *script, const char *name) {
    struct ProgramImage *image = create_program_image_from_FILE(script, name);
    fclose(script);

    if (!image) return NULL;

    return create_process_from_image(image);
}

struct PCB *create_process(const char *filename) {
    FILE *script = fopen(filename, "rt");
    if (!script) {
        perror("failed to open file for create_process");
        return NULL;
    }

    return create_process_from_FILE(script, filename);
}

void free_pcb(struct PCB *pcb) {
    struct ProgramImage *image = pcb->image;

    image->ref_count--;

    if (image->ref_count == 0) {
        for (size_t p = 0; p < image->num_pages; p++) {
            int frame = image->page_table[p];
            if (frame != INVALID_FRAME) {
                for (int i = 0; i < PAGE_SIZE; i++) {
                    free_line(frame * PAGE_SIZE + i);
                }
            }
        }
        free(image->name);
        free(image);
    }

    free(pcb);
}