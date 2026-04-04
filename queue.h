#pragma once
#include "pcb.h"

struct queue;

struct queue *alloc_queue(void);
void free_queue(struct queue *q);
int is_queue_empty(struct queue *q);
void enqueue_fcfs(struct queue *q, struct PCB *pcb);
void enqueue_rr(struct queue *q, struct PCB *pcb);
void enqueue_sjf(struct queue *q, struct PCB *pcb);
void enqueue_aging(struct queue *q, struct PCB *pcb);
void enqueue_ignoring_priority(struct queue *q, struct PCB *pcb);
struct PCB *dequeue(struct queue *q);
struct PCB *find_pcb_by_name(struct queue *q, const char *name);
