#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"

/*
 * Profil unique de toute politique d'ordonnancement.
 *
 * Paramètres :
 *   processes   : tableau des processus (modifiable)
 *   count       : nombre de processus
 *   quantum     : quantum de temps (utilisé par Round-Robin ; ignoré sinon)
 *
 * La fonction effectue la simulation complète et affiche le diagramme de Gantt
 * ainsi que les statistiques finales.
 */
typedef void (*SchedulerFunc)(Process *processes, int count, int quantum);

/* Structure décrivant une politique disponible */
typedef struct {
    char          name[64];       /* nom affiché dans le menu */
    SchedulerFunc func;
} Policy;

/* Enregistrement / recherche des politiques (chargées depuis le répertoire) */
#define MAX_POLICIES 16

typedef struct {
    Policy entries[MAX_POLICIES];
    int    count;
} PolicyRegistry;

void    registry_load(PolicyRegistry *reg, const char *dir);
Policy *registry_find(PolicyRegistry *reg, const char *name);

/* Affichage du menu et sélection */
int menu_select(const PolicyRegistry *reg);

#endif /* SCHEDULER_H */
