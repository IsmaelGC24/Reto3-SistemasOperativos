/**
 * editor.c - Editor de texto con compresión RLE y I/O optimizado
 * Sistemas Operativos 2026 - EAFIT
 *
 * Pipeline: GapBuffer → RLE compress → aligned write()/mmap → disco
 */

#define _XOPEN_SOURCE 600   /* posix_memalign */
#include "editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

/* ─────────────────────────────────────────────
   GAP BUFFER
   ───────────────────────────────────────────── */

int gb_init(GapBuffer *gb, size_t capacity) {
    gb->buffer = malloc(capacity);
    if (!gb->buffer) return -1;
    gb->size       = capacity;
    gb->gap_start  = 0;
    gb->gap_end    = capacity;
    return 0;
}

void gb_free(GapBuffer *gb) {
    free(gb->buffer);
    gb->buffer    = NULL;
    gb->size      = 0;
    gb->gap_start = 0;
    gb->gap_end   = 0;
}

/* Mueve el gap hasta la posición 'pos' para permitir inserción */
static int gb_move_gap(GapBuffer *gb, size_t pos) {
    size_t gap_len = gb->gap_end - gb->gap_start;
    if (pos == gb->gap_start) return 0;

    if (pos < gb->gap_start) {
        /* Mover gap hacia la izquierda */
        size_t delta = gb->gap_start - pos;
        memmove(gb->buffer + gb->gap_end - delta,
                gb->buffer + pos,
                delta);
        gb->gap_start = pos;
        gb->gap_end   = pos + gap_len;
    } else {
        /* Mover gap hacia la derecha */
        size_t delta = pos - gb->gap_start;
        memmove(gb->buffer + gb->gap_start,
                gb->buffer + gb->gap_end,
                delta);
        gb->gap_start = pos;
        gb->gap_end   = pos + gap_len;
    }
    return 0;
}

/* Duplica la capacidad del buffer cuando el gap se agota */
static int gb_grow(GapBuffer *gb) {
    size_t new_size = gb->size * 2;
    char *new_buf   = realloc(gb->buffer, new_size);
    if (!new_buf) return -1;

    /* Mover la parte derecha al final del nuevo buffer */
    size_t right_len = gb->size - gb->gap_end;
    memmove(new_buf + new_size - right_len,
            new_buf + gb->gap_end,
            right_len);
    gb->buffer   = new_buf;
    gb->gap_end  = new_size - right_len;
    gb->size     = new_size;
    return 0;
}

int gb_insert(GapBuffer *gb, size_t pos, const char *text, size_t len) {
    /* Crecer si hace falta */
    while ((gb->gap_end - gb->gap_start) < len) {
        if (gb_grow(gb) != 0) return -1;
    }
    gb_move_gap(gb, pos);
    memcpy(gb->buffer + gb->gap_start, text, len);
    gb->gap_start += len;
    return 0;
}

int gb_delete(GapBuffer *gb, size_t pos, size_t len) {
    size_t content_len = gb->gap_start + (gb->size - gb->gap_end);
    if (pos + len > content_len) return -1;

    gb_move_gap(gb, pos);
    gb->gap_end += len;   /* Ampliar el gap "borra" los caracteres */
    return 0;
}

/* Devuelve un buffer plano con el texto completo (caller debe free()) */
char *gb_flatten(const GapBuffer *gb, size_t *out_len) {
    size_t left  = gb->gap_start;
    size_t right = gb->size - gb->gap_end;
    size_t total = left + right;

    char *flat = malloc(total + 1);
    if (!flat) return NULL;

    memcpy(flat, gb->buffer, left);
    memcpy(flat + left, gb->buffer + gb->gap_end, right);
    flat[total] = '\0';

    if (out_len) *out_len = total;
    return flat;
}

/* ─────────────────────────────────────────────
   COMPRESIÓN / DESCOMPRESIÓN RLE
   ───────────────────────────────────────────── */

unsigned char *rle_compress(const char *input, uint32_t input_len,
                             uint32_t *output_len) {
    if (input_len == 0) { *output_len = 0; return NULL; }

    /* Peor caso: cada byte diferente → 2 bytes de salida */
    unsigned char *output = malloc((size_t)input_len * 2);
    if (!output) return NULL;

    uint32_t j = 0;
    for (uint32_t i = 0; i < input_len; ) {
        unsigned char c = (unsigned char)input[i];
        int count = 1;
        while (i + count < input_len &&
               (unsigned char)input[i + count] == c &&
               count < 255) {
            count++;
        }
        output[j++] = (unsigned char)count;
        output[j++] = c;
        i += count;
    }
    *output_len = j;
    return output;
}

char *rle_decompress(const unsigned char *input, uint32_t input_len,
                     uint32_t expected_len, uint32_t *output_len) {
    if (input_len == 0) { *output_len = 0; return NULL; }

    char *output = malloc(expected_len + 1);
    if (!output) return NULL;

    uint32_t j = 0;
    for (uint32_t i = 0; i + 1 < input_len && j < expected_len; i += 2) {
        unsigned char count = input[i];
        unsigned char c     = input[i + 1];
        for (int k = 0; k < count && j < expected_len; k++) {
            output[j++] = (char)c;
        }
    }
    output[j] = '\0';
    *output_len = j;
    return output;
}

/* ─────────────────────────────────────────────
   I/O DE BAJO NIVEL (aligned writes + mmap)
   ───────────────────────────────────────────── */

/**
 * save_to_disk — escribe header binario + payload comprimido.
 *
 * Criterio 1: usamos write() con un buffer alineado a PAGE_SIZE (4096 B)
 * mediante posix_memalign, reduciendo las interrupciones al kernel al
 * minimizar llamadas a write() con bloques del tamaño de página del SO.
 */
int save_to_disk(const char *filename, GapBuffer *gb) {
    int ret = 0;

    /* 1. Aplanar el Gap Buffer */
    size_t text_len = 0;
    char  *flat     = gb_flatten(gb, &text_len);
    if (!flat) return -1;

    /* 2. Comprimir */
    uint32_t comp_len = 0;
    unsigned char *compressed = rle_compress(flat, (uint32_t)text_len, &comp_len);
    free(flat);
    if (!compressed) return -1;

    /* 3. Construir header */
    FileHeader header;
    memcpy(header.magic, "OS-P", 4);
    header.version         = FORMAT_VERSION;
    header.original_size   = (uint32_t)text_len;
    header.compressed_size = comp_len;
    header.encryption_flag = 0;

    /* 4. Abrir descriptor */
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open");
        free(compressed);
        return -1;
    }

    /* 5. Buffer alineado a página (Criterio 1 — Nivel Arquitecto OS) */
    void *aligned_buf = NULL;
    if (posix_memalign(&aligned_buf, PAGE_SIZE, PAGE_SIZE) != 0) {
        perror("posix_memalign");
        close(fd);
        free(compressed);
        return -1;
    }

    /* Escribir header */
    if (write(fd, &header, sizeof(FileHeader)) < 0) {
        perror("write header"); free(aligned_buf); free(compressed); close(fd); return -1;
    }

    /* Escribir payload en bloques de PAGE_SIZE */
    uint32_t written = 0;
    while (written < comp_len) {
        uint32_t chunk = comp_len - written;
        if (chunk > PAGE_SIZE) chunk = PAGE_SIZE;
        memcpy(aligned_buf, compressed + written, chunk);
        ssize_t n = write(fd, aligned_buf, chunk);
        if (n < 0) { perror("write"); ret = -1; break; }
        written += (uint32_t)n;
    }

    printf("[save] Original: %u B | Comprimido: %u B | Ratio: %.1f%%\n",
           (uint32_t)text_len, comp_len,
           text_len > 0 ? (1.0 - (double)comp_len / text_len) * 100.0 : 0.0);

    free(aligned_buf);
    free(compressed);
    close(fd);
    return ret;
}

/**
 * load_with_mmap — mapea el archivo completo en el espacio de direcciones
 * del proceso y descomprime el payload sin copias extra.
 *
 * Criterio 1: mmap evita la copia kernel→user del read() clásico
 * (zero-copy desde la page cache del kernel).
 */
char *load_with_mmap(const char *filename, size_t *out_len) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) { perror("open"); return NULL; }

    struct stat st;
    if (fstat(fd, &st) == -1) { perror("fstat"); close(fd); return NULL; }

    void *map = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (map == MAP_FAILED) { perror("mmap"); return NULL; }

    FileHeader *h = (FileHeader *)map;

    /* Validar magic number */
    if (memcmp(h->magic, "OS-P", 4) != 0) {
        fprintf(stderr, "Formato inválido (magic incorrecto)\n");
        munmap(map, (size_t)st.st_size);
        return NULL;
    }

    printf("[load] Magic: %.4s | Versión: %u | Disco: %u B | Original: %u B\n",
           h->magic, h->version, h->compressed_size, h->original_size);

    const unsigned char *payload = (const unsigned char *)map + sizeof(FileHeader);
    uint32_t decompressed_len = 0;
    char *text = rle_decompress(payload, h->compressed_size,
                                h->original_size, &decompressed_len);

    munmap(map, (size_t)st.st_size);

    if (!text) {
        fprintf(stderr, "Error al descomprimir\n");
        return NULL;
    }

    if (out_len) *out_len = decompressed_len;
    return text;   /* caller debe free() */
}