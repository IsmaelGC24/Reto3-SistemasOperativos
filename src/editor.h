/**
 * editor.h — Tipos y declaraciones públicas
 * Sistemas Operativos 2026 - EAFIT
 */

#ifndef EDITOR_H
#define EDITOR_H

#include <stdint.h>
#include <stddef.h>

/* ── Constantes de I/O ── */
#define PAGE_SIZE       4096    /* Tamaño de página del SO (x86-64 Linux) */
#define FORMAT_VERSION  1

/* ── Header binario empaquetado (Criterio 3 — texto enriquecido) ──
 *
 * __attribute__((packed)) elimina el padding del compilador para que
 * sizeof(FileHeader) sea exactamente 17 bytes y el layout en disco sea
 * determinista e independiente de la ABI.
 *
 * Layout:
 *   [0-3]  magic           "OS-P"
 *   [4-7]  version         uint32_t LE
 *   [8-11] original_size   uint32_t LE
 *  [12-15] compressed_size uint32_t LE
 *   [16]   encryption_flag uint8_t
 */
typedef struct __attribute__((packed)) {
    char     magic[4];
    uint32_t version;
    uint32_t original_size;
    uint32_t compressed_size;
    uint8_t  encryption_flag;
} FileHeader;

/* ── Gap Buffer (Criterio 3 — estructura eficiente) ──
 *
 *  [ texto izquierda | <-- gap --> | texto derecha ]
 *    0 .. gap_start-1               gap_end .. size-1
 *
 * Las inserciones/borrados en la posición del cursor son O(1).
 * Mover el cursor es O(distancia) por el memmove del gap.
 */
typedef struct {
    char   *buffer;
    size_t  size;        /* Capacidad total del buffer */
    size_t  gap_start;   /* Primer byte del gap */
    size_t  gap_end;     /* Primer byte después del gap */
} GapBuffer;

/* ── Gap Buffer API ── */
int   gb_init(GapBuffer *gb, size_t capacity);
void  gb_free(GapBuffer *gb);
int   gb_insert(GapBuffer *gb, size_t pos, const char *text, size_t len);
int   gb_delete(GapBuffer *gb, size_t pos, size_t len);
char *gb_flatten(const GapBuffer *gb, size_t *out_len);

/* ── Compresión RLE ── */
unsigned char *rle_compress(const char *input, uint32_t input_len,
                             uint32_t *output_len);
char          *rle_decompress(const unsigned char *input, uint32_t input_len,
                               uint32_t expected_len, uint32_t *output_len);

/* ── I/O ── */
int   save_to_disk(const char *filename, GapBuffer *gb);
char *load_with_mmap(const char *filename, size_t *out_len);

#endif /* EDITOR_H */