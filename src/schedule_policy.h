#pragma once
#include "pcb.h"
#include "queue.h"

struct schedule_policy {
    void (*enqueue)(struct queue *q, struct PCB *pcb);
    void (*enqueue_ignoring_priority)(struct queue *q, struct PCB *pcb);
    struct PCB *(*dequeue)(struct queue *q);
    struct PCB *(*run_pcb)(struct PCB *pcb);
};

const struct schedule_policy *get_policy(const char *name);
