# CompilerPlan.md — Lisp-to-C compiler

A plan to get the Franz Lisp compiler (`liszt`) working on x86_64
Linux using **C as the intermediate language**, taking advantage of
the existing `#+for-c` back-end that's already in `cliszt/` and Jeff
Dalton's runtime support preserved at `reference/cliszt-i386/`.

This is a plan, not a contract. As with [PortPlan.md](PortPlan.md),
expect revisions after each phase as concrete issues surface. The
estimates assume working in 1-2 day chunks with feedback in between.

## What we already have

The compile-to-C path was implemented in 1992-1994 by Jeff Dalton at
the AI Applications Institute (Edinburgh) and is sitting in this tree:

  - `cliszt/*.l` — the liszt compiler source. Roughly 9700 lines of
    Lisp. The back-end is feature-gated: `#+for-vax`, `#+for-68k`,
    and `#+for-c`. The `#+for-c` branches occur in 13 of the 18
    files; the highest concentration is in `io.l` (28 occurrences)
    where most of the C-emission logic lives.
  - `reference/cliszt-i386/clink.c` — the per-file linker invoked by
    compiled C-output Lisp. Sets up transfer tables, resolves
    cross-file calls, processes the bind-table that defines functions
    and runs top-level forms. ~320 lines of K&R C.
  - `reference/cliszt-i386/clinkfns.c` — additional link-time helpers.
    ~330 lines.
  - `reference/cliszt-i386/cinit.c` — 11-line `#include` template
    that goes into every compiled `.c` file's initializer.
  - `reference/cliszt-i386/compiled.h` — macros expanded by compiled
    C output: `CALLTRAN(n)`, `BCDCALL(loc)`, `QADD1(loc)` /
    `QSUB1(loc)`, `safenewint`, `fastnewint`, the `bindspec` struct.
  - `reference/cliszt-i386/zfasl.l` / `in-c-fasl.l` — Lisp-side
    runtime that drives `(cfasl ...)` to load compiled `.o` files
    into the running image.
  - `franz/fasl.c` already has the `#if in_c` slice activated for
    `linux_x86_64`; it's mostly a stub today, but the gating is in
    place.

What's **missing**:

  - A built `liszt` executable.
  - The `clink.c` family hasn't been ported (still in `reference/`,
    untouched since 1994).
  - `cfasl` is stubbed — `arch.c` errors with "not supported on
    linux_x86_64".

## Goal and non-goals

**Goal:** the user can do the following from `bin/lisp`:

```lisp
-> (sstatus feature for-c)
-> (load 'foo.l)                ; runs interpreted
-> (liszt 'foo.l)               ; compiles foo.l -> foo.c -> foo.so
-> (cfasl 'foo.so 'foo)         ; loads it; subsequent calls run compiled
-> (foo 100)                    ; orders of magnitude faster
```

**Non-goals (deferred):**

  - VAX or 68k back-ends. We have only `#+for-c`.
  - Bootstrapping liszt to compile *itself* via the C path. Useful
    eventually but not on the critical path.
  - Cross-compilation, MERLIN-style multi-target builds.
  - The `lxref` cross-reference tool — independent of this plan.

## The two strategies

There's a key strategic choice: how does compiled code end up in the
running interpreter?

**A. Runtime dynamic loading (`dlopen`).** Compile the C output to a
shared object (`gcc -shared -fPIC`), `dlopen` it from the kernel,
look up the per-file `init` function with `dlsym`, call it. Init
runs `clink()` which registers the compiled functions in the
running image. This matches the original `(cfasl ...)` semantics —
fully interactive, REPL-friendly. Most code is portable C.

**B. Static link (AOT).** Compile the C output and link it directly
into a custom `rawlisp` at build time. No runtime loading. Loses
interactive `(cfasl ...)`; gains a much simpler runtime story.

**Recommendation: do A.** It's only modestly more work, and it's
what makes the whole feature actually useful. A v0 with B as a
fallback is fine if `dlopen` proves harder than expected, but the
plan below assumes A.

## Phase 0 — Make liszt buildable

**Outcome:** `bin/liszt` is a working executable that runs the
compiler. Doesn't need to produce correct C yet; just needs to start
up and parse a trivial `.l` file.

The original built liszt via `cliszt/cmake.l`'s `genl` function: load
each `.l` file in dependency order, set features, `(dumplisp 'nliszt)`.
We don't have `dumplisp`, so use the same pattern as `bin/lisp`:

  1. Create `lisplib/liszt-init.l` that:
     - Sets `;` as the comment macro (already needed for buildlisp).
     - `(sstatus feature for-c)` — the only back-end we care about.
     - `(sstatus nofeature for-vax)`, `(sstatus nofeature for-68k)`
       — explicitly nothing else.
     - `(load <each cliszt/ file>)` in the order from `cmake.l`:
       `decl`, `array`, `vector`, `datab`, `expr`, `io`, `funa`,
       `funb`, `func`, `tlev`, `instr`, `fixnum`, `util`,
       `lversion`. Plus the new headers: `chead`, `cmacros`,
       `const`, `cload`.
     - Calls the compiler entry (`liszt-it` or whatever the top-level
       function is) on `argv` if any.
  2. Create `bin/liszt` shell wrapper, parallel to `bin/lisp`, that
     pipes `liszt-init.l` into `rawlisp` and forwards command-line
     args.

**Risks:**

  - **liszt depends on Lisp library functions that the bare kernel
    lacks.** liszt is non-trivial Lisp code; expect it to need at
    least the `common0..3.l` + `syntax.l` + `charmac.l` + `macros.l`
    base, similar to how the standard library bootstraps. Likely
    just load the standard library first, then load liszt on top.
  - **Some `cliszt/*.l` files may have 32-bit assumptions** (similar
    to what Phase 3 of the port surfaced in the kernel). Each will
    show up as a runtime error during loading; fix as encountered.
    Most likely: bignum-pretending-to-be-fixnum issues, address
    arithmetic that assumes `(int)ptr` round-trips.
  - **The compiler uses `(dumplisp ...)`** to produce its own
    fast-start binary. We don't have `dumplisp`; the launcher just
    re-loads on every invocation. Cost: maybe a second on modern
    hardware.

**Time estimate:** half a day to a day.

**Done condition:** `bin/liszt --version` (or whatever the existing
flag is) prints the liszt banner.

## Phase 1 — Verify `#+for-c` produces sensible C output

**Outcome:** running `bin/liszt foo.l` on a small test file produces
a `foo.c` that compiles cleanly with gcc, even if it doesn't yet
load correctly.

Read carefully through `cliszt/io.l` (the `d-printfileheader`,
`d-prelude`, the `cwrite` calls in the `#+for-c` branches), plus
`cliszt/funa.l`, `funb.l`, `decl.l`, and `expr.l` for the
function/expression emission paths. Identify any places where the
emitted C makes 32-bit assumptions:

  - Casts like `(int)ptr` in the emitted code — broken on x86_64.
  - The `int linker_size, trans_size` parameters to `clink()` are
    fine because the values are small counts. But if any emitted
    code stores an address as `int`, that needs widening.
  - The `compiled.h` macros `QADD1`/`QSUB1` use `(loc)->i + 1`
    arithmetic and pointer comparisons against `Lastfix[]` —
    Lastfix isn't defined anywhere I see, suggesting it's a Dalton
    extension. May need to add it, or rewrite the macros to use the
    `Fixzero[]/Negs[]` we already have.

Test inputs, simplest first:

```lisp
;; level 1: arithmetic only, no top-level forms
(defun add2 (a b) (+ a b))

;; level 2: recursion + cond
(defun fact (n) (cond ((zerop n) 1) (t (* n (fact (sub1 n))))))

;; level 3: list operations
(defun my-length (l) (cond ((null l) 0) (t (add1 (my-length (cdr l))))))

;; level 4: closures via funarg, mapcar
(defun double-each (l) (mapcar '(lambda (x) (* 2 x)) l))
```

For each: `bin/liszt foo.l && gcc -c foo.c -I franz/h -I reference/cliszt-i386`.
Fix emission bugs as they surface in the C output.

**Time estimate:** one to two days. The for-c back-end was actually
exercised on i386 in 1994, so the emission logic is mature. The risk
is that "i386" assumptions baked into output (e.g., int-sized
literals) need a few targeted changes.

**Done condition:** `gcc -c foo.c` produces a clean object file for
all four test inputs. We don't load it yet.

## Phase 2 — Port `clink.c` and friends to `franz/linux_x86_64/`

**Outcome:** `clink`, the support functions in `clinkfns.c`, and
`cinit.c` are part of the kernel build, link cleanly into rawlisp,
and can be called at runtime.

  1. Move `reference/cliszt-i386/clink.c` →
     `franz/linux_x86_64/clink.c`. Same for `clinkfns.c`. Add to
     `ArchSrc` in the per-arch Makefile.
  2. Apply the same widening patterns from kernel Phase 1: K&R
     prototypes preserved, `(int)ptr` → `(uintptr_t)ptr` in any
     pointer-arithmetic contexts, `int *bind_lists` → `long
     *bind_lists` (already done in `data.c`; the `clink.c` extern
     decl needs to follow).
  3. Replace the `arch.c` stub for `clinker` (the trampoline that
     transfer tables initially point at) with the real version from
     `clinkfns.c` — or, simpler, leave the stub and have `zfasl`
     call `dlsym` for it. Investigate.
  4. The `cinit.c` file is a template — it gets `#include`d into
     every compiled `.c` output. Place it where liszt's emission
     code expects (look at `d-printfileheader`).

**Risks:**

  - `clink.c` uses `Vreadtable`, `ibase`, `gcdis`, `tatom`, etc. —
    these all exist in our kernel but the externs in clink.c need
    to match.
  - The `bindspec` struct layout: `lispval *btype, lispfun *bentry,
    char *bname` — three pointers, 24 bytes on 64-bit. Compiled C
    output emits these literal arrays; sizes must match. Any
    mismatch silently produces wrong dispatch.
  - `make_linktable`, `rdstr`, `gettran` are static helpers — their
    signatures may use `int` where `long` is now wanted.

**Time estimate:** a few hours to half a day. The code is already
near-portable C; just needs the same pass we did for the kernel.

**Done condition:** `franz/linux_x86_64/rawlisp` still links and
boots, with `clink` and friends now defined for real (instead of
stubbed).

## Phase 3 — Implement `cfasl` via `dlopen`

**Outcome:** from inside Lisp, `(cfasl 'foo.so 'init-foo)` opens
foo.so via `dlopen`, looks up `init-foo` via `dlsym`, calls it. The
init function does whatever `clink` setup is needed, after which the
defined Lisp functions are callable by name from the running image.

The original `cfasl` (in the gated-out part of `ffasl.c`) read BSD
a.out object files directly: parse the symbol table, mmap text/data
into a chosen address, fix up relocations. We don't reproduce that
on Linux — we just delegate to the dynamic linker.

  1. In `arch.c`, replace the error stub `Lcfasl` with a real
     implementation:
     - Take args: a string filename (the `.so`), a string init-func
       name (the C symbol to call), and optionally a discipline.
     - `void *handle = dlopen(filename, RTLD_NOW | RTLD_LOCAL);`
       Error if NULL.
     - `void (*init)() = dlsym(handle, initname);` Error if NULL.
     - Call `init()`. It runs the per-file boilerplate from
       `cinit.c` which calls `clink()` which registers the file's
       functions.
     - Stash `handle` somewhere (e.g., a global list) so a future
       mechanism could `dlclose` it; for now we just leak handles.
  2. Update `lisplib/in-c-fasl.l` (port from
     `reference/cliszt-i386/`) to call the new `cfasl` and to know
     about the `.so` extension instead of `.o`.
  3. Add `-ldl` to the rawlisp link line.

**Risks:**

  - **PIE conflict:** rawlisp is currently built with `-no-pie -fno-pic`
    for deterministic addressing in debugging. `dlopen` works fine on
    `-no-pie` mains as long as the loaded `.so` is `-fPIC`, but we
    might hit relocation-overflow issues if compiled code references
    rawlisp globals (which are at fixed low addresses in `-no-pie`).
    Likely workaround: emit `extern` references via PLT-friendly
    `lispval *handy_ptr` indirections instead of direct reference to
    `nilatom` etc.
  - **Symbol visibility:** rawlisp must export the kernel symbols
    that compiled code calls back into (`eval`, `Lapply`, `Ifuncal`,
    `inewint`, `cons`, `markdp`, etc.). With static link to libc,
    these symbols may be hidden by default. Add `-rdynamic` to the
    rawlisp link.
  - **One-time initialization order:** if a `.so` references a
    function that's registered by *another* `.so` not yet loaded,
    you get an undefined symbol at `dlopen` time. RTLD_LAZY can
    defer; or we order loads explicitly via the existing
    transfer-table mechanism (`zlinker`) which Dalton's design
    already accounts for.

**Time estimate:** half a day to a day, mostly driven by the
`-rdynamic` / PIE interaction shaking out.

**Done condition:** a "hello world" sequence works end to end:

```sh
$ cat > hello.l <<'EOF'
(defun greet (name) (princ "hello, ") (princ name) (terpri) name)
EOF
$ bin/liszt hello.l    # produces hello.c
$ gcc -shared -fPIC -I franz/h -I franz/linux_x86_64 hello.c -o hello.so
$ bin/lisp
-> (cfasl 'hello.so 'inithello)
-> (greet 'world)
hello, world
world
```

## Phase 4 — Make the build pipeline ergonomic

**Outcome:** `bin/liszt foo.l` produces `foo.so` directly, hiding
the gcc step.

  1. After emitting `foo.c`, liszt automatically invokes gcc with
     the right flags. Either:
     - Lisp side: a built-in `(system "gcc -shared -fPIC ...")` call
       at the end of compile-to-C. The kernel has `(syscall ...)`;
       use it. Or `(execl ...)` style.
     - Shell wrapper: have `bin/liszt` post-process by running gcc
       on the produced `.c`.
  2. Optional: a `.l` -> `.so` Makefile rule that makes
     `make foo.so` work for users who like Makefiles.
  3. Document the include path (`-I franz/h -I franz/linux_x86_64`)
     so user-visible code doesn't need to know it.

**Time estimate:** a few hours. Pure quality-of-life work.

## Phase 5 — Performance verification

**Outcome:** measure the speedup vs. interpreted code.

A canonical benchmark: tak, fib, or the boyer test from the
Gabriel benchmark suite. Run interpreted, then compiled, time both.

A reasonable target: 5-20x speedup on tight numeric code, less on
list manipulation. The original Franz Lisp on i386 reportedly hit
~10x; our `__int128` bignum kernels and a modern gcc optimizer might
do somewhat better.

If speedup is disappointing, common culprits:

  - The compiled code goes through `Ifuncal` for every call instead
    of using the transfer-table direct-call path. Check
    `(sstatus translink on)`.
  - Calls back into the interpreter for things that should be open-
    coded. The `#+for-c` back-end may generate `Ifuncal(a:plus, ...)`
    when it could open-code `+` for fixnums. Inspect emitted C.

## Risks and open questions

  - **`compiled.h`'s `Lastfix` reference.** I haven't found a
    definition in this tree. May be an i386-era extension Dalton
    added; need to either define it (as `&Fixzero[1023]`?) or
    rewrite the macros that use it. Will surface in Phase 1.
  - **`(int)pointer` casts in emitted C.** The cliszt back-end was
    written when `int = pointer = long = 4 bytes`. Even after
    Phase 1 review there may be subtle widenings needed.
  - **Static-vs-PIE dance.** Already discussed. If `-no-pie` proves
    incompatible with practical `dlopen` use, rebuild rawlisp PIE
    and accept the debugging-trace-file fragility.
  - **No tests.** Like the rest of this port, there's no automated
    test suite to verify the compiler hasn't regressed. Phase 5's
    Gabriel benchmarks become the de-facto regression suite.

## Summary table

| Phase | Outcome | Estimate |
|---|---|---|
| 0 | `bin/liszt` runs | 0.5–1 day |
| 1 | Trivial `.l` → clean `.c` | 1–2 days |
| 2 | `clink.c` ported and linked | 0.5 day |
| 3 | `dlopen`-based `cfasl` works | 0.5–1 day |
| 4 | `bin/liszt` → `.so` in one step | 0.25 day |
| 5 | Benchmark vs. interpreted | 0.25 day |
| **Total** | **~3–5 days** | |

The big unknown is Phase 1 — how much the `#+for-c` back-end's
emission rules need updating for x86_64. If Dalton's code happens to
be already 64-bit-clean (he was thinking ahead), Phase 1 collapses to
half a day. If it bakes 32-bit assumptions deeply, it's the dominant
cost. We'll know after Phase 0 finishes and we can run liszt on a
trivial input.
