;;;;  -*- mode:lisp;coding:utf-8 -*-
;;;;**************************************************************************
;;;; FILE:               minimal-compiler.lisp
;;;; LANGUAGE:           Common-Lisp
;;;; SYSTEM:             Common-Lisp
;;;; USER-INTERFACE:     NONE
;;;; DESCRIPTION
;;;;
;;;;     This file implements a minimal-compiler.
;;;;
;;;; AUTHORS
;;;;     <PJB> Pascal Bourguignon <pjb@informatimago.com>
;;;; MODIFICATIONS
;;;;    2021-06-09 <PJB> Completed implementation.
;;;;    2008-05-29 <PJB> Created.
;;;; BUGS
;;;; LEGAL
;;;;     GPL
;;;;
;;;;     Copyright Pascal Bourguignon 2008 - 2021
;;;;
;;;;     This program is free software; you can redistribute it and/or
;;;;     modify it under the terms of the GNU General Public License
;;;;     as published by the Free Software Foundation; either version
;;;;     2 of the License, or (at your option) any later version.
;;;;
;;;;     This program is distributed in the hope that it will be
;;;;     useful, but WITHOUT ANY WARRANTY; without even the implied
;;;;     warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
;;;;     PURPOSE.  See the GNU General Public License for more details.
;;;;
;;;;     You should have received a copy of the GNU General Public
;;;;     License along with this program; if not, write to the Free
;;;;     Software Foundation, Inc., 59 Temple Place, Suite 330,
;;;;     Boston, MA 02111-1307 USA
;;;; *************************************************************************

(cl:defpackage "COM.INFORMATIMAGO.COMMON-LISP.LISP.MINIMAL-COMPILER"
  (:nicknames "COM.INFORMATIMAGO.BOCL.MINIMAL-COMPILER")
  (:nicknames "MC" "MINIMAL-COMPILER")
  (:use "COMMON-LISP")
  (:use "COM.INFORMATIMAGO.COMMON-LISP.CESARUM.SYMBOL")
  (:use "COM.INFORMATIMAGO.COMMON-LISP.LISP-SEXP.SOURCE-FORM")
  (:shadow
   . #1=(
         "COMPILER-MACRO-FUNCTION" "MACRO-FUNCTION"
         "MACROEXPAND" "MACROEXPAND-1"
         "LAMBDA" "SETF"))
  (:export . #1#)
  (:export "FUNCTION-NAME-P" "LAMBDA-EXPRESSION-P" "GO-TAG-P"
           "SYMBOL-MACRO-P" "SYMBOL-MACRO-EXPANSION"

           "MINIMAL-COMPILE/LAMBDA-EXPRESSION"
           "MINIMAL-COMPILE"))
(cl:in-package "COM.INFORMATIMAGO.COMMON-LISP.LISP.MINIMAL-COMPILER")

;;;
;;; Conditions
;;;

(define-condition variable-name-collision-error (program-error)
  ((name :initarg :name :reader variable-name-collision-name)
   (new-definition :initarg :new-definition :reader variable-name-collision-new-definition))
  (:report (cl:lambda (condition stream)
             (format stream "A variable with the name ~S is already defined, while defining: ~S"
                     (variable-name-collision-name condition)
                     (variable-name-collision-new-definition condition)))))


(defvar *macroexpand-hook* 'funcall)
;; It will be (coerce *macroexpand-hook* 'function)


;; symbol-plist keys:
(defconstant +key/compiler-macro-function+ 'compiler-macro-function)


;;;
;;; Utilities
;;;

(defun function-name-p (object)
  (or (symbolp object)
      (and (consp object)
           (or (eql 'setf (car object))
               (cdr object)
               (symbolp (cadr object))
               (null (cddr object))))))

(defun lambda-expression-p (form)
  (and (consp form)
       (eql 'lambda (first form))
       (cdr form)
       (listp (cadr form))))

(defun go-tag-p (form)
  (or (integerp form) (symbolp form)))

(defun coerce-to-function (form)
  (cond
    ((function-name-p form)

     ;; (eval `(function ,form))
     ;; (compile nil form)
     (coerce form 'function))

    ((lambda-expression-p form)

     ;; (eval form)
     ;; (compile nil form)
     (coerce form 'function))

    (t
     (warn "Is that a function? ~S" form)
     (coerce form 'function))))


;;;
;;; Compiler macros
;;;

(defun compiler-macro-function (name &optional environment)
  (if (null environment)
      (get name 'kernel:$compiler-macro-function)
      (env:find-compiler-macro name environment)))

(defun (cl:setf compiler-macro-function) (new-function name &optional environment)
  (if (null environment)
      (cl:setf (get name 'kernel:$compiler-macro-function) new-function)
      (let ((cm (env:find-compiler-macro name environment)))
        (cond
          ((env:compiler-macro-p cm)
           (cl:setf (env:compiler-macro-function cm) new-function))
          ((eql environment (env:global-environment))
           (env:push-function-onto-environment
            (env:make-compiler-macro :name name :function new-function)
            (env:global-environment)))
          (t
           (error "Compiler macros can be stored only in the global environment.")))))
  new-function)


;;;
;;; Symbol macros
;;;

(defun symbol-macro-p (symbol environment)
  (if (null environment)
      (let ((not-there (cons nil nil)))
        (not (eql not-there (get symbol 'kernel:$symbol-macro-expansion not-there))))
      (env:symbol-macro-p (env:find-variable symbol environment))))

(defun symbol-macro-expansion (symbol &optional environment)
  (if (null environment)
      (get symbol 'kernel:$symbol-macro-expansion)
      (let ((sm (env:find-variable symbol environment)))
        (when (env:symbol-macro-p sm)
          (env:symbol-macro-expansion sm)))))

(defun (cl:setf symbol-macro-expansion) (new-expansion symbol &optional environment)
  (if (null environment)
      (cl:setf (get symbol 'kernel:$symbol-macro-expansion) new-expansion)
      (let ((sm (env:find-variable symbol environment)))
        (cond
          ((null sm)
           (env:push-variable-onto-environment
            (if (eql environment (env:global-environment))
                (env:global-symbol-macro symbol new-expansion)
                (env:local-symbol-macro  symbol new-expansion))
            environment))
          ((env:symbol-macro-p sm)
           (cl:setf (env:symbol-macro-expansion sm) new-expansion))
          (t (error "There's already a ~A named ~S" (type-of sm) symbol)))))
  new-expansion)

(defun so/symbol-macrolet (symbol expansion environment)
  (check-type symbol symbol)
  (cl:setf (symbol-macro-expansion symbol environment) expansion))

(defun minimal-compile/symbol-macrolet (env form)
  ;; TODO: check environment management
  (destructuring-bind (symbol-macrolet (&rest bindings) &body decl-and-body) form
    (declare (ignore symbol-macrolet))
    (let ((env (env:extend-environment-with-functions '() env)))
      (dolist (binding bindings)
        (destructuring-bind (name expansion) binding
          ;; add the symbol-macrolet to the environment
          (so/symbol-macrolet name expansion env)))
      (multiple-value-bind (docstrings declarations body) decl-and-body
        (declare (ignore docstrings)
                 (ignore declarations)) ; TODO: where to use it?
        `(progn
           ,@(mapcar (cl:lambda (subform) (MINIMAL-COMPILE env subform)) body))))))


;;;
;;; Normal macros
;;;

(defun macro-function (symbol &optional environment)
  (if (null environment)
      (cl:macro-function symbol nil)
      (let ((mac (env:find-function symbol environment)))
        (if (env:macrop mac)
            (env:macro-function mac)
            nil))))

(defun (cl:setf macro-function) (new-function symbol &optional environment)
  (if (null environment)
      (cl:setf (cl:macro-function symbol nil) new-function)
      (let ((mac (env:find-function symbol environment)))
        (cond
          ((null mac)
           (env:push-function-onto-environment
            (if (eql environment (env:global-environment))
                (env:make-global-macro :name symbol :function new-function)
                (env:make-local-macro  :name symbol :function new-function))
            environment))
          ((env:macrop mac)
           (cl:setf (env:macro-function mac) new-function))
          (t (error "There's already a ~A named ~S" (type-of mac) symbol)))))
  new-function)

(defun so/macrolet (name lambda-list decl-and-body environment)
  (let* ((ll            (parse-lambda-list lambda-list :macro))
         (whole         (if (lambda-list-whole-parameter-p ll)
                            (parameter-specifier (lambda-list-whole-parameter ll))
                            (gensym "whole")))
         (env-parameter (if (lambda-list-environment-parameter-p ll)
                            (parameter-specifier (lambda-list-environment-parameter ll))
                            (gensym "environment"))))
    (remove-whole-parameter ll)
    (remove-environment-parameter ll)
    (multiple-value-bind (docstrings declarations body) (parse-body :lambda decl-and-body)
      (declare (ignore docstrings))
      ;; TODO: what about the other slots of the environment object?
      ;; TODO: minimal-compilation of the macro function.
      (cl:setf (macro-function name environment)
            (coerce-to-function `(lambda (,whole ,env-parameter)
                                   (declare (ignorable ,env-parameter))
                                   (destructuring-bind ,lambda-list ,whole
                                     ,@declarations
                                     ,@body)))))))

(defun minimal-compile/macrolet (env form)
  ;; TODO: check environment management
  (destructuring-bind (macrolet (&rest bindings) &body decl-and-body) form
    (declare (ignore macrolet))
    (let ((env (env:extend-environment-with-functions '() env)))
      (dolist (binding bindings)
        (destructuring-bind (name lambda-list &body decl-and-body) binding
          ;; add the macrolet to the environment
          (so/macrolet name lambda-list decl-and-body env)))
      (multiple-value-bind (docstrings declarations body) decl-and-body
           (declare (ignore docstrings)
                 (ignore declarations)) ; TODO: where to use it?
        `(progn
           ,@(mapcar (cl:lambda (subform) (MINIMAL-COMPILE env subform)) body))))))


(defun macroformp (form environment)
  (and (consp form) (macro-function (first form) environment)))

(defun macroexpand (form &optional environment)
  (loop
     (multiple-value-bind (expansion expandedp) (macroexpand-1 form environment)
       (unless expandedp
         (return-from macroexpand (values expansion expandedp)))
       (cl:setf form expansion))))

(defun macroexpand-1 (form &optional environment)
  (cond ((consp form)
         (let ((macro (macro-function (first form) environment)))
           (if macro
               (values (funcall (coerce-to-function *macroexpand-hook*)
                                macro
                                form environment)
                       t)
               (values form nil))))
        ((symbolp form)
         (if (symbol-macro-p form environment)
             (values (funcall (coerce-to-function *macroexpand-hook*)
                              (constantly (symbol-macro-expansion form environment))
                              form environment)
                     t)
             (values form nil)))
        (t
         (values form nil))))


;;;
;;; Minimal compiler
;;;


(declaim (inline operator arguments))
(defun operator  (form) (first form))
(defun arguments (form) (rest  form))


(defun minimal-compile/symbol (env form)
  (let ((expansion (macroexpand form env)))
    (if (eql expansion form)
        form
        (minimal-compile env expansion))))

(defun minimal-compile/quote (env form)
  (declare (ignore env))
  (destructuring-bind (quote object) form
    `(,quote ,object)))

(eval-when (:compile-toplevel :load-toplevel :execute)

  (defun minimal-compile/lambda-list (env ll specials)
    "Mutates the lambda-list ll, replacing the initform expressions by their minimally compiled versions."
    (loop
      ;; Open a new environment frame for the lambda-list:
    :with llenv := (env:make-environment :next env)
      :for parameter :in (lambda-list-parameters ll)
      :do (when (parameter-initform-p parameter)
            (cl:setf (parameter-initform parameter)
                  (minimal-compile llenv (parameter-initform parameter))))
          ;; Add the new parameter variable bindings to the environment:
          (dolist (name (parameter-parameter-list parameter))
            (env:push-variable-onto-environment
             (if (or (member name specials)
                     (let ((var (env:find-variable name env)))
                       (and var (env:global-special-variable-p var))))
                 (env:local-special-variable name)
                 (env:lexical-variable name))
             llenv))
      :finally (return (values ll env))))

  (defun minimal-compile/lambda-expression (env form)
    (destructuring-bind (lambda (&rest lambda-list) &body decl-and-body) form
      (assert (eq lambda 'lambda))
      (multiple-value-bind (declarations docstrings body) decl-and-body
        (declare (ignore docstrings))
        (let ((ll           (parse-lambda-list lambda-list :ordinary))
              (new-specials (mapcan (cl:lambda (decl) (copy-list (rest decl)))
                                    (remove 'special declarations
                                            :key (function first)
                                            :test-not (function eql)))))
          (multiple-value-bind (mc-ll mc-env) (minimal-compile/lambda-list env ll new-specials)
            (let ((mc-env (env:extend-environment-with-declarations declarations mc-env)))
              `(lambda ,(make-lambda-list mc-ll)
                 ,@declarations
                 ,@(mapcar (cl:lambda (subform) (minimal-compile mc-env subform))
                           body))))))))

  );;eval-when

(defun minimal-compile/function (env form)
  (destructuring-bind (function fname) form
    (if (lambda-expression-p fname)
        ;; TODO: generate a closure? coerce-to-closure env le ?
        ;; TODO: return a function object? coerce-to-function ?
        `(,function ,(minimal-compile/lambda-expression env fname))
        `(,function ,fname))))

(defun minimal-compile/call (env form)
  (destructuring-bind (function &rest arguments) form
    `(,(if (lambda-expression-p function)
           ;; TODO: generate a closure?
           (minimal-compile/lambda-expression env function)
           function)
      ,@(mapcar (cl:lambda (expr) (minimal-compile env expr)) arguments))))

(defun minimal-compile/if (env form)
  (destructuring-bind (if test then &optional else) form
    `(,if ,(minimal-compile env test)
       ,(minimal-compile env then)
       ,(minimal-compile env else))))

(defun minimal-compile/block (env form)
  (destructuring-bind (block name &body body) form
    (let ((env (env:extend-environment-with-blocks (list (env:block name)) env)))
      `(,block ,name
         ,@(mapcar
            (cl:lambda (subform) (minimal-compile env subform))
            body)))))

(defun minimal-compile/return-from (env form)
  (destructuring-bind (return-from name &optional value) form
    (let ((blk (env:find-block name env)))
      (unless (env:blockp blk)
        (error "No block named ~S in the lexical scope." name)))
    `(,return-from ,name ,(minimal-compile env value))))

(defun minimal-compile/catch (env form)
  (destructuring-bind (catch object &body body) form
    `(,catch ,(minimal-compile env object)
       ,@(mapcar (cl:lambda (subform) (minimal-compile env subform)) body))))

(defun minimal-compile/throw (env form)
  (destructuring-bind (throw object &optional value) form
    `(,throw ,(minimal-compile env object) ,(minimal-compile env value))))

(defun minimal-compile/unwind-protect (env form)
  (destructuring-bind (unwind-protect expression &body cleanup-body) form
    `(,unwind-protect ,(minimal-compile env expression)
       ,@(mapcar (cl:lambda (subform) (minimal-compile env subform)) cleanup-body))))

(defconstant +TAG-BITS+         (byte 32 0))
(defconstant +TAGBODY-BITS+     (byte 30 32))

(defun compute-tag-index (environment tagname)
  (labels ((tagbody-environments (environment)
             (when environment
               (if (env:environment-tags environment)
                   (cons environment (tagbody-environments (env:environment-next environment)))
                   (tagbody-environments (env:environment-next environment))))))
    (let* ((tagbody-envs (tagbody-environments environment))
           (tbenv (member tagname tagbody-envs
                          :key (function env:tagbody-tags)
                          :test (function member))))
      (when tbenv
        (dpb (length (rest tbenv)) +tagbody-bits+
             (position tagname (env:tagbody-tags (first tbenv))))))))

(defun minimal-compile/tagbody (env form)
  (labels ((go-tag-p (object)
             (or (symbolp object) (integerp object)))
           (go-form-p (object)
             (and (consp object) (eql (car object) 'go) (cdr object) (go-tag-p (cadr object)) (not (cddr object))))
           (parse-tagbody (body)
             (coerce (mapcon (cl:lambda (form) (if (go-tag-p (car form)) (list form))) body) 'vector)))
    (destructuring-bind (tagbody &body body) form
      (let* ((body     (copy-tree body)) ; we'll be mutating the tags cells.
             ;; Build the tag-to-index map into the environment for subform GO.
             (tagtable (parse-tagbody body))
             (etb      (env:make-tagbody))
             (tags     (loop :for i :below (length tagtable)
                             :collect (env:tag (aref tagtable 0) i etb)))
             (env      (env:extend-environment-with-tags tags env)))
        (cl:setf (env:tagbody-tags etb) tags)
        (prog1 `(,tagbody
                   ,tagtable
                   ,@(mapcar (cl:lambda (subform)
                               (cond
                                 ((go-tag-p subform)
                                  subform)
                                 ((go-form-p subform)
                                  (let* ((tagname (cadr subform))
                                         (tag-index (compute-tag-index env tagname)))
                                    (if tag-index
                                        (error "No tag ~S in the lexical scope" tagname)
                                        `(go ,tag-index))))
                                 (t
                                  (minimal-compile env subform))))
                             body))
          (dotimes (i (length tagtable))
            (cl:setf (car (aref tagtable i)) (compute-tag-index env (aref tagtable i)))))))))

#|
(pprint
 (minimal-compile/tagbody nil '(tagbody (setq val 1)
                                (go point-a)
                                (setq val (+ val 16))
                                point-c (setq val (+ val 4))
                                (go point-b)
                                (setq val (+ val 32))
                                point-a (setq val (+ val 2))
                                (go point-c)
                                (setq val (+ val 64))
                                point-b (setq val (+ val 8)))))

(minimal-compile/tagbody (env:global-environment)
                         '(tagbody
                           start
                           (go end)
                           (go start)
                           end))

(minimal-compile/tagbody (env:global-environment)
                         '(tagbody
                           start
                           (go end)
                           (tagbody
                            loop
                              (go loop)
                              (go start)
                              (go end)
                            end)
                           (go start)
                           end))

|#

(defun minimal-compile/go (env form)
  (destructuring-bind (go tagname) form
    (let ((tag-index (compute-tag-index env tagname)))
      (if tag-index
          (error "No tag ~S in the lexical scope" tagname)
          `(,go ,tag-index)))))

(defun minimal-compile/flet (env form)
  (destructuring-bind (flet bindings &body body) form
    (let ((funs (mapcar
                 (cl:lambda (fun)
                   ;; Return a cons: ( mc-fun-clause  . fun-object )
                   (destructuring-bind (fname lambda-list &body decl-and-body) fun
                     (let ((ll (parse-lambda-list lambda-list :ordinary)))
                       (multiple-value-bind (declarations docstrings body) decl-and-body
                         (declare (ignore docstrings))
                         (let* ((new-specials (mapcan (cl:lambda (decl) (copy-list (rest decl)))
                                                      (remove 'special declarations
                                                              :key (function first)
                                                              :test-not (function eql))))
                                (env (env:extend-environment-with-variables
                                      (mapcar (cl:lambda (parameter)
                                                (if (or (member parameter new-specials)
                                                        (let ((var (env:find-variable parameter env)))
                                                          (and var (env:global-special-variable-p var))))
                                                    (env:local-special-variable parameter)
                                                    (env:lexical-variable parameter)))
                                              (lambda-list-parameters ll))
                                      env))
                                (env (env:extend-environment-with-declarations declarations env))
                                (mc-body (mapcar (cl:lambda (form) (minimal-compile env form))
                                                 body)))
                           (cons
                            `(,fname ,lambda-list ,@mc-body)
                            (env:make-local-function :name fname
                                                     :lambda-list ll
                                                     :body `(lambda ,lambda-list
                                                              ,@declarations
                                                              (block ,fname ,@mc-body))
                                                     :environment env)))))))
                 bindings)))
      `(,flet ,(mapcar (function car) funs)
         ,@(let ((env (env:extend-environment-with-functions (mapcar (function cdr) funs) env)))
             (mapcar (cl:lambda (subform) (minimal-compile env subform)) body))))))


(defun minimal-compile/labels (env form)
  (destructuring-bind (labels bindings &body body) form
    (let* ((funinfos (mapcar (cl:lambda (fun)
                               (destructuring-bind (fname lambda-list &body decl-and-body) fun
                                 (let ((ll (parse-lambda-list lambda-list :ordinary)))
                                   (multiple-value-bind (declarations docstrings body) decl-and-body
                                     (declare (ignore docstrings))
                                     (list (env:make-local-function :name fname :lambda-list ll)
                                           lambda-list declarations body)))))
                             bindings))
           (env  (env:extend-environment-with-functions
                  (mapcar (function first) funinfos)
                  env))
           (funs (mapcar
                  (cl:lambda (funinfo)
                    (destructuring-bind (funobj lambda-list declarations body) funinfo
                      (let* ((fname       (env:function-name        funobj))
                             (ll          (env:function-lambda-list funobj))
                             (new-specials (mapcan (cl:lambda (decl) (copy-list (rest decl)))
                                                   (remove 'special declarations
                                                           :key (function first)
                                                           :test-not (function eql))))
                             (benv (env:extend-environment-with-variables
                                    (mapcar (cl:lambda (parameter)
                                              (if (or (member parameter new-specials)
                                                      (let ((var (env:find-variable parameter env)))
                                                        (and var (env:global-special-variable-p var))))
                                                  (env:local-special-variable parameter)
                                                  (env:lexical-variable parameter)))
                                            (lambda-list-parameters ll))
                                    env))
                             (benv (env:extend-environment-with-declarations declarations benv))
                             (mc-body (mapcar (cl:lambda (form) (minimal-compile benv form))
                                              body)))
                        (cl:setf (env:local-function-environment funobj) env
                                 (env:function-body funobj) `(lambda ,lambda-list
                                                               ,@declarations
                                                               (block ,fname
                                                                 ,@mc-body)))
                        `(,fname ,lambda-list ,@mc-body))))
                  funinfos)))
      `(,labels ,funs
         ,@(mapcar (cl:lambda (subform) (minimal-compile env subform))
                   body)))))

(defun minimal-compile/setq (env form)
  (destructuring-bind (setq &rest var-val-pairs) form
    (declare (ignore setq))
    (if var-val-pairs
        (loop
           :with has-symbol-macros = nil
           :with has-normal-variables = nil
           :for (var nil) :on var-val-pairs :by (function cddr)
           :until (and has-symbol-macros has-normal-variables)
           :do (if (symbol-macro-p var env)
                   (cl:setf has-symbol-macros t)
                   (cl:setf has-normal-variables t))
           :finally (return
                      (cond
                        ((and has-symbol-macros has-normal-variables)
                         (minimal-compile env
                                          `(progn
                                             ,@(loop
                                                  :for (var val) :on var-val-pairs :by (function cddr)
                                                  :collect (if (symbol-macro-p var env)
                                                               `(setq ,var ,val)
                                                               `(setf ,var ,val))))))
                        (has-symbol-macros ; only symbol-macros
                         (minimal-compile env `(setf ,@var-val-pairs)))
                        ;; only variables
                        ((null (cddr var-val-pairs))
                         `(setq ,(first var-val-pairs) ,(minimal-compile env (second var-val-pairs))))
                        (t
                         `(progn
                            ,@(loop
                                 :for (var val) :on var-val-pairs :by (function cddr)
                                 :collect `(setq ,var ,(minimal-compile env val))))))))
        'nil)))


(defun minimal-compile/let (env form)
  (destructuring-bind (let bindings &body decl-and-body) form
    (multiple-value-bind (docstrings declarations body) (parse-body :locally decl-and-body)
      (declare (ignore docstrings))
      (let* ((new-specials (mapcan (cl:lambda (decl) (copy-list (rest decl)))
                                   (remove 'special declarations
                                           :key (function first)
                                           :test-not (function eql))))
             (lenv (env:extend-environment-with-variables
                    (mapcar (cl:lambda (binding)
                              (let ((name (if (atom binding)
                                              binding
                                              (first binding))))
                                (if (or (member name new-specials)
                                        (let ((var (env:find-variable name env)))
                                          (and var (env:global-special-variable-p var))))
                                    (env:local-special-variable name)
                                    (env:lexical-variable name))))
                            bindings)
                    env))
             (lenv (env:extend-environment-with-declarations declarations lenv)))
        `(,let ,(mapcar (cl:lambda (binding)
                          (if (atom binding)
                              `binding
                              `(,(first binding) ,(minimal-compile env (second binding)))))
                 bindings)
           ,@declarations
           ,@(mapcar (cl:lambda (subform) (minimal-compile lenv subform))
                     body))))))


(defun minimal-compile/let* (env form)
  (destructuring-bind (let* bindings &body body) form
    (declare (ignore let*))
    (minimal-compile
     env (labels ((wrap (bindings expr)
                    (if (null bindings)
                        expr
                        (wrap (cdr bindings) `(let (,(car bindings)) ,expr)))))
           (let ((bindings (reverse bindings)))
             (wrap (rest bindings) `(let (,(first bindings)) ,@body)))))))


(defun minimal-compile/multiple-value-call (env form)
  (destructuring-bind (multiple-value-call function-form &rest forms) form
    `(,multiple-value-call
         ,@(mapcar (cl:lambda (expr) (minimal-compile env expr))
                   (cons function-form forms)))))


(defun minimal-compile/multiple-value-prog1 (env form)
  (destructuring-bind (multiple-value-prog1 first-form &body body) form
    `(,multiple-value-prog1
         ,@(mapcar (cl:lambda (expr) (minimal-compile env expr))
                   (cons first-form body)))))


(defun minimal-compile/progn (env form)
  (destructuring-bind (progn &body body) form
    `(,progn
       ,@(mapcar (cl:lambda (expr) (minimal-compile env expr)) body))))


(defun minimal-compile/progv (env form)
  (destructuring-bind (progv symbols values forms) form
    `(,progv
       ,@(mapcar (cl:lambda (expr) (minimal-compile env expr))
                 (list* symbols values forms)))))


(defun minimal-compile/locally (env form)
  (destructuring-bind (locally &body body) form
    (multiple-value-bind (docstrings declarations forms) (parse-body :locally body)
      (declare (ignore docstrings))
      (let ((env (env:extend-environment-with-declarations declarations env)))
        `(,locally ,declarations
           ,@(mapcar (cl:lambda (expr) (minimal-compile env expr)) forms))))))


(defun minimal-compile/the (env form)
  (destructuring-bind (the value-type expr) form
    `(,the ,value-type ,(minimal-compile env expr))))


(defun minimal-compile/eval-when (env form)
  (destructuring-bind (eval-when situations &rest forms) form
    `(,eval-when ,situations
       ,@(mapcar (cl:lambda (expr) (minimal-compile env expr)) forms))))


(defun minimal-compile (env form)
  (cond
    ((symbolp form)            (minimal-compile/symbol               env form))
    ;; The other atoms are unchanged:
    ((atom form)               form)
    ;; Now we have a list.
    (t
     (case (operator form)
       ;; First we check the special operators:
       ((FUNCTION)             (minimal-compile/function             env form))
       ((QUOTE)                (minimal-compile/quote                env form)) ;
       ((SETQ)                 (minimal-compile/setq                 env form))
       ((IF)                   (minimal-compile/if                   env form)) ;
       ((BLOCK)                (minimal-compile/block                env form)) ;
       ((RETURN-FROM)          (minimal-compile/return-from          env form)) ;
       ((CATCH)                (minimal-compile/catch                env form)) ;
       ((THROW)                (minimal-compile/throw                env form)) ;
       ((UNWIND-PROTECT)       (minimal-compile/unwind-protect       env form)) ;
       ((TAGBODY)              (minimal-compile/tagbody              env form)) ;
       ((GO)                   (minimal-compile/go                   env form)) ;
       ((LET)                  (minimal-compile/let                  env form))
       ((LET*)                 (minimal-compile/let*                 env form))
       ((FLET)                 (minimal-compile/flet                 env form))
       ((LABELS)               (minimal-compile/labels               env form))
       ((SYMBOL-MACROLET)      (minimal-compile/symbol-macrolet      env form))
       ((MACROLET)             (minimal-compile/macrolet             env form))
       ((MULTIPLE-VALUE-CALL)  (minimal-compile/multiple-value-call  env form))
       ((MULTIPLE-VALUE-PROG1) (minimal-compile/multiple-value-prog1 env form))
       ((PROGN)                (minimal-compile/progn                env form)) ;
       ((PROGV)                (minimal-compile/progv                env form))
       ((LOCALLY)              (minimal-compile/locally              env form)) ;
       ((THE)                  (minimal-compile/the                  env form)) ;
       ((EVAL-WHEN)            (minimal-compile/eval-when            env form)) ;
       ((LOAD-TIME-VALUE)
        ;; do...
        )
       (otherwise
        (let ((cmf (compiler-macro-function (operator form) env))
              (mf  (macro-function (operator form) env)))
          (cond
            (cmf
             (let ((expansion (funcall (coerce-to-function *macroexpand-hook*)
                                       cmf form env)))
               (minimal-compile env expansion)))
            (mf
             (let ((expansion (funcall (coerce-to-function *macroexpand-hook*)
                                       mf form env)))
               (minimal-compile env expansion)))
            (t (minimal-compile/call env form)))))))))


;; #-(and)
;; (defun macroexpand-all (form &optional env)
;;   "Recursivly expand all macros in FORM."
;;   ;; TODO: instanciated global environment
;;   (unless environment (setf environment *global-environment*))
;;   (flet ((mexpand (forms env)
;;            (mapcar (cl:lambda (form) (macroexpand-all form env)) forms)))
;;     (macrolet ((destructuring-bind-body (binds form &body body)
;;                  (if (eql '&body (first (last binds)))
;;                      (let ((&body (gensym "&BODY")))
;;                        `(destructuring-bind ,(append (butlast binds) (list '&body body)) ,form
;;                           (multiple-value-bind (docstrings declarations body) (parse-body :locally body)
;;                             ,@body)))
;;                      `(destructuring-bind ,binds ,form ,@body))))
;;       (multiple-value-bind (expansion win) (macroexpand-1 form env)
;;         (if win
;;             (macroexpand-all expansion env)
;;             (if (atom form)
;;                 form
;;                 (case (first form)
;;
;;                   ((macrolet)
;;                    (destructuring-bind-body (macros &body) (rest form)
;;                                             (setf env (augment-environment env
;;                                                                            :macro (mapcar (cl:lambda (macro)
;;                                                                                             (destructuring-bind
;;                                                                                                 (name arglist &body body)
;;                                                                                                 macro
;;                                                                                               (list name (enclose (parse-macro name arglist body env)))))
;;                                                                                           macros)
;;                                                                            :declare (decl-specs-from-declarations decls)))
;;                                             (let ((body (mexpand body env)))
;;                                               (if decls
;;                                                   `(locally ,@decls ,@body)
;;                                                   `(progn ,@body)))))
;;
;;                   ((symbol-macrolet)
;;                    (destructuring-bind-body (symbol-macros &body) (rest form)
;;                                             (setf env (augment-environment env :symbol-macro symbol-macros :declare (decl-specs-from-declarations decls)))
;;                                             (let ((body (mexpand body env)))
;;                                               (if decls
;;                                                   `(locally ,@decls ,@body)
;;                                                   `(progn ,@body)))))
;;
;;                   ((let let* compiler-let)
;;                    (destructuring-bind-body (bindings &body) (rest form)
;;                                             `(,(first form)
;;                                                ,(mapcar (cl:lambda (binding)
;;
;;                                                           (if (listp binding)
;;                                                               (list (first binding) (macroexpand-all (second binding) env))
;;                                                               binding))
;;                                                         bindings)
;;                                                ,@decls
;;                                                ,@(mexpand body env))))
;;
;;                   ((flet labels)
;;                    (destructuring-bind-body (bindings &body) (rest form)
;;                                             (let ((augmented-env
;;                                                     (augment-environment env :function (mapcar #'car bindings))))
;;                                               `(,(first form)
;;                                                 ,(mapcar (cl:lambda (binding)
;;                                                            (list* (first binding)
;;                                                                   (cdr (macroexpand-all `(lambda ,@(rest binding))
;;                                                                                         (if (eq (first form) 'labels)
;;                                                                                             augmented-env
;;                                                                                             env)))))
;;                                                          bindings)
;;                                                 ,@decls
;;                                                 ,@(mexpand body augmented-env)))))
;;                   (function
;;                    (if (and (consp (second form))
;;                             (eql 'lambda (first (second form))))
;;                        (destructuring-bind (cl:lambda arglist &body body&decls)
;;                            (second form)
;;                          (declare (ignore lambda))
;;                          (multiple-value-bind (body decls)
;;                              (parse-body body&decls env)
;;                            `(lambda ,arglist ,@decls ,@(mexpand body env))))
;;                        form))
;;
;;                   ((eval-when the locally block return-from)
;;                    (list* (first form) (second form) (mexpand (cddr form) env)))
;;
;;                   (setq
;;                    `(setq ,@(loop for (name value) on (rest form) by #'cddr
;;                                   collect name
;;                                   collect (macroexpand-all value env))))
;;
;;                   ((go quote) form)
;;
;;                   ((fbind with-c-frame with-variable-c-frame ppc-lap-function)
;;                    (error "Unable to macroexpand ~S." form))
;;
;;                   ((catch if load-time-value multiple-value-call multiple-value-prog1 progn
;;                           progv tagbody throw unwind-protect)
;;                    (cons (first form) (mexpand (rest form) env)))
;;
;;                   (t
;;                    ;; need to check that (first form) is either fboundp or a local function...
;;                    (cons (first form) (mexpand (rest form) env))))))))))


;;;; THE END ;;;;
