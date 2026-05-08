# ══════════════════════════════════════════════════════════════
#  Makefile — Editor OS-EAFIT 2026
#  Targets principales:
#    make          → compila el editor
#    make test     → compila y corre los tests unitarios
#    make bench    → genera archivo de prueba y mide con time + strace
#    make valgrind → revisa fugas de memoria
#    make clean    → limpia binarios y archivos temporales
# ══════════════════════════════════════════════════════════════

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -g -std=c11
SRC_DIR = src
TST_DIR = tests
OBJ_DIR = build
DOC_DIR = docs

SRCS    = $(SRC_DIR)/editor.c $(SRC_DIR)/main.c
OBJS    = $(OBJ_DIR)/editor.o $(OBJ_DIR)/main.o

TARGET  = editor
TEST_BIN= $(OBJ_DIR)/test_editor

# ── Archivo de prueba para benchmarks ──
BENCH_FILE = /tmp/bench_test.osp
BENCH_BIG  = /tmp/bench_big.osp

.PHONY: all test bench valgrind strace clean

all: $(OBJ_DIR) $(TARGET)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/editor.o: $(SRC_DIR)/editor.c $(SRC_DIR)/editor.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/main.o: $(SRC_DIR)/main.c $(SRC_DIR)/editor.h
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@
	@echo "✓ Binario compilado: ./$(TARGET)"

# ── Tests unitarios ──
$(TEST_BIN): $(SRC_DIR)/editor.c $(TST_DIR)/test_editor.c $(SRC_DIR)/editor.h
	$(CC) $(CFLAGS) $(SRC_DIR)/editor.c $(TST_DIR)/test_editor.c \
	      -I$(SRC_DIR) -o $(TEST_BIN)

test: $(OBJ_DIR) $(TEST_BIN)
	@echo "── Corriendo tests unitarios ──"
	$(TEST_BIN)

# ── Benchmark: time (Criterio 4) ──
bench: $(TARGET)
	@echo ""
	@echo "══════════════════════════════════════════════"
	@echo " BENCHMARK — Criterio 4: Profiling de I/O"
	@echo "══════════════════════════════════════════════"
	@echo ""
	@echo "▶ Escritura (write mode):"
	@/usr/bin/time -v ./$(TARGET) write $(BENCH_FILE) 2>&1 | \
	    grep -E "wall clock|Maximum resident|Voluntary context"
	@echo ""
	@echo "▶ Lectura (read mode / mmap):"
	@/usr/bin/time -v ./$(TARGET) read $(BENCH_FILE) 2>&1 | \
	    grep -E "wall clock|Maximum resident|Voluntary context"
	@echo ""
	@echo "▶ Tamaño en disco:"
	@ls -lh $(BENCH_FILE)

# ── strace: conteo de syscalls (Criterio 4) ──
strace: $(TARGET)
	@echo ""
	@echo "══════════════════════════════════════════════"
	@echo " STRACE — Conteo de Syscalls"
	@echo "══════════════════════════════════════════════"
	@echo ""
	@echo "▶ strace write:"
	strace -c ./$(TARGET) write $(BENCH_FILE) 2>&1
	@echo ""
	@echo "▶ strace read:"
	strace -c ./$(TARGET) read $(BENCH_FILE) 2>&1

# ── Valgrind: detección de fugas (Criterio 2) ──
valgrind: $(TARGET)
	@echo ""
	@echo "══════════════════════════════════════════════"
	@echo " VALGRIND — Detección de Memory Leaks"
	@echo "══════════════════════════════════════════════"
	valgrind --leak-check=full --show-leak-kinds=all \
	         --track-origins=yes --error-exitcode=1 \
	         ./$(TARGET) write $(BENCH_FILE)
	valgrind --leak-check=full --show-leak-kinds=all \
	         --track-origins=yes --error-exitcode=1 \
	         ./$(TARGET) read $(BENCH_FILE)

clean:
	rm -rf $(OBJ_DIR) $(TARGET) $(BENCH_FILE) $(BENCH_BIG)
	@echo "✓ Limpieza completa"