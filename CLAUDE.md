# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Franz Lisp (Opus 38.92/38.93) — the Berkeley dialect of Lisp from the early 1980s — being ported to **x86_64 Linux**. The original distribution targeted vax/tahoe/68k/i386 BSD; in this tree the C kernel is the only kept piece, and it is in the middle of a multi-phase port. See `PortPlan.md` for the staged plan and current phase.

The port's *immediate* goal is a working **interpreter** (no compiler, no fasl, no dumplisp). Code paths for those features still exist in the source but are intentionally inert on this target.

## Build (current state — Phase 0 complete)

```
./lispconf linux_x86_64    # writes franz/h/lconf.h (one platform supported)
make smoke                 # Phase 0: compile one kernel .c file
make kernel                # Phase 1: compile every kernel .c file
make rawlisp               # Phase 2: link rawlisp (links cleanly; segfaults on entry)
make clean
```

`make slow`, `make fast`, and `make install` are deliberately disabled — they invoke the multi-stage bootstrap (build liszt, dump a Lisp image), which depends on Phase 5 work (replace `dumplisp` with library auto-load).

`rawlisp` boots and runs Lisp. `(+ 1 2)` returns `3`; `(fact 10)` returns `3628800`. The full Lisp library isn't loaded yet (Phase 5 work), so library-only functions (`princ`, `reverse`, `length`, `defun` etc.) error as undefined — but kernel built-ins (`eq`, `cons`, `car`, `mapcar`, `cond`, `def`) all work.

The per-arch Makefile lives at `franz/linux_x86_64/Makefile` and uses modern gcc with porting-friendly flags (`-std=gnu89`, `-Wno-implicit-function-declaration`, `-Wno-int-conversion`, etc.). Phase 1 will tighten these as cast issues are fixed at the source level.

`RootDir` in the top-level Makefile resolves at make time via `$(shell pwd)`, so the tree can live anywhere.

## Repository layout

- `franz/` — C kernel of the interpreter. ~30 machine-independent `.c` files plus headers in `franz/h/`. **All vax/tahoe/68k/i386 source has been deleted** (see git history if you need it).
- `franz/linux_x86_64/` — the only per-arch directory. Holds the Makefile, `callg.c` (x86_64 ABI varargs trampoline), `bignum.c` (the 16-or-30-bit-halfword bignum kernels ported from `reference/i386/`), and `arch.c` (inewint/blzero/prunei, PORTABLE_FRAME helpers, tynames[], stubs for things that don't translate to x86_64).
- `franz/h/` — shared headers. `config.h` reads `lconf.h` (written by `lispconf`) and configures the kernel for the active platform. `aout.h` and `lispo.h` are local stubs (the originals were symlinks to `/usr/include/a.out.h`, which doesn't exist on Linux).
- `cliszt/` — Lisp source for the **liszt compiler**. Kept around as future reference; the per-arch subdirs (`vax/`, `68k/`, `i386/`, `in-c/`) and `Makefile` are gone. Liszt is deferred (Phase 6+) and would need an entirely new x86_64 codegen.
- `lisplib/` — Lisp library loaded by the interpreter. `.l` source remains; `autorun/` (per-machine bootstrap files) was deleted. See `lisplib/ReadMe` for descriptions of individual `.l` files. The `manual/` subdir holds `.r` files for the in-Lisp `help` command.
- `utils/` — `append` and `tackon` (small C helpers). May or may not be needed in the final port.
- `doc/` — troff `-me` source for the Lisp manual.
- `man/` — Unix man pages.
- `lisp` symlink → `.` and `liszt` symlink → `cliszt`. Don't delete; `lisplib/lisztcc` references them.
- `PortPlan.md` — the porting strategy and phase definitions. Read before any non-trivial change.

## Architecture (the single most important thing to internalize)

The runtime uses a **page-table-tagged-pointer** value representation:

- `lispval` is `union lispobj *` — every value is a real heap pointer; there are no immediate fixnums (small ints are boxed too).
- Object **type** is determined by indexing a parallel array `typetable[]` with `((ptr - OFFSET) >> 9)`. So every 512-byte page holds objects of one type.
- Types are enumerated in `franz/h/ltypes.h` (ATOM, INT, DTPR (cons), DOUB, ARRAY, HUNK*, VECTOR, etc.).
- Allocation uses fixed `*SPP` constants ("structures per page") so each page is fully sub-divided. On x86_64 these constants will need to change because struct sizes double.
- GC is precise, with a mark bitmap (`bitmapi[]` in `alloc.c`) at 1 bit per long word of heap.

This scheme is *pointer-width-independent in principle* but the C code bakes 32-bit assumptions in many places (cast-to-int macros, fixed shift counts, `OFFSET=0`). Resolving these is the bulk of Phase 1.

## How the original system worked (deferred for later phases)

Reference only — none of this currently runs on Linux:

1. **Raw kernel** (`rawlisp`) — interpreter with no library loaded. Phase 2 target.
2. **`snlisp`** — `rawlisp` loads every library `.l` from source and calls `dumplisp` to write a memory image. Phase 5 will replace this with always-source-load (no dumplisp needed).
3. **`liszt`** — Lisp-coded compiler from `cliszt/`, dumped via `cmake.l`. Phase 6+.
4. **fasl / cfasl / ffasl** — runtime loading of compiled `.o` files. Depends on liszt and on an ELF-based replacement for the original a.out loader. Phase 6+.

## Conventions and gotchas

- **File extensions.** `.l` = Lisp source, `.o` from `liszt` = fasl-format compiled Lisp (*not* a C object), `.x` = lxref output, `.s` = assembly. The C kernel also produces `.o` files; same directory but different format. The build sequencing keeps them distinct.
- **`OFFSET` is a runtime `uintptr_t`** declared `extern` in `global.h`, defined in `data.c`, and set by `heap_init()` in `alloc.c` on first allocation. The Lisp heap is a single mmap'd region of `TTSIZE * 512` bytes (currently 32 MB); `xsbrk`/`ysbrk`/`csegment` all bump-allocate from it via `lisp_heap_alloc()`. `OFFSET` equals the base of that region.
- **`lispval` size doubles on x86_64.** `struct dtpr` was 8 bytes (pair of 32-bit pointers); now 16. To keep all the `*SPP` and `type_len` constants stable, `LBPG` was doubled to 1024 on x86_64 (Phase 1c) — slot count per page is unchanged because both slot size and page size doubled. `ATOX` shifts `>> 10` instead of `>> 9` accordingly. Bitmap density is per-long, so it works out the same on both platforms.
- **`(int)ptr` casts are bugs.** They truncate pointers on x86_64. Most live in macros (`ATOX`, `UPTR`, the `setbit`/`readbit` family in `alloc.c`). Phase 1 work item 1a.
- **Stdio peeking.** Original code reads `FILE->_cnt`/`_ptr` directly. We define `torek_stdio` so the portable `getc`/`ungetc` `peekc()` is used. Don't reintroduce `_cnt`/`_ptr` references on glibc.
- **Source style.** K&R C; some files have implicit `int` returns and unprototyped functions. Don't modernize cosmetically — wait until Phase 1 cast-fix work covers a file naturally.
- **No automated test suite.** Verification is empirical: does the kernel compile, does `rawlisp` run, does the library load, do simple expressions evaluate.
