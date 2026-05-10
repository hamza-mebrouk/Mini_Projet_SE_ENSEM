/*
 * Auteur : Hamza Mebrouk — ENSEM 2026
 */
#include <stdio.h>
#include <string.h>
#include "../include/gantt.h"

void gantt_add(GanttChart *g, int time, const char *name) {
    if (g->count >= MAX_GANTT) return;
    /* Éviter les doublons consécutifs */
    if (g->count > 0 &&
        strcmp(g->slots[g->count - 1].name, name) == 0)
        return;
    g->slots[g->count].time = time;
    strncpy(g->slots[g->count].name, name, MAX_NAME_LEN - 1);
    g->count++;
}

void gantt_print(const GanttChart *g) {
    if (g->count == 0) return;

    printf("\n╔══════════════════════════════════════════════════════╗\n");
    printf("║              DIAGRAMME DE GANTT                     ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    /* Ligne supérieure */
    printf("  ");
    for (int i = 0; i < g->count; i++) {
        int end = (i + 1 < g->count) ? g->slots[i + 1].time : g->total_time;
        int width = end - g->slots[i].time;
        if (width < 1) width = 1;
        printf("+");
        for (int k = 0; k < width * 2; k++) printf("-");
    }
    printf("+\n");

    /* Noms des processus */
    printf("  ");
    for (int i = 0; i < g->count; i++) {
        int end = (i + 1 < g->count) ? g->slots[i + 1].time : g->total_time;
        int width = (end - g->slots[i].time) * 2;
        if (width < 1) width = 2;
        printf("|");
        int nlen = (int)strlen(g->slots[i].name);
        int pad_l = (width - nlen) / 2;
        int pad_r = width - nlen - pad_l;
        for (int k = 0; k < pad_l; k++) printf(" ");
        printf("%s", g->slots[i].name);
        for (int k = 0; k < pad_r; k++) printf(" ");
    }
    printf("|\n");

    /* Ligne inférieure */
    printf("  ");
    for (int i = 0; i < g->count; i++) {
        int end = (i + 1 < g->count) ? g->slots[i + 1].time : g->total_time;
        int width = end - g->slots[i].time;
        if (width < 1) width = 1;
        printf("+");
        for (int k = 0; k < width * 2; k++) printf("-");
    }
    printf("+\n");

    /* Échelle de temps */
    printf("  ");
    for (int i = 0; i < g->count; i++) {
        int end = (i + 1 < g->count) ? g->slots[i + 1].time : g->total_time;
        int width = (end - g->slots[i].time) * 2;
        printf("%-*d", width + 1, g->slots[i].time);
    }
    printf("%d\n", g->total_time);
    printf("\n");
}

void stats_print(Process *processes, int count, int total_time) {
    printf("╔══════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                        STATISTIQUES                                     ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════════╝\n\n");

    printf("  %-12s %8s %8s %10s %12s %10s\n",
           "Processus", "Arrivée", "Fin", "Attente", "Turnaround", "Réponse");
    printf("  %-12s %8s %8s %10s %12s %10s\n",
           "------------","--------","-------","----------","------------","----------");

    double avg_wait = 0, avg_turn = 0, avg_resp = 0;
    for (int i = 0; i < count; i++) {
        Process *p = &processes[i];
        p->turnaround_time = p->finish_time - p->arrival_time;
        p->response_time   = (p->start_time >= 0) ? p->start_time - p->arrival_time : 0;

        printf("  %-12s %8d %8d %10d %12d %10d\n",
               p->name, p->arrival_time, p->finish_time,
               p->waiting_time, p->turnaround_time, p->response_time);

        avg_wait += p->waiting_time;
        avg_turn += p->turnaround_time;
        avg_resp += p->response_time;
    }

    avg_wait /= count;
    avg_turn /= count;
    avg_resp /= count;

    printf("\n  Temps total de simulation : %d unités\n", total_time);
    printf("  Temps d'attente moyen    : %.2f\n", avg_wait);
    printf("  Turnaround moyen         : %.2f\n", avg_turn);
    printf("  Temps de réponse moyen   : %.2f\n\n", avg_resp);
}
