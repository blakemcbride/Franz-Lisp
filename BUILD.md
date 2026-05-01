# BUILD.md

How to build and run the Linux x86_64 port of Franz Lisp from this
source tree.

## Quick start

```sh
./lispconf linux_x86_64
make rawlisp
bin/lisp
```

Type `(+ 1 2)` and you should get back `3`.

## Prerequisites

The port targets Linux x86_64 with the GNU toolchain. You need:

- A C compiler that accepts `-std=gnu89`. gcc 4 and later all work;
  this tree was developed against gcc 15. Clang should also work but
  hasn't been tested.
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

The tree is configured for a single platform; the configuration file
`franz/h/lconf.h` is regenerated from scratch each time:

```sh
./lispconf linux_x86_64
```

Then build the kernel and link the bare interpreter:

```sh
make rawlisp
```

You'll see `cc -c ...` lines for each kernel source, then a final
`cc ... -o rawlisp ...` link step. The result is
`franz/linux_x86_64/rawlisp` — about 540 KB, dynamically linked.

Other useful targets:

| Target | What it does |
|---|---|
| `make smoke` | Compile a single kernel `.c` file. Fast sanity check. |
| `make kernel` | Compile every kernel source. Stops short of linking. |
| `make rawlisp` | Compile + link the bare interpreter. |
| `make clean` | Remove `.o` files and the binary. |

`make` from the top-level dispatches into `franz/linux_x86_64/Makefile`,
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
-> [load /…/lisplib/common0.l]
[load /…/lisplib/syntax.l]
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
debugging), invoke `franz/linux_x86_64/rawlisp` straight. You'll
have only the kernel built-ins (`def`, `eq`, `cons`, `mapcar`, `cond`,
arithmetic, the break loop). `defun`, `princ`, `reverse`, etc. are
library-only and won't be defined.

## What gets built

After `make rawlisp`:

```
franz/*.o                        37 machine-independent kernel objects
franz/linux_x86_64/callg.o       x86_64 ABI varargs trampoline
franz/linux_x86_64/bignum.o      bignum kernels (multiply, divide, etc.)
franz/linux_x86_64/arch.o        small arch helpers, frame primitives
franz/linux_x86_64/rawlisp       the linked interpreter
```

The kernel comes to ~20k lines of C across `franz/`; the arch-specific
glue under `franz/linux_x86_64/` is ~700 lines.

## What works

The library loads completely on every startup (~1 second). What you
can use:

- All core forms: `def`, `defun`, `defmacro`, `cond`, `let`, `prog`,
  `setq`, `quote`, `function`, `lambda`, `mapc`, `mapcar`, `apply`,
  …
- Arithmetic: fixnum and bignum on `+ - times //`. Bignum products
  fitting in 60 bits work (`(times 1000000 1000000)` is fine).
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
  a fast-start binary. Not implemented; the launcher's source-load
  approach is fast enough that this hasn't been missed.
- **`(showstack)`**, **`(baktrace)`** — these stack-walked the C
  call frames using i386 layout assumptions; they'd need libunwind
  on x86_64.
- **`ffasl`**, the foreign-function fasl that loaded raw `.o`
  files of compiled C extensions. Different from `cfasl` (the
  Lisp-to-C compile path), which works.
- **Bignum products beyond 60 bits** wrap or hang. The bignum kernel
  uses 30-bit halfwords (preserved from the original to minimize
  port risk); a 32-bit-fixnum × 32-bit-fixnum product can overflow
  the two-halfword temporary in `Ltimes`. Phase 1h (described in
  PortPlan.md) would widen halfwords to 60 bits to fix this.

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

**Crashes on first allocation** → if you've changed `OFFSET`,
`TTSIZE`, `LBPG`, or any of the `*SPP` constants in `franz/h/`,
recompile cleanly: `make clean && make rawlisp`. The bitmap and
typetable sizes need to match across compilation units.
