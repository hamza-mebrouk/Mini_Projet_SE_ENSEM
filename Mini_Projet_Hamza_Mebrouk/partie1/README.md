# Simulateur d'Ordonnancement de Processus
**ENSEM — Mini-Projet Systèmes d'Exploitation 2026 — Partie 1**

> **Auteur : Hamza Mebrouk**

---

## Table des matières

1. [Présentation](#présentation)
2. [Structure du projet](#structure-du-projet)
3. [Compilation](#compilation)
4. [Utilisation](#utilisation)
5. [Format du fichier de configuration](#format-du-fichier-de-configuration)
6. [Politiques d'ordonnancement](#politiques-dordonnancement)
7. [Architecture technique](#architecture-technique)
8. [Ajouter une nouvelle politique](#ajouter-une-nouvelle-politique)
9. [Exemples de résultats](#exemples-de-résultats)

---

## Présentation

Ce programme simule l'ordonnancement de processus selon différentes politiques.
Il lit un jeu de processus depuis un fichier de configuration, propose un menu
interactif pour choisir la politique, exécute la simulation et affiche :

- Le **diagramme de Gantt** textuel
- Les **statistiques** : temps d'attente, turnaround, temps de réponse

---

## Structure du projet

```
ordonnanceur/
├── main.c                    # Point d'entrée principal
├── Makefile                  # Construction du projet
├── README.md                 # Ce document
│
├── include/                  # En-têtes partagés
│   ├── process.h             # Structure Process, types, états
│   ├── parser.h              # Interface du parseur de config
│   ├── scheduler.h           # Interface du registre de politiques
│   └── gantt.h               # Interface Gantt + statistiques
│
├── src/                      # Sources du cœur du simulateur
│   ├── process.c             # Utilitaires sur les processus
│   ├── parser.c              # Parseur du fichier de configuration
│   ├── scheduler.c           # Chargement dynamique des politiques + menu
│   └── gantt.c               # Affichage Gantt + statistiques
│
├── policies/                 # Politiques d'ordonnancement (sources)
│   ├── fifo.c                # FIFO — First In First Out
│   ├── sjf.c                 # SJF — Shortest Job First
│   ├── rr.c                  # Round-Robin préemptif
│   ├── priority.c            # Priorité préemptive
│   └── lib/                  # Bibliothèques .so générées par make
│
├── obj/                      # Fichiers objets (générés par make)
│
└── examples/
    └── exemple.conf          # Fichier de configuration d'exemple
```

---

## Compilation

### Prérequis

- GCC (>= 7)
- `make`
- Linux / WSL (pour `dlopen`, `dlsym`)

### Construire le projet

```bash
make
```

Cette commande :
1. Compile les sources en `.o` dans `obj/`
2. Produit l'exécutable `ordonnanceur`
3. Compile chaque politique en bibliothèque partagée `.so` dans `policies/lib/`

### Nettoyer

```bash
make clean
```

---

## Utilisation

```
./ordonnanceur <fichier_config> [quantum] [politique]
```

| Argument        | Description                                              | Obligatoire |
|-----------------|----------------------------------------------------------|-------------|
| `fichier_config`| Chemin vers le fichier de description des processus      | ✔           |
| `quantum`       | Quantum de temps pour Round-Robin (défaut = 2)           | ✘           |
| `politique`     | `fifo`, `sjf`, `rr`, `priority` (menu si absent)        | ✘           |

### Exemples

```bash
# Menu interactif
./ordonnanceur examples/exemple.conf

# Round-Robin avec quantum = 3
./ordonnanceur examples/exemple.conf 3 rr

# FIFO directement
./ordonnanceur examples/exemple.conf 2 fifo

# SJF directement
./ordonnanceur examples/exemple.conf 2 sjf
```

---

## Format du fichier de configuration

```
# Commentaire (ligne ignorée)

PROCESS <nom> <date_arrivée> [priorité]
CPU <durée>
IO  <durée>
CPU <durée>
END

# Lignes vides autorisées

PROCESS <nom2> <date_arrivée>
CPU <durée>
END
```

### Règles

- Les lignes commençant par `#` sont des commentaires.
- Les lignes vides sont ignorées.
- `PROCESS` lance la définition d'un processus.
- `CPU <n>` : burst CPU de n unités de temps.
- `IO <n>` : burst d'entrée/sortie de n unités de temps.
- `END` (optionnel) : termine la définition.
- La **priorité** est optionnelle (défaut = 0) ; plus la valeur est grande, plus le processus est prioritaire.

### Exemple

```
# Fichier exemple
PROCESS P1 0 2
CPU 6
END

PROCESS P2 2 1
CPU 3
IO  2
CPU 2
END
```

---

## Politiques d'ordonnancement

### 1. FIFO — First In, First Out (`fifo`)
- **Non préemptive**
- Les processus sont servis dans l'ordre de leur arrivée.
- Un processus s'exécute jusqu'à la fin de son burst CPU courant.

### 2. SJF — Shortest Job First (`sjf`)
- **Non préemptive**
- Parmi les processus prêts, on choisit celui dont le burst CPU **total** est le plus court.
- En cas d'égalité : FIFO.

### 3. Round-Robin (`rr`)
- **Préemptive**
- Chaque processus reçoit un **quantum** de temps fixe.
- Si son burst n'est pas terminé, il est remis en fin de file READY.
- Le quantum est passé en 2e argument de la ligne de commande.

### 4. PRIORITY (`priority`)
- **Préemptive**
- À chaque tick, le processus READY avec la **priorité la plus haute** s'exécute.
- En cas d'égalité : FIFO sur la date d'arrivée.
- La priorité est définie dans le fichier de configuration.

---

## Architecture technique

### Chargement dynamique des politiques

Chaque politique est compilée en bibliothèque partagée (`.so`).
Au démarrage, le programme parcourt le répertoire `policies/lib/` avec `opendir/readdir`,
charge chaque `.so` via `dlopen()` et récupère le symbole `schedule` via `dlsym()`.

**Le menu est donc construit dynamiquement** : ajouter un nouveau fichier `.c` dans
`policies/`, l'ajouter au `Makefile`, et il apparaît automatiquement dans le menu.

### Point d'entrée unique de toute politique

```c
typedef void (*SchedulerFunc)(Process *processes, int count, int quantum);
```

Chaque fichier de politique expose exactement cette fonction sous le nom `schedule`.

### Gestion des cycles CPU/IO

La structure `Process` modélise une alternance de cycles CPU et IO.
Pendant la simulation, un compteur `current_cycle` et `cycle_remaining` suivent
l'avancement. Quand un processus passe en IO, il passe à l'état `STATE_WAITING`
et son compteur `io_remaining` décrément à chaque tick.

---

## Ajouter une nouvelle politique

1. Créer `policies/ma_politique.c` avec :

```c
#include "process.h"
#include "gantt.h"

void schedule(Process *processes, int count, int quantum) {
    printf("\n>>> Politique : MA_POLITIQUE\n");
    // ... votre logique ...
}
```

2. Ajouter dans le `Makefile` :

```makefile
$(LIB_DIR)/ma_politique.so: $(POL_DIR)/ma_politique.c $(POL_DEPS)
    $(CC) $(CFLAGS) -shared -fPIC -o $@ $^
```

3. L'ajouter à la cible `all` :

```makefile
POLICIES = ... $(LIB_DIR)/ma_politique.so
```

4. `make` → la politique apparaît dans le menu.

---

## Exemples de résultats

### FIFO

```
  +------------+----+------+--+----------------+----+
  |     P1     | P4 |  P2  |P4|       P3       | P2 |
  +------------+----+------+--+----------------+----+
  0            6    8      11 12               20   22
```

### Round-Robin (quantum=3)

```
  +------+----+------+------+------+--+----+----------+
  |  P1  | P4 |  P2  |  P1  |  P3  |P4| P2 |    P3    |
  +------+----+------+------+------+--+----+----------+
  0      3    5      8      11     14 15   17         22
```
