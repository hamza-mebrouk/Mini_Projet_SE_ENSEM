/*
 * Auteur : Hamza Mebrouk — ENSEM 2026
 */
/*
 * Politique PRIORITY — préemptive.
 * À chaque tick, le processus READY avec la plus haute priorité s'exécute.
 * En cas d'égalité, FIFO sur la date d'arrivée.
 */
#include <stdio.h>
#include <string.h>
#include "process.h"
#include "gantt.h"

void schedule(Process *processes, int count, int quantum) {
    (void)quantum;
    printf("\n>>> Politique : PRIORITY (préemptive, plus haute priorité d'abord)\n");

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
        /* Arrivées */
        for (int i = 0; i < count; i++) {
            if (procs[i].state == STATE_NEW && procs[i].arrival_time <= time)
                procs[i].state = STATE_READY;
        }

        /* Sélection : priorité max, puis FIFO */
        int chosen = -1;
        for (int i = 0; i < count; i++) {
            if (procs[i].state != STATE_READY) continue;
            if (chosen < 0) { chosen = i; continue; }
            if (procs[i].priority > procs[chosen].priority) chosen = i;
            else if (procs[i].priority == procs[chosen].priority &&
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
        gantt_add(&gantt, time, p->name);

        /* Exécuter 1 tick (préemptif) */
        Cycle *c = &p->cycles[p->current_cycle];
        if (c->type == CYCLE_CPU) {
            p->cycle_remaining--;
            time++;
            if (p->cycle_remaining <= 0) {
                p->current_cycle++;
                if (p->current_cycle >= p->nb_cycles) {
                    p->state       = STATE_FINISHED;
                    p->finish_time = time;
                    finished++;
                } else {
                    p->cycle_remaining = p->cycles[p->current_cycle].duration;
                    if (p->cycles[p->current_cycle].type == CYCLE_IO) {
                        p->io_remaining = p->cycle_remaining;
                        p->state        = STATE_WAITING;
                    } else {
                        p->state = STATE_READY;
                    }
                }
            } else {
                p->state = STATE_READY;
            }
        } else {
            p->io_remaining = c->duration;
            p->state        = STATE_WAITING;
        }

        /* Avancer les IO et incrémenter l'attente des autres READY */
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

    gantt.total_time = time;
    gantt_print(&gantt);
    stats_print(procs, count, time);
    memcpy(processes, procs, sizeof(Process) * count);
}
