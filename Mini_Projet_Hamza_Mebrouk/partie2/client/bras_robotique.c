/*
 * ============================================================
 *  Smart Factory — Client : Bras Robotique
 *  Mini-Projet Systèmes d'Exploitation — ENSEM 2026
 *
 *  Auteur : Hamza Mebrouk
 * ============================================================
 *
 *  Architecture multi-thread (3 threads par bras) :
 *   Thread 1 (idle)      — simule les phases de réflexion/attente
 *   Thread 2 (comm)      — envoie les requêtes TCP au serveur
 *   Thread 3 (assemblage)— exécute la tâche d'assemblage
 *
 *  Stratégie anti-deadlock côté client :
 *   - Demande toujours les outils dans l'ordre croissant d'indice
 *   - Timeout sur recv() + réessai (Option C)
 *   - Si le serveur refuse, pause aléatoire puis réessai
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include "../include/commun.h"

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ── Configuration du bras ──────────────────────────────── */
typedef struct {
    int   bras_id;
    char  bras_name[32];
    int   priorite;
    int   nb_taches;        /* nombre de tâches d'assemblage à effectuer */
    int   outil1;           /* premier outil nécessaire */
    int   outil2;           /* second outil nécessaire */
    int   sock_fd;          /* socket vers le serveur */
} BrasConfig;

/* ── État partagé entre les threads ─────────────────────── */
typedef enum {
    PHASE_IDLE,
    PHASE_DEMANDE,
    PHASE_ASSEMBLAGE,
    PHASE_TERMINE
} Phase;

typedef struct {
    Phase           phase;
    pthread_mutex_t mutex;
    pthread_cond_t  cond_comm;
    pthread_cond_t  cond_idle;
    pthread_cond_t  cond_assemblage;
    int             outils_obtenus;
    int             tache_courante;
    BrasConfig     *cfg;
} BrasState;

/* ── Communication TCP ──────────────────────────────────── */
static int tcp_connect(const char *host, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    /* Timeout recv = 5s */
    struct timeval tv = { 5, 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(port)
    };
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }
    return fd;
}

/* Envoie une commande et attend la réponse */
static int envoyer_commande(int fd, const char *cmd, char *rep, int rep_sz) {
    if (send(fd, cmd, strlen(cmd), 0) < 0) return -1;
    memset(rep, 0, rep_sz);
    ssize_t n = recv(fd, rep, rep_sz - 1, 0);
    if (n <= 0) return -1;
    rep[strcspn(rep, "\r\n")] = '\0';
    return (int)n;
}

/* ── Thread 1 : Idle ────────────────────────────────────── */
static void *thread_idle(void *arg) {
    BrasState *s = (BrasState *)arg;

    while (1) {
        pthread_mutex_lock(&s->mutex);
        while (s->phase != PHASE_IDLE && s->phase != PHASE_TERMINE)
            pthread_cond_wait(&s->cond_idle, &s->mutex);

        if (s->phase == PHASE_TERMINE) {
            pthread_mutex_unlock(&s->mutex);
            break;
        }
        pthread_mutex_unlock(&s->mutex);

        int ms = rand_range(500, 1500);
        LOG("💤 %s en réflexion (%d ms)...\n", s->cfg->bras_name, ms);
        sleep_ms(ms);

        pthread_mutex_lock(&s->mutex);
        if (s->phase == PHASE_IDLE) {
            s->phase = PHASE_DEMANDE;
            pthread_cond_signal(&s->cond_comm);
        }
        pthread_mutex_unlock(&s->mutex);
    }
    return NULL;
}

/* ── Thread 2 : Communication ───────────────────────────── */
static void *thread_comm(void *arg) {
    BrasState  *s   = (BrasState *)arg;
    BrasConfig *cfg = s->cfg;
    char cmd[BUF_SIZE], rep[BUF_SIZE];

    while (1) {
        pthread_mutex_lock(&s->mutex);
        while (s->phase != PHASE_DEMANDE && s->phase != PHASE_TERMINE)
            pthread_cond_wait(&s->cond_comm, &s->mutex);

        if (s->phase == PHASE_TERMINE) {
            pthread_mutex_unlock(&s->mutex);
            break;
        }
        pthread_mutex_unlock(&s->mutex);

        LOG("🔧 %s demande les outils [%s]+[%s]\n",
            cfg->bras_name, NOM_OUTIL[cfg->outil1], NOM_OUTIL[cfg->outil2]);

        /* Demander les deux outils (ordre fixe : min en premier) */
        int o1 = cfg->outil1 < cfg->outil2 ? cfg->outil1 : cfg->outil2;
        int o2 = cfg->outil1 < cfg->outil2 ? cfg->outil2 : cfg->outil1;

        snprintf(cmd, sizeof(cmd), "DEMANDE_DEUX_OUTILS %d %d %d %d\n",
                 o1, o2, cfg->bras_id, cfg->priorite);

        int ok = 0;
        int tentatives = 0;
        while (!ok && tentatives < 5) {
            tentatives++;
            int r = envoyer_commande(cfg->sock_fd, cmd, rep, sizeof(rep));
            if (r < 0) {
                LOG("⚠  %s: erreur réseau, réessai %d/5...\n",
                    cfg->bras_name, tentatives);
                sleep_ms(rand_range(300, 800));
                continue;
            }

            if (strncmp(rep, "OK2", 3) == 0) {
                LOG("✅ %s a obtenu [%s]+[%s]\n",
                    cfg->bras_name, NOM_OUTIL[o1], NOM_OUTIL[o2]);
                ok = 1;
            } else if (strncmp(rep, "OK ", 3) == 0) {
                /* Premier outil ok, attendre le second */
                LOG("⏳ %s: [%s] obtenu, attend [%s]...\n",
                    cfg->bras_name, NOM_OUTIL[o1], NOM_OUTIL[o2]);
                /* Attendre la notification (réponse différée du serveur) */
                memset(rep, 0, sizeof(rep));
                ssize_t nr = recv(cfg->sock_fd, rep, sizeof(rep) - 1, 0);
                if (nr > 0) {
                    rep[strcspn(rep, "\r\n")] = '\0';
                    if (strncmp(rep, "OK", 2) == 0) {
                        LOG("✅ %s a obtenu [%s] (depuis file)\n",
                            cfg->bras_name, NOM_OUTIL[o2]);
                        ok = 1;
                    } else {
                        /* Libérer le premier outil pour éviter le livelock */
                        char lib_cmd[BUF_SIZE], lib_rep[BUF_SIZE];
                        snprintf(lib_cmd, sizeof(lib_cmd),
                                 "LIBERATION_OUTIL %d %d\n", o1, cfg->bras_id);
                        envoyer_commande(cfg->sock_fd, lib_cmd, lib_rep, sizeof(lib_rep));
                        LOG("🔓 %s: libère [%s] pour éviter livelock\n",
                            cfg->bras_name, NOM_OUTIL[o1]);
                    }
                } else {
                    /* Timeout réseau : libérer o1 */
                    char lib_cmd[BUF_SIZE], lib_rep[BUF_SIZE];
                    snprintf(lib_cmd, sizeof(lib_cmd),
                             "LIBERATION_OUTIL %d %d\n", o1, cfg->bras_id);
                    envoyer_commande(cfg->sock_fd, lib_cmd, lib_rep, sizeof(lib_rep));
                }
            } else if (strncmp(rep, "REFUSE", 6) == 0 ||
                       strncmp(rep, "ATTENTE", 7) == 0) {
                int wait_ms = rand_range(500, 1500);
                LOG("❌ %s: refus/attente, réessai dans %d ms\n",
                    cfg->bras_name, wait_ms);
                sleep_ms(wait_ms);
            } else {
                LOG("⚠  %s: réponse inattendue: %s\n", cfg->bras_name, rep);
                sleep_ms(500);
            }
        }

        pthread_mutex_lock(&s->mutex);
        if (ok) {
            s->outils_obtenus = 1;
            s->phase          = PHASE_ASSEMBLAGE;
            pthread_cond_signal(&s->cond_assemblage);
        } else {
            LOG("⚠  %s: impossible d'obtenir les outils après %d tentatives\n",
                cfg->bras_name, tentatives);
            s->phase = PHASE_IDLE;
            pthread_cond_signal(&s->cond_idle);
        }
        pthread_mutex_unlock(&s->mutex);
    }
    return NULL;
}

/* ── Thread 3 : Assemblage ──────────────────────────────── */
static void *thread_assemblage(void *arg) {
    BrasState  *s   = (BrasState *)arg;
    BrasConfig *cfg = s->cfg;
    char cmd[BUF_SIZE], rep[BUF_SIZE];

    while (1) {
        pthread_mutex_lock(&s->mutex);
        while (s->phase != PHASE_ASSEMBLAGE && s->phase != PHASE_TERMINE)
            pthread_cond_wait(&s->cond_assemblage, &s->mutex);

        if (s->phase == PHASE_TERMINE) {
            pthread_mutex_unlock(&s->mutex);
            break;
        }
        s->tache_courante++;
        int tache = s->tache_courante;
        int nb    = cfg->nb_taches;
        pthread_mutex_unlock(&s->mutex);

        int duree = rand_range(800, 2000);
        LOG("⚙  %s ASSEMBLAGE tâche %d/%d (durée %d ms)...\n",
            cfg->bras_name, tache, nb, duree);
        sleep_ms(duree);
        LOG("🏁 %s tâche %d terminée\n", cfg->bras_name, tache);

        /* Libérer les outils */
        int o1 = cfg->outil1 < cfg->outil2 ? cfg->outil1 : cfg->outil2;
        int o2 = cfg->outil1 < cfg->outil2 ? cfg->outil2 : cfg->outil1;

        snprintf(cmd, sizeof(cmd), "LIBERATION_OUTIL %d %d\n", o1, cfg->bras_id);
        envoyer_commande(cfg->sock_fd, cmd, rep, sizeof(rep));

        snprintf(cmd, sizeof(cmd), "LIBERATION_OUTIL %d %d\n", o2, cfg->bras_id);
        envoyer_commande(cfg->sock_fd, cmd, rep, sizeof(rep));

        LOG("🔓 %s a libéré [%s]+[%s]\n",
            cfg->bras_name, NOM_OUTIL[o1], NOM_OUTIL[o2]);

        pthread_mutex_lock(&s->mutex);
        s->outils_obtenus = 0;
        if (tache >= nb) {
            s->phase = PHASE_TERMINE;
            pthread_cond_broadcast(&s->cond_idle);
            pthread_cond_broadcast(&s->cond_comm);
            pthread_cond_broadcast(&s->cond_assemblage);
        } else {
            s->phase = PHASE_IDLE;
            pthread_cond_signal(&s->cond_idle);
        }
        pthread_mutex_unlock(&s->mutex);
    }

    LOG("🏆 %s a terminé toutes ses tâches !\n", cfg->bras_name);
    return NULL;
}

/* ── Lancement d'un bras ────────────────────────────────── */
typedef struct {
    BrasConfig cfg;
} BrasArgs;

static void *lancer_bras(void *arg) {
    BrasArgs   *ba  = (BrasArgs *)arg;
    BrasConfig *cfg = &ba->cfg;

    /* Connexion au serveur */
    cfg->sock_fd = tcp_connect(SERVER_ADDR, SERVER_PORT);
    if (cfg->sock_fd < 0) {
        LOG("❌ %s: impossible de se connecter au serveur\n", cfg->bras_name);
        free(ba);
        return NULL;
    }
    LOG("🔌 %s connecté au serveur\n", cfg->bras_name);

    BrasState state = {
        .phase          = PHASE_IDLE,
        .mutex          = PTHREAD_MUTEX_INITIALIZER,
        .cond_comm      = PTHREAD_COND_INITIALIZER,
        .cond_idle      = PTHREAD_COND_INITIALIZER,
        .cond_assemblage= PTHREAD_COND_INITIALIZER,
        .outils_obtenus = 0,
        .tache_courante = 0,
        .cfg            = cfg
    };

    pthread_t t1, t2, t3;
    pthread_create(&t1, NULL, thread_idle,       &state);
    pthread_create(&t2, NULL, thread_comm,       &state);
    pthread_create(&t3, NULL, thread_assemblage, &state);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    close(cfg->sock_fd);
    free(ba);
    return NULL;
}

/* ── Main ───────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    srand((unsigned)time(NULL));

    printf("\n╔══════════════════════════════════════════════════════╗\n");
    printf("║   Smart Factory — Bras Robotiques                   ║\n");
    printf("║   ENSEM 2026  —  Auteur : Hamza Mebrouk             ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    /*
     * Utilisation : ./bras_robotique [nb_taches]
     * Par défaut : 3 tâches par bras, 4 bras lancés en parallèle
     */
    int nb_taches = 3;
    if (argc >= 2) nb_taches = atoi(argv[1]);
    if (nb_taches <= 0) nb_taches = 3;

    printf("Démarrage de %d bras robotiques (%d tâches chacun)...\n\n",
           NB_BRAS, nb_taches);

    /*
     * Configuration des 4 bras :
     * Chaque bras a besoin de 2 outils spécifiques.
     * Les paires sont choisies pour créer des contentions intéressantes.
     *
     *  Bras-Alpha  : Tournevis(0) + Pince(1)      priorité URGENTE
     *  Bras-Beta   : Pince(1)     + Soudeuse(2)   priorité HAUTE
     *  Bras-Gamma  : Soudeuse(2)  + Perceuse(3)   priorité NORMALE
     *  Bras-Delta  : Perceuse(3)  + Cle(4)        priorité BASSE
     */
    int outils_paires[NB_BRAS][2] = {
        {0, 1},  /* Alpha  */
        {1, 2},  /* Beta   */
        {2, 3},  /* Gamma  */
        {3, 4}   /* Delta  */
    };
    int priorites[NB_BRAS] = {
        PRIORITE_URGENTE, PRIORITE_HAUTE, PRIORITE_NORMALE, PRIORITE_BASSE
    };

    pthread_t bras_threads[NB_BRAS];

    for (int i = 0; i < NB_BRAS; i++) {
        /* Petite pause pour échelonner les connexions */
        sleep_ms(rand_range(50, 200));

        BrasArgs *ba = malloc(sizeof(BrasArgs));
        ba->cfg.bras_id  = i;
        ba->cfg.priorite = priorites[i];
        ba->cfg.nb_taches= nb_taches;
        ba->cfg.outil1   = outils_paires[i][0];
        ba->cfg.outil2   = outils_paires[i][1];
        ba->cfg.sock_fd  = -1;
        snprintf(ba->cfg.bras_name, sizeof(ba->cfg.bras_name),
                 "%s", NOM_BRAS[i]);

        pthread_create(&bras_threads[i], NULL, lancer_bras, ba);
    }

    for (int i = 0; i < NB_BRAS; i++)
        pthread_join(bras_threads[i], NULL);

    printf("\n✅ Tous les bras ont terminé leurs tâches.\n\n");
    return 0;
}
