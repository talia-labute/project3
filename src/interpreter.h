#pragma once
#include "pcb.h"
int interpreter(char *command_args[], int args_size);
struct PCB *run_pcb_to_completion(struct PCB *pcb);
struct PCB *run_pcb_for_n_steps(struct PCB *pcb, size_t n);
