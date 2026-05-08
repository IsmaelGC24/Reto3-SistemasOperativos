/**
 * main.c — Punto de entrada del editor interactivo
 * Sistemas Operativos 2026 - EAFIT
 *
 * Uso:
 *   ./editor write <archivo>   — Editor interactivo: escribe tu texto y guarda
 *   ./editor read  <archivo>   — Carga, descomprime y muestra el contenido
 *   ./editor edit  <archivo>   — Abre el contenido existente para modificarlo
 */

#include "editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_BUF 1024

static void usage(const char *prog) {
    fprintf(stderr, "Uso: %s <write|read|edit> <archivo>\n", prog);
    fprintf(stderr, "  write  Abre el editor interactivo y guarda el texto comprimido\n");
    fprintf(stderr, "  read   Carga el archivo y muestra todo el contenido\n");
    fprintf(stderr, "  edit   Carga el archivo existente y permite modificarlo\n");
}

static int run_interactive_editor(GapBuffer *gb) {
    char line[LINE_BUF];

    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║      Editor OS-EAFIT 2026  (RLE + mmap) ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║  Comandos:                               ║\n");
    printf("║   :w   → Guardar y salir                 ║\n");
    printf("║   :q   → Salir sin guardar               ║\n");
    printf("║   :p   → Ver el texto actual             ║\n");
    printf("║   :d N → Borrar línea N  (ej: :d 2)      ║\n");
    printf("║   :i N → Insertar antes de línea N       ║\n");
    printf("╚══════════════════════════════════════════╝\n");
    printf("Escribe tu texto (una línea a la vez):\n\n");

    int saved = 0;

    while (1) {
        printf("> ");
        fflush(stdout);

        if (!fgets(line, LINE_BUF, stdin)) break;

        /* :w — guardar y salir */
        if (strncmp(line, ":w", 2) == 0) { saved = 1; break; }

        /* :q — salir sin guardar */
        if (strncmp(line, ":q", 2) == 0) {
            printf("Saliendo sin guardar.\n");
            return 0;
        }

        /* :p — mostrar contenido actual */
        if (strncmp(line, ":p", 2) == 0) {
            size_t len;
            char *flat = gb_flatten(gb, &len);
            if (flat && len > 0) {
                printf("\n── Contenido actual (%zu bytes) ──\n", len);
                fwrite(flat, 1, len, stdout);
                printf("── fin ──\n\n");
            } else {
                printf("(vacío)\n");
            }
            free(flat);
            continue;
        }

        /* :d N — borrar línea N */
        if (strncmp(line, ":d ", 3) == 0) {
            int target = atoi(line + 3);
            if (target < 1) { printf("Número inválido.\n"); continue; }

            size_t len;
            char *flat = gb_flatten(gb, &len);
            if (!flat) continue;

            int cur = 1;
            size_t start = 0, end = 0;
            int found = 0;
            for (size_t i = 0; i <= len; i++) {
                if (cur == target && !found) { start = i; found = 1; }
                if (found && (i == len || flat[i] == '\n')) {
                    end = (i < len) ? i + 1 : i;
                    break;
                }
                if (i < len && flat[i] == '\n') cur++;
            }
            free(flat);

            if (!found) { printf("Línea %d no existe.\n", target); continue; }
            gb_delete(gb, start, end - start);
            printf("Línea %d eliminada.\n", target);
            continue;
        }

        /* :i N — insertar antes de línea N */
        if (strncmp(line, ":i ", 3) == 0) {
            int target = atoi(line + 3);
            if (target < 1) { printf("Número inválido.\n"); continue; }

            size_t len;
            char *flat = gb_flatten(gb, &len);
            size_t insert_pos = 0;
            if (flat) {
                int cur = 1;
                for (size_t i = 0; i < len; i++) {
                    if (cur == target) { insert_pos = i; break; }
                    if (flat[i] == '\n') cur++;
                }
                free(flat);
            }

            printf("Texto a insertar: ");
            fflush(stdout);
            if (!fgets(line, LINE_BUF, stdin)) continue;
            gb_insert(gb, insert_pos, line, strlen(line));
            printf("Insertado antes de línea %d.\n", target);
            continue;
        }

        /* Línea normal — insertar al final */
        size_t content_len = gb->gap_start + (gb->size - gb->gap_end);
        gb_insert(gb, content_len, line, strlen(line));
    }

    return saved;
}

int main(int argc, char *argv[]) {
    if (argc < 3) { usage(argv[0]); return 1; }

    const char *cmd      = argv[1];
    const char *filename = argv[2];

    /* ── WRITE: editor interactivo desde cero ── */
    if (strcmp(cmd, "write") == 0) {
        GapBuffer gb;
        if (gb_init(&gb, 1024) != 0) { fprintf(stderr, "Error GapBuffer\n"); return 1; }

        if (run_interactive_editor(&gb)) {
            if (save_to_disk(filename, &gb) != 0) {
                fprintf(stderr, "Error al guardar\n");
                gb_free(&gb);
                return 1;
            }
            printf("✓ Archivo guardado: %s\n", filename);
        }
        gb_free(&gb);
        return 0;
    }

    /* ── READ: mostrar contenido completo ── */
    if (strcmp(cmd, "read") == 0) {
        size_t len = 0;
        char *text = load_with_mmap(filename, &len);
        if (!text) return 1;

        printf("\n── Contenido de '%s' (%zu bytes) ──\n", filename, len);
        fwrite(text, 1, len, stdout);
        printf("\n── fin ──\n");
        free(text);
        return 0;
    }

    /* ── EDIT: cargar existente y modificar ── */
    if (strcmp(cmd, "edit") == 0) {
        size_t elen = 0;
        char *existing = load_with_mmap(filename, &elen);
        if (!existing) { fprintf(stderr, "No se pudo abrir '%s'\n", filename); return 1; }

        GapBuffer gb;
        if (gb_init(&gb, elen + 1024) != 0) { free(existing); return 1; }
        gb_insert(&gb, 0, existing, elen);
        free(existing);

        printf("Archivo cargado (%zu bytes). Usa :p para ver el contenido.\n", elen);

        if (run_interactive_editor(&gb)) {
            if (save_to_disk(filename, &gb) != 0) {
                fprintf(stderr, "Error al guardar\n");
                gb_free(&gb);
                return 1;
            }
            printf("✓ Archivo guardado: %s\n", filename);
        }
        gb_free(&gb);
        return 0;
    }

    usage(argv[0]);
    return 1;
}