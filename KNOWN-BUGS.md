# KNOWN-BUGS.md

Bugs surfaced by the test suite or by manual exercise of the
interpreter that are real (not test-expectation issues) but were not
on the critical path for the port. Documented here so they're not
forgotten.

## Bignum literal reader: hangs / xstack overflow on 10+ digit numbers

```
$ echo "(princ 9999999999) (terpri)" | bin/lisp
... -> Ran out of alternate stack
```

```
$ echo "(princ 1000000000000) (terpri)" | bin/lisp
(hangs indefinitely)
```

Reading numeric literals that exceed the fixnum range
(`MaxINT 0x3fffffff` ≈ 1.07 billion) goes through `calcnum` →
`dmlad` to build an SDOT chain, one digit at a time. Somewhere in
that path the Lisp argument stack (`xstack[]` in `data.c`, 16384
longs) is being exhausted, or `dmlad` enters an infinite loop on
two-or-more-cell bignums.

**Workaround:** use `(times A B)` to *construct* bignum values at
runtime instead of writing them as literals. `(times 1000000
1000000)` works; `1000000000000` as a source-code literal does not.

**Fix:** likely a bug in `bignum.c`'s `dmlad` or `calcnum`'s loop
exit condition. Or possibly the `emul`/`ediv` width fix from
Phase 3 left some other path stale. See PortPlan.md Phase 1h.

## Bignums corrupted across multiple allocations

```
$ echo "(setq a (times 1000000 1000000)) \
        (setq b (times 1000000 1000000)) \
        (princ a) (terpri) (princ b) (terpri)" | bin/lisp
141728898420736
... (b prints something else)
```

A single `(times 1000000 1000000)` at the top level prints the
correct `1000000000000`. But if you hold the result in a variable
and then perform another bignum-producing operation, the first
result becomes corrupted. Likely cause: GC during the second
allocation walks the SDOT pages and misclassifies / overwrites
something.

This is consistent with the `markdp` / TYPE() bounds-check work
done in Phase 3 *probably* not catching every callsite, or with
the CDR field of struct sdot getting trampled by the free-list link
in some corner case.

**Workaround:** don't depend on bignum equality across multiple
calls. Comparing a bignum result to a fixnum (e.g.
`(equal (times 1000 1000000) 1000000000)`) does work because the
result fits in a fixnum.

**Fix:** instrument the GC sweep over SDOT pages and find what
gets corrupted. Alternatively, the broader Phase 1h bignum-redesign
(60-bit halfwords in 64-bit longs) would replace the SDOT chain
with a more conventional layout and likely sidestep this.

## `abs`, `minusp` broken on negatives

```
$ echo "(abs -7) (minusp -3)" | bin/lisp
-7        ; should be 7
nil       ; should be t
```

Probably a sign-handling issue in the kernel C code -- both
involve checking the sign bit of a fixnum's `i` field. The boxing
through `inewint` widened to `long` correctly but somewhere the
sign comparison happens through a 32-bit path.

**Fix:** trace `Labs` and `Lminusp` in `franz/lam4.c` (or wherever
they live) and look for `(int)` casts where the value is the boxed
integer's `i` field.

## `(showstack)` / `(baktrace)` are stubs

By design, see `arch.c`. Would need libunwind to implement
properly.

## `dumplisp` / `(ffasl)` not implemented

By design, see PortPlan.md and CompilerPlan.md. The
Lisp-to-C compiler path (`liszt` + `cfasl`) does work; the gap is
loading raw compiled-C `.o` files (foreign-function fasl), not
compiled Lisp.
