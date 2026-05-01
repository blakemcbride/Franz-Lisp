(int:setsyntax '\; 'splicing 'zapline)
;; init.l -- bootstrap script fed into rawlisp by the `lisp` wrapper
;; (bin/lisp) before stdin. Sets the build:* variables that buildlisp.l
;; needs and triggers the library load.
;;
;; The original Franz Lisp captured this state via dumplisp(); we don't
;; have dumplisp on x86_64 (PortPlan.md) so every start re-loads from
;; .l source. Cost: ~roughly a fraction of a second.

(setq build:load t)
(setq build:lisp-type 'franz)

;; LISPLIB env var overrides the path; default is the build-time install
;; root. The lisp wrapper exports it.
(cond
  ((getenv '|LISPLIB|)
   (setq build:dir (getenv '|LISPLIB|))
   (setq lisp-library-directory (getenv '|LISPLIB|)))
  (t
   (setq build:dir '"/usr/local/lib/lisp")
   (setq lisp-library-directory '"/usr/local/lib/lisp")))

(setq lisp-object-directory build:dir)

(load (concat build:dir "/buildlisp"))
