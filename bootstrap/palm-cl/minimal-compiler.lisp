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
;;;;     2008-05-29 <PJB> Created.
;;;; BUGS
;;;; LEGAL
;;;;     GPL
;;;;     
;;;;     Copyright Pascal Bourguignon 2008 - 2008
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

(defpackage "MINIMAL-COMPILER"
  (:nicknames "MC")
  (:use "CL")
  (:shadow "*MACROEXPAND-HOOK*"
           "COMPILER-MACRO-FUNCTION"
           "DEFINE-COMPILER-MACRO"))
(in-package "MINIMAL-COMPILER")


(defstruct environment
  variables
  functions
  symbol-macros
  macros)

(defun extend-environment-with-lambda-list (lambda-list env)
  (make-environment
   :variables     (append lambda-list (environment-variables env))
   :functions     (environment-functions     env)
   :symbol-macros (environment-symbol-macros env)
   :macros        (environment-macros        env)))

(defun extend-environment-with-variables (variables env)
  (make-environment
   :variables     (append variables (environment-variables env))
   :functions     (environment-functions     env)
   :symbol-macros (environment-symbol-macros env)
   :macros        (environment-macros        env)))

(defun extend-environment-with-functions (functions env)
  (make-environment
   :variables     (environment-variables     env)
   :functions     (append functions (environment-functions env))
   :symbol-macros (environment-symbol-macros env)
   :macros        (environment-macros        env)))


(defun symbol-macro-p (sym env)
  (assoc sym (environment-symbol-macros env)))


(defvar *macroexpand-hook* 'funcall) ; it will be (coerce *macroexpand-hook* 'function)


(defun compiler-macro-function (name &optional environment)
  (declare (ignore environment))
  (get name 'compiler-macro-function nil))


(defun (setf compiler-macro-function) (new-function name &optional environment)
  (assert (null environment) (environment))
  (setf (get name 'compiler-macro-function nil) new-function))


(defmacro define-compiler-macro (name lambda-list &body decl-doc-body)
  ;; extract from lambda-list whole and environment
  (let ((whole)
        (environment)
        (lambda-list)
        (docstring)
        (declarations)
        (body))
    `(progn
       (setf (compiler-macro-function ',name)
             (lambda (,whole ,environment)
                 (destructuring-bind ,lambda-list ,whole
                   (locally ,@declarations ,@body)))
              (documentation ',name 'compiler-macro) ',docstring)
       ',name)))


(declaim (inline operator arguments))
(defun operator  (form) (first form))
(defun arguments (form) (rest  form))


(defun minimal-compile/symbol              (env form)
  (let ((symac (assoc form (environment-symbol-macros env))))
    (if symac
        (minimal-compile env (cdr symac))
        form)))

(defun minimal-compile/function             (env form)
  (declare (ignore env))
  (destructuring-bind (function fname) form
    `(,function ,fname)))

(defun minimal-compile/quote                (env form)
  (declare (ignore env))
  (destructuring-bind (quote object) form
    `(,quote ,object)))

(defun minimal-compile/if                   (env form)
  (destructuring-bind (if test then &optional else) form
    `(,if ,(minimal-compile env test)
       ,(minimal-compile env then)
       ,(minimal-compile env else))))

(defun minimal-compile/block                (env form)
  (destructuring-bind (block name &rest body) form
    `(,block ,name
       ,@(mapcar (lambda (subform) (minimal-compile env subform)) body))))

(defun minimal-compile/return-from          (env form)
  (destructuring-bind (return-from name &optional value) form
    `(,return-from ,name ,(minimal-compile env value))))

(defun minimal-compile/catch                (env form)
  (destructuring-bind (catch object &rest body) form
    `(,catch ,(minimal-compile env object)
       ,@(mapcar (lambda (subform) (minimal-compile env subform)) body))))

(defun minimal-compile/throw                (env form)
  (destructuring-bind (throw object &optional value) form
    `(,throw ,(minimal-compile env object) (minimal-compile env value))))

(defun minimal-compile/unwind-protect       (env form)
  (destructuring-bind (unwind-protect expression &rest body) form
    `(,unwind-protect ,(minimal-compile env expression)
       ,@(mapcar (lambda (subform) (minimal-compile env subform)) body))))
  
(defun minimal-compile/tagbody              (env form)
  (destructuring-bind (tagbody &rest body) form
    `(,tagbody
        ,@(mapcar (lambda (subform)
                      (if (atom subform)
                        subform         ; a tag
                        (minimal-compile env subform)))
                  body))))

(defun minimal-compile/go                   (env form)
  (destructuring-bind (go label) form
    `(,go ,label)))

(defun minimal-compile/flet                 (env form)
  (destructuring-bind (flet bindings &rest body) form
    `(,flet
         ,(mapcar (lambda (fun) `(,(first fun) ,(second fun)
                              ,(minimap-compile
                                (extend-environment-with-lambda-list
                                 (second fun) env)
                                form)))
                  bindings)
       ,@(let ((env (extend-environment-with-functions
                     (mapcar (function first) bindings) env)))
              (mapcar (lambda (subform)
                        (if (atom subform)
                            subform     ; a tag
                            (minimal-compile env subform)))
                      body)))))

(defun minimal-compile/labels                (env form)
  (destructuring-bind (labels bindings &rest body) form
    (let ((env (extend-environment-with-functions
                      (mapcar (function first) bindings) env)))
      `(,labels
           ,(mapcar (lambda (fun) `(,(first fun) ,(second fun)
                               ,(minimap-compile
                                 (extend-environment-with-lambda-list
                                  (second fun) env)
                                 form)))
                   bindings)
         ,@(mapcar (lambda (subform)
                     (if (atom subform)
                         subform        ; a tag
                         (minimal-compile env subform)))
                   body)))))


(defun minimal-compile/setq                 (env form)
  (destructuring-bind (setq &rest var-val-pairs) form
    (if var-val-pairs
        (loop
           :with has-symbol-macros = nil
           :with has-normal-variables = nil
           :for (var val) :on var-val-pairs :by (function cddr)
           :until (and has-symbol-macros has-normal-variables)
           :do (if (symbol-macro-p var env)
                   (setf has-symbol-macros t)
                   (setf has-normal-variables t))
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
                         `(setq ,(first var-val-pairs)
                                ,(minimal-compile env (second var-val-pairs))))
                        (t                
                         `(progn
                            ,@(loop
                                 :for (var val) :on var-val-pairs :by (function cddr)
                                 :collect `(setq ,var ,(minimal-compile env val))))))))
        'nil)))


(defun split-declarations (body)
  (loop
     :for forms :on body
     :while (and (listp (car forms))
                 (eq 'declare (caar forms)))
     :collect (car forms) :into declarations
     :finally (return (values declarations forms))))


(defun minimal-compile/let                  (env form)
  (destructuring-bind (let bindings &rest body) form
      `(,let
           ,(mapcar (lambda (binding)
                      (if (atom binding)
                          `binding
                          `(,(first binding)
                             ,(minimal-compile env (second binding)))))
                    bindings)
         ,@(let ((env (extend-environment-with-variables
                       (mapcar (lambda (binding)
                                 (if (atom binding)
                                     binding
                                     (first binding)))
                               bindings)
                       env)))
                (multiple-value-bind (declarations forms)
                    (split-declarations body)
                  (append declarations
                          (mapcar (lambda (subform) (minimal-compile env subform))
                                  forms)))))))

(defun minimal-compile/let*                 (env form)
  (destructuring-bind (let* bindings &rest body) form
    (minimal-compile
     env (labels ((wrap (bindings expr)
                    (if (null bindings)
                        expr
                        (wrap (cdr bindings) `(let (,(car bindings)) ,expr)))))
           (wrap (reverse bindings) `(,let () ,@body))))))


(defun minimal-compile/multiple-value-call  (env form)
  (destructuring-bind (multiple-value-call function-form &rest forms) form
    `(,multiple-value-call
         ,@(mapcar (lambda (expr) (minimal-compile env expr))
                   (cons function-form forms)))))


(defun minimal-compile/multiple-value-prog1 (env form)
  (destructuring-bind (multiple-value-prog1 first-form &rest body) for
    `(,multiple-value-prog1
         ,@(mapcar (lambda (expr) (minimal-compile env expr))
                   (cons first-form body)))))


(defun minimal-compile/progn                (env form)
  (destructuring-bind (progn &rest body) form
    `(,progn
       ,@(mapcar (lambda (expr) (minimal-compile env expr)) body))))


(defun minimal-compile/progv                (env form)
  (destructuring-bind (progv symbols values forms) form
    `(,progv
       ,@(mapcar (lambda (expr) (minimal-compile env expr))
                 (list* symbols values form)))))


(defun minimal-compile/locally              (env form)
  (destructuring-bind (locally &rest body) form
    (multiple-value-bind (declarations forms) (split-declarations body)
      `(,locally ,declarations
         ,@(mapcar (lambda (expr) (minimal-compile env expr)) forms)))))


(defun minimal-compile/the                  (env form)
  (destructuring-bind (the value-type expr) form
    `(,the ,value-type ,(minimap-compile env expr))))


(defun minimal-compile/eval-when            (env form)
  (destructuring-bind (eval-when situations &rest forms) form
    `(,eval-when ,situations
       ,@(mapcar (lambda (expr) (minimal-compile env expr)) forms))))


(defun minimal-compile/funcall            (env form)
  (destructuring-bind (funcall function &rest arguments) form
    `(,funcall ,@(mapcar (lambda (expr) (minimal-compile env expr))
                         (cons function arguments)))))


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
       ((QUOTE)                (minimal-compile/quote                env form))
       ((IF)                   (minimal-compile/if                   env form))
       ((BLOCK)                (minimal-compile/block                env form))
       ((RETURN-FROM)          (minimal-compile/return-from          env form))
       ((CATCH)                (minimal-compile/catch                env form))
       ((THROW)                (minimal-compile/throw                env form))
       ((UNWIND-PROTECT)       (minimal-compile/unwind-protect       env form))
       ((TAGBODY)              (minimal-compile/tagbody              env form))
       ((GO)                   (minimal-compile/go                   env form))
       ((FLET)                 (minimal-compile/flet                 env form))
       ((LABELS)               (minimal-compile/labels               env form))
       ((SETQ)                 (minimal-compile/setq                 env form))
       ((LET)                  (minimal-compile/let                  env form))
       ((LET*)                 (minimal-compile/let*                 env form))
       ((MULTIPLE-VALUE-CALL)  (minimal-compile/multiple-value-call  env form))
       ((MULTIPLE-VALUE-PROG1) (minimal-compile/multiple-value-prog1 env form))
       ((PROGN)                (minimal-compile/progn                env form))
       ((PROGV)                (minimal-compile/progv                env form))
       ((LOCALLY)              (minimal-compile/locally              env form))
       ((THE)                  (minimal-compile/the                  env form))
       ((EVAL-WHEN)            (minimal-compile/eval-when            env form))
       ((SYMBOL-MACROLET)
        ;; add the symbol-macrolet to the environment
        )
       ((MACROLET)
        ;; add the macrolet to the environment
        )
       ((LOAD-TIME-VALUE)
        ;; do...
        )
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


(cl:defun print-minimal-expansion (form)
  (let ((*print-circle* nil)
        (*PRINT-RIGHT-MARGIN* 80))
    (format t "~2%Source form:~%")
    (pprint form)
    (format t "~2%Minimal compilation:~%")
    (pprint (minimal-compile (make-environment) form))
    #+clisp (format t "~2%Clisp ext:expand-form:~%")
    #+clisp (pprint (ext:expand-form form))
    (values)))


(shadow 'defun)
(defmacro defun (name lambda-list &body body)
  `(progn
     (setf (symbol-value ',name) (lambda ,lambda-list (block ,name ,@body)))
     ,@(when (and (stringp (first body))
                  (rest body))
             `((setf (documentation ',name 'function) ',(first body))))
     ',name))

(print-minimal-expansion '(dotimes (i 42) (print i)))
(print-minimal-expansion '(defun minimal-compile/function             (env form)
                             (declare (ignore env))
                             (destructuring-bind (function fname) form
                               `(,function ,fname))))