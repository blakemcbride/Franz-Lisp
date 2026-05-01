;;; tests/test-all.l -- comprehensive interpreter regression suite.
;;;
;;; Designed to be loadable interpreted (via `(load 'tests/test-all)`)
;;; AND compilable by liszt to a .so so the same suite re-runs as
;;; compiled code (smoke-tests the compiler).
;;;
;;; Loading this file defines `run-all-tests` and DOES NOT call it;
;;; the suite runs when you call `(run-all-tests)` from the REPL.
;;; Compiled mode: the per-file init runs the bind-table forms which
;;; INCLUDE a top-level `(run-all-tests)` call, so loading the .so
;;; both defines and runs.
;;;
;;; (Tests use `chk-eq` and `chk-err` from harness.l; load that first.)

(declare (special tests-passed tests-failed))

;; ---------------- core types and predicates ----------------

(defun test-core ()
   (begin-section "core types/predicates")
   ;; eq / equal / null
   (chk-eq "eq self atom"      (eq 'foo 'foo) t)
   (chk-eq "eq distinct atoms" (eq 'foo 'bar) nil)
   (chk-eq "eq nil nil"        (eq nil nil) t)
   (chk-eq "eq () nil"         (eq '() nil) t)
   (chk-eq "equal lists"       (equal '(a b c) '(a b c)) t)
   (chk-eq "equal nested"      (equal '(a (b c) d) '(a (b c) d)) t)
   (chk-eq "equal numeric"     (equal 42 42) t)
   (chk-eq "equal string"      (equal "abc" "abc") t)
   (chk-eq "null nil"          (null nil) t)
   (chk-eq "null t"            (null t) nil)
   (chk-eq "null empty list"   (null '()) t)
   (chk-eq "not nil"           (not nil) t)
   (chk-eq "not 5"             (not 5) nil)
   ;; predicates
   (chk-eq "atom symbol"       (atom 'foo) t)
   (chk-eq "atom number"       (atom 42) t)
   (chk-eq "atom list"         (atom '(1 2)) nil)
   (chk-eq "listp list"        (listp '(a b)) t)
   (chk-eq "listp nil"         (listp nil) t)
   (chk-eq "listp atom"        (listp 'foo) nil)
   (chk-eq "numberp fix"       (numberp 7) t)
   (chk-eq "numberp atom"      (numberp 'foo) nil)
   (chk-eq "fixp 5"            (fixp 5) t)
   (chk-eq "fixp 5.0"          (fixp 5.0) nil)
   (chk-eq "stringp"           (stringp "hi") t)
   (chk-eq "stringp atom"      (stringp 'hi) nil)
   (chk-eq "symbolp"           (symbolp 'foo) t)
   (chk-eq "symbolp number"    (symbolp 42) nil)
   ;; quote
   (chk-eq "quote literal"     '(1 2 3) '(1 2 3))
   (chk-eq "quote symbol"      'abc 'abc)
   nil)

;; ---------------- arithmetic ----------------

(defun test-arith ()
   (begin-section "arithmetic")
   ;; basic + - * /
   (chk-eq "plus"              (+ 1 2 3 4) 10)
   (chk-eq "plus zero args"    (+) 0)
   (chk-eq "minus"             (- 10 3) 7)
   (chk-eq "minus negate"      (- 5) -5)
   (chk-eq "times"             (* 3 4) 12)
   (chk-eq "times empty"       (*) 1)
   (chk-eq "quotient int"      (/ 17 5) 3)
   (chk-eq "remainder"         (mod 17 5) 2)
   (chk-eq "1+"                (add1 41) 42)
   (chk-eq "1-"                (sub1 43) 42)
   (chk-eq "abs neg"           (abs -7) 7)
   (chk-eq "abs pos"           (abs 7) 7)
   (chk-eq "abs zero"          (abs 0) 0)
   (chk-eq "minusp t"          (minusp -3) t)
   (chk-eq "minusp f"          (minusp 3) nil)
   (chk-eq "minusp zero"       (minusp 0) nil)
   (chk-eq "min"               (min 3 5 1 4) 1)
   (chk-eq "max"               (max 3 5 1 4) 5)
   ;; comparisons
   (chk-eq "lessp t"           (lessp 3 5) t)
   (chk-eq "lessp f"           (lessp 5 3) nil)
   (chk-eq "lessp eq"          (lessp 5 5) nil)
   (chk-eq "greaterp t"        (greaterp 5 3) t)
   (chk-eq "= same"            (= 5 5) t)
   (chk-eq "= diff"            (= 5 6) nil)
   (chk-eq "zerop t"           (zerop 0) t)
   (chk-eq "zerop f"           (zerop 1) nil)
   (chk-eq "times fits-fixnum" (times 1000 1000) 1000000)
   (chk-eq "1 billion ok"      (times 50000 20000) 1000000000)
   (chk-eq "times negative"    (greaterp 0 (times -1000 1000000)) t)
   ;; bignum arithmetic via equal (eq is identity for bignums)
   (chk-eq "bignum equal"      (equal (times 1000000 1000000) 1000000000000) t)
   (chk-eq "big square"        (equal (times 999999999 999999999)
                                      999999998000000001) t)
   (chk-eq "bignum plus"       (equal (plus (times 1000000 1000000)
                                            (times 1000000 1000000))
                                      2000000000000) t)
   (chk-eq "bignum diff"       (difference (times 1000000 1000000) 999999999999) 1)
   (chk-eq "bignum negate"     (equal (times -1000000 1000000) -1000000000000) t)
   ;; expt
   (chk-eq "expt"              (expt 2 10) 1024)
   ;; mod
   (chk-eq "mod 7 3"           (mod 7 3) 1)
   (chk-eq "mod 0 5"           (mod 0 5) 0)
   nil)

;; ---------------- lists ----------------

(defun test-lists ()
   (begin-section "lists")
   ;; cons / car / cdr
   (chk-eq "cons"              (cons 1 2) '(1 . 2))
   (chk-eq "car"               (car '(a b c)) 'a)
   (chk-eq "cdr"               (cdr '(a b c)) '(b c))
   (chk-eq "caar"              (caar '((a b) c)) 'a)
   (chk-eq "cadr"              (cadr '(a b c)) 'b)
   (chk-eq "caddr"             (caddr '(a b c)) 'c)
   (chk-eq "cddr"              (cddr '(a b c d)) '(c d))
   ;; list construction
   (chk-eq "list"              (list 1 2 3) '(1 2 3))
   (chk-eq "list empty"        (list) nil)
   (chk-eq "append"            (append '(1 2) '(3 4)) '(1 2 3 4))
   (chk-eq "append nil"        (append nil '(a b)) '(a b))
   (chk-eq "append empty"      (append '(a) nil) '(a))
   (chk-eq "reverse"           (reverse '(1 2 3 4)) '(4 3 2 1))
   (chk-eq "reverse empty"     (reverse nil) nil)
   (chk-eq "length"            (length '(a b c d)) 4)
   (chk-eq "length empty"      (length nil) 0)
   (chk-eq "last"              (last '(a b c)) '(c))
   ;; membership
   (chk-eq "memq found"        (memq 'b '(a b c)) '(b c))
   (chk-eq "memq missing"      (memq 'z '(a b c)) nil)
   (chk-eq "member found"      (member 'b '(a b c)) '(b c))
   (chk-eq "member equal"      (member '(x) '((x) y)) '((x) y))
   (chk-eq "assoc found"       (assoc 'b '((a 1) (b 2) (c 3))) '(b 2))
   (chk-eq "assoc missing"     (assoc 'z '((a 1) (b 2))) nil)
   ;; nth / nthcdr
   (chk-eq "nth 0"             (nthelem 1 '(a b c)) 'a)
   (chk-eq "nth 2"             (nthelem 3 '(a b c d)) 'c)
   ;; mapping
   (chk-eq "mapcar add1"       (mapcar 'add1 '(1 2 3)) '(2 3 4))
   (chk-eq "mapcar lambda"     (mapcar '(lambda (x) (* x x)) '(1 2 3 4))
                                '(1 4 9 16))
   (chk-eq "mapcar empty"      (mapcar 'add1 nil) nil)
   nil)

;; ---------------- control flow ----------------

(defun test-control ()
   (begin-section "control flow")
   ;; cond
   (chk-eq "cond first"        (cond ((eq 1 1) 'a) (t 'b)) 'a)
   (chk-eq "cond second"       (cond ((eq 1 2) 'a) (t 'b)) 'b)
   (chk-eq "cond falls through" (cond ((eq 1 2) 'a)) nil)
   (chk-eq "cond multi-form"   (cond (t 'first 'second 'third)) 'third)
   ;; if (library macro)
   (chk-eq "if true"           (if t 'yes 'no) 'yes)
   (chk-eq "if false"          (if nil 'yes 'no) 'no)
   ;; and / or
   (chk-eq "and all true"      (and t t 5) 5)
   (chk-eq "and false"         (and t nil 5) nil)
   (chk-eq "and empty"         (and) t)
   (chk-eq "or first true"     (or 1 nil) 1)
   (chk-eq "or fall through"   (or nil nil 'last) 'last)
   (chk-eq "or all nil"        (or nil nil nil) nil)
   ;; let / let*
   (chk-eq "let basic"         (let ((x 5)) (* x x)) 25)
   (chk-eq "let two"           (let ((x 3) (y 4)) (+ x y)) 7)
   (chk-eq "let* sequential"   (let* ((x 3) (y (* x x))) (+ x y)) 12)
   ;; prog with go
   (chk-eq "prog return"       (prog (x) (setq x 10) (return x)) 10)
   (chk-eq "prog go"
      (prog (x)
         (setq x 0)
         loop
         (cond ((greaterp x 3) (return x)))
         (setq x (add1 x))
         (go loop))
      4)
   nil)

;; ---------------- definitions and macros ----------------

(defun fact-tst (n)
   (cond ((zerop n) 1)
         (t (* n (fact-tst (sub1 n))))))

(defun fib-tst (n)
   (cond ((lessp n 2) n)
         (t (+ (fib-tst (sub1 n)) (fib-tst (- n 2))))))

(defmacro inc-tst (var)
   (list 'setq var (list 'add1 var)))

(defun test-defs ()
   (begin-section "definitions and macros")
   (chk-eq "fact 0"            (fact-tst 0) 1)
   (chk-eq "fact 1"            (fact-tst 1) 1)
   (chk-eq "fact 5"            (fact-tst 5) 120)
   (chk-eq "fact 10"           (fact-tst 10) 3628800)
   (chk-eq "fib 0"             (fib-tst 0) 0)
   (chk-eq "fib 1"             (fib-tst 1) 1)
   (chk-eq "fib 10"            (fib-tst 10) 55)
   (chk-eq "fib 15"            (fib-tst 15) 610)
   ;; macro expansion
   (chk-eq "inc-tst macro"
      (let nil
         (setq counter-tst 5)
         (inc-tst counter-tst)
         (inc-tst counter-tst)
         counter-tst)
      7)
   ;; lambda / funcall
   (chk-eq "funcall lambda"
      (funcall '(lambda (x y) (+ x y)) 3 4) 7)
   (chk-eq "apply"
      (apply '+ '(1 2 3 4 5)) 15)
   nil)

;; ---------------- higher-order / closures ----------------

(defun add-fixed-tst (x) (+ x 100))

(defun test-higher ()
   (begin-section "higher-order")
   ;; Franz Lisp uses dynamic scope; lexical "closures" via
   ;; (function (lambda ...)) don't capture lexical vars the way
   ;; modern Lisps do. Test funcall/apply/mapcar with named functions
   ;; and lambda forms that don't depend on outer scope.
   (chk-eq "funcall named"
      (funcall 'add-fixed-tst 7) 107)
   (chk-eq "funcall lambda const"
      (funcall '(lambda (x) (+ x 1)) 41) 42)
   (chk-eq "apply named"
      (apply '+ '(1 2 3 4 5)) 15)
   (chk-eq "mapcar named"
      (mapcar 'add-fixed-tst '(1 2 3)) '(101 102 103))
   ;; mapc returns the original list
   (chk-eq "mapc returns list"
      (mapc 'add1 '(1 2 3)) '(1 2 3))
   nil)

;; ---------------- strings ----------------

(defun test-strings ()
   (begin-section "strings")
   (chk-eq "concat strings"    (concat "abc" "def") 'abcdef)
   (chk-eq "concat with number" (concat "x" 42) 'x42)
   (chk-eq "stringp t"         (stringp "abc") t)
   (chk-eq "string length"     (flatc "hello") 5)
   nil)

;; ---------------- properties ----------------

(defun test-props ()
   (begin-section "property lists")
   (putprop 'sym-tst 'val-1 'key1)
   (putprop 'sym-tst 'val-2 'key2)
   (chk-eq "get key1"          (get 'sym-tst 'key1) 'val-1)
   (chk-eq "get key2"          (get 'sym-tst 'key2) 'val-2)
   (chk-eq "get missing"       (get 'sym-tst 'nope) nil)
   (remprop 'sym-tst 'key1)
   (chk-eq "get after remprop" (get 'sym-tst 'key1) nil)
   nil)

;; ---------------- error handling ----------------

(defun test-errors ()
   (begin-section "errors")
   ;; Franz Lisp returns nil from (car ATOM) / (cdr ATOM) -- not an
   ;; error; atoms have those fields too. Use a non-list-ish error
   ;; instead.
   (chk-eq "car of atom is nil" (car 'foo) nil)
   (chk-eq "cdr of atom is nil" (cdr 'foo) nil)
   (chk-err "div by zero"       (/ 5 0))
   (chk-err "+ on symbol"       (+ 'a 1))
   ;; errset returns (val) on success, nil on error.
   (chk-eq "errset success"     (errset (+ 1 2) nil) '(3))
   ;; catch / throw
   (chk-eq "catch + throw"
      (catch (progn (throw 'val 'tag) 'unreached) tag) 'val)
   (chk-eq "catch no-throw"
      (catch (+ 1 2) tag) 3)
   nil)

;; ---------------- main ----------------

(defun run-all-tests ()
   (setq tests-passed 0)
   (setq tests-failed 0)
   (test-core)
   (test-arith)
   (test-lists)
   (test-control)
   (test-defs)
   (test-higher)
   (test-strings)
   (test-props)
   (test-errors)
   (report-summary))

;; When loaded interpreted: defines run-all-tests, doesnt call it
;; (call it manually from the REPL).
;; When loaded as compiled .so: the bind table will run this
;; top-level form as part of init.
;; Disabled at file scope until interpreted run is verified clean.
;;(run-all-tests)
