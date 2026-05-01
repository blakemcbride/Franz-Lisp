(int:setsyntax '\; 'splicing 'zapline)
;; lisplib/liszt-init.l -- bootstrap script for the Lisp compiler.
;;
;; Sequence:
;;   1. Sets `;` as the comment macro.
;;   2. Loads the standard library via buildlisp.l (same path bin/lisp
;;      uses).
;;   3. Selects the C back-end (#+for-c features) before any compiler
;;      source is read, so the conditional reader macros pick the right
;;      branch.
;;   4. Pre-loads cliszt/{const,cmacros,chead}.l so the (include-if
;;      ...) and (eval-when ... (load '../foo.l)) lines in the rest of
;;      the compiler source see them already loaded -- avoids depending
;;      on a working directory under cliszt/ that doesn't exist in our
;;      tree.
;;   5. Loads the rest of the compiler in the order from cliszt/cmake.l.
;;
;; tlev.l sets `user-top-level` to `lisztinit` at load time. When control
;; returns to the toplevel loop, lisztinit checks command-line args:
;;   - if there are args, it compiles them and exits;
;;   - otherwise it prints the banner and falls through to a normal
;;     interactive Lisp prompt with the compiler loaded.

;; Standard library first (defun, defmacro, mapcar, the toplevel,
;; command-line-args, etc.).
(setq build:load t)
(setq build:lisp-type 'franz)

(cond
  ((getenv '|LISPLIB|)
   (setq build:dir (getenv '|LISPLIB|))
   (setq lisp-library-directory (getenv '|LISPLIB|)))
  (t
   (setq build:dir '"/usr/local/lib/lisp")
   (setq lisp-library-directory '"/usr/local/lib/lisp")))

(setq lisp-object-directory build:dir)

(load (concat build:dir "/buildlisp"))


;; Now switch to back-end selection and load the compiler. The reader
;; conditional `#+for-c` is keyed on the `for-c` feature being on AT
;; READ TIME, so set it before loading any cliszt file.
(sstatus feature for-c)
(sstatus nofeature for-vax)
(sstatus nofeature for-68k)
(sstatus nofeature for-tahoe)

;; Compiler source location. CLISZT env var overrides; otherwise infer
;; from FRANZ_ROOT or fall back to a sibling of $LISPLIB.
(setq liszt-src-dir
      (cond
        ((getenv '|CLISZT|)        (getenv '|CLISZT|))
        ((getenv '|FRANZ_ROOT|)    (concat (getenv '|FRANZ_ROOT|) "/cliszt"))
        (t                         (concat build:dir "/../cliszt"))))

;; Pre-load const, cmacros, chead. The cliszt body files reference
;; them via (eval-when ... (load '../foo.l)) with paths relative to
;; CWD; pre-loading them sets the loaded/version flags so those guarded
;; loads are skipped, sidestepping the path issue.
(load (concat liszt-src-dir "/const"))
(load (concat liszt-src-dir "/cmacros"))
(load (concat liszt-src-dir "/chead"))

;; Body files in the order from cliszt/cmake.l.
(mapc '(lambda (name)
         (load (concat liszt-src-dir "/" name)))
      '(decl array vector datab expr io funa funb func tlev
        instr fixnum util lversion))

;; A few features the compiler queries during runtime decisions.
(sstatus feature complr)
;; "in-c" tells the compile path "we are emitting C" (separate from
;; #+for-c which selects the back-end at read time). Without this,
;; liszt's runtime path fl-asm gets cleared and it tries to write
;; the .c straight to the project tree instead of a tempfile.
(sstatus feature in-c)
;; Don't keep np/lbot in dedicated registers (we don't on x86_64).
(setq $global-reg$ nil)
(sstatus feature no-global-reg)
