# reference/

Source files from the original i386 BSD distribution, kept as
**reference for maintaining the 64-bit port**. Nothing in this
directory is part of the build, and the top-level Makefile does not
look here.

  - `i386/callg.c` — the varargs trampoline. The 64-bit
    `franz/linux_x86_64/callg.c` is a different design (the x86_64 and
    arm64 ABIs pass arguments in registers), but this shows what the
    trampoline has to do.
  - `i386/i386.c` — small machine-specific helpers (`mmuladd`,
    `Imuldiv`, `Lrot`, etc.), mostly ordinary C.
  - `i386/{adbig,calqhat,dodiv,dsmult,dmlad,mlsb,mulbig,qfuncl,exarith}.c`
    — bignum primitives, which treat bignums as arrays of 30-bit
    halfwords. Useful next to `franz/linux_x86_64/bignum.c`, which
    implements the same algorithms.
  - `i386/{inewint,nargs}.c` — short, mostly portable.
  - `i386/clinkfns.c` — C-link runtime used by liszt's compile-to-C
    path; compare `franz/linux_x86_64/clinkfns.c`.
  - `i386/{prunei,emulgcc}.c` — image pruning, gcc-flavored 32-bit
    multiply emulation.
  - `cliszt-i386/` — the Lisp source of the in-C fasl loader
    (`in-c-fasl.l`, `zfasl.l`, `new-fasl.l`, `better-zfasl.l`) and
    its C-side support (`cinit.c`, `clink.c`, `clinkfns.c`,
    `compiled.h`).

Not included: the i386 assembly files (`emul.s`, `ediv.s`,
`lowaux.s`, ...), `franz/i386/malloc.c`, scratch and test files, and
the vax/tahoe/68k trees. `git show ae31d01:<path>` retrieves any of
them from the initial commit.
