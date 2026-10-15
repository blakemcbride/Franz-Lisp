# KNOWN-BUGS.md

Known gaps and bugs in the interpreter and compiler that aren't fixed
yet.

## `(showstack)` / `(baktrace)` are stubs

By design, see `arch.c`. Would need libunwind to implement
properly.

## `dumplisp` / `(ffasl)` not implemented

By design, see PortPlan.md and CompilerPlan.md. The
Lisp-to-C compiler path (`liszt` + `cfasl`) does work; the gap is
loading raw compiled-C `.o` files (foreign-function fasl), not
compiled Lisp.
