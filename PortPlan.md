# PortPlan.md — Franz Lisp on x86_64 Linux

This is a plan, not a contract. Many decisions depend on what we discover when the C kernel first compiles and runs. Expect revisions after each phase.

**Status:** Phase 0 complete (build infrastructure, single-file smoke test). Old-machine support (vax/tahoe/68k/i386) was deleted as a follow-up cleanup; only `linux_x86_64` is configured. Phase 1 is next.

## Goal and non-goals

**Goal of this plan:** a usable interpreter — `rawlisp` boots on x86_64 Linux, loads the Lisp library from `.l` source at startup, and runs a REPL.

**Out of scope (future plans):**
- `liszt` (the Lisp-to-machine-code compiler) — needs a brand-new x86_64 codegen, since the existing one targets vax/68k/i386 instructions
- `fasl` / `cfasl` / `ffasl` — runtime loading of compiled `.o` files; depends on `liszt` and on building an ELF/`dlopen` equivalent of the existing a.out fasl
- `dumplisp` — writing the running image to disk; needs an unexec-style ELF writer (Emacs has solved this; nontrivial)

Without those three, we lose: fast startup (every boot reloads the library from source), compiled user code, and C-callable extensions. We retain a complete interpreter that can run all `.l` files in `lisplib/`.

## Why this is hard

The runtime is a **page-table-tagged-pointer** Lisp:
- Every Lisp value is a real heap pointer (`lispval = union lispobj *`); there are no immediate fixnums.
- Type is determined by `typetable[((ptr - OFFSET) >> 9)]` — i.e. each 512-byte page holds objects of one type only.
- Allocation sub-divides 512-byte pages into fixed numbers of objects per type: `DTPRSPP=64` dotted-pairs/page, `INTSPP=128`, `ATOMSPP=25`, etc. — sized so `sizeof(struct) * SPP == 512`.
- GC uses a precise mark bitmap (`bitmapi[]`) with **1 bit per long-word** of heap.

This scheme is *pointer-width-independent in principle*, but in practice the code bakes 32-bit assumptions into:
- The SPP constants (struct sizes double on 64-bit)
- Bitmap shifts (`>> 2`, `>> 5`, `>> 7` indexing assumes 4-byte longs)
- `(int)ptr` casts in macros like `ATOX`, `ATOLX`, `MARKVAL`, the `setbit`/`readbit` macros
- `MaxINT 0x3fffffff` boxed-int range
- `OFFSET=0` and `CNIL = OFFSET-4` sentinel — assumes the heap starts near address 0, which is impossible under Linux ASLR

So Phase 1 is largely a hunt for every place 32-bit-ness leaked into the design.

## Phase 0 — Build environment and platform target  *(DONE)*

**Outcome:** a fresh build target compiles a single source file with modern gcc to x86_64 ELF. No linking yet.

What was actually done:
- `lispconf` rewritten as `/bin/sh` (was csh; Fedora 42 doesn't ship csh), reduced to writing `franz/h/lconf.h`. The original `cvt.awk` Makefile preprocessor was missing from the distribution anyway, so this is a net simplification.
- `franz/h/config.h` rewritten to support only `linux_x86_64` (vax/tahoe/68k/sun/dual/pixel/mc500/lisa/i386 conditional blocks all stripped). It now `#error`s on any other target.
- `franz/h/aout.h` and `franz/h/lispo.h` replaced with local stub headers (originals were dangling symlinks to `/usr/include/a.out.h`).
- `fasl.c` updated so the existing "compile-to-C" slice-out path (`#define in_c 1`) is active on `linux_x86_64` — keeps the a.out-using code excluded.
- `franz/linux_x86_64/Makefile` created with modern gcc flags: `-m64 -O0 -g -fno-strict-aliasing -fno-omit-frame-pointer -no-pie -fno-pic -std=gnu89 -Wno-implicit-function-declaration -Wno-implicit-int -Wno-int-conversion -Wno-builtin-declaration-mismatch`.
- Top-level `Makefile` rewritten, dropping the multi-arch dispatch and all distribution targets.
- Old-machine source trees removed: `franz/{vax,tahoe,68k,i386}/`, `cliszt/{vax,tahoe,68k,i386,in-c}/`, `cliszt/Makefile`, `cliszt/C-Makefile*`, `lisplib/autorun/`, and the per-arch headers `{68k,vax,tahoe,i386}frame.h`, `dual{aout,lispo}.h`, `hpagsiz.h`.
- **Smoke test passes:** `franz/lisp.o` compiles to valid x86_64 ELF.
- Predicted Phase 1 issues confirmed when attempting `alloc.c`: missing `setbit`/`ftstbit` macros for our arch (work item 1d) and hundreds of pointer-to-int truncation warnings (work item 1a).

## Phase 1 — Make the kernel compile (x86_64)

**Outcome:** every `.c` file in `franz/` and `franz/linux_x86_64/` compiles to a `.o`. Linking still fails (Phase 2). The code probably still doesn't run correctly (Phase 3).

This is the bulk of the work. Approach: get one file compiling, fix issues structurally (in headers when possible), watch the fix cascade through other files.

### 1a. Pointer-width fixes

The single highest-volume issue. Hunt patterns:
- `(int)ptr`, `(int)p`, `(unsigned)ptr` — replace with `(intptr_t)` / `(uintptr_t)`. Concentrated in `alloc.c` (GC), `global.h` (`ATOX`, `UPTR`), `data.c`.
- `int` used as a pointer-holder in `union`s and structs — audit `union lispobj`, `struct atom`, `struct trent`, `struct heads`.
- `lispint` is `#define`d to `long`. On x86_64 `long` is 8 bytes — that's actually what we want here. Leave it.
- `MaxINT 0x3fffffff` / `MinINT -0x4000000`: keep as 31-bit boxed-int range for compatibility with existing Lisp code; widening would be a separate decision affecting all bignum boundary code.

### 1b. Heap layout  *(DONE)*

`OFFSET=0` and the assumption that heap pointers fit comfortably in 32 bits both break under ASLR. What was done:

- `OFFSET` is now an `extern uintptr_t` declared in `global.h`, defined `0` in `data.c`, and set by `heap_init()` in `alloc.c` on the first `xsbrk()` call.
- `heap_init()` calls `mmap(NULL, TTSIZE * 512, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)` to reserve the entire heap up front. The kernel commits anonymous pages lazily so a 32 MB reservation is cheap.
- A single bump allocator `lisp_heap_alloc(nbytes)` in `alloc.c` serves `xsbrk` (single page), `ysbrk` (multi-page array), and `csegment` (variable-size, was using sbrk directly).
- `CNIL` is `(lispval)0` for `linux_x86_64` — NULL is below every valid heap pointer on Linux, so the `OFFSET-4` arithmetic isn't needed and would have broken static initializers like `nilatom` in `low.c`.
- `datalim` is set to `heap_next` after each allocation; `VALID(a)` works as before via the `UPTR(a) <= UPTR(datalim)` check.
- `TTSIZE` bumped from 6120 (3 MB) to 65536 (32 MB) in `config.h`.

The 28→15 reduction in alloc.c warnings landed naturally with this work: the `(int)datalim`, `(int)beginsweep` style casts in GC and allocator code became `(uintptr_t)`. The remaining 15 alloc.c warnings are all Phase 1c free-list-encoding issues — writing pointer values into 4-byte int slots (`*loop = (int)next` and `temp->s.I = (int)freeptr`), which is a structural redesign.

### 1c. SPP and struct-size redesign

On x86_64, `struct dtpr { lispval cdr, car; }` is 16 bytes (was 8). Either:
- **Keep 512-byte pages, halve SPP** (`DTPRSPP 32`, etc.): simplest, minimal code change, but doubles per-page bookkeeping overhead.
- **Double the page size to 1024 bytes**: keeps SPP constants but every page-related shift (`>> 9` in `ATOX`) and `bitmapi` math changes.

Recommend **option A** for Phase 1; revisit if it bottlenecks. Audit every `*SPP` constant in `global.h` and every place the spp count is used in `alloc.c`/`data.c`. The `ATOMSPP=25` is a special case — it's not 512/sizeof(atom); it accounts for atom structs being awkwardly sized. Recompute from first principles.

### 1d. GC bitmap

`bitmapi[]` is `1 bit per long word`. The shifts assume `sizeof(long)=4`:
- `BITLONGS = TTSIZE * 4` was "TTSIZE pages * 128 longs/page / 32 bits/int". On x86_64, longs are 8 bytes so 64 longs/page; `BITLONGS = TTSIZE * 2`. Or switch to 1 bit per *pointer-sized word*.
- The `>> 7` and `>> 2` shifts in `setbit`/`readbit` (alloc.c lines 46-49) encode "byte-address → long-index → bit-within-int". These need to be re-derived from `sizeof(long)` and `sizeof(int)`. Replace with named macros: `BITS_PER_LONG`, `LONGS_PER_PAGE`, etc.
- `bitmsk[]` table — 32-entry mask, fine on either platform.

### 1e. K&R → modern C

Tactically: don't rewrite, just unblock the compiler.
- Add prototypes only when implicit-decl bites a real bug. `-Wno-implicit-function-declaration` covers most of it.
- Old-style function definitions (`foo(a, b) int a; long b; { ... }`) compile fine with gcc through C17.
- `extern char *malloc()` style declarations in headers: replace with `<stdlib.h>` include or correct prototypes when they conflict with glibc.
- `index()` / `rindex()` → `strchr` / `strrchr`.
- Fix any `signal(SIGFOO, handler)` mismatches — `SIGTYPE void` in config.h handles this.

### 1f. stdio peeking

`global.h` line 23: `#define peekc(p) (p->_cnt>0 ? *(p)->_ptr&0377 : ...)` directly inspects FILE internals. The `torek_stdio` variant (line 21) uses `getc`/`ungetc`, which is the correct portable approach on glibc. Make sure `torek_stdio` is set for `linux_x86_64` and audit `io.c` for any other `_cnt`/`_ptr`/`_filbuf` references.

### 1g. Replace i386 asm helpers with C

`franz/i386/`:
- `emul.s` — 32×32→64 multiply with offset add. Replace with `__int128` or `(int64_t)a * (int64_t)b`.
- `ediv.s` — 64÷32 divide with quotient/remainder. Same: `__int128` division.
- `callg.c` / variants — varargs trampoline for calling Lisp functions from C with N args. On x86_64 this is more delicate because of register-passing ABI. Likely needs hand-written x86_64 asm or `__builtin_apply` (gcc extension). **Highest risk subtask in Phase 1.** Worst case, switch all internal calls to a single fixed-arity convention that passes args via the Lisp stack.
- `nargs.c`, `inewint.c`, `clinkfns.c`, `prunei.c`, `malloc.c` — read each, port the C, drop or rewrite asm bits.
- `lowaux.s` (in `franz/`) — frame manipulation. With `PORTABLE_FRAME` defined, this should be largely unused; verify and stub remaining symbols.

### 1h. Bignum primitives

`adbig.c`, `dodiv.c`, `mulbig.c`, `dsmult.c`, `dmlad.c`, `mlsb.c`, `calqhat.c`, `qfuncl.c`, `exarith.c`, `subbig.c`, `pbignum.c`, `divbig.c` — these treat bignums as arrays of 16-bit "halfwords" stored in 32-bit ints/longs. On x86_64 the natural redesign uses 32-bit halfwords in 64-bit ints, doubling per-limb throughput. **For Phase 1, don't redesign; just make them compile.** They probably even work as-is once the int/long types line up. Defer redesign to a later optimization pass.

### 1i. Header hygiene

`franz/h/aout.h` and `franz/h/lispo.h` are symlinks to `/usr/include/a.out.h` (which doesn't exist on Linux). Replace with stub `aout.h` containing the minimum struct definitions referenced by `fasl.c` / `inits.c`, guarded by `#error` if compiled in. `dualaout.h`, `duallispo.h`, `tahoeframe.h`, `vaxframe.h`, `68kframe.h` — leave alone, never included on our target.

## Phase 2 — Link `rawlisp`

**Outcome:** `franz/linux_x86_64/rawlisp` is a runnable ELF executable (likely segfaults on first instruction).

1. Resolve undefined symbols. Common offenders: `_filbuf`, `_doprnt`, BSD-only libc, asm symbols we removed.
2. Linker flags: drop `-Z` (BSD ZMAGIC), drop `-H` (hole), drop the `/usr/lib/crt0.o` reference. Use the system default crt and let the linker pick `_start`. Add `-no-pie -static` for now to keep the address space deterministic — ASLR is the enemy of debugging Phase 3.
3. Libraries: `-ltermcap` is unavailable on most modern Linuxes; install `ncurses-compat-libs` or replace termcap calls with `tputs`/`terminfo` (small surface area in `io.c` for the `clear-screen` family).
4. Get `ld` happy. `nm rawlisp | grep ' U '` should show only libc symbols.

## Phase 3 — Boot the kernel

**Outcome:** `./rawlisp` runs, prints a prompt or error, doesn't segfault before reaching `main`'s body.

1. `inits.c` builds the initial atom table, opens stdin/stdout, mmaps the heap, builds the typetable, allocates the initial set of atoms (`nil`, `t`, `*`, etc.). Step through with gdb on first run; everything in this file is suspect.
2. `nilatom` and `eofatom` — `PORTABLE` defines `nil` as `&nilatom`. These are actual `struct atom` globals; verify they're properly zero-initialized and live outside the heap (or in a fixed reserved page).
3. The argument-passing stack (`np`, `lbot`, `xstack[]`): `SPISFP` is set, so we use the software-managed stack rather than abusing the C SP. Verify `sp()`/`stack()`/`unstack()` work — they're macros into `xsp`/`xstack` for the SPISFP case.
4. Signal handling: SIGINT, SIGFPE, SIGSEGV (used for stack-overflow detection in some paths). Linux delivers signals with `siginfo_t`; the existing handlers ignore that, which is fine for SIG_DFL-style use.
5. Expect to spend 1-3 days here chasing crashes. Common pattern: fix one field of `struct atom`, re-run, hit next crash, repeat.

## Phase 4 — REPL

**Outcome:** `(+ 1 2)` typed at the prompt prints `3`. `(quote (a b c))` prints `(a b c)`.

1. `io.c` reader (`Lread`) and printer (`Lprint`) are exercised. Easiest is to manually call them from `main` before turning on the eval loop.
2. `eval.c` — once read/print works, hook in eval. Test `quote`, `cond`, `setq`, `cons`, `car`, `cdr`, `+`, `print`. These are all in the C kernel, no library needed.
3. `error.c` and the catch/throw machinery — first errors will probably crash; debug as encountered.
4. `frame.c` — sets up function-call frames. Important to verify frame chaining works before loading any Lisp source (which uses `funcall` extensively).

## Phase 5 — Library load and library compatibility

**Outcome:** A modified `buildlisp.l`-equivalent loads `lisplib/*.l` files from source on every startup. `lisp` runs to a Lisp-coded top-level prompt.

1. `lisplib/buildlisp.l` is the existing source-load path (`build:load=t` makes it `load` `.l` instead of `fasl` `.o`). Strip the `(dumplisp ...)` call at the end; just drop into the loaded toplevel.
2. Build a small wrapper: `rawlisp` → reads a startup `.l` file (path from env or argv) → that file does the equivalent of `make slow` step 2's `(load ...)` chain. Effectively, every startup is "slow boot."
3. Test loading order: `common0.l`, `syntax.l`, `charmac.l`, `macros.l`, `common1.l`, `common2.l`, `common3.l`, `vector.l`, `array.l`, `pp.l`, `version.l`, `tpl.l`, `toplevel.l`. Each one tests progressively more of the kernel.
4. Failures at this stage usually point back at kernel bugs — wrong `cdr` of a freshly-`cons`'d cell, GC corrupting state, reader botching dotted pairs, etc. Plan to bounce between Phase 5 issues and Phase 1/3 fixes.
5. Don't try to load `format.l` (says the existing buildlisp: "only load if compiled, saves time"). Skip it for now.
6. Don't load anything that depends on `fasl` working (e.g. CMU autoloads).

**Done condition:** the user can type `(+ 1 2)`, `(defun fact (n) (if (= n 0) 1 (* n (fact (1- n)))))`, `(fact 10)` and get correct answers. `(load 'foo.l)` works for user files.

## Tracking and intermediate milestones

Suggested intermediate checkpoints (each is a commit-worthy state):

- M0: `lispconf linux_x86_64` succeeds, one C file compiles
- M1: all C files compile (warnings allowed)
- M2: `rawlisp` links
- M3: `rawlisp` reaches `main` and prints something
- M4: minimal REPL accepts `(quote x)` and prints `x`
- M5: arithmetic and `cons`/`car`/`cdr` work
- M6: `common0.l` loads without error
- M7: full library loads, Lisp top-level prompt appears
- M8: regression sample (factorial, list-reverse, simple `defmacro`) all work

## What I'd want you to confirm before starting

1. Are you OK with the **drop-dumplisp/drop-fasl/drop-liszt** scope cuts for v1? Without these, "lisp" is interpreted-only and slow to start (re-reads the library every boot, ~1s probably). Adding any of them back is a multi-week project of its own.
2. Are you OK keeping the **page-tagged-pointer architecture**, just widened? An alternative would be a wholesale redesign to NaN-boxing or low-bit tags, which would be cleaner but would invalidate the entire allocator, GC, and most of `data.c`.
3. Toolchain: gcc (default Fedora) or do you want to commit to clang? Either works but the K&R warnings are noisier under clang.
4. ~~Are you willing to lose `franz/{vax,tahoe,68k}` machine support?~~ *Confirmed in Phase 0 cleanup; deleted.*

Once you confirm, Phase 0 is a half-day's work and gives us the first concrete signal about how much of Phase 1 is mechanical vs. genuinely difficult.
