# Mini-Projet — Systèmes d'Exploitation
## ENSEM 2026

**Auteur : Hamza Mebrouk**

---

## Contenu du projet

```
Mini_Projet_Hamza_Mebrouk/
│
├── README.md                  ← ce fichier
├── Makefile                   ← compile tout en une commande
│
├── partie1/                   ← Ordonnanceur de processus
│   ├── Makefile
│   ├── main.c
│   ├── src/                   ← sources du simulateur
│   ├── include/               ← fichiers d'en-tête
│   ├── policies/              ← politiques d'ordonnancement (FIFO, SJF, RR, PRIORITY)
│   └── examples/
│       └── exemple.conf       ← fichier de test prêt à l'emploi
│
└── partie2/                   ← Smart Factory (sockets TCP + threads)
    ├── Makefile
    ├── include/commun.h
    ├── serveur/               ← gestionnaire d'outils (serveur TCP)
    └── client/                ← bras robotiques (clients multithreadés)
```

---

## Compilation

### Tout compiler d'un coup

```bash
make
```

### Ou partie par partie

```bash
make partie1
make partie2
```

---

## Exécution

### Partie 1 — Ordonnanceur de processus

```bash
cd partie1
./ordonnanceur examples/exemple.conf 3
```

Le programme affiche un **menu interactif** pour choisir la politique :

```
| [1] FIFO     |
| [2] SJF      |
| [3] RR       |
| [4] PRIORITY |
```

Puis affiche le **diagramme de Gantt** et les **statistiques** (temps d'attente,
turnaround, temps de réponse).

**Lancer directement une politique sans menu :**

```bash
./ordonnanceur examples/exemple.conf 3 fifo
./ordonnanceur examples/exemple.conf 3 sjf
./ordonnanceur examples/exemple.conf 3 rr
./ordonnanceur examples/exemple.conf 3 priority
```

---

### Partie 2 — Smart Factory

Il faut **deux terminaux**.

**Terminal 1 — Lancer le serveur :**

```bash
cd partie2
./gestionnaire_outils
```

**Terminal 2 — Lancer les bras robotiques :**

```bash
cd partie2
./bras_robotique 3
```

*(le chiffre 3 = nombre de tâches par bras, modifiable)*

---

## Format du fichier de configuration (Partie 1)

```
# Commentaire
PROCESS <nom> <date_arrivée> [priorité]
CPU <durée>
IO  <durée>
CPU <durée>
END
```

**Exemple :**

```
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

## Fonctionnalités implémentées

### Partie 1
- [x] Lecture d'un fichier de configuration (commentaires, lignes vides)
- [x] Politique **FIFO** (non préemptif)
- [x] Politique **SJF** (non préemptif, plus court en premier)
- [x] Politique **Round-Robin** (préemptif, quantum configurable)
- [x] Politique **PRIORITY** (préemptif, priorité configurable par processus)
- [x] Gestion des cycles **CPU et IO** alternés
- [x] **Diagramme de Gantt** textuel
- [x] **Statistiques** : temps d'attente, turnaround, temps de réponse
- [x] Chargement dynamique des politiques (`.so`) via `dlopen`
- [x] Menu constitué dynamiquement selon le répertoire des politiques
- [x] Makefile complet

### Partie 2
- [x] Communication **TCP/IP** client-serveur
- [x] **3 threads** par bras (idle, communication, assemblage)
- [x] **Mutex** pour protéger l'accès aux outils
- [x] **File d'attente prioritaire** (QoS — 4 niveaux de priorité)
- [x] **Prévention deadlock** — hiérarchie fixe des ressources (Option A)
- [x] **Détection deadlock** — graphe d'attente + DFS (Option B)
- [x] **Timeout + réessai** (Option C)
- [x] **Journal horodaté** de toutes les actions (console + fichier `logs/`)
- [x] Affichage de l'état des outils toutes les 5 secondes

---

## Environnement de développement

- Système : Linux (Ubuntu)
- Compilateur : GCC 11, standard C11
- Bibliothèques : `pthreads`, `dl` (chargement dynamique)
