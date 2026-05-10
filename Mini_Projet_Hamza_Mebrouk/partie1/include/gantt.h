#ifndef GANTT_H
#define GANTT_H

#include "process.h"

/* Un slot du diagramme de Gantt */
typedef struct {
    int  time;           /* instant de début */
    char name[MAX_NAME_LEN]; /* nom du processus ou "IDLE" */
} GanttSlot;

#define MAX_GANTT 4096

typedef struct {
    GanttSlot slots[MAX_GANTT];
    int       count;
    int       total_time;
} GanttChart;

void gantt_add(GanttChart *g, int time, const char *name);
void gantt_print(const GanttChart *g);
void stats_print(Process *processes, int count, int total_time);

#endif
