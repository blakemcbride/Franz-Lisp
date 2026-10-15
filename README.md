# Franz Lisp on Linux and macOS

Franz Lisp (Opus 38.92), the Berkeley dialect of Lisp from the early
1980s, for 64-bit Linux and macOS.

Project home: <https://github.com/blakemcbride/Franz-Lisp>

## What this is

Franz Lisp was developed at UC Berkeley beginning in the late 1970s 
as a Maclisp-influenced Lisp for Unix/VAX systems, and was used to 
run the VAX port of Macsyma, whose original development at MIT had 
begun earlier in Maclisp.  Its
distribution targeted vax/tahoe/68k/i386 BSD with K&R-era C, BSD
assembler conventions, a.out object format, and gcc-i386-specific
varargs trickery. By the mid-90s it had stopped being actively
maintained and the i386 port could no longer be built on contemporary
systems.

This tree builds and runs Franz Lisp on 64-bit Linux (x86_64) and
macOS (Apple Silicon): the interpreter, the standard library, and the
Lisp-to-C compiler.

The machine-dependent parts are portable C:

- the foreign-function trampoline (`callg.c`)
- the bignum kernels, using `long long` arithmetic
- the heap allocator, backed by a single `mmap`'d region
- runtime-relocatable type tagging (the heap can live anywhere in the
  address space)

That arch-specific code, under `franz/linux_x86_64/`, is about 700
lines. The ~20k-line machine-independent kernel is the Berkeley code,
with the types and size calculations it needs to run with 64-bit
pointers. [PortPlan.md](PortPlan.md) records how the port was done.

## What works

- The standard library loads completely on every startup.
- `def`, `defun`, `defmacro`, `cond`, `let`, `prog`, `setq`, `quote`,
  `lambda`, the full Lisp form set.
- Lists, equality (`eq`/`equal`), recursion, higher-order
  (`mapc`/`mapcar`/`apply`).
- Fixnum and bignum arithmetic. Bignum products up to 60 bits.
- I/O: `princ`, `read`, `print`, `format`, file ports.
- The break loop with `(reset)` and `(continue)`.
- Defstruct, the CMU file package, the CMU top-level, flavors.
- **The compiler.** `bin/liszt foo.l` produces `foo.so` (a shared
  object). `(cfasl 'foo.so 'init …)` from `bin/lisp` loads it into
  the running image; the compiled functions then dispatch as
  ordinary Lisp atoms with measurable speedup over interpreted code
  (2-4× on a recursive-fib microbenchmark). See
  [CompilerPlan.md](CompilerPlan.md) for the design and the
  `dlopen`-based runtime loader.

## What doesn't

Not implemented; none of these is needed for normal use:

- **`dumplisp`**, image-save. Not needed because library loading is
  fast enough.
- **`(showstack)` / `(baktrace)`**, which walk the C call stack; they
  would need a stack unwinder such as libunwind.
- **`ffasl`**, the *foreign-function* fasl that loads raw `.o` files
  of compiled C extensions (a BSD a.out format). The Lisp-to-C
  compiler path (`liszt` + `cfasl`) does work.

## Project layout

```
franz/                C kernel source. ~20k lines, mostly preserved.
franz/h/              Shared headers. config.h, global.h, ltypes.h.
franz/linux_x86_64/   Per-arch sources shared by all targets (callg,
                      bignum, arch, clinkfns) and the shared build
                      rules; the Linux x86_64 build goes here.
franz/macos_arm64/    Build directory for macOS on Apple Silicon (also
                      macos_x86_64/, linux_arm64/): a short Makefile,
                      plus that target's objects and rawlisp.
lisplib/              Lisp source for the standard library, plus
                      init.l (loaded by bin/lisp at startup).
bin/lisp              Launcher. Runs rawlisp with FRANZ_INIT=init.l.
doc/                  The 1983 manual (troff source).
man/                  Unix man pages: lisp(1), liszt(1), lxref(1).
reference/            Inert reference: original i386 sources kept for
                      cross-checking the port. Not built.

BUILD.md              Build instructions and troubleshooting.
RUNNING.md            How to use the interpreter and compiler.
PortPlan.md           The interpreter porting plan, by phase.
CompilerPlan.md       The compiler porting plan, by phase.
KNOWN-BUGS.md         Catalog of known issues and workarounds.
CLAUDE.md             Working notes for AI-assisted development.
```

## Issues and contributions

Bug reports, patches, and questions go through the project's GitHub
page: <https://github.com/blakemcbride/Franz-Lisp/issues>.

## History and credit

- The original Franz Lisp was developed at UC Berkeley by John
  Foderaro, Keith Sklower, Kevin Layer, and others. Opus 38.92
  released ~1991.
- Compile-to-C compiler and the NetBSD-on-i386 port were by Jeff
  Dalton at the AI Applications Institute, University of Edinburgh,
  in 1992-1994.
- 64-bit Linux and macOS port (2026): Blake McBride, with Claude
  (Anthropic) doing most of the porting work in dialogue.

## License

BSD 4-clause (Regents of the University of California). See
[COPYRIGHT](COPYRIGHT) and [COPYING](COPYING).

The 4-clause "advertising" form means redistributions in binary form
must reproduce the copyright acknowledgement; check the exact text
before incorporating into another project.

## Quick start

```sh
git clone https://github.com/blakemcbride/Franz-Lisp.git
cd Franz-Lisp

sudo dnf install gcc make glibc-devel ncurses-devel    # Fedora/RHEL
# or:
sudo apt install build-essential libncurses-dev        # Debian/Ubuntu
# or, on macOS:
xcode-select --install

./lispconf          # detects the host (./lispconf -l lists targets)
make rawlisp
bin/lisp
```

macOS specifics are in [macOS.md](macOS.md).

Full build instructions, including troubleshooting, in
[BUILD.md](BUILD.md).

## Example session

```
$ bin/lisp
Franz Lisp, Opus 38
[load /…/lisplib/common2.l]
… (the full library loads in about a second)
-> (defun fact (n) (cond ((zerop n) 1) (t (* n (fact (sub1 n))))))
fact
-> (fact 10)
3628800
-> (defmacro inc (x) (list 'setq x (list 'add1 x)))
inc
-> (setq counter 5) (inc counter) (inc counter) counter
7
-> (mapcar 'add1 '(10 20 30))
(11 21 31)
```

Compiling a Lisp file to native code:

```
$ cat fib.l
(defun fib (n)
  (cond ((lessp n 2) n)
        (t (+ (fib (sub1 n)) (fib (- n 2))))))

$ bin/liszt fib.l                    # produces fib.so
%Note: fib.l: Compilation complete
%Note: fib.l: Assembly completed successfully

$ bin/lisp
-> (cfasl 'fib.so 'init 'init-fib "subroutine" "")
t
-> (init-fib)        ; runs the .so's per-file init, defines fib
t
-> (fib 30)          ; now runs the compiled code
832040
```
