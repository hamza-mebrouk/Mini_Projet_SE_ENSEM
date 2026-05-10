# ============================================================
#  Makefile — Smart Factory : Simulation de Gestion Distribuée
#  Mini-Projet Systèmes d'Exploitation — ENSEM 2026
#  Auteur : Hamza Mebrouk
# ============================================================

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g -Iinclude -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lpthread

SRV_SRC = serveur/gestionnaire_outils.c
CLI_SRC = client/bras_robotique.c

SRV_BIN = gestionnaire_outils
CLI_BIN = bras_robotique

.PHONY: all clean run-serveur run-client demo help

all: $(SRV_BIN) $(CLI_BIN)
	@echo ""
	@echo "✔  Compilation terminée."
	@echo "    Serveur : ./$(SRV_BIN)"
	@echo "    Client  : ./$(CLI_BIN) [nb_taches]"

$(SRV_BIN): $(SRV_SRC) include/commun.h
	$(CC) $(CFLAGS) -o $@ $(SRV_SRC) $(LDFLAGS)
	@echo "✔  Serveur compilé : $(SRV_BIN)"

$(CLI_BIN): $(CLI_SRC) include/commun.h
	$(CC) $(CFLAGS) -o $@ $(CLI_SRC) $(LDFLAGS)
	@echo "✔  Client compilé  : $(CLI_BIN)"

# Lance le serveur en arrière-plan puis le client
demo: all
	@mkdir -p logs
	@echo "Lancement du serveur en arrière-plan..."
	./$(SRV_BIN) &
	@sleep 1
	@echo "Lancement des bras robotiques (3 tâches)..."
	./$(CLI_BIN) 3
	@kill %1 2>/dev/null || true

run-serveur: $(SRV_BIN)
	@mkdir -p logs
	./$(SRV_BIN)

run-client: $(CLI_BIN)
	./$(CLI_BIN) 3

clean:
	rm -f $(SRV_BIN) $(CLI_BIN)
	rm -f logs/*.log
	@echo "✔  Nettoyage terminé"

help:
	@echo "Cibles disponibles :"
	@echo "  make all         — compile serveur et client"
	@echo "  make demo        — compile, lance serveur + client automatiquement"
	@echo "  make run-serveur — lance uniquement le serveur"
	@echo "  make run-client  — lance uniquement le client"
	@echo "  make clean       — supprime les binaires et logs"
	@echo ""
	@echo "Usage manuel :"
	@echo "  Terminal 1 : ./$(SRV_BIN)"
	@echo "  Terminal 2 : ./$(CLI_BIN) [nb_taches]"
