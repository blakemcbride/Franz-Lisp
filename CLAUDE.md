# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Franz Lisp (Opus 38.92/38.93) — the Berkeley dialect of Lisp from the early 1980s — running on **64-bit Unix: Linux x86_64 and macOS (Apple Silicon)**. The original distribution targeted vax/tahoe/68k/i386 BSD; this tree keeps the C kernel, the Lisp library, and the liszt compiler (with its Lisp-to-C back end). `PortPlan.md` and `CompilerPlan.md` record how the port was done.

## Build

```
./lispconf                 # detect host; writes franz/h/lconf.h (./lispconf -l lists targets)
make smoke                 # compile one kernel .c file
make kernel                # compile every kernel .c file
make rawlisp               # link rawlisp (the bare interpreter)
make clean                 # every target's objects and rawlisp
make distclean             # clean + lconf.h, doc/ build, .so/.x output

bin/lisp                   # full Franz Lisp -- rawlisp + library loaded from source
bin/liszt foo.l            # compile foo.l -> foo.so
bin/test                   # interpreted regression suite (tests/)
bin/test-compiled          # the same suite compiled with liszt
```

`make slow`, `make fast`, and `make install` are disabled: they run the original bootstrap that dumps Lisp images with `dumplisp`, which isn't implemented. `bin/lisp` loads the library from `.l` source on every startup instead (about a second).

`bin/lisp` sets `FRANZ_INIT` to `lisplib/init.l`, and `rawlisp` loads that file before reading stdin (`Ntpl` in `lisp.c`), so stdin stays the user's terminal. Don't pipe it in with `cat init.l - | rawlisp`: that leaves `cat` blocked on the terminal after `(exit)`. `init.l` sets up `;` as the comment reader-macro and runs `buildlisp.l`. Override the library path with `LISPLIB=/path/to/lisplib bin/lisp`. `bin/liszt` does the same with `lisplib/liszt-init.l`, which ends with `(reset)` so control passes to `lisztinit`.

The compiler: `bin/liszt foo.l` emits C, and `lisplib/lisztcc` compiles it to `foo.so`, which `(cfasl 'foo.so 'init 'init-fn "subroutine" "")` loads into `bin/lisp` with `dlopen`. Compiled code runs 2-4× faster than interpreted on recursive-arithmetic code.

Not implemented: `dumplisp` (saving images), `(showstack)`/`(baktrace)` (need a C stack unwinder), and the BSD a.out `ffasl` (`cfasl` of liszt output works).

**Targets.** `lispconf` accepts `linux_x86_64`, `linux_arm64`, `macos_arm64`, `macos_x86_64` (or detects the host) and writes `#define <target> 1` to `franz/h/lconf.h` (generated, not tracked). `config.h` maps any of them to the internal macro **`unix64`** — kernel code guards the 64-bit port with `#if unix64`, never with a target name — plus `m_64bit` (LP64 code paths), `os_linux`/`os_macos`, and `MACHINE` (real CPU, for `(status features)`). Each target builds in `franz/<target>/`; the top-level Makefile and `bin/lisp`/`bin/liszt` read the target back from `lconf.h`. The per-arch *sources* and build rules exist once, in `franz/linux_x86_64/`; the other target directories hold only a short Makefile (`VPATH` + `include ../linux_x86_64/Makefile`) and build output. Never copy sources into a target directory.

The shared Makefile uses gcc flags on Linux and Apple-clang flags on Darwin (see `macOS.md`). Both silence only K&R-style diagnostics (implicit int, implicit declarations, unprototyped calls, int/pointer conversion); the build is otherwise warning-free, so treat any new warning as a possible bug. Every object depends on every header in `franz/h/`.

`RootDir` in the top-level Makefile resolves at make time via `$(shell pwd)`, so the tree can live anywhere.

## Repository layout

- `franz/` — C kernel of the interpreter. ~30 machine-independent `.c` files plus headers in `franz/h/`. Only the 64-bit Unix configuration is present (git history has the vax/tahoe/68k/i386 sources).
- `franz/linux_x86_64/` — the per-arch sources and build rules, shared by every target; also the Linux x86_64 build directory. Holds the Makefile, `callg.c` (foreign-call trampoline: dispatches on argument count with direct C calls), `bignum.c` (30-bit-halfword bignum kernels in portable C), `clinkfns.c` (links liszt-compiled code into the running Lisp), and `arch.c` (inewint/blzero/prunei, PORTABLE_FRAME helpers, tynames[], `Lcfasl`, stubs for what has no 64-bit equivalent). `franz/macos_arm64/`, `franz/macos_x86_64/`, `franz/linux_arm64/` are thin build directories that include its Makefile.
- `franz/h/` — shared headers. `config.h` reads `lconf.h` (written by `lispconf`) and configures the kernel. `aout.h` and `lispo.h` are local stubs for BSD a.out headers that modern systems don't have.
- `cliszt/` — Lisp source for the **liszt compiler**, loaded from source by `bin/liszt` (C back end, `#+for-c`).
- `lisplib/` — the Lisp library, loaded from source by the interpreter. See `lisplib/ReadMe` for descriptions of individual `.l` files. The `manual/` subdir holds `.r` files for the in-Lisp `help` command. `lisztcc` is the C-compile step liszt runs.
- `tests/` — `harness.l` (the `chk-eq`/`chk-err` macros) and `test-all.l` (the cases) for `bin/test` and `bin/test-compiled`.
- `reference/` — original i386 sources kept for comparison; not built.
- `utils/` — `append` and `tackon` (small C helpers from the original distribution); not built.
- `doc/` — troff `-me` source for the Lisp manual.
- `man/` — Unix man pages.
- `lisp` symlink → `.` and `liszt` symlink → `cliszt`. Don't delete; `lisplib/lisztcc` references them.

## Architecture (the single most important thing to internalize)

The runtime uses a **page-table-tagged-pointer** value representation:

- `lispval` is `union lispobj *` — every value is a real heap pointer; there are no immediate fixnums (small ints are boxed too).
- Object **type** is determined by indexing a parallel array `typetable[]` with `ATOX(ptr)` = `(ptr - OFFSET) >> 10`. So every `LBPG` = 1024-byte page holds objects of one type.
- Types are enumerated in `franz/h/ltypes.h` (ATOM, INT, DTPR (cons), DOUB, ARRAY, HUNK*, VECTOR, etc.).
- Allocation uses fixed `*SPP` constants ("structures per page") so each page is fully sub-divided. `LBPG` is 1024 (twice the 32-bit value) so that, with 8-byte slots, the slot counts per page match the `*SPP` and `type_len` constants.
- GC is precise, with a mark bitmap (`bitmapi[]` in `alloc.c`) at 1 bit per `long` of heap (4 bitmap ints per page).
- Sizes in the allocator are in `long`s and bytes: `type_len` counts long-sized slots, vector size headers are in bytes, and `VecTotSize`/`VecSize` divide by `sizeof(long)`. Never hard-code 4 or 512.

## Conventions and gotchas

- **File extensions.** `.l` = Lisp source, `.so` = liszt-compiled Lisp (a shared object for `cfasl`), `.x` = lxref output, `.s` = assembly. The C kernel's `.o` files are ordinary C objects.
- **`OFFSET` is a runtime `uintptr_t`** declared `extern` in `global.h`, defined in `data.c`, and set by `heap_init()` in `alloc.c` on first allocation. The Lisp heap is a single mmap'd region of `TTSIZE * LBPG` bytes (32 MB); `xsbrk`/`ysbrk`/`csegment` all bump-allocate from it via `lisp_heap_alloc()`. `OFFSET` equals the base of that region.
- **Pointers are 64-bit and live above 4 GB on macOS.** `(int)ptr` casts truncate them. So does calling an undeclared function that returns a pointer or `long` — its result is taken as `int`. Declare such functions (kernel ones go in `h/dfuncs.h`), and check that every pointer-returning function actually `return`s its value: on arm64 a value left in a register by a callee doesn't survive falling off the end of a function.
- **Variadic C functions need prototypes.** On Apple arm64, variadic arguments are passed on the stack, so calling `execl`, `printf`, etc. without a prototype passes them wrongly. For the same reason the foreign-call trampoline (`callg_`) can't call variadic C functions on Apple arm64.
- **Integer division by zero doesn't trap on arm64.** Fixnum `/` and `mod` raise SIGFPE explicitly (as `quotient` does); keep that pattern for any new division.
- **Use `%ld`/`%p` in printf formats** for `long`s and pointers, and `%lf` (not BSD `%F`) to `scanf` a `double`.
- **`load` binds `readtable` per file.** Library files that begin with `(environment-maclisp)` (struct, loop, hash, flavors) switch to a private Maclisp-syntax readtable that ends with the file; files loaded from inside them are read with the caller's syntax. See `load-a-file` (common1.l) and `environment-syntax` (common2.l).
- **Stdio peeking.** `torek_stdio` is defined so the portable `getc`/`ungetc` `peekc()` is used. Don't reintroduce `FILE->_cnt`/`_ptr` references.
- **Source style.** K&R C; some files have implicit `int` returns and unprototyped functions. Don't modernize cosmetically.
- **Testing.** Run `bin/test` and `bin/test-compiled` after any kernel or library change; both should report all cases passed.
