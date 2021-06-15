(cl:defpackage "COM.INFORMATIMAGO.COMMON-LISP.LISP.MINIMAL-COMPILER.TEST"
  (:nicknames "MC-TEST")
  (:use "COMMON-LISP"
        "COM.INFORMATIMAGO.COMMON-LISP.LISP.MINIMAL-COMPILER"
        "COM.INFORMATIMAGO.COMMON-LISP.LISP.MACROS")

  (:shadowing-import-from
   "COM.INFORMATIMAGO.COMMON-LISP.LISP.MINIMAL-COMPILER"

   "COMPILER-MACRO-FUNCTION" "MACRO-FUNCTION"
   "MACROEXPAND" "MACROEXPAND-1"
   "LAMBDA" "SETF")

  (:shadowing-import-from
   "COM.INFORMATIMAGO.COMMON-LISP.LISP.MACROS"

   "DECLAIM" "DEFINE-COMPILER-MACRO"
   "DEFINE-SYMBOL-MACRO" "DEFMACRO" "DEFTYPE"
   "AND" "CASE" "CCASE" "COND" "CTYPECASE" "DEFCONSTANT"
   "DEFINE-MODIFY-MACRO" "DEFINE-SETF-EXPANDER"
   "DEFPARAMETER" "DEFSETF" "DEFUN" "DEFVAR"
   "DESTRUCTURING-BIND" "ECASE" "ETYPECASE"
   "MULTIPLE-VALUE-BIND" "MULTIPLE-VALUE-LIST"
   "MULTIPLE-VALUE-SETQ" "NTH-VALUE" "OR" "PROG" "PROG*"
   "PROG1" "PROG2" "PSETF" "PSETQ" "RETURN" "ROTATEF"
   "SHIFTF" "TYPECASE" "UNLESS" "WHEN" "DO" "DO*"
   "DOTIMES" "DOLIST" "LOOP" "LOOP-FINISH" "CALL-METHOD"
   "DEFCLASS" "DEFGENERIC" "DEFINE-METHOD-COMBINATION"
   "DEFMETHOD" "WITH-ACCESSORS" "WITH-SLOTS" "DEFSTRUCT"
   "ASSERT" "CHECK-TYPE" "DEFINE-CONDITION"
   "HANDLER-BIND" "HANDLER-CASE" "IGNORE-ERRORS"
   "RESTART-BIND" "RESTART-CASE"
   "WITH-CONDITION-RESTARTS" "WITH-SIMPLE-RESTART"
   "DEFPACKAGE" "DO-SYMBOLS" "DO-EXTERNAL-SYMBOLS"
   "DO-ALL-SYMBOLS" "IN-PACKAGE" "WITH-PACKAGE-ITERATOR"
   "DECF" "INCF" "POP" "PUSH" "PUSHNEW" "REMF"
   "WITH-HASH-TABLE-ITERATOR" "WITH-INPUT-FROM-STRING"
   "WITH-OPEN-STREAM" "WITH-OUTPUT-TO-STRING"
   "WITH-OPEN-FILE" "PPRINT-EXIT-IF-LIST-EXHAUSTED"
   "PPRINT-POP" "FORMATTER" "PPRINT-LOGICAL-BLOCK"
   "PRINT-UNREADABLE-OBJECT" "WITH-STANDARD-IO-SYNTAX"
   "WITH-COMPILATION-UNIT" "STEP" "TIME" "TRACE"
   "UNTRACE"))
(cl:in-package "COM.INFORMATIMAGO.COMMON-LISP.LISP.MINIMAL-COMPILER.TEST")

(cl:defun print-minimal-expansion (form)
  (cl:let ((*print-circle* nil)
           (*PRINT-RIGHT-MARGIN* 80))
    (format t "~2%SOURCE FORM:~%")
    (pprint form)
    (format t "~2%MACROEXPAND-1:~%")
    (pprint (macroexpand-1 form (env:global-environment)))
    (format t "~2%MACROEXPAND:~%")
    (pprint (macroexpand   form (env:global-environment)))
    (format t "~2%MINIMAL COMPILATION:~%")
    (pprint (minimal-compile (env:global-environment) form))
    #+ccl   (format t "~2%CCL ccl::macroexpand-all:~%")
    #+ccl   (pprint (ccl::macroexpand-all form))
    #+clisp (format t "~2%CLISP ext:expand-form:~%")
    #+clisp (pprint (ext:expand-form form))
    (values)))

(trace
 MACROEXPAND MACROEXPAND-1
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/lambda-expression
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/progv
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/symbol
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/block
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/labels
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/tagbody
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/if
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/let
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/call
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/flet
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/multiple-value-call
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/go
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/catch
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/unwind-protect
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/throw
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/let*
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/progn
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/return-from
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/multiple-value-prog1
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/the
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/eval-when
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/quote
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/lambda-list
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/setq
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/function
 com.informatimago.common-lisp.lisp.minimal-compiler::minimal-compile/locally
 )

;; (print-minimal-expansion '(dotimes (i 42) (print i)))
;; (print-minimal-expansion '(defun minimal-compile/function (env form)
;;                              (declare (ignore env))
;;                              (destructuring-bind (function fname) form
;;                                `(,function ,fname))))

(cl:defparameter *env* (env:global-environment))
(cl:defparameter *env* nil) ; macros stored in host environment.

(cl:defun equalg (a b)
  "equal modulo the gensyms"
  (cl:labels ((unify (a b bindings)
                (cl:typecase a
                  (cons   (if (consp b)
                              (if (eql a b)
                                  (values t bindings)
                                  (cl:multiple-value-bind (eq bindings)
                                      (unify (car a) (car b) bindings)
                                    (if  eq
                                         (unify (cdr a) (cdr b) bindings)
                                         (values nil bindings))))
                              (values nil bindings)))
                  (string (values (and (stringp b)
                                       (string= a b))
                                  bindings))
                  (symbol (if (null (symbol-package a))
                              (cl:let ((entry (assoc a bindings)))
                                (if entry
                                    (values (eql (cdr entry) b) bindings)
                                    (unify a b (acons a b bindings))))
                              (values (eql a b) bindings)))
                  (t      (values (equalp a b) bindings)))))
    (unify a b '())))

(cl:defmacro  mcexp1 (env form expected)
  (cl:let ((result (gensym)))
   `(cl:let ((,result (cl:multiple-value-list (macroexpand-1 ',form ,env))))
      (cl:assert (equalg ,result ',expected)
                 (,result)
                 "FAILURE: macroexpansion-1 of ~S~
                ~%         in the environment  ~S~
                ~%         results in          ~S~
                ~%         expected was        ~S~
                ~%" ',form ,env ,result ',expected))))

(cl:defmacro  mctest (env form expected)
  (cl:let ((result (gensym)))
    `(cl:let ((,result (minimal-compile ,env ',form)))
       (cl:assert (equalg ,result ',expected)
                  (,result)
                  "FAILURE: minimal-compile of ~S~
                ~%         in the environment  ~S~
                ~%         results in          ~S~
                ~%         expected was        ~S~
                ~%" ',form ,env ,result ',expected))))

(mcexp1 *env* foo (foo nil))

(mcexp1 *env* (and)    (t t))
(mcexp1 *env* (and a)  (a t))
(mcexp1 *env* (and a b)  ((let ((va a)) (if va b nil)) t))
(mcexp1 *env* (and (values 1 2) (values 2 3)(values 4 5))
        ((let ((va (values 1 2)))
           (if va
               (let ((vb (values 2 3)))
                 (if vb (values 4 5) nil))
               nil))
         t))


(mctest *env*  foo  foo)
(mctest *env*  42   42)

(mctest *env*  (quote 42)        (quote 42))
(mctest *env*  (quote foo)       (quote foo))
(mctest *env*  (quote (foo bar)) (quote (foo bar)))

(mctest *env*  (quote (foo bar)) (quote (foo bar)))

(mctest *env* (and)      t)
(mctest *env* (and a)    a)
(mctest *env* (and a b)  (let ((va a)) (if va b nil)))
(mctest *env* (and (values 1 2) (values 2 3)(values 4 5))
        (let ((va (values 1 2)))
          (if va
              (let ((vb (values 2 3)))
                (if vb (values 4 5) nil))
              nil)))

(mctest *env* (function foo) (function foo))
(mctest *env* (function car) (function car))
(mctest *env* (function (setf foo)) (function (setf foo)))
(mctest *env* (function (setf car)) (function (setf car)))
(assert (mc:lambda-expression-p '(lambda (x) (and x (+ 1 x)))))
(mctest *env* (function (lambda (x) (and x (+ 1 x)))) (function (setf car)))
(symbol-package 'lambda)


(minimal-compile *env* '(function foo))
#'foo


;;;; THE END ;;;;

