#include <stdlib.h>
#include <string.h>
#include "queue.h"
#include "pcb.h"

struct queue {
    struct PCB *head;
    struct PCB *tail;
    int size;
};

struct queue *alloc_queue(void) {
    struct queue *q = calloc(1, sizeof(struct queue));
    return q;
}

void free_queue(struct queue *q) {
    // Don't free PCBs - caller manages them
    free(q);
}

int is_queue_empty(struct queue *q) {
    return q->head == NULL;
}

static void append(struct queue *q, struct PCB *pcb) {
    pcb->next = NULL;
    if (!q->tail) {
        q->head = q->tail = pcb;
    } else {
        q->tail->next = pcb;
        q->tail = pcb;
    }
    q->size++;
}

void enqueue_fcfs(struct queue *q, struct PCB *pcb) { append(q, pcb); }
void enqueue_rr(struct queue *q, struct PCB *pcb)   { append(q, pcb); }
void enqueue_ignoring_priority(struct queue *q, struct PCB *pcb) { append(q, pcb); }

void enqueue_sjf(struct queue *q, struct PCB *pcb) {
    // Insert by duration (shortest first)
    struct PCB **pp = &q->head;
    while (*pp && (*pp)->duration <= pcb->duration)
        pp = &(*pp)->next;
    pcb->next = *pp;
    *pp = pcb;
    if (!pcb->next) q->tail = pcb;
    q->size++;
}

void enqueue_aging(struct queue *q, struct PCB *pcb) { append(q, pcb); }

struct PCB *dequeue(struct queue *q) {
    if (!q->head) return NULL;
    struct PCB *pcb = q->head;
    q->head = pcb->next;
    if (!q->head) q->tail = NULL;
    pcb->next = NULL;
    q->size--;
    return pcb;
}

struct PCB *find_pcb_by_name(struct queue *q, const char *name) {
    struct PCB *p = q->head;
    while (p) {
        if (p->image && p->image->name && strcmp(p->image->name, name) == 0)
            return p;
        p = p->next;
    }
    return NULL;
}
