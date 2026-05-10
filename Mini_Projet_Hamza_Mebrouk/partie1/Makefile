# ============================================================
#  Makefile — Simulateur d'ordonnancement de processus
#  ENSEM 2026
# ============================================================

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g -Iinclude
LDFLAGS = -ldl

# Répertoires
SRC_DIR    = src
POL_DIR    = policies
LIB_DIR    = policies/lib
OBJ_DIR    = obj

# Sources principales
SRCS = main.c \
       $(SRC_DIR)/process.c \
       $(SRC_DIR)/parser.c \
       $(SRC_DIR)/scheduler.c \
       $(SRC_DIR)/gantt.c

OBJS = $(patsubst %.c, $(OBJ_DIR)/%.o, $(notdir $(SRCS)))

# Politiques (chargées dynamiquement en .so)
POLICIES = $(LIB_DIR)/fifo.so \
           $(LIB_DIR)/sjf.so  \
           $(LIB_DIR)/rr.so   \
           $(LIB_DIR)/priority.so

TARGET = ordonnanceur

# ============================================================
.PHONY: all clean dirs run help

all: dirs $(TARGET) $(POLICIES)

dirs:
	@mkdir -p $(OBJ_DIR) $(LIB_DIR)

# --- Exécutable principal ---
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✔  Exécutable : $(TARGET)"

$(OBJ_DIR)/main.o: main.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR)/process.o: $(SRC_DIR)/process.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR)/parser.o: $(SRC_DIR)/parser.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR)/scheduler.o: $(SRC_DIR)/scheduler.c
	$(CC) $(CFLAGS) -c -o $@ $< $(LDFLAGS)

$(OBJ_DIR)/gantt.o: $(SRC_DIR)/gantt.c
	$(CC) $(CFLAGS) -c -o $@ $<

# --- Bibliothèques de politiques (.so) ---
# Chaque politique inclut gantt.o et process.o
POL_DEPS = $(OBJ_DIR)/gantt.o $(OBJ_DIR)/process.o

$(LIB_DIR)/fifo.so: $(POL_DIR)/fifo.c $(POL_DEPS)
	$(CC) $(CFLAGS) -shared -fPIC -o $@ $^
	@echo "✔  Politique : fifo.so"

$(LIB_DIR)/sjf.so: $(POL_DIR)/sjf.c $(POL_DEPS)
	$(CC) $(CFLAGS) -shared -fPIC -o $@ $^
	@echo "✔  Politique : sjf.so"

$(LIB_DIR)/rr.so: $(POL_DIR)/rr.c $(POL_DEPS)
	$(CC) $(CFLAGS) -shared -fPIC -o $@ $^
	@echo "✔  Politique : rr.so"

$(LIB_DIR)/priority.so: $(POL_DIR)/priority.c $(POL_DEPS)
	$(CC) $(CFLAGS) -shared -fPIC -o $@ $^
	@echo "✔  Politique : priority.so"

# --- Lancer une simulation de test ---
run: all
	./$(TARGET) examples/exemple.conf 3

# --- Nettoyage ---
clean:
	rm -rf $(OBJ_DIR) $(LIB_DIR) $(TARGET)
	@echo "✔  Nettoyage terminé"

help:
	@echo "Cibles disponibles:"
	@echo "  make all      — compile tout (défaut)"
	@echo "  make run      — compile et lance la simulation d'exemple"
	@echo "  make clean    — supprime les fichiers générés"
	@echo ""
	@echo "Usage: ./$(TARGET) <config> [quantum] [politique]"
	@echo "  politique : fifo | sjf | rr | priority"
