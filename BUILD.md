# BUILD.md

How to build and run the 64-bit Unix port of Franz Lisp (Linux and
macOS) from this source tree.

## Quick start

```sh
./lispconf
make rawlisp
bin/lisp
```

Type `(+ 1 2)` and you should get back `3`.

## Prerequisites

**On macOS** (Apple Silicon or Intel) see [macOS.md](macOS.md): install
the Xcode Command Line Tools and the same commands work unchanged.

On Linux you need:

- A C compiler that accepts `-std=gnu89`: gcc 4 or later (tested with
  gcc 15). Clang on Linux is untested.
- GNU make.
- glibc development headers.
- A termcap-compatible library with the classic API (`tgetent`,
  `tgetstr`, `tputs`, `tgoto`). On modern systems this comes from
  ncurses, exposed as `libtermcap`.

On Fedora / RHEL:

```sh
sudo dnf install gcc make glibc-devel ncurses-devel
```

On Debian / Ubuntu:

```sh
sudo apt install build-essential libncurses-dev
```

POSIX shell (`/bin/sh`) is the only requirement at runtime besides
the C library — there's no Python, no autoconf, no CMake.

## Build

First configure the tree. With no argument `lispconf` detects the
host; you can also name the target explicitly:

```sh
./lispconf                 # detect: linux_x86_64, macos_arm64, ...
./lispconf linux_x86_64    # or name it
./lispconf -l              # list targets
```

| Target | Platform |
|---|---|
| `linux_x86_64` | Linux on x86_64 (gcc) |
| `linux_arm64` | Linux on arm64/aarch64 (untested) |
| `macos_arm64` | macOS on Apple Silicon (Apple clang) |
| `macos_x86_64` | macOS on Intel (untested) |

`lispconf` writes the target into `franz/h/lconf.h` (for example
`#define macos_arm64 1`), and each target builds in its own directory,
`franz/<target>/`. All targets share one 64-bit configuration, though:
`config.h` maps every target name to the kernel's internal `unix64`
macro, and the per-arch sources (`callg.c`, `bignum.c`, `arch.c`,
`clinkfns.c`) and build rules live once, in `franz/linux_x86_64/`. The
other target directories hold a short Makefile that includes those
rules, plus that target's objects and `rawlisp`. The Makefile adapts
its compiler flags to the host (gcc on Linux, Apple clang on macOS).

To switch targets in the same tree, run `make clean` and then
`./lispconf <target>` again. `make` and `bin/lisp` refuse to run until
the tree is configured.

Then build the kernel and link the bare interpreter:

```sh
make rawlisp
```

You'll see `cc -c ...` lines for each kernel source, then a final
`cc ... -o rawlisp ...` link step. The result is
`franz/<target>/rawlisp` (e.g. `franz/macos_arm64/rawlisp`) — about
500 KB, dynamically linked.

Other useful targets:

| Target | What it does |
|---|---|
| `make smoke` | Compile a single kernel `.c` file. Fast sanity check. |
| `make kernel` | Compile every kernel source. Stops short of linking. |
| `make rawlisp` | Compile + link the bare interpreter. |
| `make clean` | Remove `.o` files and the binary, for every target directory (so a target switch never leaves stale objects). |
| `make distclean` | `clean`, plus `franz/h/lconf.h` (rerun `./lispconf` afterwards), the manual build in `doc/`, compiled Lisp (`.so`), lxref (`.x`) output and editor droppings anywhere in the tree. Source edits are left alone. |

`make` from the top-level dispatches into `franz/<target>/Makefile`,
where the actual work happens.

## Run

[RUNNING.md](RUNNING.md) covers using the interpreter and compiler
in detail; this section is the minimum.

`bin/lisp` is the launcher you'll use day-to-day. It runs the bare
interpreter (`rawlisp`) with the standard library auto-loaded via
`lisplib/init.l`:

```sh
bin/lisp
```

Output:

```
Franz Lisp, Opus 38
[load /…/lisplib/common2.l]
[load /…/lisplib/common3.l]
… (the full library loads in about a second on modern hardware)
-> 
```

The `->` is the prompt. Try:

```lisp
(defun fact (n) (cond ((zerop n) 1) (t (* n (fact (sub1 n))))))
(fact 10)
(reverse '(1 2 3 4 5))
(mapcar 'add1 '(10 20 30))
(times 1000000 1000000)
```

To override the library location, set `LISPLIB`:

```sh
LISPLIB=/some/other/path/lisplib bin/lisp
```

To run `rawlisp` directly without the library (useful for kernel
debugging), invoke `franz/<target>/rawlisp` straight. You'll
have only the kernel built-ins (`def`, `eq`, `cons`, `mapcar`, `cond`,
arithmetic, the break loop). `defun`, `princ`, `reverse`, etc. are
library-only and won't be defined.

## What gets built

After `make rawlisp`:

```
franz/*.o                   37 machine-independent kernel objects
franz/<target>/callg.o      foreign-call trampoline
franz/<target>/bignum.o     bignum kernels (multiply, divide, etc.)
franz/<target>/arch.o       small arch helpers, frame primitives
franz/<target>/clinkfns.o   linker for compiled (liszt) code
franz/<target>/rawlisp      the linked interpreter
```

The kernel comes to ~20k lines of C across `franz/`; the arch-specific
glue (shared by all targets, in `franz/linux_x86_64/`) is ~700 lines.

## What works

The library loads completely on every startup (~1 second). What you
can use:

- All core forms: `def`, `defun`, `defmacro`, `cond`, `let`, `prog`,
  `setq`, `quote`, `function`, `lambda`, `mapc`, `mapcar`, `apply`,
  …
- Arithmetic: fixnum and bignum (`+ - times quotient expt` …), and
  floating point.
- Lists: `cons`, `car`, `cdr`, `caar`, …, `assoc`, `member`, `memq`,
  `reverse`, `length`, `append`, `nth`, `last`.
- I/O: `princ`, `prin1`, `terpri`, `read`, `print`, `format`, file
  ports via `infile` / `outfile`.
- The break loop with `(reset)` to escape, `(continue)` to retry.
- Defstruct, the CMU file package, the CMU top-level, flavors.

## The compiler

`bin/liszt foo.l` compiles a Lisp file to a shared object `foo.so`.
Load it from `bin/lisp` with `(cfasl …)`. The pipeline:

```sh
$ cat > add2.l <<'EOF'
(defun add2 (a b) (+ a b))
EOF
$ bin/liszt add2.l                  # -> add2.so
$ bin/lisp
-> (cfasl 'add2.so 'init 'add2-init "subroutine" "")
-> (add2-init)                       ; runs the per-file init
-> (add2 10 32)                      ; calls the compiled function
42
```

See [CompilerPlan.md](CompilerPlan.md) for the design (Lisp-to-C
back-end, `dlopen` runtime loader). Performance is roughly 2-4× the
interpreted version on a recursive `fib` benchmark.

## What doesn't work

- **`(dumplisp 'name)`** — writing the running Lisp image to disk as
  a fast-start binary. Not implemented; loading the library from
  source at startup is fast enough.
- **`(showstack)`**, **`(baktrace)`** — these walk the C call stack;
  they would need a stack unwinder such as libunwind.
- **`ffasl`**, the foreign-function fasl that loads raw `.o` files of
  compiled C extensions. Different from `cfasl` (the Lisp-to-C
  compile path), which works.

## Troubleshooting

**"undefined reference to `tgoto'"** at link time → install ncurses-
devel (or libncurses-dev). The build links `-ltermcap`, which on
modern systems comes from ncurses.

**"`<UNBOUND>`" appearing where you expected `nil`** → that's the
print form of an unbound atom. Often it just means a function
returned no useful value (like `(setq x 5)` evaluating to t but
something one step up didn't). Not an error.

**Banner prints twice** → you're feeding `bin/lisp` an input that
re-triggers init. Don't run `bin/lisp` inside another `bin/lisp`
session via stdin redirection.

**Library doesn't load (`Unbound Variable: ;;`)** → you ran `rawlisp`
directly without `init.l`. Use `bin/lisp` instead, or first evaluate
`(int:setsyntax '\; 'splicing 'zapline)` so `;` is the comment macro.

**Crashes on first allocation** → if you've changed `TTSIZE`,
`LBPG`, or any of the `*SPP` constants in `franz/h/`, recompile
cleanly: `make clean && make rawlisp`. The bitmap and typetable sizes
need to match across compilation units.
