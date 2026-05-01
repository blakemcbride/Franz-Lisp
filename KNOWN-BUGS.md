# KNOWN-BUGS.md

Bugs surfaced by the test suite or by manual exercise of the
interpreter that are real (not test-expectation issues) but were not
on the critical path for the port. Documented here so they're not
forgotten.

## `(showstack)` / `(baktrace)` are stubs

By design, see `arch.c`. Would need libunwind to implement
properly.

## `dumplisp` / `(ffasl)` not implemented

By design, see PortPlan.md and CompilerPlan.md. The
Lisp-to-C compiler path (`liszt` + `cfasl`) does work; the gap is
loading raw compiled-C `.o` files (foreign-function fasl), not
compiled Lisp.
