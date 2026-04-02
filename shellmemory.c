#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include "shellmemory.h"

/* ================================================================
   FRAME STORE
   Flat array of FRAME_STORE_SIZE lines, divided into NUM_FRAMES
   frames of PAGE_SIZE lines each.
   ================================================================ */

static char *framestore[FRAME_STORE_SIZE];
static int   frame_allocated[NUM_FRAMES];
static int   num_allocated_frames = 0;

/* Inverse page table: each frame knows which page_table entry points to it,
   so we can invalidate that entry when the frame is evicted.
   pc_ptr points to the owning PCB's pc field so we can detect cold pages. */
struct frame_owner {
    int    *page_table;  /* pointer into a ProgramImage's page_table array */
    int     page_index;  /* which slot in that array */
    size_t *pc_ptr;      /* pointer to owning PCB's pc, or NULL if shared/unknown */
};
static struct frame_owner inv_table[NUM_FRAMES];

/* ---- internal helpers ---- */

size_t frame_line_index(int frame, int offset) {
    return (size_t)(frame * PAGE_SIZE + offset);
}

void frame_set_owner(int frame, int *page_table, int page_index) {
    inv_table[frame].page_table = page_table;
    inv_table[frame].page_index = page_index;
    inv_table[frame].pc_ptr     = NULL; /* caller sets pc_ptr separately if needed */
}

/* Extended setter used by pcb.c to also register the pc pointer. */
void frame_set_owner_with_pc(int frame, int *page_table, int page_index, size_t *pc_ptr) {
    inv_table[frame].page_table = page_table;
    inv_table[frame].page_index = page_index;
    inv_table[frame].pc_ptr     = pc_ptr;
}

void frame_clear_owner(int frame) {
    inv_table[frame].page_table = NULL;
    inv_table[frame].page_index = -1;
    inv_table[frame].pc_ptr     = NULL;
}

void frame_get_owner(int frame, int **page_table_out, int *page_index_out) {
    *page_table_out = inv_table[frame].page_table;
    *page_index_out = inv_table[frame].page_index;
}

/* Returns 1 if the page in this frame has already been fully executed
   (the owning PCB's pc is past the last line of this page). */
static int frame_is_cold(int frame) {
    size_t *pc_ptr = inv_table[frame].pc_ptr;
    int     pi     = inv_table[frame].page_index;
    if (!pc_ptr) return 0;
    /* Last logical line of this page is (pi+1)*PAGE_SIZE - 1.
       If pc > that, the page has been completely executed. */
    return (int)(*pc_ptr) >= (pi + 1) * PAGE_SIZE;
}

static void write_page_to_frame(int frame,
                                char page_buffer[PAGE_SIZE][MAX_USER_INPUT]) {
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
            if (len == 0 || framestore[idx][len - 1] != '\n') printf("\n");
        }
    }
}

/* Pick a victim frame using this priority:
   1. Cold page of a different process (already executed past, safe to evict).
   2. Cold page of the requesting process (less ideal but still safe).
   3. Any page of a different process (random).
   4. Any page of the requesting process (last resort).
   This prevents thrashing when all frames are in use. */
static int pick_victim(int *requesting_page_table) {
    int r = rand() % NUM_FRAMES;

    /* Pass 1: cold page belonging to a different process. */
    for (int i = 0; i < NUM_FRAMES; ++i) {
        int f = (r + i) % NUM_FRAMES;
        if (!frame_allocated[f]) continue;
        if (inv_table[f].page_table == requesting_page_table) continue;
        if (frame_is_cold(f)) return f;
    }
    /* Pass 2: cold page of the requesting process. */
    for (int i = 0; i < NUM_FRAMES; ++i) {
        int f = (r + i) % NUM_FRAMES;
        if (!frame_allocated[f]) continue;
        if (frame_is_cold(f)) return f;
    }
    /* Pass 3: any page of a different process (random). */
    for (int i = 0; i < NUM_FRAMES; ++i) {
        int f = (r + i) % NUM_FRAMES;
        if (!frame_allocated[f]) continue;
        if (inv_table[f].page_table != requesting_page_table) return f;
    }
    /* Pass 4: last resort — any allocated frame. */
    for (int i = 0; i < NUM_FRAMES; ++i) {
        int f = (r + i) % NUM_FRAMES;
        if (frame_allocated[f]) return f;
    }
    return r;
}

/* ---- public API ---- */

/*
 * Silent allocation: used during initial eager page loading.
 * Returns frame number or -1 if full.
 */
int allocate_frame(char page_buffer[PAGE_SIZE][MAX_USER_INPUT]) {
    int frame = find_free_frame();
    if (frame < 0) return -1;

    frame_allocated[frame] = 1;
    num_allocated_frames++;
    write_page_to_frame(frame, page_buffer);
    frame_clear_owner(frame);
    return frame;
}

/*
 * Demand-load with page-fault messaging and eviction.
 * requesting_pcb: the PCB pointer (void*) requesting the page — used only
 *   to find its page_table so we avoid evicting its own pages.
 *   Pass NULL if you don't have a PCB handy.
 * victim_page_table / victim_page_index: output params filled with the
 *   victim's owner info so the caller can invalidate the right entry.
 *   Pass NULL for both if you don't need this.
 */
int demand_load_page(char page_buffer[PAGE_SIZE][MAX_USER_INPUT],
                     void *requesting_pt) {
    int frame;
    int *req_pt = (int *)requesting_pt;

    if (!framestore_is_full()) {
        printf("Page fault!\n");
        frame = find_free_frame();
        frame_allocated[frame] = 1;
        num_allocated_frames++;
        write_page_to_frame(frame, page_buffer);
    } else {
        frame = pick_victim(req_pt);

        printf("Page fault!\n");
        printf("Victim page contents:\n");
        print_frame_contents(frame);
        printf("End of victim page contents.\n");

        /* Invalidate the victim's page table entry via the inverse table. */
        int *vpt; int vpi;
        frame_get_owner(frame, &vpt, &vpi);
        if (vpt && vpi >= 0) {
            vpt[vpi] = -1; /* INVALID_FRAME */
        }

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

void assert_linememory_is_empty(void) {
    /* No-op: pages are not freed on process termination per spec,
       so the frame store may be non-empty between execs. */
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
    /* Initialise frame store. */
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
