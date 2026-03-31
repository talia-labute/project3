#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include "shellmemory.h"
#include "shell.h"
#include "pcb.h"

/* ================================================================
   FRAME STORE
   The frame store is a flat array of FRAME_STORE_SIZE lines,
   logically split into NUM_FRAMES frames of PAGE_SIZE lines each.
   ================================================================ */

static char *framestore[FRAME_STORE_SIZE]; /* NULL = empty slot */
static int   frame_allocated[NUM_FRAMES];  /* 1 = frame is in use */
static int   num_allocated_frames = 0;

/* Inverse page table: for each frame, which PCB and page owns it? */
struct frame_owner {
    struct PCB *pcb;
    int         page; /* logical page index within that PCB */
};
static struct frame_owner inv_table[NUM_FRAMES];

/* ---- internal helpers ---- */

size_t frame_line_index(int frame, int offset) {
    return (size_t)(frame * PAGE_SIZE + offset);
}

void frame_get_owner(int frame, struct PCB **pcb_out, int *page_out) {
    *pcb_out  = inv_table[frame].pcb;
    *page_out = inv_table[frame].page;
}

void frame_set_owner(int frame, struct PCB *pcb, int page) {
    inv_table[frame].pcb  = pcb;
    inv_table[frame].page = page;
}

void frame_clear_owner(int frame) {
    inv_table[frame].pcb  = NULL;
    inv_table[frame].page = -1;
}

/* ---- public frame store API ---- */

/* Write page_buffer into frame f. */
static void write_page_to_frame(int frame, char page_buffer[PAGE_SIZE][MAX_USER_INPUT]) {
    for (int i = 0; i < PAGE_SIZE; ++i) {
        size_t idx = frame_line_index(frame, i);
        free(framestore[idx]);
        framestore[idx] = strdup(page_buffer[i]);
    }
}


int framestore_is_full(void) {
    return num_allocated_frames >= NUM_FRAMES;
}

/* Find first free frame, or -1 if full. */
static int find_free_frame(void) {
    for (int f = 0; f < NUM_FRAMES; ++f) {
        if (!frame_allocated[f]) return f;
    }
    return -1;
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

int evict_and_replace_frame(int frame, char page_buffer[PAGE_SIZE][MAX_USER_INPUT]) {
    assert(frame >= 0 && frame < NUM_FRAMES);
    /* Caller is responsible for updating page tables before calling this.
       We just overwrite the physical lines. */
    write_page_to_frame(frame, page_buffer);
    /* Owner will be set by caller via frame_set_owner. */
    return frame;
}

void print_frame_contents(int frame) {
    for (int i = 0; i < PAGE_SIZE; ++i) {
        size_t idx = frame_line_index(frame, i);
        if (framestore[idx]) {
            printf("%s", framestore[idx]);
            /* add newline if the stored line doesn't end with one */
            size_t len = strlen(framestore[idx]);
            if (len == 0 || framestore[idx][len - 1] != '\n') printf("\n");
        }
    }
}

/* Pick a victim frame, avoiding frames owned by 'requesting_pcb' if possible.
   Uses void* to avoid circular header dependency with pcb.h. */
int pick_random_victim_frame_for(void *requesting_pcb) {
    int r = rand() % NUM_FRAMES;
    /* First pass: find a frame NOT owned by the requesting PCB */
    for (int i = 0; i < NUM_FRAMES; ++i) {
        int f = (r + i) % NUM_FRAMES;
        if (!frame_allocated[f]) continue;
        if ((void *)inv_table[f].pcb != requesting_pcb) return f;
    }
    /* Fall back: all frames belong to requesting_pcb, just pick any */
    for (int i = 0; i < NUM_FRAMES; ++i) {
        int f = (r + i) % NUM_FRAMES;
        if (frame_allocated[f]) return f;
    }
    return r;
}

int pick_random_victim_frame(void) {
    return pick_random_victim_frame_for(NULL);
}

const char *get_line(size_t index) {
    assert(index < FRAME_STORE_SIZE);
    assert(framestore[index] != NULL);
    return framestore[index];
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

void assert_linememory_is_empty(void) {
    /* No-op in demand-paging mode: pages are not freed on process termination
       per assignment spec, so the frame store may be non-empty between execs. */
}

/* ================================================================
   VARIABLE STORE
   ================================================================ */

struct memory_struct {
    char *var;
    char *value;
};

static struct memory_struct shellmemory[VAR_MEM_SIZE];

void mem_init(void) {
    srand((unsigned)time(NULL));
    for (int i = 0; i < VAR_MEM_SIZE; i++) {
        shellmemory[i].var   = "none";
        shellmemory[i].value = "none";
    }
    /* initialise framestore */
    for (int i = 0; i < FRAME_STORE_SIZE; ++i) {
        framestore[i] = NULL;
    }
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
        if (strcmp(shellmemory[i].var, var_in) == 0) {
            return strdup(shellmemory[i].value);
        }
    }
    return NULL;
}
