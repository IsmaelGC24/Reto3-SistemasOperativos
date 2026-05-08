#!/usr/bin/env bash
# ══════════════════════════════════════════════════════
#  benchmark.sh — Genera evidencia empírica (Criterio 4)
#
#  Uso: bash docs/benchmark.sh
#  Salida: docs/profiling_report.txt
# ══════════════════════════════════════════════════════

set -e

EDITOR=./editor
BENCH_FILE=/tmp/bench.osp
REPORT=docs/profiling_report.txt

command -v strace   >/dev/null || { echo "Instala strace:  sudo apt install strace";   exit 1; }
command -v valgrind >/dev/null || { echo "Instala valgrind: sudo apt install valgrind"; exit 1; }

[ -f "$EDITOR" ] || { echo "Compila primero: make"; exit 1; }

{
echo "═══════════════════════════════════════════════════════"
echo " REPORTE DE PROFILING — Editor OS-EAFIT 2026"
echo " Fecha: $(date)"
echo " Host:  $(uname -a)"
echo "═══════════════════════════════════════════════════════"
echo ""

# ── 1. time (wall / user / sys) ──
echo "───────────────────────────────────────────────────────"
echo " 1. TIEMPO DE EJECUCIÓN (time)"
echo "───────────────────────────────────────────────────────"
echo ""
echo "[WRITE]"
{ time $EDITOR write $BENCH_FILE ; } 2>&1
echo ""
echo "[READ]"
{ time $EDITOR read  $BENCH_FILE ; } 2>&1
echo ""

# ── 2. strace -c (conteo de syscalls) ──
echo "───────────────────────────────────────────────────────"
echo " 2. CONTEO DE SYSCALLS (strace -c)"
echo "───────────────────────────────────────────────────────"
echo ""
echo "[WRITE — syscalls]"
strace -c $EDITOR write $BENCH_FILE 2>&1
echo ""
echo "[READ  — syscalls]"
strace -c $EDITOR read  $BENCH_FILE 2>&1
echo ""

# ── 3. Tamaño en disco ──
echo "───────────────────────────────────────────────────────"
echo " 3. TAMAÑO EN DISCO"
echo "───────────────────────────────────────────────────────"
ls -lh $BENCH_FILE
echo ""

# ── 4. valgrind ──
echo "───────────────────────────────────────────────────────"
echo " 4. MEMORY LEAKS (valgrind)"
echo "───────────────────────────────────────────────────────"
echo ""
valgrind --leak-check=full --show-leak-kinds=all \
         --track-origins=yes \
         $EDITOR write $BENCH_FILE 2>&1
echo ""
valgrind --leak-check=full --show-leak-kinds=all \
         --track-origins=yes \
         $EDITOR read  $BENCH_FILE 2>&1
echo ""

echo "═══════════════════════════════════════════════════════"
echo " FIN DEL REPORTE"
echo "═══════════════════════════════════════════════════════"
} | tee $REPORT

echo ""
echo "✓ Reporte guardado en: $REPORT"