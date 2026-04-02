#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include "shellmemory.h"

/* ---- Frame store ---- */
static char *framestore[FRAME_STORE_SIZE];
static int   frame_allocated[NUM_FRAMES];
static int   num_allocated_frames = 0;

struct frame_owner {
    int *page_table;
    int  page_index;
};
static struct frame_owner inv_table[NUM_FRAMES];

size_t frame_line_index(int frame, int offset) {
    return (size_t)(frame * PAGE_SIZE + offset);
}

void frame_set_owner(int frame, int *page_table, int page_index) {
    inv_table[frame].page_table = page_table;
    inv_table[frame].page_index = page_index;
}

void frame_clear_owner(int frame) {
    inv_table[frame].page_table = NULL;
    inv_table[frame].page_index = -1;
}

void frame_get_owner(int frame, int **page_table_out, int *page_index_out) {
    *page_table_out = inv_table[frame].page_table;
    *page_index_out = inv_table[frame].page_index;
}

static void write_page_to_frame(int frame, char page_buffer[PAGE_SIZE][MAX_USER_INPUT]) {
    for (int i = 0; i < PAGE_SIZE; ++i) {
        size_t idx = frame_line_index(frame, i);
        free(framestore[idx]);
        framestore[idx] = strdup(page_buffer[i]);
    }
}

static int find_free_frame(void) {
    for (int f = 0; f < NUM_FRAMES; ++f)
        if (!frame_allocated[f]) return f;
    return -1;
}

int framestore_is_full(void) {
    return num_allocated_frames >= NUM_FRAMES;
}

void print_frame_contents(int frame) {
    for (int i = 0; i < PAGE_SIZE; ++i) {
        size_t idx = frame_line_index(frame, i);
        if (framestore[idx] && framestore[idx][0] != '\0') {
            printf("%s", framestore[idx]);
            size_t len = strlen(framestore[idx]);
            if (len == 0 || framestore[idx][len-1] != '\n') printf("\n");
        }
    }
}

/* Pick a random allocated frame to evict, preferring frames
   not belonging to the requesting process */
static int pick_victim(int *requesting_page_table) {
    int r = rand() % NUM_FRAMES;
    /* prefer a frame from a different process */
    for (int i = 0; i < NUM_FRAMES; ++i) {
        int f = (r + i) % NUM_FRAMES;
        if (frame_allocated[f] && inv_table[f].page_table != requesting_page_table)
            return f;
    }
    /* fallback: any allocated frame */
    for (int i = 0; i < NUM_FRAMES; ++i) {
        int f = (r + i) % NUM_FRAMES;
        if (frame_allocated[f]) return f;
    }
    return r;
}

int allocate_frame(char page_buffer[PAGE_SIZE][MAX_USER_INPUT]) {
    int frame = find_free_frame();
    if (frame < 0) return -1;
    frame_allocated[frame] = 1;
    num_allocated_frames++;
    write_page_to_frame(frame, page_buffer);
    frame_clear_owner(frame);
    return frame;
}

int demand_load_page(char page_buffer[PAGE_SIZE][MAX_USER_INPUT],
                     int *requesting_page_table) {
    int frame;
    if (!framestore_is_full()) {
        printf("Page fault!\n");
        frame = find_free_frame();
        frame_allocated[frame] = 1;
        num_allocated_frames++;
        write_page_to_frame(frame, page_buffer);
    } else {
        frame = pick_victim(requesting_page_table);
        printf("Page fault!\n");
        printf("Victim page contents:\n");
        print_frame_contents(frame);
        printf("End of victim page contents.\n");
        /* invalidate the victim's page table entry */
        int *vpt; int vpi;
        frame_get_owner(frame, &vpt, &vpi);
        if (vpt && vpi >= 0) vpt[vpi] = INVALID_FRAME;
        write_page_to_frame(frame, page_buffer);
    }
    return frame;
}

const char *get_line(size_t index) {
    assert(index < (size_t)FRAME_STORE_SIZE);
    assert(framestore[index] != NULL);
    return framestore[index];
}

void free_line(size_t index) {
    free(framestore[index]);
    framestore[index] = NULL;
}

void reset_linememory_allocator(void) {
    for (int i = 0; i < FRAME_STORE_SIZE; ++i) {
        free(framestore[i]);
        framestore[i] = NULL;
    }
    for (int f = 0; f < NUM_FRAMES; ++f) {
        frame_allocated[f] = 0;
        frame_clear_owner(f);
    }
    num_allocated_frames = 0;
}

void assert_linememory_is_empty(void) { /* no-op per spec */ }

/* ---- Variable store ---- */
struct memory_struct { char *var; char *value; };
static struct memory_struct shellmemory[VAR_MEM_SIZE];

void mem_init(void) {
    srand((unsigned)time(NULL));
    for (int i = 0; i < VAR_MEM_SIZE; i++) {
        shellmemory[i].var   = "none";
        shellmemory[i].value = "none";
    }
    for (int i = 0; i < FRAME_STORE_SIZE; ++i) framestore[i] = NULL;
    for (int f = 0; f < NUM_FRAMES; ++f) {
        frame_allocated[f] = 0;
        frame_clear_owner(f);
    }
}

void mem_set_value(char *var_in, char *value_in) {
    for (int i = 0; i < VAR_MEM_SIZE; i++) {
        if (strcmp(shellmemory[i].var, var_in) == 0) {
            free(shellmemory[i].value);
            shellmemory[i].value = strdup(value_in);
            return;
        }
    }
    for (int i = 0; i < VAR_MEM_SIZE; i++) {
        if (strcmp(shellmemory[i].var, "none") == 0) {
            shellmemory[i].var   = strdup(var_in);
            shellmemory[i].value = strdup(value_in);
            return;
        }
    }
}

char *mem_get_value(char *var_in) {
    for (int i = 0; i < VAR_MEM_SIZE; i++) {
        if (strcmp(shellmemory[i].var, var_in) == 0)
            return strdup(shellmemory[i].value);
    }
    return NULL;
}