/*
 * Auteur : Hamza Mebrouk — ENSEM 2026
 */
/*
 * Politique SJF (Shortest Job First) — non préemptive.
 * Parmi les processus prêts, on choisit celui dont le burst CPU total est le plus court.
 * En cas d'égalité, on prend celui arrivé en premier.
 */
#include <stdio.h>
#include <string.h>
#include "process.h"
#include "gantt.h"

void schedule(Process *processes, int count, int quantum) {
    (void)quantum;

    printf("\n>>> Politique : SJF (Shortest Job First) — non préemptif\n");

    Process procs[MAX_PROCESSES];
    memcpy(procs, processes, sizeof(Process) * count);
    for (int i = 0; i < count; i++) {
        procs[i].state           = STATE_NEW;
        procs[i].current_cycle   = 0;
        procs[i].cycle_remaining = procs[i].nb_cycles > 0
                                   ? procs[i].cycles[0].duration : 0;
        procs[i].io_remaining    = 0;
        procs[i].waiting_time    = 0;
        procs[i].start_time      = -1;
        procs[i].finish_time     = 0;
    }

    GanttChart gantt = {.count = 0, .total_time = 0};
    int time = 0, finished = 0;

    while (finished < count) {
        /* Mise à jour des arrivées */
        for (int i = 0; i < count; i++) {
            if (procs[i].state == STATE_NEW && procs[i].arrival_time <= time)
                procs[i].state = STATE_READY;
        }

        /* Choisir le READY avec le plus petit burst CPU total restant */
        int chosen = -1;
        for (int i = 0; i < count; i++) {
            if (procs[i].state != STATE_READY) continue;
            if (chosen < 0) { chosen = i; continue; }
            if (procs[i].total_cpu < procs[chosen].total_cpu) chosen = i;
            else if (procs[i].total_cpu == procs[chosen].total_cpu &&
                     procs[i].arrival_time < procs[chosen].arrival_time)
                chosen = i;
        }

        if (chosen < 0) {
            gantt_add(&gantt, time, "IDLE");
            time++;
            for (int i = 0; i < count; i++) {
                if (procs[i].state == STATE_WAITING) {
                    procs[i].io_remaining--;
                    if (procs[i].io_remaining <= 0) {
                        procs[i].current_cycle++;
                        if (procs[i].current_cycle >= procs[i].nb_cycles) {
                            procs[i].state       = STATE_FINISHED;
                            procs[i].finish_time = time;
                            finished++;
                        } else {
                            procs[i].cycle_remaining =
                                procs[i].cycles[procs[i].current_cycle].duration;
                            procs[i].state = STATE_READY;
                        }
                    }
                }
            }
            continue;
        }

        Process *p = &procs[chosen];
        if (p->start_time < 0) p->start_time = time;
        p->state = STATE_RUNNING;

        while (p->current_cycle < p->nb_cycles) {
            Cycle *c = &p->cycles[p->current_cycle];

            if (c->type == CYCLE_CPU) {
                gantt_add(&gantt, time, p->name);
                int dur = p->cycle_remaining;
                for (int t = 0; t < dur; t++) {
                    time++;
                    for (int i = 0; i < count; i++) {
                        if (procs[i].state == STATE_NEW &&
                            procs[i].arrival_time <= time)
                            procs[i].state = STATE_READY;
                    }
                    for (int i = 0; i < count; i++) {
                        if (i == chosen) continue;
                        if (procs[i].state == STATE_WAITING) {
                            procs[i].io_remaining--;
                            if (procs[i].io_remaining <= 0) {
                                procs[i].current_cycle++;
                                if (procs[i].current_cycle >= procs[i].nb_cycles) {
                                    procs[i].state       = STATE_FINISHED;
                                    procs[i].finish_time = time;
                                    finished++;
                                } else {
                                    procs[i].cycle_remaining =
                                        procs[i].cycles[procs[i].current_cycle].duration;
                                    procs[i].state = STATE_READY;
                                }
                            }
                        }
                        if (procs[i].state == STATE_READY)
                            procs[i].waiting_time++;
                    }
                }
                p->current_cycle++;
                p->cycle_remaining = (p->current_cycle < p->nb_cycles)
                    ? p->cycles[p->current_cycle].duration : 0;
            } else {
                p->io_remaining = c->duration;
                p->state        = STATE_WAITING;
                break;
            }
        }

        if (p->state == STATE_RUNNING) {
            p->state       = STATE_FINISHED;
            p->finish_time = time;
            finished++;
        }
    }

    gantt.total_time = time;
    gantt_print(&gantt);
    stats_print(procs, count, time);
    memcpy(processes, procs, sizeof(Process) * count);
}
