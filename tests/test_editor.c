/**
 * test_editor.c — Suite de tests + Profiling de rendimiento
 * Sistemas Operativos 2026 - EAFIT
 *
 * Compilar: gcc -o build/test_editor tests/test_editor.c src/editor.c -Isrc
 * Correr (desde raíz del proyecto): make test
 */

#define _XOPEN_SOURCE 600
#include "../src/editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

/* ─── colores ─── */
#define GREEN  "\033[32m"
#define RED    "\033[31m"
#define YELLOW "\033[33m"
#define CYAN   "\033[36m"
#define BOLD   "\033[1m"
#define RESET  "\033[0m"

/* ─── marco de tests ─── */
static int tests_run = 0, tests_pass = 0;
#define TEST(name, expr) do { tests_run++; \
    if (expr) { tests_pass++; printf("  [" GREEN "PASS" RESET "] %s\n", name); } \
    else { printf("  [" RED   "FAIL" RESET "] %s  (línea %d)\n", name, __LINE__); } \
} while(0)

static void section(const char *t) {
    printf("\n" BOLD CYAN "── %s ──" RESET "\n", t);
}

/* Ruta al binario (resuelta en main desde argv[0]) */
static char EDITOR[512] = "./editor";

/* ════════════════════════════════════════════════════
   TESTS UNITARIOS
   ════════════════════════════════════════════════════ */

static void test_gb_basico(void) {
    GapBuffer gb; gb_init(&gb, 16);
    gb_insert(&gb, 0, "hola", 4);
    size_t len; char *f = gb_flatten(&gb, &len);
    TEST("gb_insert: longitud = 4",       len == 4);
    TEST("gb_insert: contenido correcto",  memcmp(f, "hola", 4) == 0);
    free(f); gb_free(&gb);
}

static void test_gb_medio(void) {
    GapBuffer gb; gb_init(&gb, 32);
    gb_insert(&gb, 0, "hola mundo", 10);
    gb_insert(&gb, 4, " bello", 6);
    size_t len; char *f = gb_flatten(&gb, &len);
    TEST("gb_insert_medio: longitud = 16",     len == 16);
    TEST("gb_insert_medio: 'hola bello mundo'", memcmp(f, "hola bello mundo", 16) == 0);
    free(f); gb_free(&gb);
}

static void test_gb_delete(void) {
    GapBuffer gb; gb_init(&gb, 32);
    gb_insert(&gb, 0, "hola mundo", 10);
    gb_delete(&gb, 4, 6);
    size_t len; char *f = gb_flatten(&gb, &len);
    TEST("gb_delete: longitud = 4",  len == 4);
    TEST("gb_delete: queda 'hola'",  memcmp(f, "hola", 4) == 0);
    free(f); gb_free(&gb);
}

static void test_gb_grow(void) {
    GapBuffer gb; gb_init(&gb, 4);
    const char *txt = "abcdefghijklmnopqrstuvwxyz";
    gb_insert(&gb, 0, txt, 26);
    size_t len; char *f = gb_flatten(&gb, &len);
    TEST("gb_grow: longitud = 26",      len == 26);
    TEST("gb_grow: contenido correcto",  memcmp(f, txt, 26) == 0);
    free(f); gb_free(&gb);
}

static void test_rle(void) {
    const char *orig = "aaabbbccccdddddeeeee";
    uint32_t olen = (uint32_t)strlen(orig), clen = 0, dlen = 0;
    unsigned char *comp = rle_compress(orig, olen, &clen);
    char *decomp = rle_decompress(comp, clen, olen, &dlen);
    TEST("rle: comprime repetitivos", clen < olen);
    TEST("rle: longitud roundtrip",   dlen == olen);
    TEST("rle: contenido roundtrip",  memcmp(decomp, orig, olen) == 0);
    free(comp); free(decomp);
}

static void test_rle_sin_rep(void) {
    const char *orig = "abcdefgh";
    uint32_t olen = (uint32_t)strlen(orig), clen = 0, dlen = 0;
    unsigned char *comp = rle_compress(orig, olen, &clen);
    char *decomp = rle_decompress(comp, clen, olen, &dlen);
    TEST("rle_sin_rep: roundtrip OK",
         dlen == olen && memcmp(decomp, orig, olen) == 0);
    free(comp); free(decomp);
}

static void test_io(void) {
    const char *tmp = "/tmp/test_unit_io.osp";
    GapBuffer gb; gb_init(&gb, 64);
    gb_insert(&gb, 0, "Sistemas Operativos EAFIT", 25);
    TEST("save_to_disk: sin error", save_to_disk(tmp, &gb) == 0);
    gb_free(&gb);
    size_t len = 0;
    char *loaded = load_with_mmap(tmp, &len);
    TEST("load_with_mmap: no NULL",   loaded != NULL);
    TEST("load_with_mmap: longitud",  len == 25);
    TEST("load_with_mmap: contenido",
         loaded && memcmp(loaded, "Sistemas Operativos EAFIT", 25) == 0);
    free(loaded);
}

/* ════════════════════════════════════════════════════
   HELPERS DE BENCHMARK
   ════════════════════════════════════════════════════ */

static double ms_now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

static long file_bytes(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 ? (long)st.st_size : -1;
}

static void fill_repetitivo(GapBuffer *gb, int n) {
    const char *c = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
                    "Sistemas Operativos EAFIT 2026          ";
    for (int i = 0; i < n; i++) {
        size_t pos = gb->gap_start + (gb->size - gb->gap_end);
        gb_insert(gb, pos, c, strlen(c));
    }
}

static void fill_variado(GapBuffer *gb, int n) {
    const char *c = "La velocidad del bus I/O depende del tamano "
                    "del bloque y las interrupciones al kernel "
                    "generadas por cada syscall write() pequeno. ";
    for (int i = 0; i < n; i++) {
        size_t pos = gb->gap_start + (gb->size - gb->gap_end);
        gb_insert(gb, pos, c, strlen(c));
    }
}

/* ════════════════════════════════════════════════════
   BENCHMARK 1 — Ratio de compresión RLE
   ════════════════════════════════════════════════════ */
static void bench_compresion(void) {
    section("BENCHMARK 1: Ratio de Compresión RLE");
    printf("  %-42s %10s %10s %10s %10s\n",
           "Caso", "Original", "Comprimido", "Ratio", "CPU(ms)");
    printf("  %s\n", "──────────────────────────────────────────────────────────────────────────────");

    struct { const char *label; int n; int rep; } casos[] = {
        { "Texto MUY repetitivo (500 chunks)", 500, 1 },
        { "Texto variado/prosa  (500 chunks)", 500, 0 },
    };

    for (int c = 0; c < 2; c++) {
        GapBuffer gb; gb_init(&gb, 4096);
        if (casos[c].rep) fill_repetitivo(&gb, casos[c].n);
        else              fill_variado   (&gb, casos[c].n);

        size_t olen = 0;
        char *flat = gb_flatten(&gb, &olen);

        double t0 = ms_now();
        uint32_t clen = 0;
        unsigned char *comp = rle_compress(flat, (uint32_t)olen, &clen);
        double cpu_ms = ms_now() - t0;

        double ratio = (1.0 - (double)clen / olen) * 100.0;
        const char *color = ratio > 0 ? GREEN : RED;

        printf("  %-42s %10zu %10u %s%+.1f%%%s   %8.3f\n",
               casos[c].label, olen, clen, color, ratio, RESET, cpu_ms);

        free(flat); free(comp); gb_free(&gb);
    }
    printf("\n  " YELLOW "Nota:" RESET " RLE es eficiente con texto repetitivo (caracteres\n"
           "  consecutivos iguales). Con texto variado expande el tamaño.\n"
           "  El objetivo del proyecto es demostrar el pipeline, no el ratio.\n");
}

/* ════════════════════════════════════════════════════
   BENCHMARK 2 — I/O: write alineado vs mmap
   ════════════════════════════════════════════════════ */
static void bench_io(void) {
    section("BENCHMARK 2: I/O — write() alineado a 4096B vs mmap lectura");

    printf("  %-20s %12s %12s %12s %12s\n",
           "", "Orig(bytes)", "Disco(bytes)", "write(ms)", "mmap(ms)");
    printf("  %s\n", "──────────────────────────────────────────────────────────────────────");

    const char *files[] = { "/tmp/bench_rep.osp", "/tmp/bench_var.osp" };
    const char *labels[]= { "Texto repetitivo", "Texto variado" };

    for (int t = 0; t < 2; t++) {
        GapBuffer gb; gb_init(&gb, 4096);
        if (t == 0) fill_repetitivo(&gb, 800);
        else        fill_variado   (&gb, 800);

        size_t olen = 0;
        char *flat = gb_flatten(&gb, &olen);
        free(flat);

        double tw0 = ms_now();
        save_to_disk(files[t], &gb);
        double tw = ms_now() - tw0;
        gb_free(&gb);

        long disco = file_bytes(files[t]);

        double tr0 = ms_now();
        size_t rlen = 0;
        char *loaded = load_with_mmap(files[t], &rlen);
        double tr = ms_now() - tr0;
        free(loaded);

        printf("  %-20s %12zu %12ld %12.3f %12.3f\n",
               labels[t], olen, disco, tw, tr);
    }

    printf("\n  " CYAN "Análisis:" RESET "\n"
           "  • write() usa posix_memalign(PAGE_SIZE=4096) → un write() por bloque de 4KB.\n"
           "    Esto reduce drásticamente las llamadas al kernel vs writes de 1-byte.\n"
           "  • mmap() mapea el archivo completo desde la page cache del kernel\n"
           "    al espacio de direcciones del proceso en UNA sola syscall.\n"
           "    No hay copia kernel→user (zero-copy): el acceso es directo a memoria.\n"
           "  • Tiempo mmap < write: leer desde page cache es más rápido que escribir\n"
           "    porque el SO puede mantener el archivo cacheado en RAM.\n");
}

/* ════════════════════════════════════════════════════
   BENCHMARK 3 — strace: syscalls reales
   ════════════════════════════════════════════════════ */
static void bench_strace(void) {
    section("BENCHMARK 3: Syscalls reales — strace -c");

    if (system("which strace > /dev/null 2>&1") != 0) {
        printf("  " RED "strace no encontrado." RESET
               " Instala: sudo apt install strace\n");
        return;
    }

    const char *f_w = "/tmp/bench_strace_write.osp";
    const char *f_r = "/tmp/bench_strace_read.osp";
    char cmd[768];

    /* ── Crear archivo de prueba grande para lectura ── */
    snprintf(cmd, sizeof(cmd),
        "printf 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\\n:w\\n' | "
        "%s write %s > /dev/null 2>&1", EDITOR, f_r);
    if (system(cmd)) {}

    /* ── WRITE ── */
    printf("\n  " YELLOW "▶ WRITE — strace -c" RESET
           " (compresión RLE + write() alineado a 4096B)\n\n");
    snprintf(cmd, sizeof(cmd),
        "printf 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\\n"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB\\n:w\\n' | "
        "strace -c %s write %s 2>&1 | cat", EDITOR, f_w);
    if (system(cmd)) {}

    /* ── READ ── */
    printf("\n  " YELLOW "▶ READ — strace -c" RESET
           " (mmap: mapeo directo desde page cache)\n\n");
    snprintf(cmd, sizeof(cmd),
        "strace -c %s read %s 2>&1", EDITOR, f_r);
    if (system(cmd)) {}

    printf("\n  " CYAN "Interpretación:" RESET "\n"
           "  • write()  → pocas llamadas: bloques de 4096B minimizan context switches\n"
           "  • mmap()   → 1 llamada: mapeo único del archivo completo en RAM\n"
           "  • read()   → NO aparece en modo lectura (zero-copy via mmap)\n"
           "  • Menos 'calls' totales = menos interrupciones al kernel = mejor rendimiento\n");
}

/* ════════════════════════════════════════════════════
   BENCHMARK 4 — valgrind: fugas de memoria
   ════════════════════════════════════════════════════ */
static void bench_valgrind(void) {
    section("BENCHMARK 4: Memoria — Valgrind leak-check=full");

    if (system("which valgrind > /dev/null 2>&1") != 0) {
        printf("  " RED "valgrind no encontrado." RESET
               " Instala: sudo apt install valgrind\n");
        return;
    }

    const char *f = "/tmp/bench_valgrind.osp";
    char cmd[768];

    /* ── WRITE ── */
    printf("\n  " YELLOW "▶ WRITE — valgrind" RESET "\n\n");
    snprintf(cmd, sizeof(cmd),
        "printf 'Sistemas Operativos EAFIT 2026\\nHola mundo\\n:w\\n' | "
        "valgrind --leak-check=full --show-leak-kinds=all "
        "--track-origins=yes %s write %s 2>&1 | "
        "grep -E '(in use at exit|total heap|definitely|indirectly|possibly|reachable|ERROR SUMMARY)'",
        EDITOR, f);
    if (system(cmd)) {}

    /* ── READ ── */
    printf("\n  " YELLOW "▶ READ — valgrind" RESET "\n\n");
    snprintf(cmd, sizeof(cmd),
        "valgrind --leak-check=full --show-leak-kinds=all "
        "--track-origins=yes %s read %s 2>&1 | "
        "grep -E '(in use at exit|total heap|definitely|indirectly|possibly|reachable|ERROR SUMMARY)'",
        EDITOR, f);
    if (system(cmd)) {}

    printf("\n  " CYAN "Resultado esperado:" RESET
           " 'in use at exit: 0 bytes' y 'ERROR SUMMARY: 0 errors'\n"
           "  Esto confirma que toda la memoria dinámica es liberada correctamente.\n");
}

/* ════════════════════════════════════════════════════
   MAIN
   ════════════════════════════════════════════════════ */
int main(int argc, char *argv[]) {
    /* Resolver ruta del binario editor relativa a este ejecutable */
    if (argc > 0) {
        char tmp[512];
        snprintf(tmp, sizeof(tmp), "%s", argv[0]);
        char *slash = strrchr(tmp, '/');
        if (slash) {
            *slash = '\0';
            snprintf(EDITOR, sizeof(EDITOR), "%s/../editor", tmp);
        }
    }
    (void)argc;

    printf(BOLD
        "\n╔══════════════════════════════════════════════════╗\n"
        "║   Test Suite + Profiling — Editor OS-EAFIT 2026  ║\n"
        "╚══════════════════════════════════════════════════╝\n"
        RESET);

    section("TESTS UNITARIOS");
    test_gb_basico();
    test_gb_medio();
    test_gb_delete();
    test_gb_grow();
    test_rle();
    test_rle_sin_rep();
    test_io();

    printf("\n  " BOLD "Resultado: %d/%d tests pasaron%s\n" RESET,
           tests_pass, tests_run,
           tests_pass == tests_run ? "  ✓" : "  ✗");

    bench_compresion();
    bench_io();
    bench_strace();
    bench_valgrind();

    printf(BOLD
        "\n╔══════════════════════════════════════════════════╗\n"
        "║                 FIN DEL REPORTE                  ║\n"
        "╚══════════════════════════════════════════════════╝\n\n"
        RESET);

    return (tests_pass == tests_run) ? 0 : 1;
}