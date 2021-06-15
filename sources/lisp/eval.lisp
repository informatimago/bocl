;;;; -*- mode:lisp;coding:iso-8859-1 -*-

;;; This file contains a prototype of the interpreter EVAL function
;;; equivalent to the KERNEL:EVAL function, implemented in COMMON-LISP.

(defpackage "COM.INFORMATIMAGO.BOCL.PROTOTYPE.EVAL"
  (:use "COMMON-LISP")
  (:use "COM.INFORMATIMAGO.COMMON-LISP.LISP-SEXP.SOURCE-FORM")
  (:shadow "EVAL")
  (:export "EVAL"))
(in-package "COM.INFORMATIMAGO.BOCL.PROTOTYPE.EVAL")



(defun check-arg-count (form min max)
  (assert (<= min (length (cdr form)) max)))

(defun check-arg-count/even (form)
  (assert (evenp (length (cdr form)))))

(defun check-arg-count/min (form min)
  (assert (<= min (length (cdr form)))))

(defun evale (form environment)
  ;; This eval function is called on forms that have been minimally-compiled,
  ;; ie. fully macroexpanded.

  (cond

    ((symbolp form)
     ;; We don't implement symbol macros, so we only have variables:
     (let ((variable (env:find-variable form environment)))

       (env:get-variable-value form environment)))

    ((atom form)
     ;; The other atoms are unchanged literals:
     form)

    (t
     ;; Now we have a list.
     (case (first form)
       ;; First we check the special operators:

       ((QUOTE)
        (check-arg-count form 1 1)
        (second form))

       ((FUNCTION)
        ;; We don't implement (function (setf x)), only symbols and lambda:
        (check-arg-count form 1 1)
        (if (symbolp (second form))
            (env:get-function-value (second form) environment)
            (make-closure (second form) environment)))

       ((IF)
        (check-arg-count form 2 3)
        (if (evale (second form) environment)
            (evale (third  form) environment)
            (evale (fourth form) environment)))

       ((SETQ)
        ;; After minimal compilation, the variables are not symbol-macros.
        (check-arg-count/even form)
        (loop
          with result = nil
          for (varname expr) on (cdr form) by (function cddr)
          do (let ((variable      (env:find-variable var environment))
                   (primary-value (evale expr environment)))
               (if (variable-constantp variable)
                   (error "Cannot SETQ a constant variable ~S" var))
               (env:set-variable-value variable primary-value)
               (setf result primary-value))
          finally (return result)))

       ((UNWIND-PROTECT)
        (check-arg-count/min form 1)
        (let ((form    (second form))
              (cleanup (cddr form)))

          )
        (minimal-compile/unwind-protect       env form))

       ((BLOCK)
        (let ((environment (environment/block (second form) environment)))
          (tiny-eval `(progn ,(cddr form)))))
       ((RETURN-FROM)
        (check-arg-count form 1 2)
        (environment/return-from (second form) (tiny-eval (third form))))

       ((CATCH)                (minimal-compile/catch                env form))
       ((THROW)                (minimal-compile/throw                env form))

       ((TAGBODY GO) (error "Must have been removed from compiled code ~S" form))

       ((FLET)                 (minimal-compile/flet                 env form))
       ((LABELS)               (minimal-compile/labels               env form))


       ((LET)                  (minimal-compile/let                  env form))
       ((LET*)                 (minimal-compile/let*                 env form))
       ((LOCALLY)              (minimal-compile/locally              env form))
       ((PROGN)                (minimal-compile/progn                env form))
       ((PROGV)                (minimal-compile/progv                env form))

       ((MULTIPLE-VALUE-CALL)  (minimal-compile/multiple-value-call  env form))
       ((MULTIPLE-VALUE-PROG1) (minimal-compile/multiple-value-prog1 env form))


       ((SYMBOL-MACROLET)
        ;; add the symbol-macrolet to the environment
        )
       ((MACROLET)
        ;; add the macrolet to the environment
        )


       ((LOAD-TIME-VALUE)
        ;; do...
        )

       ((EVAL-WHEN)            (minimal-compile/eval-when            env form))
       ((THE)                  (minimal-compile/the                  env form))

       (otherwise
        (let ((cmf (compiler-macro-function (operator form)))
              (mf  (macro-function (operator form))))
          (cond
            (cmf
             (let ((expansion (funcall (coerce *macroexpand-hook* 'function)
                                       cmf form env)))
               (minimal-compile env expansion)))
            (mf
             (let ((expansion (funcall (coerce *macroexpand-hook* 'function)
                                       mf form env)))
               (minimal-compile env expansion)))
            (t (minimal-compile/funcall env form)))))))))

(defun eval (form)
  (evale form (env:global-environment)))


;;;; THE END ;;;;
