#include <string.h>
#include "schedule_policy.h"
#include "interpreter.h"
#include "queue.h"

static struct PCB *run_to_completion(struct PCB *pcb) {
    return run_pcb_to_completion(pcb);
}

static struct PCB *run_rr(struct PCB *pcb) {
    return run_pcb_for_n_steps(pcb, 2);
}

static const struct schedule_policy fcfs_policy = {
    .enqueue = enqueue_fcfs,
    .enqueue_ignoring_priority = enqueue_ignoring_priority,
    .dequeue = dequeue,
    .run_pcb = run_to_completion,
};

static const struct schedule_policy rr_policy = {
    .enqueue = enqueue_rr,
    .enqueue_ignoring_priority = enqueue_ignoring_priority,
    .dequeue = dequeue,
    .run_pcb = run_rr,
};

static const struct schedule_policy sjf_policy = {
    .enqueue = enqueue_sjf,
    .enqueue_ignoring_priority = enqueue_ignoring_priority,
    .dequeue = dequeue,
    .run_pcb = run_to_completion,
};

static const struct schedule_policy aging_policy = {
    .enqueue = enqueue_aging,
    .enqueue_ignoring_priority = enqueue_ignoring_priority,
    .dequeue = dequeue,
    .run_pcb = run_rr,
};

const struct schedule_policy *get_policy(const char *name) {
    if (strcmp(name, "FCFS") == 0) return &fcfs_policy;
    if (strcmp(name, "RR")   == 0) return &rr_policy;
    if (strcmp(name, "SJF")  == 0) return &sjf_policy;
    if (strcmp(name, "AGING")== 0) return &aging_policy;
    return NULL;
}
