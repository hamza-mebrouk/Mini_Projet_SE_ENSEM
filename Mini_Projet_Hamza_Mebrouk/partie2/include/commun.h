/*
 * ============================================================
 *  Smart Factory — Simulation de Gestion Distribuée de Ressources
 *  Mini-Projet Systèmes d'Exploitation — ENSEM 2026
 *
 *  Auteur : Hamza Mebrouk
 * ============================================================
 *  commun.h — Définitions partagées client/serveur
 * ============================================================
 */
#ifndef COMMUN_H
#define COMMUN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ── Réseau ─────────────────────────────────────────────── */
#define SERVER_PORT     9000
#define SERVER_ADDR     "127.0.0.1"
#define BACKLOG         16
#define BUF_SIZE        256

/* ── Outils ─────────────────────────────────────────────── */
#define NB_OUTILS       5       /* tournevis, pince, soudeuse, perceuse, clé */
#define NB_BRAS         4       /* nombre de bras robotiques */

/* ── Priorités QoS ──────────────────────────────────────── */
#define PRIORITE_BASSE    1
#define PRIORITE_NORMALE  2
#define PRIORITE_HAUTE    3
#define PRIORITE_URGENTE  4

/* ── Protocole textuel client → serveur ─────────────────── */
/*
 * DEMANDE_OUTIL     <id_outil> <id_bras> <priorite>
 * DEMANDE_DEUX_OUTILS <id1> <id2> <id_bras> <priorite>
 * LIBERATION_OUTIL  <id_outil> <id_bras>
 * STATUS
 */

/* ── Réponses serveur → client ──────────────────────────── */
/*
 * OK      <id_outil>
 * REFUSE  <id_outil>   (outil occupé)
 * ATTENTE <id_outil>   (mis en file d'attente)
 * LIBERE  <id_outil>
 * STATUS  <données>
 */

/* ── Noms des outils ────────────────────────────────────── */
static const char *NOM_OUTIL[NB_OUTILS] = {
    "Tournevis", "Pince", "Soudeuse", "Perceuse", "Cle"
};

/* ── Noms des bras ──────────────────────────────────────── */
static const char *NOM_BRAS[NB_BRAS] = {
    "Bras-Alpha", "Bras-Beta", "Bras-Gamma", "Bras-Delta"
};

/* ── Utilitaire : timestamp ─────────────────────────────── */
static inline void print_timestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm *tm_info = localtime(&ts.tv_sec);
    char buf[32];
    strftime(buf, sizeof(buf), "%H:%M:%S", tm_info);
    printf("[%s.%03ld] ", buf, ts.tv_nsec / 1000000);
}

/* ── Utilitaire : log avec mutex ────────────────────────── */
extern pthread_mutex_t log_mutex;

#define LOG(...) do { \
    pthread_mutex_lock(&log_mutex); \
    print_timestamp(); \
    printf(__VA_ARGS__); \
    fflush(stdout); \
    pthread_mutex_unlock(&log_mutex); \
} while(0)

/* ── Utilitaire : sleep aléatoire ───────────────────────── */
static inline void sleep_ms(int ms) {
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static inline int rand_range(int min, int max) {
    return min + rand() % (max - min + 1);
}

#endif /* COMMUN_H */
