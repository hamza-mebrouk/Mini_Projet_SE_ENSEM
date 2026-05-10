/*
 * Auteur : Hamza Mebrouk — ENSEM 2026
 */
#include "../include/process.h"

int process_total_cpu(const Process *p) {
    int total = 0;
    for (int i = 0; i < p->nb_cycles; i++) {
        if (p->cycles[i].type == CYCLE_CPU)
            total += p->cycles[i].duration;
    }
    return total;
}

void process_print(const Process *p) {
    printf("Process %-12s | arrivée=%2d | priorité=%d | cycles=",
           p->name, p->arrival_time, p->priority);
    for (int i = 0; i < p->nb_cycles; i++) {
        printf("%s(%d) ", p->cycles[i].type == CYCLE_CPU ? "CPU" : "IO",
               p->cycles[i].duration);
    }
    printf("| CPU_total=%d\n", process_total_cpu(p));
}
