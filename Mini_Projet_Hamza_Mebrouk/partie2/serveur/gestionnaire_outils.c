/*
 * ============================================================
 *  Smart Factory — Serveur : Gestionnaire d'Outils
 *  Mini-Projet Systèmes d'Exploitation — ENSEM 2026
 *
 *  Auteur : Hamza Mebrouk
 * ============================================================
 *
 *  Fonctionnalités :
 *   - Gestion des connexions TCP multi-clients (un thread par bras)
 *   - État de chaque outil protégé par mutex
 *   - File d'attente prioritaire (QoS) par outil
 *   - Prévention des interblocages : hiérarchie fixe des ressources
 *     (Option A) + timeout avec réessai (Option C)
 *   - Journal horodaté de toutes les actions
 *   - Commande STATUS pour visualiser l'état en temps réel
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include "../include/commun.h"

/* ── Journal fichier ────────────────────────────────────── */
static FILE *log_file = NULL;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static void log_action(const char *fmt, ...) {
    pthread_mutex_lock(&log_mutex);

    /* Timestamp */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm *t = localtime(&ts.tv_sec);
    char tbuf[32];
    strftime(tbuf, sizeof(tbuf), "%H:%M:%S", t);

    /* Console */
    printf("[%s.%03ld] ", tbuf, ts.tv_nsec / 1000000);
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);

    /* Fichier log */
    if (log_file) {
        fprintf(log_file, "[%s.%03ld] ", tbuf, ts.tv_nsec / 1000000);
        va_list ap2;
        va_start(ap2, fmt);
        vfprintf(log_file, fmt, ap2);
        va_end(ap2);
        fflush(log_file);
    }

    pthread_mutex_unlock(&log_mutex);
}

/* ── File d'attente prioritaire ─────────────────────────── */
#define MAX_WAITERS 32

typedef struct {
    int  client_fd;   /* socket du client en attente */
    int  bras_id;
    int  priorite;
    char bras_name[32];
} Waiter;

typedef struct {
    Waiter items[MAX_WAITERS];
    int    count;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
} WaitQueue;

static void wq_init(WaitQueue *q) {
    q->count = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);
}

/* Insertion triée par priorité décroissante */
static void wq_push(WaitQueue *q, Waiter w) {
    if (q->count >= MAX_WAITERS) return;
    int i = q->count - 1;
    while (i >= 0 && q->items[i].priorite < w.priorite) {
        q->items[i + 1] = q->items[i];
        i--;
    }
    q->items[i + 1] = w;
    q->count++;
}

static int wq_pop(WaitQueue *q, Waiter *out) {
    if (q->count == 0) return 0;
    *out = q->items[0];
    memmove(&q->items[0], &q->items[1], sizeof(Waiter) * (q->count - 1));
    q->count--;
    return 1;
}

static int wq_remove(WaitQueue *q, int bras_id) {
    for (int i = 0; i < q->count; i++) {
        if (q->items[i].bras_id == bras_id) {
            memmove(&q->items[i], &q->items[i+1],
                    sizeof(Waiter) * (q->count - i - 1));
            q->count--;
            return 1;
        }
    }
    return 0;
}

/* ── État des outils ────────────────────────────────────── */
typedef struct {
    int          libre;          /* 1 = disponible, 0 = occupé */
    int          proprietaire;   /* bras_id du détenteur (-1 si libre) */
    char         prop_name[32];
    pthread_mutex_t mutex;
    WaitQueue    waiters;
} Outil;

static Outil outils[NB_OUTILS];

static void outils_init(void) {
    for (int i = 0; i < NB_OUTILS; i++) {
        outils[i].libre        = 1;
        outils[i].proprietaire = -1;
        outils[i].prop_name[0] = '\0';
        pthread_mutex_init(&outils[i].mutex, NULL);
        wq_init(&outils[i].waiters);
    }
}

/* ── Graphe d'attente (détection deadlock) ──────────────── */
/*
 * wait_graph[i][j] = 1 signifie que le bras i attend un outil
 * détenu par le bras j.
 * On vérifie l'absence de cycle avant chaque mise en attente.
 */
static int wait_graph[NB_BRAS][NB_BRAS];
static pthread_mutex_t wg_mutex = PTHREAD_MUTEX_INITIALIZER;

static void wg_add(int from, int to) {
    pthread_mutex_lock(&wg_mutex);
    if (from >= 0 && from < NB_BRAS && to >= 0 && to < NB_BRAS)
        wait_graph[from][to] = 1;
    pthread_mutex_unlock(&wg_mutex);
}

static void wg_remove(int from, int to) {
    pthread_mutex_lock(&wg_mutex);
    if (from >= 0 && from < NB_BRAS && to >= 0 && to < NB_BRAS)
        wait_graph[from][to] = 0;
    pthread_mutex_unlock(&wg_mutex);
}

/* DFS pour détecter un cycle depuis 'start' */
static int dfs_cycle(int node, int visited[], int rec_stack[]) {
    visited[node]   = 1;
    rec_stack[node] = 1;
    for (int i = 0; i < NB_BRAS; i++) {
        if (!wait_graph[node][i]) continue;
        if (!visited[i] && dfs_cycle(i, visited, rec_stack)) return 1;
        if (rec_stack[i]) return 1;
    }
    rec_stack[node] = 0;
    return 0;
}

static int wg_has_cycle(void) {
    int visited[NB_BRAS]   = {0};
    int rec_stack[NB_BRAS] = {0};
    for (int i = 0; i < NB_BRAS; i++)
        if (!visited[i] && dfs_cycle(i, visited, rec_stack))
            return 1;
    return 0;
}

/* ── Allocation / Libération ────────────────────────────── */

/*
 * Tente d'allouer l'outil id_outil au bras bras_id.
 * Retourne :
 *   1 = succès
 *   0 = refus (deadlock détecté ou outil occupé sans mise en file)
 *  -1 = mis en file d'attente (réponse différée)
 *
 * Prévention deadlock (Option A) : ordre fixe d'acquisition.
 * Si le bras détient déjà un outil d'indice supérieur, on refuse.
 */
static int allouer_outil(int id_outil, int bras_id, int priorite,
                          const char *bras_name, int client_fd,
                          int en_file) {
    /* Option A : hiérarchie fixe — un bras ne peut demander qu'un outil
     * d'indice >= au maximum outil qu'il détient déjà. */
    for (int i = id_outil + 1; i < NB_OUTILS; i++) {
        pthread_mutex_lock(&outils[i].mutex);
        int detient = (outils[i].proprietaire == bras_id);
        pthread_mutex_unlock(&outils[i].mutex);
        if (detient) {
            log_action("⚠  DEADLOCK PREVENTIF : %s refuse outil %s "
                       "(détient déjà outil %s d'ordre supérieur)\n",
                       bras_name, NOM_OUTIL[id_outil], NOM_OUTIL[i]);
            return 0;
        }
    }

    pthread_mutex_lock(&outils[id_outil].mutex);

    if (outils[id_outil].libre) {
        outils[id_outil].libre        = 0;
        outils[id_outil].proprietaire = bras_id;
        strncpy(outils[id_outil].prop_name, bras_name, 31);
        pthread_mutex_unlock(&outils[id_outil].mutex);

        log_action("✔  %s a obtenu  [%s]\n", bras_name, NOM_OUTIL[id_outil]);
        return 1;
    }

    /* Outil occupé → mise en file si autorisé */
    if (en_file) {
        int prop = outils[id_outil].proprietaire;

        /* Vérifier deadlock avant mise en attente */
        pthread_mutex_lock(&wg_mutex);
        wait_graph[bras_id][prop] = 1;
        int cycle = wg_has_cycle();
        if (cycle) wait_graph[bras_id][prop] = 0;
        pthread_mutex_unlock(&wg_mutex);

        if (cycle) {
            pthread_mutex_unlock(&outils[id_outil].mutex);
            log_action("🔴 DEADLOCK DETECTE : %s abandonne la demande de [%s]\n",
                       bras_name, NOM_OUTIL[id_outil]);
            return 0;
        }

        Waiter w = { client_fd, bras_id, priorite, "" };
        strncpy(w.bras_name, bras_name, 31);
        wq_push(&outils[id_outil].waiters, w);
        pthread_mutex_unlock(&outils[id_outil].mutex);

        log_action("⏳ %s en attente de [%s] (détenu par %s, priorité=%d)\n",
                   bras_name, NOM_OUTIL[id_outil],
                   outils[id_outil].prop_name, priorite);
        return -1; /* réponse différée */
    }

    pthread_mutex_unlock(&outils[id_outil].mutex);
    return 0;
}

static void liberer_outil(int id_outil, int bras_id, const char *bras_name) {
    pthread_mutex_lock(&outils[id_outil].mutex);

    if (outils[id_outil].proprietaire != bras_id) {
        pthread_mutex_unlock(&outils[id_outil].mutex);
        log_action("⚠  %s tente de libérer [%s] qu'il ne détient pas\n",
                   bras_name, NOM_OUTIL[id_outil]);
        return;
    }

    log_action("🔓 %s libère [%s]\n", bras_name, NOM_OUTIL[id_outil]);

    /* Retirer les arêtes du graphe d'attente */
    for (int i = 0; i < NB_BRAS; i++)
        wg_remove(i, bras_id);

    /* Donner l'outil au prochain en file (priorité la plus haute) */
    Waiter next;
    if (wq_pop(&outils[id_outil].waiters, &next)) {
        outils[id_outil].proprietaire = next.bras_id;
        strncpy(outils[id_outil].prop_name, next.bras_name, 31);
        /* L'outil reste non-libre, appartient à next */
        pthread_mutex_unlock(&outils[id_outil].mutex);

        wg_remove(next.bras_id, bras_id);
        log_action("✔  [%s] transmis à %s (depuis file)\n",
                   NOM_OUTIL[id_outil], next.bras_name);

        /* Notifier le client en attente */
        char rep[BUF_SIZE];
        snprintf(rep, sizeof(rep), "OK %d\n", id_outil);
        send(next.client_fd, rep, strlen(rep), 0);
    } else {
        outils[id_outil].libre        = 1;
        outils[id_outil].proprietaire = -1;
        outils[id_outil].prop_name[0] = '\0';
        pthread_mutex_unlock(&outils[id_outil].mutex);
    }
}

/* ── Affichage status ───────────────────────────────────── */
static void afficher_status(int client_fd) {
    char buf[2048];
    int  pos = 0;

    pos += snprintf(buf + pos, sizeof(buf) - pos,
        "\n╔══════════════════════════════════════════╗\n"
        "║         ÉTAT DES OUTILS                 ║\n"
        "╠══════════════════════════════════════════╣\n");

    for (int i = 0; i < NB_OUTILS; i++) {
        pthread_mutex_lock(&outils[i].mutex);
        if (outils[i].libre)
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                "║  %-12s : LIBRE%-17s║\n",
                NOM_OUTIL[i], "");
        else
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                "║  %-12s : OCCUPE par %-10s ║\n",
                NOM_OUTIL[i], outils[i].prop_name);

        for (int w = 0; w < outils[i].waiters.count; w++)
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                "║    └─ en attente : %-20s║\n",
                outils[i].waiters.items[w].bras_name);
        pthread_mutex_unlock(&outils[i].mutex);
    }

    pos += snprintf(buf + pos, sizeof(buf) - pos,
        "╚══════════════════════════════════════════╝\n");

    if (client_fd >= 0)
        send(client_fd, buf, pos, 0);
    else
        printf("%s", buf);
}

/* ── Thread par client ──────────────────────────────────── */
typedef struct {
    int  client_fd;
    int  bras_id;
    char bras_name[32];
    struct sockaddr_in addr;
} ClientCtx;

static void *handle_client(void *arg) {
    ClientCtx *ctx = (ClientCtx *)arg;
    int fd          = ctx->client_fd;
    int bras_id     = ctx->bras_id;
    char *bname     = ctx->bras_name;

    log_action("🤖 %s connecté (fd=%d)\n", bname, fd);

    char buf[BUF_SIZE];
    char rep[BUF_SIZE];

    while (1) {
        memset(buf, 0, sizeof(buf));
        ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;

        /* Enlever \n\r */
        buf[strcspn(buf, "\r\n")] = '\0';

        /* ── DEMANDE_OUTIL <id> <bras_id> <priorite> ── */
        if (strncmp(buf, "DEMANDE_OUTIL", 13) == 0) {
            int id, bid, prio;
            sscanf(buf + 13, " %d %d %d", &id, &bid, &prio);
            if (id < 0 || id >= NB_OUTILS) {
                snprintf(rep, sizeof(rep), "ERREUR outil invalide\n");
                send(fd, rep, strlen(rep), 0);
                continue;
            }
            int r = allouer_outil(id, bras_id, prio, bname, fd, 1);
            if (r == 1) {
                snprintf(rep, sizeof(rep), "OK %d\n", id);
                send(fd, rep, strlen(rep), 0);
            } else if (r == 0) {
                snprintf(rep, sizeof(rep), "REFUSE %d\n", id);
                send(fd, rep, strlen(rep), 0);
            }
            /* r == -1 : réponse différée (envoyée lors de la libération) */
        }

        /* ── DEMANDE_DEUX_OUTILS <id1> <id2> <bras_id> <priorite> ── */
        else if (strncmp(buf, "DEMANDE_DEUX_OUTILS", 19) == 0) {
            int id1, id2, bid, prio;
            sscanf(buf + 19, " %d %d %d %d", &id1, &id2, &bid, &prio);

            /* Ordre fixe (Option A) : toujours demander le plus petit en premier */
            if (id1 > id2) { int tmp = id1; id1 = id2; id2 = tmp; }

            log_action("🔧 %s demande [%s]+[%s] (priorité=%d)\n",
                       bname, NOM_OUTIL[id1], NOM_OUTIL[id2], prio);

            int r1 = allouer_outil(id1, bras_id, prio, bname, fd, 1);
            if (r1 == 1) {
                int r2 = allouer_outil(id2, bras_id, prio, bname, fd, 1);
                if (r2 == 1) {
                    snprintf(rep, sizeof(rep), "OK2 %d %d\n", id1, id2);
                    send(fd, rep, strlen(rep), 0);
                } else if (r2 == 0) {
                    /* Libérer le premier pour éviter le deadlock */
                    liberer_outil(id1, bras_id, bname);
                    snprintf(rep, sizeof(rep), "REFUSE %d\n", id2);
                    send(fd, rep, strlen(rep), 0);
                }
                /* r2==-1 : en attente, notification différée */
            } else if (r1 == 0) {
                snprintf(rep, sizeof(rep), "REFUSE %d\n", id1);
                send(fd, rep, strlen(rep), 0);
            }
        }

        /* ── LIBERATION_OUTIL <id> <bras_id> ── */
        else if (strncmp(buf, "LIBERATION_OUTIL", 16) == 0) {
            int id, bid;
            sscanf(buf + 16, " %d %d", &id, &bid);
            if (id < 0 || id >= NB_OUTILS) continue;
            liberer_outil(id, bras_id, bname);
            snprintf(rep, sizeof(rep), "LIBERE %d\n", id);
            send(fd, rep, strlen(rep), 0);
        }

        /* ── STATUS ── */
        else if (strncmp(buf, "STATUS", 6) == 0) {
            afficher_status(fd);
        }

        else {
            snprintf(rep, sizeof(rep), "ERREUR commande inconnue: %s\n", buf);
            send(fd, rep, strlen(rep), 0);
        }
    }

    /* Libérer tous les outils détenus par ce bras à la déconnexion */
    log_action("🔌 %s déconnecté — libération de ses outils\n", bname);
    for (int i = 0; i < NB_OUTILS; i++) {
        pthread_mutex_lock(&outils[i].mutex);
        if (outils[i].proprietaire == bras_id) {
            pthread_mutex_unlock(&outils[i].mutex);
            liberer_outil(i, bras_id, bname);
        } else {
            wq_remove(&outils[i].waiters, bras_id);
            pthread_mutex_unlock(&outils[i].mutex);
        }
    }

    close(fd);
    free(ctx);
    return NULL;
}

/* ── Thread de surveillance (affiche status toutes les 5s) ─ */
static void *monitor_thread(void *arg) {
    (void)arg;
    while (1) {
        sleep(5);
        afficher_status(-1);
    }
    return NULL;
}

/* ── Gestionnaire SIGINT ────────────────────────────────── */
static volatile int running = 1;
static void sigint_handler(int s) { (void)s; running = 0; }

/* ── Main ───────────────────────────────────────────────── */
int main(void) {
    /* Ouvrir le fichier log */
    log_file = fopen("logs/serveur.log", "a");
    if (!log_file) { mkdir("logs", 0755); log_file = fopen("logs/serveur.log","a"); }

    signal(SIGINT, sigint_handler);
    outils_init();
    memset(wait_graph, 0, sizeof(wait_graph));

    printf("\n╔══════════════════════════════════════════════════════╗\n");
    printf("║   Smart Factory — Gestionnaire d'Outils             ║\n");
    printf("║   ENSEM 2026  —  Auteur : Hamza Mebrouk             ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");
    printf("Outils disponibles : ");
    for (int i = 0; i < NB_OUTILS; i++) printf("[%d]%s ", i, NOM_OUTIL[i]);
    printf("\n\n");

    /* Socket serveur */
    int srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons(SERVER_PORT)
    };

    if (bind(srv_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    if (listen(srv_fd, BACKLOG) < 0) { perror("listen"); return 1; }

    log_action("🏭 Serveur en écoute sur le port %d\n", SERVER_PORT);

    /* Thread de surveillance */
    pthread_t mon_tid;
    pthread_create(&mon_tid, NULL, monitor_thread, NULL);
    pthread_detach(mon_tid);

    int bras_counter = 0;

    while (running) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int cli_fd = accept(srv_fd, (struct sockaddr *)&cli_addr, &cli_len);
        if (cli_fd < 0) {
            if (running) perror("accept");
            break;
        }

        ClientCtx *ctx = malloc(sizeof(ClientCtx));
        ctx->client_fd  = cli_fd;
        ctx->bras_id    = bras_counter % NB_BRAS;
        ctx->addr       = cli_addr;
        snprintf(ctx->bras_name, sizeof(ctx->bras_name),
                 "%s", NOM_BRAS[bras_counter % NB_BRAS]);
        bras_counter++;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, ctx);
        pthread_detach(tid);
    }

    close(srv_fd);
    if (log_file) fclose(log_file);
    log_action("🛑 Serveur arrêté.\n");
    return 0;
}
