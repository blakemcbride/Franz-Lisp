# macOS.md

Building and running Franz Lisp on macOS. The full system works: the
interpreter, the library, the liszt compiler, and `cfasl` loading of
compiled code.

Tested on: macOS 27.0.1, Apple Silicon (arm64), Apple clang 21.0.0,
the stock `/usr/bin/make` (GNU make 3.81). Intel Macs should work the
same way but haven't been tested.

## Quick start

```sh
xcode-select --install          # once: clang, make, libtermcap
./lispconf                      # detects macos_arm64 (or macos_x86_64)
make rawlisp
bin/lisp
```

Type `(+ 1 2)` and you get back `3`.

`./lispconf` with no argument detects the host; `./lispconf macos_arm64`
names it explicitly, and `./lispconf -l` lists all targets. It writes
`#define macos_arm64 1` into `franz/h/lconf.h`, and the Mac build goes
in `franz/macos_arm64/` (Intel: `franz/macos_x86_64/`). The binary is
built for the host CPU: `file franz/macos_arm64/rawlisp` reports
`Mach-O 64-bit executable arm64`.

Linux and macOS share one 64-bit configuration (the kernel's internal
`unix64` macro) and one set of per-arch sources, which live in
`franz/linux_x86_64/` (also the Linux x86_64 build directory).
`franz/macos_arm64/` holds only a short Makefile that compiles
those shared sources, plus the Mac's objects and `rawlisp`. The shared
Makefile checks `uname -s` and switches to clang-friendly flags on
Darwin, so no extra options are needed.

## Verify

```sh
bin/test             # interpreted regression suite: 142 passed
bin/test-compiled    # same suite compiled with liszt: 142 passed
```

Compiler round trip:

```sh
$ echo '(defun add2 (a b) (+ a b))' > add2.l
$ bin/liszt add2.l                   # -> add2.so (a Mach-O dylib)
$ bin/lisp
-> (cfasl 'add2.so 'init 'add2-init "subroutine" "")
-> (add2-init)
-> (add2 10 32)
42
```

## Prerequisites

Only the Xcode Command Line Tools. They provide `cc` (clang), `make`,
and `libtermcap` (via the system ncurses), which the Makefile's
termcap probe finds automatically. Homebrew isn't needed. Rosetta
isn't needed either; the build is native arm64.

## How the macOS build differs

All of this is handled automatically. It's listed here for anyone
touching the build.

- **Compiler flags** (`franz/linux_x86_64/Makefile`, the shared rules
  that `franz/macos_arm64/Makefile` includes). clang makes
  several K&R-era diagnostics hard errors that gcc only warns about.
  On Darwin the Makefile adds `-Wno-return-mismatch`,
  `-Wno-incompatible-function-pointer-types` and
  `-Wno-deprecated-non-prototype`, and drops the gcc-only
  `-Wno-builtin-declaration-mismatch`. It also drops `-no-pie -fno-pic`,
  because Apple's toolchain always links PIE, and adds `-fno-common`
  (gcc's default since version 10), which keeps `ld` from complaining
  about the alignment of the `__common` section.
- **Compiled-Lisp shared objects** (`lisplib/lisztcc`). On Darwin
  these are built with `-dynamiclib -undefined dynamic_lookup`, so
  kernel symbols (`eval`, `inewint`, ...) resolve against `rawlisp`
  when `cfasl` `dlopen`s them. The output is still named `foo.so`.
- **No warnings.** `make rawlisp` and `bin/liszt` both build without
  compiler or linker warnings. The flags above only switch off K&R-era
  style diagnostics, so a new warning deserves a look.

## Portability notes

macOS on Apple Silicon is stricter than Linux on x86_64 in ways that
matter to this K&R-era code. Code that works on both follows these
rules:

- **Pointers don't fit in an `int`.** Every macOS address is above
  4 GB. An `(int)` cast truncates a pointer, and so does calling an
  undeclared function that returns a pointer or `long`, because its
  result is taken as `int`. Kernel functions like that are declared in
  `franz/h/dfuncs.h`.
- **Return values must be returned.** A function that computes its
  result and falls off the end without `return` hands back garbage on
  arm64.
- **Variadic functions need prototypes.** Apple's arm64 ABI passes
  variadic arguments on the stack, so calling `execl`, `printf` and the
  like without a prototype passes them in the wrong place.
- **Integer division by zero doesn't trap.** arm64 returns 0 instead of
  raising SIGFPE, so the fixnum division functions check for zero
  explicitly.
- **Use ISO C formats.** `%ld` and `%p` for `long`s and pointers in
  `printf`; `%lf` to `scanf` a `double` (macOS reads BSD's `%F` as a
  `float`).
- **Sizes are in `long`s and bytes.** In the allocator, `type_len`
  counts 8-byte slots and pages are `LBPG` (1024) bytes; nothing may
  assume 4-byte words or 512-byte pages.

## Debugging tips

- **Get a C backtrace from a Lisp-level segfault.** The kernel turns
  `SIGSEGV`/`SIGBUS` into the "Internal bad memory reference" break.
  Evaluate `(sstatus dumpcore t)` first so the signal goes through,
  and macOS writes a symbolized crash report to
  `~/Library/Logs/DiagnosticReports/rawlisp-*.ips`.
- `lldb` needs Developer Mode (or a debug entitlement) to attach.
  Without it, `lldb --batch` can hang at `process launch`.
- After editing anything in `franz/h/`, `make rawlisp` rebuilds
  every object. The Makefile makes all objects depend on all headers.
  Stale objects built against old struct sizes fail in confusing ways.

## Known limitations

These are the same as on Linux (see [KNOWN-BUGS.md](KNOWN-BUGS.md)):
no `dumplisp`, no `ffasl`, and `showstack`/`baktrace` are stubs.
Specific to arm64:

- Foreign-function calls (`Ifcall`/`callg_`) into **variadic** C
  functions such as `printf` don't work on Apple arm64. The trampoline
  passes every argument in registers, but Apple's ABI expects variadic
  arguments on the stack. Fixed-argument C functions are fine.
