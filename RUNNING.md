# RUNNING.md

How to use the interpreter and compiler. Assumes you've already
built the system per [BUILD.md](BUILD.md) — i.e. you have working
`bin/lisp` and `bin/liszt` launchers.

## The interpreter

```sh
$ bin/lisp
Franz Lisp, Opus 38
-> [load /…/lisplib/common0.l]
…
->
```

The `->` is the prompt. Type any Lisp form, press Enter, and the
result is printed. Ctrl-D (EOF) or `(exit)` quits. After an error
you'll see a `1:>` prompt (a *break loop* that lets you inspect or
recover from the error). Type `(reset)` to abort back to the top
level, or `(continue)` to retry the failing form.

### Defining and calling

```lisp
-> (defun fact (n)
     (cond ((zerop n) 1)
           (t (* n (fact (sub1 n))))))
fact
-> (fact 10)
3628800
```

`defun` defines a function; the function is then callable like any
built-in. `cond` is the conditional (`if` is also available, defined
as a macro by the library). `t` is true; `nil` is false (empty list).

### Macros

```lisp
-> (defmacro inc (x) (list 'setq x (list 'add1 x)))
inc
-> (setq counter 5)
5
-> (inc counter)
6
-> counter
6
```

### Lists and higher-order

```lisp
-> (cons 1 (cons 2 (cons 3 nil)))
(1 2 3)
-> '(1 2 3)                  ; quote == cons-cons-cons
(1 2 3)
-> (mapcar 'add1 '(10 20 30))
(11 21 31)
-> (reverse '(a b c d))
(d c b a)
-> (length '(a b c d e))
5
-> (assoc 'b '((a 1) (b 2) (c 3)))
(b 2)
```

### Arithmetic

```lisp
-> (+ 1 2 3 4 5)
15
-> (* 6 7)
42
-> (expt 2 30)
1073741824
-> (times 1000000 1000000)
1000000000000
```

`+ - *` and `times` work for fixnums (≤ 30 bits); `times` chains
to bignums automatically when results overflow. Bignum products
beyond 60 bits currently wrap or hang (see BUILD.md "What doesn't
work").

### Loading source from a file

```lisp
-> (load 'foo)              ; tries foo.o (compiled) then foo.l
-> (load '/abs/path/to/foo.l)
```

Without an extension, `load` first looks for a compiled `.o` then a
source `.l` in the current directory and along
`lisp-library-directory`. Always-source-loading the standard library
on every startup is built into `bin/lisp`.

### Reading files for I/O

```lisp
-> (setq p (infile 'foo.txt))
-> (read p)
-> (close p)

-> (setq p (outfile 'bar.txt "w"))
-> (princ "hello" p) (terpri p)
-> (close p)
```

### Errors and the break loop

```lisp
-> (car 'not-a-list)
+: non list arg to car
1:> 'not-a-list                  ; you can inspect things
not-a-list
1:> (reset)                       ; bail out
->
```

### Quitting

```lisp
-> (exit)
$
```

Or Ctrl-D on a fresh prompt.

## The compiler

`bin/liszt` compiles a Lisp file into a shared object that can be
loaded back into the interpreter for native-code performance.

### Compiling

```sh
$ cat > /tmp/double.l <<'EOF'
(defun double (n) (+ n n))
(defun quad   (n) (double (double n)))
EOF
$ bin/liszt /tmp/double.l
…
%Note: double.l: Compilation complete
%Note: double.l: Assembly completed successfully

$ ls /tmp/double.*
/tmp/double.l   /tmp/double.so
```

`bin/liszt foo.l` produces `foo.so` in the same directory. The `.l`
extension is optional; `bin/liszt foo` works too.

If you want to keep just the intermediate C without the `.so`, use
`-S`:

```sh
$ bin/liszt -S /tmp/double.l
$ ls /tmp/double.c                 # .c kept; no .so produced
```

### Loading and using compiled code

From inside `bin/lisp`:

```lisp
-> (cfasl '/tmp/double.so 'init 'init-double "subroutine" "")
t
-> (init-double)             ; runs the per-file init, defines double + quad
t
-> (double 21)
42
-> (quad 5)
20
```

The five arguments to `cfasl` are:

| arg | meaning |
|---|---|
| `'/tmp/double.so` | path to the shared object |
| `'init` | C symbol to look up (always `init` for liszt output) |
| `'init-double` | a Lisp atom you want bound to the dlsym'd address |
| `"subroutine"` | calling-convention discipline |
| `""` | discipline argument (unused for `subroutine`) |

After `(init-double)` runs, **the symbols defined in double.l are
bound globally** — the compiled `double` and `quad` are now first-
class Lisp functions. You can `(getd 'double)` to see the binding
(prints as `#XXXX-lambda`, indicating a BCD with lambda discipline).

### A complete example

```sh
$ cat > /tmp/fib.l <<'EOF'
(defun fib (n)
  (cond ((lessp n 2) n)
        (t (+ (fib (sub1 n)) (fib (- n 2))))))
EOF

$ bin/liszt /tmp/fib.l                      # compile
$ bin/lisp <<'EOF'                           # run both versions
(load '/tmp/fib.l)
(setq t0 (ptime)) (fib 32) (setq t1 (ptime))
(princ "interpreted: ") (princ (- (car t1) (car t0))) (princ " jiffies")
(terpri)

(cfasl '/tmp/fib.so 'init 'init-fib "subroutine" "")
(init-fib)
(setq t0 (ptime)) (fib 32) (setq t1 (ptime))
(princ "compiled:    ") (princ (- (car t1) (car t0))) (princ " jiffies")
(terpri)
EOF
```

Typical output (60 jiffies = 1 second):

```
interpreted: 16 jiffies
compiled:    4 jiffies
```

### Recompiling and reloading

If you edit `foo.l`, recompile with `bin/liszt foo.l` and run
`(cfasl …)` + `(init-foo)` again from a fresh `bin/lisp`. Each
`cfasl` `dlopen`s the `.so` once; reloading without a fresh `bin/lisp`
session works but the previous compiled definitions stay bound to
the old `.so`'s memory.

## Useful environment variables

| | |
|---|---|
| `LISPLIB` | path to the Lisp library directory. Defaults to the `lisplib/` of the source tree the launcher lives in. |
| `FRANZ_ROOT` | source-tree root. The launchers infer this from their own location, but you can override (useful for installed builds). |
| `CLISZT` | source location for the compiler (`bin/liszt` only). Defaults to `$FRANZ_ROOT/cliszt`. |
| `CC` | C compiler used by `lisplib/lisztcc`. Defaults to `cc`. Set to `clang` if you want clang. |
| `TMPDIR` | temp dir liszt uses for intermediate `.c` files. Defaults to `/tmp`. |

## What's not in this guide

- The compiler's command-line flags (`-S`, `-Q`, `-w`, `-x`, `-i`,
  …): see `man/liszt.1` or run `bin/liszt -h`. The doc set in
  `doc/Franz_Lisp_July_1983.pdf` covers the language at length.
- Tracing, the stepper, defstruct, flavors, the CMU file
  package: all loaded as part of `bin/lisp` startup but not
  documented here. The classic Franz Lisp manual chapters
  (`doc/ch11.n` "trace package", `ch13.n` "CMU top level",
  `ch14.n` "stepper", `ch16.n` "lisp editor") are the original
  references.
