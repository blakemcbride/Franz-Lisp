# reference/

Source files preserved from the original i386 BSD distribution as
**reference for porting work**. Nothing in this directory is part of
the build, and the top-level Makefile does not look here.

These files were initially deleted in the Phase 0 cleanup commit
(see git log) and restored once it was clear that several would
inform x86_64 ports later in the plan:

  - `i386/callg.c` — the varargs trampoline (Phase 1g, will need a
    full x86_64 ABI rewrite, but this shows the shape of what's
    required)
  - `i386/i386.c` — small machine-specific helpers (`mmuladd`,
    `Imuldiv`, `Lrot`, etc.) most of which are ordinary C and port
    almost as-is
  - `i386/{adbig,calqhat,dodiv,dsmult,dmlad,mlsb,mulbig,qfuncl,exarith}.c`
    — bignum primitives. C source treats bignums as arrays of 16-bit
    halfwords held in 32-bit ints. On x86_64 the natural redesign
    uses 32-bit halfwords in 64-bit ints (or `__int128`); having the
    original algorithms next to the new code is invaluable.
  - `i386/{inewint,nargs}.c` — short, mostly portable
  - `i386/clinkfns.c` — C-link runtime (used by liszt's compile-to-C
    path; relevant in Phase 6+)
  - `i386/{prunei,emulgcc}.c` — image pruning, gcc-flavored 32-bit
    multiply emulation
  - `cliszt-i386/` — the Lisp source of the in-C fasl loader
    (`in-c-fasl.l`, `zfasl.l`, `new-fasl.l`, `better-zfasl.l`) and
    its C-side support (`cinit.c`, `clink.c`, `clinkfns.c`,
    `compiled.h`). Reference for Phase 6+.

What was **deliberately not restored**:

  - `*.s` assembly files (`emul.s`, `ediv.s`, `lowaux.s`, etc.) —
    i386 32-bit asm is not informative for x86_64.
  - `franz/i386/malloc.c` — Linux ships a perfectly good malloc.
  - `franz/i386/foo.c`, `temp.c`, `testemul.c`, `callg-test.c`,
    `rmt-emul.c` — scratch / test files.
  - vax/tahoe/68k trees — the i386 versions are the right starting
    point for x86_64; older arch sources just add noise.

If you need any of those, `git show ae31d01:<path>` will retrieve
them from the initial commit.
