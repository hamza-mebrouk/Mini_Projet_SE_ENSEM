/*
 * Auteur : Hamza Mebrouk — ENSEM 2026
 */
/*
 * Politique FIFO (First In, First Out) — aussi appelée FCFS.
 * Non préemptive : un processus s'exécute jusqu'à la fin de son cycle CPU.
 * Les processus sont servis dans l'ordre de leur arrivée.
 */
#include <stdio.h>
#include <string.h>
#include "process.h"
#include "gantt.h"

void schedule(Process *processes, int count, int quantum) {
    (void)quantum; /* inutilisé en FIFO */

    printf("\n>>> Politique : FIFO (First In, First Out)\n");

    /* Copier et réinitialiser l'état des processus */
    Process procs[MAX_PROCESSES];
    memcpy(procs, processes, sizeof(Process) * count);
    for (int i = 0; i < count; i++) {
        procs[i].state         = STATE_NEW;
        procs[i].current_cycle = 0;
        procs[i].cycle_remaining = procs[i].nb_cycles > 0
                                   ? procs[i].cycles[0].duration : 0;
        procs[i].io_remaining  = 0;
        procs[i].waiting_time  = 0;
        procs[i].start_time    = -1;
        procs[i].finish_time   = 0;
    }

    GanttChart gantt = {.count = 0, .total_time = 0};
    int time = 0;
    int finished = 0;

    while (finished < count) {
        /* Chercher le processus FIFO : arrivé, READY, premier arrivé */
        int chosen = -1;
        for (int i = 0; i < count; i++) {
            if (procs[i].state == STATE_NEW && procs[i].arrival_time <= time)
                procs[i].state = STATE_READY;
        }

        /* Trouver le premier READY (plus petit arrival_time = FIFO) */
        for (int i = 0; i < count; i++) {
            if (procs[i].state != STATE_READY) continue;
            if (chosen < 0 ||
                procs[i].arrival_time < procs[chosen].arrival_time)
                chosen = i;
        }

        if (chosen < 0) {
            /* CPU idle */
            gantt_add(&gantt, time, "IDLE");
            time++;
            /* Avancer les IO en attente */
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

        /* Exécuter le processus choisi jusqu'à la fin de son cycle CPU */
        Process *p = &procs[chosen];
        if (p->start_time < 0) p->start_time = time;
        p->state = STATE_RUNNING;

        while (p->current_cycle < p->nb_cycles) {
            Cycle *c = &p->cycles[p->current_cycle];

            if (c->type == CYCLE_CPU) {
                gantt_add(&gantt, time, p->name);
                int dur = p->cycle_remaining;

                /* Pendant ce burst CPU, les autres processus arrivent ou font IO */
                for (int t = 0; t < dur; t++) {
                    time++;
                    /* Arrivées */
                    for (int i = 0; i < count; i++) {
                        if (procs[i].state == STATE_NEW &&
                            procs[i].arrival_time <= time)
                            procs[i].state = STATE_READY;
                    }
                    /* IO des autres */
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
                        /* Temps d'attente des READY */
                        if (procs[i].state == STATE_READY)
                            procs[i].waiting_time++;
                    }
                }
                p->current_cycle++;
                p->cycle_remaining = (p->current_cycle < p->nb_cycles)
                    ? p->cycles[p->current_cycle].duration : 0;

            } else { /* CYCLE_IO */
                p->io_remaining = c->duration;
                p->state        = STATE_WAITING;
                break; /* on repasse la main */
            }
        }

        if (p->state == STATE_RUNNING) {
            /* Plus de cycles : terminé */
            p->state       = STATE_FINISHED;
            p->finish_time = time;
            finished++;
        }
    }

    gantt.total_time = time;
    gantt_print(&gantt);
    stats_print(procs, count, time);

    /* Copier les stats dans le tableau original */
    memcpy(processes, procs, sizeof(Process) * count);
}
