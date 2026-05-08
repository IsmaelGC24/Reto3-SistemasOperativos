# Reto3-SistemasOperativos
## Editor OS-EAFIT 2026
**Reto 3 — Sistemas Operativos | Universidad EAFIT**

Editor de texto en C nativo con compresión RLE, Gap Buffer y I/O optimizado mediante `write()` alineado a página y `mmap()`.

---

## Requisitos

```bash
sudo apt install gcc make strace valgrind
```

---

## Compilar

```bash
make
```

---

## Usar el editor

**Crear un archivo nuevo (editor interactivo):**
```bash
./editor write mi_archivo.osp
```
Escribe línea por línea. Comandos dentro del editor:

| Comando | Acción |
|---------|--------|
| `:w` | Guardar y salir |
| `:q` | Salir sin guardar |
| `:p` | Ver el texto actual |
| `:d N` | Borrar línea N |
| `:i N` | Insertar antes de línea N |

**Leer un archivo:**
```bash
./editor read mi_archivo.osp
```

**Editar un archivo existente:**
```bash
./editor edit mi_archivo.osp
```

---

## Tests y Profiling

```bash
make test
```

Ejecuta en orden:
1. **16 tests unitarios** (Gap Buffer, RLE, I/O roundtrip)
2. **Benchmark 1** — Ratio de compresión RLE con texto repetitivo vs variado
3. **Benchmark 2** — Tiempos reales de `write()` alineado vs `mmap()`
4. **Benchmark 3** — `strace -c` con conteo exacto de syscalls
5. **Benchmark 4** — `valgrind` confirmando 0 memory leaks

---

## Estructura del proyecto

```
├── src/
│   ├── editor.h     ← Tipos y declaraciones (FileHeader packed, GapBuffer)
│   ├── editor.c     ← Gap Buffer + RLE + I/O (write alineado / mmap)
│   └── main.c       ← CLI interactiva (write / read / edit)
├── tests/
│   └── test_editor.c ← Tests unitarios + benchmarks (strace, valgrind)
├── docs/
│   └── benchmark.sh  ← Script alternativo de profiling completo
└── Makefile
```

---

## Limpieza

```bash
make clean
```