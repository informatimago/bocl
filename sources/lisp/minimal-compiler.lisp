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
  (:use "COM.INFORMATIMAGO.COMMON-LISP.LISP.CT-REGISTER")
  (:import-from "COM.INFORMATIMAGO.BOCL.COMPILER"
                "FUNCTION-NAME-P" "LAMBDA-EXPRESSION-P"
                "COERCE-TO-FUNCTION"  "COERCE-TO-CLOSURE")
  (:shadowing-import-from
   "COM.INFORMATIMAGO.BOCL"
   . #1=(
         "COMPILER-MACRO-FUNCTION" "MACRO-FUNCTION"
         "MACROEXPAND" "MACROEXPAND-1" "*MACROEXPAND-HOOK*"
         "LAMBDA" "SETF" "FUNCALL"))
  (:export . #1#)
  (:export "FUNCTION-NAME-P" "LAMBDA-EXPRESSION-P" "GO-TAG-P"
           "COERCE-TO-FUNCTION"
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


;;;
;;; Utilities
;;;

(defun go-tag-p (form)
  (or (integerp form) (symbolp form)))

(defun compute-variable-indices (var venv env)
  (declare (ignore env))
  (values (env:variable-index var)
          (env:environment-current-frame-index venv)))

(defun generate-variable-reader (env name)
  (multiple-value-bind (var venv) (env:find-variable name env)
    (if var
        (if (env:lexical-variable-p var)
            (multiple-value-bind (vindex findex) (compute-variable-indices var venv env)
              `(kernel:variable-value ,vindex ,findex))
            `(kernel:symbol-value ',name))
        (progn
          (cerror "Assume a global dynamic variable"
                  "Undefined variable ~S" name)
          `(kernel:symbol-value ',name)))))

(defun generate-variable-writer (env name new-value-form)
  (multiple-value-bind (var venv) (env:find-variable name env)
    (if var
        (if (env:lexical-variable-p var)
            (multiple-value-bind (vindex findex) (compute-variable-indices var venv env)
              `(kernel:set-variable-value ,new-value-form ,vindex ,findex))
            `(kernel:set-symbol-value ,new-value-form ',name))
        (progn
          (cerror "Assume a global dynamic variable"
                  "Undefined variable ~S" name )
          `(kernel:set-symbol-value ,new-value-form ',name)))))


;;;
;;; Symbol macros
;;;


(defun minimal-compile/symbol-macrolet (env form)
  (destructuring-bind (symbol-macrolet (&rest bindings) &body decl-and-body) form
    (declare (ignore symbol-macrolet))
    (let ((env (env:extend-environment-with-variables '() env)))
      (dolist (binding bindings)
        (destructuring-bind (name expansion) binding
          (register-symbol-macro name expansion env)))
      (multiple-value-bind (docstrings declarations body) decl-and-body
        (declare (ignore docstrings)
                 (ignore declarations)) ; TODO: where to use it?
        `(progn
           ,@(minimal-compile/body env body))))))


;;;
;;; Normal macros
;;;


(defun minimal-compile/macrolet (env form)
  ;; TODO: check environment management
  (destructuring-bind (macrolet (&rest bindings) &body decl-and-body) form
    (declare (ignore macrolet))
    (let ((env (env:extend-environment-with-functions '() env)))
      (dolist (binding bindings)
        (destructuring-bind (name lambda-list &body decl-and-body) binding
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
              (register-macrolet name lambda-list
                                 (coerce-to-function
                                  (minimal-compile/lambda-expression
                                   env
                                   `(lambda (,whole ,env-parameter)
                                      (declare (ignorable ,env-parameter))
                                      (destructuring-bind ,lambda-list ,whole
                                        ,@declarations
                                        (block ,name
                                          ,@body))))
                                   name)
                                 (when docstrings (first docstrings))
                                 env)))))
      (multiple-value-bind (docstrings declarations body) (parse-body :lambda decl-and-body)
        (declare (ignore docstrings)
                 (ignore declarations)) ; TODO: where to use it?
        `(progn
           ,@(minimal-compile/body env body))))))


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
        (generate-variable-reader env form)
        (minimal-compile env expansion))))

(defun minimal-compile/quote (env form)
  (declare (ignore env))
  (destructuring-bind (quote object) form
    `(,quote ,object)))

(eval-when (:compile-toplevel :load-toplevel :execute)

  (defun add-local-variable (name specials env )
    (let ((index (length (env:environment-variables env))))
      (env:push-variable-onto-environment
       (let ((var (env:find-variable name env)))
         (if (env:constant-variable-p var)
             (error "Binding a constant variable ~S" name))
         (if (or (member name specials)
                 (env:global-dynamic-variable-p var))
             (env:local-dynamic-variable name index)
             (env:lexical-variable       name index)))
       env)))

  (defun minimal-compile/lambda-list (env ll declarations)
    "Mutates the lambda-list ll, replacing the initform expressions by their minimally compiled versions."
    (loop
      ;; List of new special variables:
      :with specials := (mapcan (cl:lambda (decl) (copy-list (rest decl)))
                                (remove 'special declarations
                                        :key (function first)
                                        :test-not (function eql)))
      ;; Open a new environment frame for the lambda-list:
      :with llenv := (env:make-environment
                      :next env
                      :frame-index (let ((cfi (env:environment-current-frame-index env)))
                                     (if cfi (1+ cfi) 0)))
      :for parameter :in (lambda-list-parameters ll)
        :initially (dolist (declaration declarations)
                     (env:push-declaration-onto-environment declaration llenv))
      :do (if (parameter-initform-p parameter)
              (cl:setf (parameter-initform parameter)
                       (minimal-compile llenv (parameter-initform parameter))))
          (if (typep parameter 'optional-parameter)
              (if (not (parameter-indicator-p parameter))
                  (cl:setf (parameter-indicator parameter) (gensym))))
          ;; Add the new parameter variable bindings to the environment:
          (add-local-variable (parameter-name parameter) specials llenv)
      :finally (return (values ll llenv))))

  (defun minimal-compile/body (env body)
    (mapcar (cl:lambda (subform) (minimal-compile env subform)) body))

  (defun minimal-compile/lambda-expression (env form)
    (cl:destructuring-bind (lambda (&rest lambda-list) &body decl-and-body) form
      (assert (eq lambda 'lambda))
      (let ((ll (parse-lambda-list lambda-list :ordinary)))
        (cl:multiple-value-bind (docstrings declarations body) (parse-body :lambda decl-and-body)
          (declare (ignore docstrings))
          (cl:multiple-value-bind (mc-ll mc-llenv) (minimal-compile/lambda-list env ll declarations)
            `(lambda ,(make-lambda-list mc-ll)
               ,@declarations
               ,@(minimal-compile/body mc-llenv body)))))))

  ) ;;eval-when

(defun minimal-compile/function (env form)
  ;; Local functions and lambda expressions can be resolved now.
  ;; Global functions may change at run-time, or may be not defined yet,
  ;; so they have to be defered.
  ;; Only (function global-fun) or (function (setf global-fun)) are left for the interpreter.
  (destructuring-bind (function fname) form
    (if (lambda-expression-p fname)
        (coerce-to-closure (minimal-compile/lambda-expression env fname)
                           env)
        (let ((fun (env:find-function fname env)))
          (if (env:local-function-p fun)
              (coerce-to-closure (env:local-function-function fun)
                                 (env:local-function-environment fun)
                                 (env:local-function-name fun))
              `(,function ,fname))))))

(defun minimal-compile/call (env form)
  (destructuring-bind (function &rest arguments) form
    `(,(if (lambda-expression-p function)
           (coerce-to-closure (minimal-compile/lambda-expression env function)
                              env)
           (let ((fun (env:find-function function env)))
             (if (env:local-function-p fun)
                 (coerce-to-closure (env:local-function-function fun)
                                    (env:local-function-environment fun)
                                    (env:local-function-name fun))
                 form)))
      ,@(minimal-compile/body env arguments))))

(defun minimal-compile/if (env form)
  (destructuring-bind (if test then &optional else) form
    `(,if ,(minimal-compile env test)
       ,(minimal-compile env then)
       ,(minimal-compile env else))))

(defun minimal-compile/block (env form)
  (destructuring-bind (block name &body body) form
    (let ((env (env:extend-environment-with-blocks (list (env:block name)) env)))
      `(,block ,name
         ,@(minimal-compile/body env body)))))

(defun minimal-compile/return-from (env form)
  (destructuring-bind (return-from name &optional value) form
    (let ((blk (env:find-block name env)))
      (unless (env:blockp blk)
        (error "No block named ~S in the lexical scope." name)))
    `(,return-from ,name ,(minimal-compile env value))))

(defun minimal-compile/catch (env form)
  (destructuring-bind (catch object &body body) form
    `(,catch ,(minimal-compile env object)
       ,@(minimal-compile/body env body))))

(defun minimal-compile/throw (env form)
  (destructuring-bind (throw object &optional value) form
    `(,throw ,(minimal-compile env object) ,(minimal-compile env value))))

(defun minimal-compile/unwind-protect (env form)
  (destructuring-bind (unwind-protect expression &body cleanup-body) form
    `(,unwind-protect ,(minimal-compile env expression)
       ,@(minimal-compile/body env cleanup-body))))

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
                       (multiple-value-bind (docstrings declarations body) (parse-body :lambda decl-and-body)
                         (declare (ignore docstrings))
                         (multiple-value-bind (mc-ll mc-llenv) (minimal-compile/lambda-list env ll declarations)
                           (let ((mc-lambda-list (make-lambda-list mc-ll))
                                 (mc-body        (minimal-compile/body mc-ll body)))
                             (cons `(,fname ,mc-lambda-list ,@declarations ,@mc-body)
                                   (env:make-local-function :name fname
                                                            :lambda-list mc-ll
                                                            :function (coerce-to-closure
                                                                       `(lambda ,mc-lambda-list
                                                                          ,@declarations
                                                                          (block ,fname ,@mc-body))
                                                                       mc-llenv
                                                                       fname)
                                                            :environment env))))))))
                 bindings)))
      `(,flet ,(mapcar (function car) funs)
         ,@(let ((env  (env:make-environment
                        :next env
                        :frame-index (let ((cfi (env:environment-current-frame-index env)))
                                       (if cfi (1+ cfi) 0)))))
             (loop
               :for index :from 0
               :for funinfo :in funs
               :for fun := (cdr funinfo)
               :do (cl:setf (env:local-function-index fun) index)
                   (env:push-function-onto-environment fun env))
             (minimal-compile/body env body))))))

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
                                                        (and var (env:global-dynamic-variable-p var))))
                                                  (env:local-dynamic-variable parameter)
                                                  (env:lexical-variable parameter)))
                                            (lambda-list-parameters ll))
                                    env))
                             (benv (env:extend-environment-with-declarations declarations benv))
                             (mc-body (mapcar (cl:lambda (form) (minimal-compile benv form))
                                              body)))
                        (cl:setf (env:local-function-environment funobj) env
                                 (env:function-function funobj) (coerce-to-closure `(lambda ,lambda-list
                                                                                      ,@declarations
                                                                                      (block ,fname
                                                                                        ,@mc-body))
                                                                                   env))
                        `(,fname ,lambda-list ,@mc-body))))
                  funinfos)))
      `(,labels ,funs
         ,@(minimal-compile/body env body)))))

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
                                                             `(setf ,var ,val)
                                                             `(setq ,var ,val))))))
                       (has-symbol-macros ; only symbol-macros
                        (minimal-compile env `(setf ,@var-val-pairs)))
                       ;; only variables
                       ((null (cddr var-val-pairs))
                        (generate-variable-writer env (first var-val-pairs) (minimal-compile env (second var-val-pairs))))
                       (t
                        `(progn
                           ,@(loop
                               :for (var val) :on var-val-pairs :by (function cddr)
                               :collect (generate-variable-writer env var (minimal-compile env val))))))))
        'nil)))

(defun minimal-compile/let (env form)
  (destructuring-bind (let bindings &body decl-and-body) form
    (multiple-value-bind (docstrings declarations body) (parse-body :locally decl-and-body)
      (declare (ignore docstrings))
      (let* ((specials (mapcan (cl:lambda (decl) (copy-list (rest decl)))
                               (remove 'special declarations
                                       :key (function first)
                                       :test-not (function eql))))
             (lenv      (env:make-environment
                         :next env
                         :frame-index (let ((cfi (env:environment-current-frame-index env)))
                                        (if cfi (1+ cfi) 0)))))
        (dolist (declaration declarations)
          (env:push-declaration-onto-environment declaration lenv))
        (dolist (binding bindings)
          (let ((name (if (atom binding)
                          binding
                          (progn
                            (if (< 2 (length binding))
                                (error "Too many elements in the binding list ~S" binding))
                            (first binding)))))
            (add-local-variable name specials lenv)))

        `(,let ,(mapcar (cl:lambda (binding)
                          (if (atom binding)
                              `binding
                              `(,(first binding) ,(minimal-compile env (second binding)))))
                 bindings)
           ,@declarations
           ,@(minimal-compile/body lenv body))))))

(defun minimal-compile/let* (env form)
  (destructuring-bind (let* bindings &body decl-and-body) form
    (declare (ignore let*))
    (multiple-value-bind (docstrings declarations body) (parse-body :locally decl-and-body)
      (declare (ignore docstrings))
      (let* ((specials (mapcan (cl:lambda (decl) (copy-list (rest decl)))
                               (remove 'special declarations
                                       :key (function first)
                                       :test-not (function eql))))
             (lenv     (env:make-environment
                        :next env
                        :frame-index (let ((cfi (env:environment-current-frame-index env)))
                                       (if cfi (1+ cfi) 0)))))
        (dolist (declaration declarations)
          (env:push-declaration-onto-environment declaration lenv))
        `(bocl:let ,(mapcar (cl:lambda (binding)
                              (let ((name (if (atom binding)
                                              binding
                                              (progn
                                                (if (< 2 (length binding))
                                                    (error "Too many elements in the binding list ~S" binding))
                                                (first binding))))
                                    (init-form (if (atom binding)
                                                   'nil
                                                   ;; We minimal-compile the init-form in the lenv before adding the variable.
                                                   (minimal-compile lenv (second binding)))))
                                (add-local-variable name specials lenv)
                                `(,name ,init-form)))
                     bindings)
           ,@declarations
           ,@(minimal-compile/body lenv body))))))

(defun minimal-compile/multiple-value-call (env form)
  (destructuring-bind (multiple-value-call function-form &rest forms) form
    `(,multiple-value-call
         ,@(minimal-compile/body env (cons function-form forms)))))

(defun minimal-compile/multiple-value-prog1 (env form)
  (destructuring-bind (multiple-value-prog1 first-form &body body) form
    `(,multiple-value-prog1
         ,@(minimal-compile/body env (cons first-form body)))))

(defun minimal-compile/progn (env form)
  (destructuring-bind (progn &body body) form
    `(,progn
       ,@(minimal-compile/body env body))))

(defun minimal-compile/progv (env form)
  (destructuring-bind (progv symbols values forms) form
    `(,progv
         ,(minimal-compile env symbols)
         ,(minimal-compile env values)
       ,@(minimal-compile/body env forms))))

(defun minimal-compile/locally (env form)
  ;; LOCALLY processes the declarations now, and generates a PROGN form.
  ;; If some declaration needs run-time effects, they'd be generated in the PROGN.
  (destructuring-bind (locally &body body) form
    (declare (ignore locally))
    (multiple-value-bind (docstrings declarations forms) (parse-body :locally body)
      (declare (ignore docstrings))
      (let ((env (env:extend-environment-with-declarations declarations env)))
        `(bocl:progn
           ,@(minimal-compile/body env forms))))))

(defun minimal-compile/the (env form)
  (destructuring-bind (the value-type expr) form
    `(,the ,value-type ,(minimal-compile env expr))))

(defun minimal-compile/eval-when (env form)
  (destructuring-bind (eval-when situations &rest forms) form
    `(,eval-when ,situations
       ,@(minimal-compile/body env forms))))

(defun minimal-compile/load-time-value (env form)
  (destructuring-bind (load-time-value form &optional read-only-p) form
    `(,load-time-value ,@(minimal-compile env form) ,read-only-p)))


(defun minimal-compile (env form)
  (cond
    ((symbolp form)            (minimal-compile/symbol               env form))
    ;; The other atoms are unchanged:
    ((atom form)               form)
    ;; Now we have a list.
    (t
     (case (operator form)
       ;; First we check the special operators:
       ((bocl:FUNCTION)             (minimal-compile/function             env form))
       ((bocl:QUOTE)                (minimal-compile/quote                env form)) ;
       ((bocl:SETQ)                 (minimal-compile/setq                 env form))
       ((bocl:IF)                   (minimal-compile/if                   env form)) ;
       ((bocl:BLOCK)                (minimal-compile/block                env form)) ;
       ((bocl:RETURN-FROM)          (minimal-compile/return-from          env form)) ;
       ((bocl:CATCH)                (minimal-compile/catch                env form)) ;
       ((bocl:THROW)                (minimal-compile/throw                env form)) ;
       ((bocl:UNWIND-PROTECT)       (minimal-compile/unwind-protect       env form)) ;
       ((bocl:TAGBODY)              (minimal-compile/tagbody              env form)) ;
       ((bocl:GO)                   (minimal-compile/go                   env form)) ;
       ((bocl:LET)                  (minimal-compile/let                  env form))
       ((bocl:LET*)                 (minimal-compile/let*                 env form))
       ((bocl:FLET)                 (minimal-compile/flet                 env form))
       ((bocl:LABELS)               (minimal-compile/labels               env form))
       ((bocl:SYMBOL-MACROLET)      (minimal-compile/symbol-macrolet      env form))
       ((bocl:MACROLET)             (minimal-compile/macrolet             env form))
       ((bocl:MULTIPLE-VALUE-CALL)  (minimal-compile/multiple-value-call  env form))
       ((bocl:MULTIPLE-VALUE-PROG1) (minimal-compile/multiple-value-prog1 env form))
       ((bocl:PROGN)                (minimal-compile/progn                env form)) ;
       ((bocl:PROGV)                (minimal-compile/progv                env form))
       ((bocl:LOCALLY)              (minimal-compile/locally              env form)) ;
       ((bocl:THE)                  (minimal-compile/the                  env form)) ;
       ((bocl:EVAL-WHEN)            (minimal-compile/eval-when            env form)) ;
       ((bocl:LOAD-TIME-VALUE)      (minimal-compile/load-time-value      env form))
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
            (t
             (minimal-compile/call env form)))))))))

;;;; THE END ;;;;
