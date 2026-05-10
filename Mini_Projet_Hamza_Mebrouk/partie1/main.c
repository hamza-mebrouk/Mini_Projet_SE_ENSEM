/*
 * ============================================================
 *  Simulateur d'Ordonnancement de Processus
 *  Mini-Projet — Systèmes d'Exploitation
 *  ENSEM 2026
 *
 *  Auteur  : Hamza Mebrouk
 * ============================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/process.h"
#include "include/parser.h"
#include "include/scheduler.h"
#include "include/gantt.h"

#define DEFAULT_QUANTUM  2
#define POLICIES_DIR     "policies/lib"

static void print_banner(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║        SIMULATEUR D'ORDONNANCEMENT DE PROCESSUS     ║\n");
    printf("║                  ENSEM — 2026                       ║\n");
    printf("║           Réalisé par : Hamza Mebrouk               ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");
}

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s <fichier_config> [quantum] [politique]\n", prog);
    fprintf(stderr, "  fichier_config : fichier de description des processus\n");
    fprintf(stderr, "  quantum        : quantum pour Round-Robin (défaut=%d)\n", DEFAULT_QUANTUM);
    fprintf(stderr, "  politique      : fifo | sjf | rr | priority (menu si absent)\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    print_banner();

    /* --- Lecture du fichier de configuration --- */
    ProcessSet ps;
    int n = parse_config(argv[1], &ps);
    if (n <= 0) {
        fprintf(stderr, "Erreur: aucun processus chargé depuis '%s'\n", argv[1]);
        return EXIT_FAILURE;
    }

    printf("✔ %d processus chargés depuis '%s'\n\n", n, argv[1]);
    printf("Liste des processus :\n");
    for (int i = 0; i < n; i++)
        process_print(&ps.processes[i]);
    printf("\n");

    /* --- Quantum --- */
    int quantum = DEFAULT_QUANTUM;
    if (argc >= 3) quantum = atoi(argv[2]);
    if (quantum <= 0) quantum = DEFAULT_QUANTUM;

    /* --- Chargement dynamique des politiques --- */
    PolicyRegistry reg;
    registry_load(&reg, POLICIES_DIR);

    if (reg.count == 0) {
        fprintf(stderr, "Erreur: aucune politique trouvée dans '%s'\n", POLICIES_DIR);
        fprintf(stderr, "Compilez d'abord avec 'make'\n");
        return EXIT_FAILURE;
    }

    /* --- Sélection de la politique --- */
    int policy_idx = -1;

    if (argc >= 4) {
        /* Politique passée en argument */
        Policy *p = registry_find(&reg, argv[3]);
        if (!p) {
            fprintf(stderr, "Politique '%s' inconnue. Politiques disponibles:\n", argv[3]);
            for (int i = 0; i < reg.count; i++)
                fprintf(stderr, "  - %s\n", reg.entries[i].name);
            return EXIT_FAILURE;
        }
        for (int i = 0; i < reg.count; i++)
            if (&reg.entries[i] == p) { policy_idx = i; break; }
    } else {
        /* Menu interactif */
        policy_idx = menu_select(&reg);
        if (policy_idx < 0) {
            printf("Au revoir.\n");
            return EXIT_SUCCESS;
        }
    }

    /* --- Simulation --- */
    printf("\n──────────────────────────────────────────────────────\n");
    reg.entries[policy_idx].func(ps.processes, n, quantum);
    printf("──────────────────────────────────────────────────────\n");

    /* --- Relancer avec une autre politique ? --- */
    char again[8];
    printf("\nLancer une autre simulation ? (o/n) : ");
    if (scanf("%7s", again) == 1 && (again[0] == 'o' || again[0] == 'O')) {
        policy_idx = menu_select(&reg);
        if (policy_idx >= 0) {
            /* Réinitialiser les processus depuis le fichier */
            parse_config(argv[1], &ps);
            printf("\n──────────────────────────────────────────────────────\n");
            reg.entries[policy_idx].func(ps.processes, n, quantum);
            printf("──────────────────────────────────────────────────────\n");
        }
    }

    printf("\nSimulation terminée.\n\n");
    return EXIT_SUCCESS;
}
