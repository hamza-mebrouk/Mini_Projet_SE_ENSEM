# ============================================================
#  Makefile GLOBAL — Mini-Projet Systèmes d'Exploitation
#  ENSEM 2026 — Auteur : Hamza Mebrouk
#
#  Usage :
#    make          → compile les deux parties
#    make partie1  → compile uniquement l'ordonnanceur
#    make partie2  → compile uniquement le Smart Factory
#    make clean    → nettoie tout
# ============================================================

.PHONY: all partie1 partie2 clean

all: partie1 partie2
	@echo ""
	@echo "============================================"
	@echo "  Compilation terminee avec succes !"
	@echo "============================================"
	@echo "  Partie 1 : ./partie1/ordonnanceur"
	@echo "  Partie 2 : ./partie2/gestionnaire_outils"
	@echo "             ./partie2/bras_robotique"
	@echo "============================================"

partie1:
	@echo ">>> Compilation Partie 1 : Ordonnanceur..."
	$(MAKE) -C partie1

partie2:
	@echo ">>> Compilation Partie 2 : Smart Factory..."
	$(MAKE) -C partie2

clean:
	$(MAKE) -C partie1 clean
	$(MAKE) -C partie2 clean
	@echo "Nettoyage global termine."
