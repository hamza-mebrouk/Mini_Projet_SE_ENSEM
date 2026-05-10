#ifndef PROCESS_H
#define PROCESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAME_LEN     64
#define MAX_CYCLES       64
#define MAX_PROCESSES    32

/* Type d'un cycle : CPU ou IO */
typedef enum {
    CYCLE_CPU,
    CYCLE_IO
} CycleType;

/* Un cycle d'exécution (CPU ou IO) */
typedef struct {
    CycleType type;
    int       duration; /* en unités de temps */
} Cycle;

/* États possibles d'un processus */
typedef enum {
    STATE_NEW,
    STATE_READY,
    STATE_RUNNING,
    STATE_WAITING,  /* en attente IO */
    STATE_FINISHED
} ProcessState;

/* Descripteur d'un processus */
typedef struct {
    char         name[MAX_NAME_LEN];
    int          arrival_time;
    int          priority;          /* plus grand = plus prioritaire */
    Cycle        cycles[MAX_CYCLES];
    int          nb_cycles;

    /* État courant (utilisé pendant la simulation) */
    ProcessState state;
    int          current_cycle;     /* indice du cycle en cours */
    int          cycle_remaining;   /* temps restant dans le cycle courant */
    int          io_remaining;      /* temps restant en IO */

    /* Statistiques */
    int          start_time;        /* première fois sur CPU */
    int          finish_time;
    int          waiting_time;      /* temps passé en READY */
    int          turnaround_time;   /* finish - arrival */
    int          response_time;     /* start - arrival */

    /* Durée CPU totale (pour SJF) */
    int          total_cpu;
} Process;

/* Tableau de processus */
typedef struct {
    Process processes[MAX_PROCESSES];
    int     count;
} ProcessSet;

/* ---- utilitaires ---- */
void process_print(const Process *p);
int  process_total_cpu(const Process *p);

#endif /* PROCESS_H */
