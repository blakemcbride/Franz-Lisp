;;; tests/harness.l -- minimal test harness used by the rest of the
;;; suite. Designed to be loadable interpreted AND compilable by liszt
;;; so the same suite tests both modes.
;;;
;;; Expose three things to test files:
;;;   (chk-eq name actual expected)  -- compares with `equal`, records
;;;                                     pass/fail. `actual` is wrapped
;;;                                     in errset so a runtime error
;;;                                     during evaluation is reported
;;;                                     as a failure rather than
;;;                                     aborting the run.
;;;   (chk-err name expr)            -- expects `expr` to error.
;;;   (begin-section name)           -- header for the section.
;;;   (report-summary)               -- final pass/fail counts.

(declare (special tests-passed tests-failed tests-section))

(setq tests-passed 0)
(setq tests-failed 0)
(setq tests-section "init")

(defun report-fail (name actual expected)
   (setq tests-failed (add1 tests-failed))
   (princ "FAIL [") (princ tests-section) (princ "] ")
   (princ name) (terpri)
   (princ "  expected: ") (print expected) (terpri)
   (princ "  actual:   ") (print actual) (terpri))

(defun report-pass ()
   (setq tests-passed (add1 tests-passed)))

;;; chk-eq is the workhorse. errset returns (value) on success or nil
;;; on error. Wrapping the test expression isolates failures so one
;;; broken case doesnt take down the whole run.
(defun chk-eq-fn (name result expected)
   (cond ((null result)
            (report-fail name '<error-during-eval> expected))
         ((equal (car result) expected)
            (report-pass))
         (t (report-fail name (car result) expected))))

(defmacro chk-eq (name actual expected)
   (list 'chk-eq-fn name
                    (list 'errset actual nil)
                    expected))

;;; chk-err passes if the expression raises an error.
(defun chk-err-fn (name result)
   (cond ((null result) (report-pass))
         (t (report-fail name (car result) '<error>))))

(defmacro chk-err (name actual)
   (list 'chk-err-fn name (list 'errset actual nil)))

(defun begin-section (name)
   (setq tests-section name)
   (princ "--- ") (princ name) (princ " ---") (terpri))

(defun report-summary ()
   (princ "------------------------") (terpri)
   (princ tests-passed) (princ " passed, ")
   (princ tests-failed) (princ " failed") (terpri)
   (cond ((zerop tests-failed)
            (princ "ALL TESTS PASSED") (terpri))
         (t
            (princ "SOME TESTS FAILED") (terpri))))
