;;;; -*- mode:lisp;coding:utf-8 -*-
;;;;**************************************************************************
;;;;FILE:               macros.lisp
;;;;LANGUAGE:           Common-Lisp
;;;;SYSTEM:             Common-Lisp
;;;;USER-INTERFACE:     NONE
;;;;DESCRIPTION
;;;;
;;;;    This packages defines the standard Common Lisp macros.
;;;;
;;;;AUTHORS
;;;;    <PJB> Pascal J. Bourguignon <pjb@informatimago.com>
;;;;MODIFICATIONS
;;;;    2021-06-16 <PJB> Created.
;;;;BUGS
;;;;LEGAL
;;;;    AGPL3
;;;;
;;;;    Copyright Pascal J. Bourguignon 2021 - 2021
;;;;
;;;;    This program is free software: you can redistribute it and/or modify
;;;;    it under the terms of the GNU Affero General Public License as published by
;;;;    the Free Software Foundation, either version 3 of the License, or
;;;;    (at your option) any later version.
;;;;
;;;;    This program is distributed in the hope that it will be useful,
;;;;    but WITHOUT ANY WARRANTY; without even the implied warranty of
;;;;    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;;;    GNU Affero General Public License for more details.
;;;;
;;;;    You should have received a copy of the GNU Affero General Public License
;;;;    along with this program.  If not, see <http://www.gnu.org/licenses/>.
;;;;**************************************************************************
;;;;

(cl:defpackage "COM.INFORMATIMAGO.COMMON-LISP.LISP.MACROS"
  (:nicknames "COM.INFORMATIMAGO.BOCL.MACROS")
  (:use "COMMON-LISP")
  (:use "COM.INFORMATIMAGO.COMMON-LISP.LISP-SEXP.SOURCE-FORM")
  (:use "COM.INFORMATIMAGO.COMMON-LISP.LISP.CT-REGISTER")
  (:use "COM.INFORMATIMAGO.COMMON-LISP.LISP.MINIMAL-COMPILER")
  (:shadowing-import-from
   "COM.INFORMATIMAGO.COMMON-LISP.LISP.MINIMAL-COMPILER"
   . #1=(
         "COMPILER-MACRO-FUNCTION" "MACRO-FUNCTION"
         "MACROEXPAND" "MACROEXPAND-1"
         "LAMBDA" "SETF"))
  (:export . #1#)
  (:shadow
   . #9=(
         "AND" "ASSERT" "CALL-METHOD" "CASE" "CCASE" "CHECK-TYPE"
         "COND" "CTYPECASE" "DECF" "DECLAIM" "DEFCLASS" "DEFCONSTANT"
         "DEFGENERIC" "DEFINE-COMPILER-MACRO" "DEFINE-CONDITION"
         "DEFINE-METHOD-COMBINATION" "DEFINE-MODIFY-MACRO"
         "DEFINE-SETF-EXPANDER" "DEFINE-SYMBOL-MACRO" "DEFMACRO"
         "DEFMETHOD" "DEFPACKAGE" "DEFPARAMETER" "DEFSETF" "DEFSTRUCT"
         "DEFTYPE" "DEFUN" "DEFVAR" "DESTRUCTURING-BIND" "DO" "DO*"
         "DO-ALL-SYMBOLS" "DO-EXTERNAL-SYMBOLS" "DO-SYMBOLS" "DOLIST"
         "DOTIMES" "ECASE" "ETYPECASE" "FORMATTER" "HANDLER-BIND"
         "HANDLER-CASE" "IGNORE-ERRORS" "IN-PACKAGE" "INCF"
         "LOOP" "LOOP-FINISH" "MULTIPLE-VALUE-BIND"
         "MULTIPLE-VALUE-LIST" "MULTIPLE-VALUE-SETQ" "NTH-VALUE" "OR"
         "POP" "PPRINT-EXIT-IF-LIST-EXHAUSTED" "PPRINT-LOGICAL-BLOCK"
         "PPRINT-POP" "PRINT-UNREADABLE-OBJECT" "PROG" "PROG*" "PROG1"
         "PROG2" "PSETF" "PSETQ" "PUSH" "PUSHNEW" "REMF"
         "RESTART-BIND" "RESTART-CASE" "RETURN" "ROTATEF"
         "SHIFTF" "STEP" "TIME" "TRACE" "TYPECASE" "UNLESS" "UNTRACE"
         "WHEN" "WITH-ACCESSORS" "WITH-COMPILATION-UNIT"
         "WITH-CONDITION-RESTARTS" "WITH-HASH-TABLE-ITERATOR"
         "WITH-INPUT-FROM-STRING" "WITH-OPEN-FILE" "WITH-OPEN-STREAM"
         "WITH-OUTPUT-TO-STRING" "WITH-PACKAGE-ITERATOR"
         "WITH-SIMPLE-RESTART" "WITH-SLOTS" "WITH-STANDARD-IO-SYNTAX"

         ;; "LAMBDA" "SETF"
         ))
  (:export . #9#)

  ;; (:use "KERNEL")
  ;; (:shadowing-import-from
  ;;  "KERNEL"
  ;;
  ;;  "KERNEL-ROOTSET" "GET-EVAL-COUNTERS" "CLASS-NEW" "CLASSP" "CLASS-NAME"
  ;;  "CLASS-DIRECT-SUPERCLASSES" "CLASS-ADD-SUPERCLASS" "CLASS-OF" "FIND-CLASS"
  ;;  "SUBCLASSP" "TYPEP" "CHECK-TYPE" "CHECK-CLASS" "BUILT-IN-CLASS-NEW" "CONS"
  ;;  "CONSP" "CONS-CAR" "CONS-CDR" "CONS-RPLACA" "CONS-RPLACD" "INTEGERP"
  ;;  "FLOATP" "CHARACTERP" "CHAR-CODE" "CHAR-INT" "CHAR-NAME" "CODE-CHAR"
  ;;  "NAME-CHAR" "FIND-SYMBOL" "MAKE-SYMBOL" "INTERN" "SYMBOL-NAME"
  ;;  "SYMBOL-PACKAGE" "SYMBOL-VALUE" "SYMBOL-FUNCTION" "SYMBOL-PLIST"
  ;;  "SET-SYMBOL-PACKAGE" "SET-SYMBOL-VALUE" "SET-SYMBOL-FUNCTION"
  ;;  "SET-SYMBOL-PLIST" "SYMBOLP" "BOUNDP" "FBOUNDP" "MAKUNBOUND" "FMAKUNBOUND"
  ;;  "INTERN-KEYWORD" "KEYWORDP" "OBARRAY-ALL-SYMBOLS" "NULL"
  ;;  "NORMALIZE-EXTERNAL-FORMAT" "PHYSICAL-PATHNAME" "PHYSICAL-PATHNAME-EQUAL"
  ;;  "PHYSICAL-PATHNAME-P" "PATHNAMEP" "MAKE-FILE-STREAM" "STREAM-PATHNAME"
  ;;  "STREAM-ELEMENT-TYPE" "STREAM-EXTERNAL-FORMAT" "STREAM-DIRECTION"
  ;;  "OPEN-STREAM-P" "STANDARD-INPUT-STREAM" "STANDARD-OUTPUT-STREAM"
  ;;  "STANDARD-ERROR-STREAM" "CLOSUREP" "FUNCTIONP" "BUILT-IN-FUNCTION-P"
  ;;  "FUNCTION-NAME" "RPLACA" "RPLACD" "CAR" "CDR" "CAAR" "CADR" "CDAR" "CDDR"
  ;;  "CAAAR" "CAADR" "CADAR" "CADDR" "CDAAR" "CDADR" "CDDAR" "CDDDR" "CAAAAR"
  ;;  "CAAADR" "CAADAR" "CAADDR" "CADAAR" "CADADR" "CADDAR" "CADDDR" "CDAAAR"
  ;;  "CDAADR" "CDADAR" "CDADDR" "CDDAAR" "CDDADR" "CDDDAR" "CDDDDR" "LIST-LENGTH"
  ;;  "LIST-POSITION" "NCONC" "LAST" "COPY-LIST" "LIST" "DELETE" "NREVERSE"
  ;;  "REVERSE" "MEMQ" "ASSQ" "FIRSTS" "GETF" "PUTF" "REMF" "VECTOR" "VECTORP"
  ;;  "VECTOR-LENGTH" "SVREF" "SVREF-SET" "BIT-VECTOR-P" "BIT-VECTOR-LENGTH" "BIT"
  ;;  "SBIT" "BIT-SET" "SBIT-SET" "BIT-VECTOR-EQUAL" "STRING-LENGTH" "STRING="
  ;;  "STRING-EQUAL" "STRINGP" "CHAR" "SCHAR" "SET-CHAR" "SET-SCHAR" "EQL" "EQUAL"
  ;;  "VALUES" "VALUES-LIST" "PATHNAME" "NAMESTRING" "PRIN1-TO-STRING" "PRINT"
  ;;  "PRIN1" "PRINC" "TERPRI" "KERNEL-READ-CHAR" "KERNEL-PEEK-CHAR" "KERNEL-READ"
  ;;  "KERNEL-READ-PRESERVING-WHITESPACE" "KERNEL-READ-DELIMITED-LIST"
  ;;  "KERNEL-READ-FROM-STRING" "MAKE-STRING-INPUT-STREAM"
  ;;  "MAKE-STRING-OUTPUT-STREAM" "GET-OUTPUT-STREAM-STRING" "OPEN"
  ;;  "FINISH-OUTPUT" "FORCE-OUTPUT" "=/I" "=/F" "MAKE-STRUCTURE-CLASS"
  ;;  "STRUCTURE-CLASS-INSTANCE-SIZE" "STRUCTURE-CLASS-INSTANCE-DIRECT-SLOTS"
  ;;  "STRUCTURE-CLASS-INSTANCE-SLOTS" "MAKE-STRUCTURE-INSTANCE" "STRUCTUREP"
  ;;  "STRUCTURE-GET" "STRUCTURE-SET" "MAKE-ENVIRONMENT" "ENVIRONMENT-IS-TOPLEVEL"
  ;;  "ENVIRONMENT-VARIABLES" "ENVIRONMENT-FUNCTIONS" "ENVIRONMENT-BLOCKS"
  ;;  "ENVIRONMENT-CATCHS" "ENVIRONMENT-DECLARATIONS"
  ;;  "ENVIRONMENT-NEXT-ENVIRONMENT" "ENVIRONMENT-FRAMES"
  ;;  "ENVIRONMENT-ADD-VARIABLE" "ENVIRONMENT-ADD-FUNCTION"
  ;;  "ENVIRONMENT-ADD-BLOCK" "ENVIRONMENT-ADD-CATCH"
  ;;  "ENVIRONMENT-ADD-DECLARATION" "ENVIRONMENT-FIND-VARIABLE"
  ;;  "ENVIRONMENT-FIND-FUNCTION" "ENVIRONMENT-FIND-BLOCK"
  ;;  "ENVIRONMENT-FIND-CATCH" "ENV-SPECIAL-VARIABLE-P")

  )
(cl:in-package "COM.INFORMATIMAGO.COMMON-LISP.LISP.MACROS")


;; We have no macros, since we are defining them.

#-(and) ; Its macroexpansion follows.
(cl:defmacro defmacro (name (&rest lambda-list) &body decl-docs-body)
  (let* ((ll          (parse-lambda-list lambda-list :macro))
         (whole       (if (lambda-list-whole-parameter-p ll)
                          (parameter-specifier (lambda-list-whole-parameter ll))
                          (gensym "whole")))
         (environment (if (lambda-list-environment-parameter-p ll)
                          (parameter-specifier (lambda-list-environment-parameter ll))
                          (gensym "environment"))))
    (remove-whole-parameter ll)
    (remove-environment-parameter ll)
    (cl:multiple-value-bind (docstrings declarations body) (parse-body :lambda decl-docs-body)
      `(eval-when (:compile-toplevel :load-toplevel :execute)
         (register-macro
          ',name
          (lambda (,whole ,environment)
            (declare (ignorable ,environment))
            (destructuring-bind ,(make-lambda-list ll) ,whole
              ,@declarations
              ,@body))
          ',(first docstrings))
         ',name))))

#-(and)
(cl:defmacro defmacro (&environment environment name lambda-list &body decl-doc-body)
  (let* ((ll            (parse-lambda-list lambda-list :macro))
         (whole         (if (lambda-list-whole-parameter-p ll)
                            (parameter-specifier (lambda-list-whole-parameter ll))
                            (gensym "whole")))
         (env-parameter (if (lambda-list-environment-parameter-p ll)
                            (parameter-specifier (lambda-list-environment-parameter ll))
                            (gensym "environment"))))
    (remove-whole-parameter ll)
    (remove-environment-parameter ll)
    (cl:multiple-value-bind (docstrings declarations body) (parse-body :lambda decl-doc-body)
      `(progn
         (setf (env:macro-function ',name)
               (coerce-to-function
                ,(minimal-compile/lambda-expression
                  environment
                  `(lambda (,whole ,env-parameter)
                     (declare (ignorable ,env-parameter))
                     (block ,name
                       (destructuring-bind ,lambda-list ,whole
                         ,@declarations
                         ,@body)))))
               ,@(if docstrings
                     `((documentation ',name 'function) ',(first docstrings))))
         ',name))))

(eval-when (:compile-toplevel :load-toplevel :execute)
  (register-macro 'defmacro
                  (cl:lambda (whole environment)
                    (declare (ignorable environment))
                    (cl:destructuring-bind (name lambda-list &body decl-docs-body) whole
                      (let* ((ll (parse-lambda-list lambda-list :macro))
                             (whole
                               (if (lambda-list-whole-parameter-p ll)
                                   (parameter-specifier (lambda-list-whole-parameter ll))
                                   (gensym "whole")))
                             (environment
                               (if (lambda-list-environment-parameter-p ll)
                                   (parameter-specifier (lambda-list-environment-parameter ll))
                                   (gensym "environment"))))
                        (remove-whole-parameter ll)
                        (remove-environment-parameter ll)
                        (cl:multiple-value-bind (docstrings declarations body) (parse-body :lambda decl-docs-body)
                          (list* 'eval-when
                                 (list* '(:compile-toplevel :load-toplevel :execute)
                                        (list* (list* 'register-macro
                                                      (list* (list* 'quote (list name))
                                                             (list* (list* 'lambda
                                                                           (list* (list* whole (list environment))
                                                                                  (list* (list* 'declare (list (list* 'ignorable (list environment))))
                                                                                         (list (list* 'destructuring-bind (list* (make-lambda-list ll)
                                                                                                                                 (list* whole (append declarations body))))))))
                                                                    (list (list* 'quote (list (first docstrings)))))))
                                               (list (list* 'quote (list name))))))))))
                  'nil)
  'defmacro)


(cl:defmacro defun (&environment environment name lambda-list &body decl-doc-body)
  (cl:multiple-value-bind (docstrings declarations body) (parse-body :lambda decl-doc-body)
    `(progn
       (setf (symbol-function ',name)
             (coerce-to-function
              ,(minimal-compile/lambda-expression
                environment
                `(lambda ,lambda-list
                   ,@declarations
                   (block ,name
                     ,@body))))
             ,@(if docstrings
                   `((documentation ',name 'function) ',(first docstrings))))
       ',name)))


(cl:defmacro lambda (&whole whole (&rest lambda-list) &body decl-body)
  (declare (ignore lambda-list decl-body))
  `(function ,whole))

;;; ============================================================

(cl:defmacro declaim (&rest declaration-specifiers)
  `(eval-when (:compile-toplevel :load-toplevel :execute)
     ,@(mapcar (cl:lambda (declaration-specifier)
                 `(proclaim ',declaration-specifier))
               declaration-specifiers)))

(cl:defmacro define-compiler-macro (name lambda-list &body dec-doc-body)
  (let* ((ll          (parse-lambda-list lambda-list :macro))
         (whole       (if (lambda-list-whole-parameter-p ll)
                          (parameter-specifier (lambda-list-whole-parameter ll))
                          (gensym "whole")))
         (environment (if (lambda-list-environment-parameter-p ll)
                          (parameter-specifier (lambda-list-environment-parameter ll))
                          (gensym "environment"))))
    (remove-whole-parameter ll)
    (remove-environment-parameter ll)
    (cl:multiple-value-bind (docstrings declarations body) (parse-body :lambda dec-doc-body)
      `(eval-when (:compile-toplevel :load-toplevel :execute)
         (register-compiler-macro
          ',name
          (lambda (,whole ,environment)
            (declare (ignorable ,environment))
            (destructuring-bind ,(make-lambda-list ll) ,whole
              ,@declarations
              ,@body))
          ',(first docstrings))
         ',name))))

(cl:defmacro define-symbol-macro (symbol expansion)
  `(eval-when (:compile-toplevel :load-toplevel :execute)
     (register-symbol-macro ',symbol ',expansion)
     ',symbol))


(cl:defmacro deftype (name (&rest lambda-list) &body decl-docs-body)
  (cl:destructuring-bind (docstrings declarations body) (parse-body :lambda decl-docs-body)
    `(eval-when (:compile-toplevel :load-toplevel :execute)
       (register-type ',name (lambda ,lambda-list ,@declarations ,@body)
                      ',(first docstrings)))))

;; Macro DEFSTRUCT                                                 08_Structure


;; Macro DEFINE-MODIFY-MACRO                                       05_Flow
;; Macro DEFINE-SETF-EXPANDER                                      05_Flow
;; Macro DEFSETF                                                   05_Flow


(cl:defmacro defconstant (name initial-value &optional documentation)
  `(progn
     (eval-when (:compile-toplevel)
       (register-constant-variable ',name ',initial-value ',documentation))
     (setf (symbol-value ',name) ,initial-value)
     ,@(if documentation
           `((setf (documentation ',name 'variable) ',documentation)))))

(cl:defmacro defparameter (name initial-value &optional documentation)
  `(progn
     (eval-when (:compile-toplevel)
       (register-dynamic-variable ',name ',initial-value ',documentation))
     (declaim (special ,name))
     (setf (symbol-value ',name) ,initial-value)
     ,@(if documentation
           `((setf (documentation ',name 'variable) ',documentation)))))

(cl:defmacro defvar (name initial-value &optional documentation)
  `(progn
     (eval-when (:compile-toplevel)
       (register-dynamic-variable ',name ',initial-value ',documentation))
     (declaim (special ,name))
     (unless (boundp ',name)
       (setf (symbol-value ',name) ,initial-value))
     ,@(if documentation
           `((setf (documentation ',name 'variable) ',documentation)))))


(eval-when (:compile-toplevel :load-toplevel :execute)
  (cl:defun generate-short-circuit (operation expressions otherwise)
    (if (endp expressions)
        otherwise
        (generate-short-circuit
         operation (rest expressions)
         (let ((expr  (first expressions))
               (value (gensym)))
           (cl:ecase operation
             ((and)
              `(let ((,value ,expr))
                 (if ,value
                     ,otherwise
                     nil)))
             ((or)
              `(let ((,value ,expr))
                 (if ,value
                     ,value
                     ,otherwise)))
             ((cond)
              (cl:unless (consp expr)
                (error "Clause ~S should be a non-empty list" expr))
              `(let ((,value ,(first expr)))
                 (if ,value
                     ,(if (rest expr)
                          `(progn ,@(rest expr))
                          value)
                     ,otherwise)))))))))

(cl:defmacro and (&rest forms)
  (if (null forms)
      't
      (let ((exprs (reverse forms)))
        (generate-short-circuit 'and (rest exprs) (first exprs)))))

(cl:defmacro or (&rest forms)
  (if (null forms)
      'nil
      (let ((exprs (reverse forms)))
        (generate-short-circuit 'or (rest exprs) (first exprs)))))

(cl:defmacro cond (&rest clauses)
  (if (null clauses)
      'nil
      (generate-short-circuit 'cond (reverse clauses) nil)))

(cl:defmacro when (test &body body)
  `(if ,test (progn ,@body) nil))

(cl:defmacro unless (test &body body)
  `(if ,test nil (progn ,@body)))


(cl:defun generate-case (operation key-form-or-place &rest clauses)
  (let (normal-clauses
        otherwise-clause)

    ;; Note: For CTYPECASE and ETYPECASE, no otherwise-clause is
    ;; allowed, but a clause such as (T …) will be interpreted as type
    ;; T, a clause such as (OTHERWISE …) will be interpreted as type
    ;; OTHERWISE, if there's such a type…
    ;; On the other hand, for CCASE and ECASE, T and OTHERWISE are
    ;; forbidden as key values.

    (if (and clauses
             (or (eql operation 'case)
                 (eql operation 'typecase)))
        (let ((last-clause (first (last clauses))))
          (cl:unless (consp last-clause)
            (error "~A clause ~S should be a list" operation last-clause))
          (if (or (eql (first last-clause) 't)
                  (eql (first last-clause) 'otherwise))
              (cl:setf otherwise-clause last-clause
                       normal-clauses (butlast clauses))
              (cl:setf normal-clauses clauses))))

    (error "Not implemented yet.")
    ))


(cl:defmacro case      (keyform  &rest clauses)  (generate-case 'case      keyform  clauses))
(cl:defmacro ccase     (keyplace &rest clauses)  (generate-case 'ccase     keyplace clauses))
(cl:defmacro ecase     (keyform  &rest clauses)  (generate-case 'ecase     keyform  clauses))
(cl:defmacro typecase  (keyform  &rest clauses)  (generate-case 'typecase  keyform  clauses))
(cl:defmacro ctypecase (keyplace &rest clauses)  (generate-case 'ctypecase keyplace clauses))
(cl:defmacro etypecase (keyform  &rest clauses)  (generate-case 'etypecase keyform  clauses))

;; Macro DESTRUCTURING-BIND                                        05_Flow


(cl:defmacro multiple-value-setq ((&rest var-places) values-form)
  (let ((temps (mapcar (cl:lambda (var-place) (declare (ignore var-place)) (gensym)) var-places)))
    `(multiple-value-call (cl:lambda (,@temps)
                            (setf ,@(mapcan (function list) var-places temps))
                            (values ,@temps))
       ,values-form)))

(cl:defmacro multiple-value-bind ((&rest vars) values-form &body decl-body)
  `(multiple-value-call (lambda (,@vars) ,@decl-body) ,values-form))

(cl:defmacro multiple-value-list (values-form)
  `(multiple-values-call (function list) ,values-form))

(cl:defmacro nth-value (n form)
  `(nth ,n (multiple-value-list ,form)))



(eval-when (:compile-toplevel :load-toplevel :execute)
  (cl:defun generate-prog (letop bindings decl-tag-body)
    (cl:multiple-value-bind (docstrings declarations body) (parse-body :locally decl-tag-body)
      (declare (ignore docstrings))
      `(block nil
         (,letop (,@bindings)
                 ,@declarations
                 (tagbody ,@body))))))

(cl:defmacro prog ((&rest vars) &body decl-tag-body)
  (generate-prog 'let vars decl-tag-body))

(cl:defmacro prog* ((&rest vars) &body decl-tag-body)
  (generate-prog 'let* vars decl-tag-body))

(cl:defmacro prog1 (first-form &rest forms)
  (let ((vresult (gensym)))
    `(let ((,vresult ,first-form))
       ,@forms
       ,vresult)))

(cl:defmacro prog2 (first-form second-form &rest forms)
  (let ((vresult (gensym)))
    `(let ((,vresult (progn ,first-form ,second-form)))
       ,@forms
       ,vresult)))

(cl:defmacro return (&optional result) `(return-from nil ,result))

;; Macro SETF                                                      05_Flow
;; Macro PSETF                                                     05_Flow

(cl:defmacro psetq (&whole form &rest vars-vals)
  (let ((len (length vars-vals)))
    (cond
      ((oddp len)
       (error "Missing a value in ~S" form))
      ((zerop len)
       'nil)
      (t
       (let ((temps nil)
             (vars  nil)
             (vals  nil))
         (cl:do ((current vars-vals (cddr current)))
                ((null current))
           (setq temps (cons (gensym) temps))
           (setq vars  (cons (car  current) vars))
           (setq vals  (cons (cadr current) vals)))
         (setq vars (nreverse vars))
         (setq vals (nreverse vals))
         `(let (,@(mapcar (function list) temps vals))
            (setq ,@(mapcan (function list) vars temps))
            nil))))))

(cl:defmacro shiftf (&environment environment place &rest places-and-new-value)
  (let* ((places    (cons place (butlast places-and-new-value)))
         (new-value (first (last places-and-new-value)))
         (infos     (mapcar (cl:lambda (place)
                              (cl:multiple-value-bind (vars vals store-vars setter getter)
                                  (get-setf-expansion place environment)
                                (list (mapcar (function list) vars vals)
                                      store-vars
                                      getter
                                      setter)))
                            places))
         (vnew      (mapcar (cl:lambda (store-var) (list store-var (gensym))) (second (first (last infos)))))
         (vresults  (mapcar (cl:lambda (store-var) (list (gensym) store-var)) (second (first infos)))))
    `(let (,@(mapcan (cl:lambda (info) (append (first info) (second info) nil)) infos)
           ,@(mapcar (function second) vnew))
       ,@(mapcar (cl:lambda (info)
                   (if (cdr (second info))
                       `(multiple-value-setq ,(second info) ,(third info))
                       `(setq ,(car (second info)) ,(third info))))
                 infos)                 ; getters
       ,(if (cdr vnew)
            `(multiple-value-setq ,(mapcar (function second) vnew) ,new-value)
            `(setq ,(second (first vnew)) ,new-value))
       (let (,@vresults)           ; get the result values from place1
         ;; Shift:
         (setq ,@(mapcan (cl:lambda (dst src)
                           (let ((result '())
                                 (vsrcs (second src)))
                             (cl:dolist (vdst (second dst) (nreverse result))
                               (setq result (cons (car vsrcs) (cons vdst result)))
                               (setq vsrcs (cdr vsrcs)))))
                         infos (cdr infos)))
         ;; the new-value:
         (setq ,@(apply (function append) vnew))
         ,@(mapcar (function fourth) infos) ; setters
         (values ,@(mapcar (function first) vresults))))))

(cl:defmacro rotatef (&environment environment &rest places)
  (if (null places)
      'nil
      (let* ((infos     (mapcar (cl:lambda (place)
                                  (cl:multiple-value-bind (vars vals store-vars setter getter)
                                      (get-setf-expansion place environment)
                                    (list (mapcar (function list) vars vals)
                                          store-vars
                                          getter
                                          setter)))
                                places))
             (vsaved    (mapcar (cl:lambda (store-var) (list (gensym) store-var)) (second (first infos)))))
        `(let (,@(mapcan (cl:lambda (info) (append (first info) (second info) nil)) infos)
               ,@(mapcar (function first) vsaved))
           ,@(mapcar (cl:lambda (info)
                       (if (cdr (second info))
                           `(multiple-value-setq ,(second info) ,(third info))
                           `(setq ,(car (second info)) ,(third info))))
                     infos)             ; getters
           ;; Save the first value
           (setq ,@(apply (function append) vsaved))
           ;; Rotate:
           (setq ,@(mapcan (cl:lambda (dst src)
                             (let ((result '())
                                   (vsrcs (second src)))
                               (cl:dolist (vdst (second dst) (nreverse result))
                                 (setq result (cons (car vsrcs) (cons vdst result)))
                                 (setq vsrcs (cdr vsrcs)))))
                           infos
                           (append (cdr infos) (list (list nil (mapcar (function first) vsaved))))))
           ,@(mapcar (function fourth) infos) ; setters
           nil))))


(cl:define-modify-macro incf (&optional (increment 1)) +)
(cl:define-modify-macro decf (&optional (increment 1)) -)

(eval-when (:compile-toplevel :toplevel :execute)
  (cl:defun delete-property (plist indicator)
    (let* ((plist  (cons nil plist))
           (current plist)
           (result nil))
      (tagbody
         (go :test)
       :loop
         (setq current (cddr current))
       :test
         (if (endp (cdr current))
             (go :end))
         (if (not (eq indicator (cadr current)))
             (go :loop))
         (rplacd current  (cdddr current))
         (setq result t)
       :end)
      (values (cdr plist) result))))

(cl:defmacro remf (&environment environment place indicator)
  (cl:multiple-value-bind (vars vals store-vars setter getter)
      (get-setf-expansion place environment)
    (if (cdr store-vars) (error "Cannot ~S to a multiple-value place" 'push))
    (let ((vresult    (gensym))
          (vindicator (gensym))
          (temp       (car store-vars)))
      `(let* ((,vindicator ,indicator)
              (,vresult nil)
              ,@(mapcar (function list) vars vals)
              (,temp ,getter))
         (cl:multiple-value-setq (,temp ,vresult) (delete-property ,temp ,vindicator))
         ,setter
         ,vresult))))

(cl:defmacro pop (&environment environment place)
  (cl:multiple-value-bind (vars vals store-vars setter getter)
      (get-setf-expansion place environment)
    (if (cdr store-vars) (error "Cannot ~S to a multiple-value place" 'push))
    (let ((vresult (gensym))
          (temp    (car store-vars)))
      `(let* ((,vresult)
              ,@(mapcar (function list) vars vals)
              (,temp ,getter))
         (setq ,vresult (car ,temp))
         (setq ,temp    (cdr ,temp))
         ,setter
         ,vresult))))

(cl:defmacro push (&environment environment element place)
  (cl:multiple-value-bind (vars vals store-vars setter getter)
      (get-setf-expansion place environment)
    (if (cdr store-vars) (error "Cannot ~S to a multiple-value place" 'push))
    (let ((velement (gensym))
          (temp (car store-vars)))
      `(let* ((,velement ,element)
              ,@(mapcar (function list) vars vals)
              (,temp ,getter))
         (setq ,temp (cons ,velement ,temp))
         ,setter
         ,temp))))

(cl:defmacro pushnew (&environment environment element place
                      &key (key nil keyp) (test nil testp) (test-not nil test-not-p))
  (cl:multiple-value-bind (vars vals store-vars setter getter)
      (get-setf-expansion place environment)
    (if (cdr store-vars) (error "Cannot ~S to a multiple-value place" 'push))
    (let ((velement (gensym))
          (temp     (car store-vars)))
      `(let* ((,velement ,element)
              ,@(mapcar (function list) vars vals)
              (,temp (adjoin ,velement ,getter
                             ,@(if keyp       (list :key key))
                             ,@(if testp      (list :test test))
                             ,@(if test-not-p (list :test-not test-not)))))
         ,setter
         ,temp))))


(eval-when (:compile-toplevel :load-toplevel :execute)
  (cl:defun generate-do (letop setop
                         bindings-and-increments end-test-form
                         result-form decl-and-tag-body)
    (cl:multiple-value-bind (docstrings declarations body) (parse-body :locally decl-and-tag-body)
      (declare (ignore docstrings))
      (let* ((vtag       (gensym))
             (bindings   (mapcar (cl:lambda (binding-and-increment)
                                   (if (atom binding-and-increment)
                                       binding-and-increment
                                       `(,(first binding-and-increment)
                                         ,(second binding-and-increment))))
                                 bindings-and-increments))
             (increments (mapcan (cl:lambda (binding-and-increment)
                                   (if (and (consp binding-and-increment)
                                            (cddr binding-and-increment))
                                       (list (first binding-and-increment)
                                             (third binding-and-increment))))
                                 bindings-and-increments))
             (doblock    `(block nil
                            (tagbody
                               ,vtag
                               ,@(if end-test-form
                                     (list `(if ,end-test-form
                                                (return-from nil ,result-form))))
                               ,@body
                               ,@(if increments
                                     (list `(,setop ,@increments)))
                               (go ,vtag)))))
        (cl:cond
          (bindings     `(,letop ,bindings
                                 ,@declarations
                                 ,doblock))
          (declarations (if declarations
                            `(locally ,@declarations ,doblock)
                            doblock))
          (t            doblock))))))

(cl:defmacro do ((&rest bindings-and-increments)
                 (end-test-form &optional result-form)
                 &body decl-and-tag-body)
  (generate-do 'let 'psetf
               bindings-and-increments end-test-form
               result-form decl-and-tag-body))

(cl:defmacro do* ((&rest bindings-and-increments)
                  (end-test-form &optional result-form)
                  &body decl-and-tag-body)
  (generate-do 'let* 'setq
               bindings-and-increments end-test-form
               result-form decl-and-tag-body))

(cl:defmacro dotimes ((var count-form &optional result-form) &body decs-and-tag-body)
  (let ((ivar   (gensym))
        (vcount (gensym)))
    (generate-do 'let* 'setq
                 `((,vcount ,count-form)
                   (,ivar 0 (1+ ,ivar))
                   (,var ,ivar ,ivar))
                 `(>= ,ivar ,vcount)
                 result-form
                 decs-and-tag-body)))

(cl:defmacro dolist ((var list-form &optional result-form) &body decs-and-tag-body)
  (let ((ivar   (gensym))
        (vlist  (gensym)))
    (generate-do 'let* 'setq
                 `((,vlist ,list-form (cdr ,vlist))
                   (,ivar (car ,vlist) (car ,vlist))
                   (,var ,ivar ,ivar))
                 `(endp ,vlist)
                 result-form
                 decs-and-tag-body)))

(eval-when (:compile-toplevel :load-toplevel :execute)
  (cl:defun generate-simple-loop (body)
    ;; progn prevents parsing declarations, which should not appear in
    ;; a simple loop body.
    (generate-do nil nil nil nil nil `((progn ,@body))))
  (cl:defun generate-extended-loop (body)
    (error "~S Not implemented yet ~S" 'generate-extended-loop body)))

(cl:defmacro loop (&body body)
  (if (every (function consp) body)
      (generate-simple-loop body)
      (generate-extended-loop body)))



;; Local Macro CALL-METHOD                                         07_Object
;; Macro DEFCLASS                                                  07_Object
;; Macro DEFINE-CONDITION                                          09_Condition
;; Macro DEFGENERIC                                                07_Object
;; Macro DEFINE-METHOD-COMBINATION                                 07_Object
;; Macro DEFMETHOD                                                 07_Object

(eval-when (:compile-toplevel :load-toplevel :execute)
  (cl:defun generate-with (symbol-macros instance-var instance-form decl-and-body)
    (cl:multiple-value-bind (docstrings declarations body) (parse-body :locally decl-and-body)
      (declare (ignore docstrings))
      `(let ((,instance-var ,instance-form))
         (symbol-macrolet (,@symbol-macros)
           ,@declarations
           ,@body)))))

(cl:defmacro with-accessor ((&rest slot-entries) instance-form &body decl-and-body)
  (let ((vinstance (gensym)))
    (generate-with vinstance
                   (mapcar (cl:lambda (entry)
                             (cl:destructuring-bind (var accessor) entry
                               (cl:assert (symbolp var) () "Invalid entry ~S ; the variable should be a symbol" entry)
                               (cl:assert (function-name-p accessor) () "Invalid entry ~S ; the function-name should be a symbol a setf-expression or a lambda-expression" entry)
                               `(,var (,accessor ,vinstance))))
                           slot-entries)
                   instance-form
                   decl-and-body)))

(cl:defmacro with-slots ((&rest slot-entries) instance-form &body decl-and-body)
  (let ((vinstance (gensym)))
    (generate-with vinstance (mapcar (cl:lambda (entry)
                                       (cl:destructuring-bind (var slot) (if (symbolp entry) (list entry entry) entry)
                                         (cl:assert (symbolp var)  () "Invalid entry ~S ; the variable should be a symbol" entry)
                                         (cl:assert (symbolp slot) () "Invalid entry ~S ; the slot should be a symbol " entry)
                                         `(,var (slot-value ,vinstance ',slot))))
                                     slot-entries)
                   instance-form
                   decl-and-body)))


;; Macro ASSERT                                                    09_Condition
;; Macro CHECK-TYPE                                                09_Condition

;; Macro HANDLER-BIND                                              09_Condition
;; Macro HANDLER-CASE                                              09_Condition
;; Macro IGNORE-ERRORS                                             09_Condition
;; Macro WITH-CONDITION-RESTARTS                                   09_Condition

;; Macro RESTART-BIND                                              09_Condition
;; Macro RESTART-CASE                                              09_Condition
;; Macro WITH-SIMPLE-RESTART                                       09_Condition

;; Macro DEFPACKAGE                                                11_Package
;; Macro DO-SYMBOLS                                                11_Package
;; Macro DO-EXTERNAL-SYMBOLS                                       11_Package
;; Macro DO-ALL-SYMBOLS                                            11_Package
;; Macro IN-PACKAGE                                                11_Package
;; Macro WITH-PACKAGE-ITERATOR                                     11_Package


;; Macro WITH-HASH-TABLE-ITERATOR                                  18_HashTable
;; Macro WITH-INPUT-FROM-STRING                                    21_Stream
;; Macro WITH-OPEN-STREAM                                          21_Stream
;; Macro WITH-OUTPUT-TO-STRING                                     21_Stream
;; Macro WITH-OPEN-FILE                                            21_Stream

;; Local Macro PPRINT-EXIT-IF-LIST-EXHAUSTED                       22_Printer
;; Local Macro PPRINT-POP                                          22_Printer
;; Macro FORMATTER                                                 22_Printer
;; Macro PPRINT-LOGICAL-BLOCK                                      22_Printer
;; Macro PRINT-UNREADABLE-OBJECT                                   22_Printer

(cl:defmacro with-standard-io-syntax (&body body)
  `(let ((*package*                    (find-package "COMMON-LISP-USER"))
         (*print-array*                t)
         (*print-base*                 10)
         (*print-case*                 :upcase)
         (*print-circle*               nil)
         (*print-escape*               t)
         (*print-gensym*               t)
         (*print-length*               nil)
         (*print-level*                nil)
         (*print-lines*                nil)
         (*print-miser-width*          nil)
         (*print-pprint-dispatch*      nil #|implementation dependant|#)
         (*print-pretty*               nil)
         (*print-radix*                nil)
         (*print-readably*             t)
         (*print-right-margin*         nil)
         (*read-base*                  10)
         (*read-default-float-format*  'single-float)
         (*read-eval*                  t)
         (*read-suppress*              nil)
         (*readtable*                  (copy-readtable nil)))
     ,@body))

(cl:defmacro with-compilation-unit ((&key (override nil)) &body body)
  (warn "~S not implemened yet ~S" 'with-compilation-unit override)
  `(progn ,@body))

(cl:defmacro step (form)
  `(let ()
     ,form))

(cl:defmacro trace   (&rest function-names)
  (declare (ignore function-names))
  nil)

(cl:defmacro untrace (&rest function-names)
  (declare (ignore function-names))
  nil)

(cl:defmacro time (&body body)
  (let ((se (gensym))
        (ee (gensym))
        (sr (gensym))
        (er (gensym)))
    `(let ((,se (get-internal-real-time))
           (,sr (get-internal-run-time)))
       (unwind-protect
            (progn ,@body)
         (let ((,er (get-internal-run-time))
               (,ee (get-internal-real-time)))
           (format *trace-output*
                   "Evaluation of ~S~%Run-time:  ~12,9F seconds~%Real-time: ~12,9F seconds~%"
                   ,(if (null (cdr body))
                        `',(car body)
                        `'(progn ,@body))
                   (/ (- ,er ,sr) internal-time-units-per-second)
                   (/ (- ,ee ,se) internal-time-units-per-second)))))))

;;;; THE END ;;;;
