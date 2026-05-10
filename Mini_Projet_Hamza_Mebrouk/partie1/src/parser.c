/*
 * Auteur : Hamza Mebrouk — ENSEM 2026
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../include/parser.h"

/* Supprime les espaces en début/fin de chaîne (in-place) */
static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

int parse_config(const char *filename, ProcessSet *ps) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Erreur: impossible d'ouvrir '%s'\n", filename);
        return -1;
    }

    ps->count = 0;
    char line[256];
    Process *cur = NULL;

    while (fgets(line, sizeof(line), f)) {
        char *s = trim(line);

        /* Ignorer lignes vides et commentaires */
        if (s[0] == '\0' || s[0] == '#') continue;

        /* Début d'un nouveau processus */
        if (strncasecmp(s, "PROCESS", 7) == 0) {
            if (ps->count >= MAX_PROCESSES) {
                fprintf(stderr, "Erreur: trop de processus (max %d)\n", MAX_PROCESSES);
                fclose(f);
                return -1;
            }
            cur = &ps->processes[ps->count++];
            memset(cur, 0, sizeof(*cur));
            cur->priority = 0;
            cur->state    = STATE_NEW;
            cur->start_time = -1;

            /* Lire: PROCESS <nom> <arrivée> [priorité] */
            int n = sscanf(s + 7, " %63s %d %d",
                           cur->name, &cur->arrival_time, &cur->priority);
            if (n < 2) {
                fprintf(stderr, "Erreur syntaxe ligne: %s\n", s);
                fclose(f);
                return -1;
            }
            cur->nb_cycles = 0;
        }
        /* Cycle CPU */
        else if (strncasecmp(s, "CPU", 3) == 0 && cur) {
            if (cur->nb_cycles >= MAX_CYCLES) {
                fprintf(stderr, "Erreur: trop de cycles pour %s\n", cur->name);
                continue;
            }
            int dur = 0;
            sscanf(s + 3, " %d", &dur);
            cur->cycles[cur->nb_cycles].type     = CYCLE_CPU;
            cur->cycles[cur->nb_cycles].duration = dur;
            cur->nb_cycles++;
        }
        /* Cycle IO */
        else if (strncasecmp(s, "IO", 2) == 0 && cur) {
            if (cur->nb_cycles >= MAX_CYCLES) continue;
            int dur = 0;
            sscanf(s + 2, " %d", &dur);
            cur->cycles[cur->nb_cycles].type     = CYCLE_IO;
            cur->cycles[cur->nb_cycles].duration = dur;
            cur->nb_cycles++;
        }
        /* Fin de définition */
        else if (strncasecmp(s, "END", 3) == 0) {
            if (cur) cur->total_cpu = process_total_cpu(cur);
            cur = NULL;
        }
    }

    /* Calculer total_cpu pour le dernier processus si END manquant */
    if (cur) cur->total_cpu = process_total_cpu(cur);

    fclose(f);
    return ps->count;
}
