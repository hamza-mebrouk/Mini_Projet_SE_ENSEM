/*
 * Auteur : Hamza Mebrouk — ENSEM 2026
 */
/*
 * Politique Round-Robin (RR) — préemptive.
 * Chaque processus reçoit un quantum de temps fixe sur le CPU.
 * Si son burst CPU n'est pas terminé, il est remis en fin de file READY.
 */
#include <stdio.h>
#include <string.h>
#include "process.h"
#include "gantt.h"

/* File d'attente circulaire simple */
#define QUEUE_SIZE (MAX_PROCESSES * 128)

typedef struct {
    int data[QUEUE_SIZE];
    int head, tail, size;
} Queue;

static void q_init(Queue *q) { q->head = q->tail = q->size = 0; }

static int q_empty(const Queue *q) { return q->size == 0; }

static void q_push(Queue *q, int val) {
    if (q->size >= QUEUE_SIZE) return;
    q->data[q->tail] = val;
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    q->size++;
}

static int q_pop(Queue *q) {
    int val = q->data[q->head];
    q->head = (q->head + 1) % QUEUE_SIZE;
    q->size--;
    return val;
}

static int q_contains(const Queue *q, int val) {
    for (int i = 0; i < q->size; i++)
        if (q->data[(q->head + i) % QUEUE_SIZE] == val) return 1;
    return 0;
}

void schedule(Process *processes, int count, int quantum) {
    if (quantum <= 0) quantum = 2;
    printf("\n>>> Politique : Round-Robin (quantum = %d)\n", quantum);

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
    Queue ready;
    q_init(&ready);
    int time = 0, finished = 0;

    /* Ajouter les processus arrivés à t=0 */
    for (int i = 0; i < count; i++) {
        if (procs[i].arrival_time <= 0) {
            procs[i].state = STATE_READY;
            q_push(&ready, i);
        }
    }

    while (finished < count) {
        /* Idle si la file est vide */
        if (q_empty(&ready)) {
            gantt_add(&gantt, time, "IDLE");
            time++;
            /* Avancer les IO et les arrivées */
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
                            q_push(&ready, i);
                        }
                    }
                }
                if (procs[i].state == STATE_NEW && procs[i].arrival_time <= time) {
                    procs[i].state = STATE_READY;
                    if (!q_contains(&ready, i)) q_push(&ready, i);
                }
            }
            continue;
        }

        int chosen = q_pop(&ready);
        Process *p = &procs[chosen];
        if (p->start_time < 0) p->start_time = time;
        p->state = STATE_RUNNING;

        /* Sauter les cycles IO en cours (ne devrait pas arriver ici) */
        while (p->current_cycle < p->nb_cycles &&
               p->cycles[p->current_cycle].type == CYCLE_IO) {
            p->current_cycle++;
            if (p->current_cycle < p->nb_cycles)
                p->cycle_remaining = p->cycles[p->current_cycle].duration;
        }

        if (p->current_cycle >= p->nb_cycles) {
            p->state       = STATE_FINISHED;
            p->finish_time = time;
            finished++;
            continue;
        }

        /* Exécuter au max 'quantum' unités */
        gantt_add(&gantt, time, p->name);
        int run = 0;
        int preempted = 0;

        while (run < quantum && p->current_cycle < p->nb_cycles) {
            Cycle *c = &p->cycles[p->current_cycle];

            if (c->type == CYCLE_IO) {
                /* Passer en IO */
                p->io_remaining = c->duration;
                p->state        = STATE_WAITING;
                preempted       = 0;
                break;
            }

            /* CPU tick */
            p->cycle_remaining--;
            run++;
            time++;

            /* Arrivées et IO des autres */
            for (int i = 0; i < count; i++) {
                if (procs[i].state == STATE_NEW && procs[i].arrival_time <= time) {
                    procs[i].state = STATE_READY;
                    if (!q_contains(&ready, i)) q_push(&ready, i);
                }
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
                            if (!q_contains(&ready, i)) q_push(&ready, i);
                        }
                    }
                }
                if (procs[i].state == STATE_READY)
                    procs[i].waiting_time++;
            }

            if (p->cycle_remaining <= 0) {
                p->current_cycle++;
                if (p->current_cycle < p->nb_cycles)
                    p->cycle_remaining = p->cycles[p->current_cycle].duration;
            }

            if (run == quantum && p->current_cycle < p->nb_cycles &&
                p->cycles[p->current_cycle].type == CYCLE_CPU)
                preempted = 1;
        }

        if (p->state == STATE_RUNNING) {
            if (p->current_cycle >= p->nb_cycles) {
                p->state       = STATE_FINISHED;
                p->finish_time = time;
                finished++;
            } else if (preempted) {
                p->state = STATE_READY;
                q_push(&ready, chosen);
            } else if (p->cycles[p->current_cycle].type == CYCLE_IO) {
                p->io_remaining = p->cycles[p->current_cycle].duration;
                p->state        = STATE_WAITING;
            }
        }
    }

    gantt.total_time = time;
    gantt_print(&gantt);
    stats_print(procs, count, time);
    memcpy(processes, procs, sizeof(Process) * count);
}
