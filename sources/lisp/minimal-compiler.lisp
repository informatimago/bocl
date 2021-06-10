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

(defpackage "COM.INFORMATIMAGO.COMMON-LISP.LISP.MINIMAL-COMPILER"
  (:nicknames "MC" "MINIMAL-COMPILER")
  (:use "COMMON-LISP")
  (:use "COM.INFORMATIMAGO.COMMON-LISP.LISP-SEXP.SOURCE-FORM")
  (:shadow "*MACROEXPAND-HOOK*"
           "DEFINE-COMPILER-MACRO" "COMPILER-MACRO-FUNCTION"
           "DEFINE-SYMBOL-MACRO" "SYMBOL-MACROLET"
           "DEFMACRO" "MACRO-FUNCTION" "MACROLET"
           "MACROEXPAND" "MACROEXPAND-1"
           "DEFUN")
  (:export "MACROEXPAND-ALL"))
(in-package "COM.INFORMATIMAGO.COMMON-LISP.LISP.MINIMAL-COMPILER")

;;;
;;; Conditions
;;;

(define-condition variable-name-collision-error (program-error)
  ((name :initarg :name :reader variable-name-collision-name)
   (new-definition :initarg :new-definition :reader variable-name-collision-new-definition))
  (:report (lambda (condition stream)
             (format stream "A variable with the name ~S is already defined, while defining: ~S"
                     (variable-name-collision-name condition)
                     (variable-name-collision-new-definition condition)))))

;;;
;;; Utilities
;;;

(cl:defun scat (&rest string-designators)
  "Interns the concatenation of the STRING-DESIGNATORS."
  (intern (apply (function concatenate) 'string
                 (mapcar (function string) string-designators))
          (symbol-package 'scat)))

(cl:defun keywordize (&rest string-designators)
  "
RETURN: A new keyword with STRING-DESIGNATOR as name.
"
  (intern (apply (function concatenate) 'string
                 (mapcar (function string) string-designators))
          (load-time-value (find-package "KEYWORD"))))

(cl:defun function-name-p (form)
  (or (symbolp form)
      (and (listp form)
           (eql 'setf (first form))
           (symbolp  (second form))
           (null (cddr form)))))

(cl:defun lambda-list-p (form)
  (and (listp form) (eql 'lambda (first form))))

(cl:defun go-tag-p (form)
  (or (integerp form) (symbolp form)))

(cl:defun coerce-to-function (form)
  (cond
    ((function-name-p form)

     ;; (eval `(function ,form))
     ;; (compile nil form)
     (coerce form 'function))

    ((lambda-list-p form)

     ;; (eval form)
     ;; (compile nil form)
     (coerce form 'function))

    (t
     (warn "Is that a function? ~S" form)
     (coerce form 'function))))


;;;
;;; Environment
;;;

(defstruct (env/variable
            (:constructor env/make-variable))
  name)

(defstruct (env/function
            (:constructor env/make-function))
  name
  lambda-list
  lambda-expression
  environment) ;for closure

(defstruct (env/symbol-macro
            (:constructor env/make-symbol-macro))
  name
  expansion)

(defstruct (env/macro
            (:constructor env/make-macro))
  name
  function)


(defstruct (env/block
            (:constructor env/make-block))
  name)

(defstruct (env/catch
            (:constructor env/make-catch))
  tag)

(defstruct (env/protect
            (:constructor env/make-protect))
  cleanup)

(defstruct (env/handler
            (:constructor env/make-handler))
  conditions
  body)

(defstruct (env/restart
            (:constructor env/make-restart))
  name
  lambda-list
  body)


(defstruct environment
  variables
  functions
  symbol-macros
  macros
  ;; --------
  protects
  blocks
  catchs
  handlers
  restarts)



(defparameter *global-environment* (make-environment))

(defvar *macroexpand-hook* 'funcall)
;; It will be (coerce *macroexpand-hook* 'function)

;; symbol-plist keys:
(defconstant +key/compiler-macro-function+ 'compiler-macro-function)



(eval-when (:compile-toplevel :load-toplevel :execute)

  (defparameter *environment-slots*
    '(variable function symbol-macro macro protect block
      catch handler restart))

  (cl:defmacro extend-environment (slot objects env)
    ;; TODO: instanciated global environment
    `(progn
       (unless ,env (setf ,env *global-environment*))
       (make-environment
        ,@(loop
            :for eslot :in *environment-slots*
            :for accessor-form := `(,(scat 'environment- eslot 's) ,env)
            :collect (keywordize eslot 's)
            :collect (if (eql slot eslot)
                         `(append ,objects ,accessor-form)
                         accessor-form)))))

  (cl:defun generate-environment-functions ()
    `(progn
       ,@(mapcar
          (lambda (slot)
            (let ((parameter      (scat slot 's))
                  (accessor-form  `(,(scat 'environment- slot 's) env))
                  (name-key-form  `(function ,(scat 'env/ slot '-name))))
              `(progn

                 (cl:defun ,(scat 'extend-environment-with- slot 's) (,parameter  env)
                   (extend-environment ,slot ,parameter env))

                 (cl:defun (scat 'push- slot '-onto-environment) (,slot env)
                   ;; TODO: instanciated global environment
                   (unless env (setf env *global-environment*))
                   (push ,slot ,accessor-form)
                   env)

                 (cl:defun ,(scat 'pop- slot '-from-environment) (name env)
                   ;; TODO: instanciated global environment
                   (unless env (setf env *global-environment*))
                   (setf ,accessor-form (remove name ,accessor-form :key ,name-key-form :count 1))
                   env)

                 (cl:defun ,(scat 'env/find- slot) (name env)
                   ;; TODO: instanciated global environment
                   (unless env (setf env *global-environment*))
                   (find name ,accessor-form :key ,name-key-form))

                 )))
          *environment-slots*)))

  (generate-environment-functions)


  ) ;;eval-when



;;;
;;; Compiler macros
;;;


(cl:defun compiler-macro-function (name &optional environment)
  ;; TODO: instanciated global environment
  (declare (ignore environment))
  (get name +key/compiler-macro-function+ nil))

(cl:defun (setf compiler-macro-function) (new-function name &optional environment)
  (assert (null environment) (environment))
  ;; TODO: instanciated global environment
  (setf (get name +key/compiler-macro-function+ nil) new-function))

(cl:defmacro define-compiler-macro (name lambda-list &body decl-doc-body)
  (let* ((ll          (parse-lambda-list lambda-list :macro))
         (whole       (if (lambda-list-whole-parameter-p ll)
                          (parameter-specifier (lambda-list-whole-parameter ll))
                          (gensym "whole")))
         (environment (if (lambda-list-environment-parameter-p ll)
                          (parameter-specifier (lambda-list-environment-parameter ll))
                          (gensym "environment"))))
    (remove-whole-parameter ll)
    (remove-environment-parameter ll)
    (multiple-value-bind (declarations docstrings body) (parse-body :lambda decl-doc-body)
      `(progn
         (setf (compiler-macro-function ',name)
               (lambda (,whole ,environment)
                 (declare (ignorable ,environment))
                 (destructuring-bind ,lambda-list ,whole
                   (locally ,@declarations
                     ,@body)))
               ,@(when docstrings
                   `((documentation ',name 'compiler-macro) ',(first docstrings))))
         ',name))))

;;;
;;; Symbol macros
;;;

(cl:defun symbol-macro-p (sym env)
  ;; TODO: instanciated global environment
  (unless env (setf env *global-environment*))
  (env/find-symbol-macro sym env))

(cl:defun symbol-macro-expansion (symbol &optional environment)
  ;; TODO: instanciated global environment
  (unless environment (setf environment *global-environment*))
  (env/symbol-macro-expansion (env/find-symbol-macro symbol environment)))

(cl:defun (setf symbol-macro-expansion) (new-expansion symbol &optional environment)
  ;; TODO: instanciated global environment
  (unless environment (setf environment *global-environment*))
  (let ((symbol-macro (env/find-symbol-macro symbol environment)))
    (if symbol-macro
        (setf (env/symbol-macro-expansion symbol-macro) new-expansion)
        (push-symbol-macro-onto-environment (env/make-symbol-macro
                                             :name symbol
                                             :expansion new-expansion)
                                            environment)))
  new-expansion)

(cl:defmacro define-symbol-macro (&whole form symbol expansion)
  `(progn
     (eval-when (:compile-toplevel)
       (when (env/find-variable symbol nil)
         (cerror 'variable-name-collision-error
                 :name symbol :new-definition ',form)))
     (setf (symbol-macro-expansion ',symbol nil) ',expansion)
     ',symbol))


;; (cl:defmacro symbol-macrolet (&environment environment (&rest symbol-expansion-clauses) decl-and-body)
;;   (multiple-value-bind (declarations docstrings body) (parse-body :locally decl-and-body)
;;
;;     ))

(cl:defun so/symbol-macrolet (symbol expansion environment)
  (check-type symbol symbol)
  (setf (symbol-macro-expansion symbol environment) expansion))


;;;
;;; Normal macros
;;;

(cl:defun macro-function (symbol &optional environment)
  ;; TODO: instanciated global environment
  (unless environment (setf environment *global-environment*))
  (second (find symbol (environment-macros environment) :key (function first))))

(cl:defun (setf macro-function) (new-function symbol &optional environment)
  ;; TODO: instanciated global environment
  (unless environment (setf environment *global-environment*))
  (let ((macro (env/find-macro symbol environment)))
    (if macro
        (setf (env/macro-function macro) new-function)
        (push-symbol-macro-onto-environment (env/make-macro
                                             :name symbol
                                             :function new-function)
                                            environment))))

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
    (multiple-value-bind (declarations docstrings body) (parse-body :lambda decl-doc-body)
      `(progn
         (setf (env/macro-function ',name)
               (lambda (,whole ,env-parameter)
                 (declare (ignorable ,env-parameter))
                 (destructuring-bind ,lambda-list ,whole
                   (locally ,@declarations
                     ,@(mapcar (lambda (subform) (minimal-compile environment subform))
                               body))))
               ,@(when docstrings
                   `((documentation ',name 'function) ',(first docstrings))))
         ',name))))

(cl:defun so/macrolet (name lambda-list decl-and-body environment)
  (let* ((ll            (parse-lambda-list lambda-list :macro))
         (whole         (if (lambda-list-whole-parameter-p ll)
                            (parameter-specifier (lambda-list-whole-parameter ll))
                            (gensym "whole")))
         (env-parameter (if (lambda-list-environment-parameter-p ll)
                            (parameter-specifier (lambda-list-environment-parameter ll))
                            (gensym "environment"))))
    (remove-whole-parameter ll)
    (remove-environment-parameter ll)
    (multiple-value-bind (declarations docstrings body) (parse-body :lambda decl-and-body)
      (declare (ignore docstrings))
      (setf (macro-function name environment)
            (coerce-to-function `(lambda (,whole ,env-parameter)
                                   (declare (ignorable ,env-parameter))
                                   (destructuring-bind ,lambda-list ,whole
                                     (locally ,@declarations
                                       ,@body))))))))



(cl:defun macroformp (form environment)
  (and (consp form) (macro-function (first form) environment)))

(cl:defun macroexpand (form &optional environment)
  ;; TODO: instanciated global environment
  (unless environment (setf environment *global-environment*))
  (loop
     (multiple-value-bind (expansion expandedp) (macroexpand-1 form environment)
       (unless expandedp
         (return-from macroexpand (values expansion expandedp)))
       (setf form expansion))))

(cl:defun macroexpand-1 (form &optional environment)
  ;; TODO: instanciated global environment
  (unless environment (setf environment *global-environment*))
  (cond ((consp form)
         (let ((macro (macro-function (first form) environment)))
           (if macro
               (values (funcall (coerce-to-function *macroexpand-hook*)
                                macro
                                form environment)
                       t)
               (values form
                       nil))))
        ((symbolp form)
         (let ((symbol-macro (env/find-symbol-macro (first form) environment)))
           (if symbol-macro
               (values (funcall (coerce-to-function *macroexpand-hook*)
                                (constantly (env/symbol-macro-expansion symbol-macro))
                                form environment)
                       t)
               (values form
                       nil))))
        (t (values form nil))))



;;;
;;; Minimal compiler
;;;


(declaim (inline operator arguments))
(cl:defun operator  (form) (first form))
(cl:defun arguments (form) (rest  form))


(cl:defun minimal-compile/symbol (env form)
  (let ((symac (env/find-symbol-macro form env)))
    (if symac
        (minimal-compile env (env/symbol-macro-expansion symac))
        form)))

(cl:defun minimal-compile/function (env form)
  (destructuring-bind (function fname) form
    ;; TODO: if name is a lambda expression we needs to
    ;;       minimal-compile it.  we need to use the environment, both
    ;;       to  minimal-compile, and to lookup the function (and
    ;;       build a closure.
    (if (lambda-list-p fname)
        (destructuring-bind (lambda lambda-list &body body) fname
          ;; TODO: parse lambda-list
          `(,function (,lambda ,lambda-list
                        ,(minimal-compile (extend-environment-with-variables
                                           (mapcar (lambda (parameter)
                                                     (env/make-variable :name parameter))
                                                   lambda-list)
                                           env)
                                          `(progn ,@body)))))
        `(,function ,fname))))

(cl:defun minimal-compile/quote (env form)
  (declare (ignore env))
  (destructuring-bind (quote object) form
    `(,quote ,object)))

(cl:defun minimal-compile/if (env form)
  (destructuring-bind (if test then &optional else) form
    `(,if ,(minimal-compile env test)
       ,(minimal-compile env then)
       ,(minimal-compile env else))))

(cl:defun minimal-compile/block (env form)
  (destructuring-bind (block name &body body) form
    (let ((env (extend-environment-with-blocks
                (list (env/make-block :name name))
                env)))
      `(,block ,name
         ,@(mapcar
            (lambda (subform) (minimal-compile env subform))
            body)))))

(cl:defun minimal-compile/return-from (env form)
  (destructuring-bind (return-from name &optional value) form
    `(,return-from ,name ,(minimal-compile env value))))

(cl:defun minimal-compile/catch (env form)
  (destructuring-bind (catch object &body body) form
    `(,catch ,(minimal-compile env object)
       ,@(let ((env (extend-environment-with-catchs
                      (list (env/make-catch :tag object))
                      env)))
           (mapcar (lambda (subform) (minimal-compile env subform)) body)))))

(cl:defun minimal-compile/throw (env form)
  (destructuring-bind (throw object &optional value) form
    `(,throw ,(minimal-compile env object) ,(minimal-compile env value))))

(cl:defun minimal-compile/unwind-protect (env form)
  (destructuring-bind (unwind-protect expression &body cleanup-body) form

    (let* ((mc-cleanup-body (mapcar (lambda (subform) (minimal-compile env subform))
                                    cleanup-body))
           (cleanup         `(lambda () ,@mc-cleanup-body))
           (env (extend-environment-with-protects
                 (list (env/make-protect :cleanup (coerce-to-function cleanup)))
                 env))))
    `(,unwind-protect ,(minimal-compile env expression) ,@mc-cleanup-body)))

(cl:defun minimal-compile/tagbody (env form)
  ;; There should be a tagbody environment too, but we expect tagbody to be macro-expanded to something else.
  (destructuring-bind (tagbody &body body) form
    `(,tagbody
        ,@(mapcar (lambda (subform)
                      (if (go-tag-p subform)
                        subform         ; a tag
                        (minimal-compile env subform)))
                  body))))

(cl:defun minimal-compile/go (env form)
  (declare (ignore env))
  (destructuring-bind (go label) form
    `(,go ,label)))

(cl:defun minimal-compile/flet (env form)
  (destructuring-bind (flet bindings &body body) form
    (let ((funs (mapcar
                 (lambda (fun)
                   ;; Return a cons: ( mc-fun-clause  . fun-object )
                   (destructuring-bind (fname lambda-list &body decl-and-body) fun
                     (let ((ll (parse-lambda-list lambda-list :ordinary)))
                       (multiple-value-bind (declarations docstrings body) decl-and-body
                         (declare (ignore docstrings))
                         ;; TODO: process declarations; fundamentally
                         ;; special declaration -> dynamic-variable (and parameters).
                         ;; then declarations can be removed from the mc form.
                         (let* ((env (extend-environment-with-lambda-list
                                      (mapcar (lambda (parameter)
                                                (env/make-variable :name parameter))
                                              (lambda-list-parameters ll))
                                      env))
                                (mc-body (mapcar (lambda (form) (minimal-compile env form))
                                                 body)))
                           (cons
                            `(,fname ,lambda-list ,@mc-body)
                            (env/make-function :name fname
                                               :lambda-list ll
                                               :lambda-expression `(lambda ,lambda-list
                                                                     ,@declarations
                                                                     (block ,fname ,@mc-body))
                                               :environment env)))))))
                 bindings)))
      `(,flet ,(mapcar (function car) funs)
         ,@(let ((env (extend-environment-with-functions (mapcar (function cdr) funs) env)))
             (mapcar (lambda (subform) (minimal-compile env subform)) body))))))


(cl:defun minimal-compile/labels (env form)
  (destructuring-bind (flet bindings &body body) form
    (let* ((funinfos (mapcar (lambda (fun)
                               (destructuring-bind (fname lambda-list &body decl-and-body) fun
                                 (let ((ll (parse-lambda-list lambda-list :ordinary)))
                                   (multiple-value-bind (declarations docstrings body) decl-and-body
                                     (declare (ignore docstrings))
                                     (list (env/make-function :name fname :lambda-list ll)
                                           lambda-list declarations body)))))
                             bindings))
           (env  (extend-environment-with-functions
                  (mapcar (function first) funinfos)
                  env))
           (funs (mapcar
                  (lambda (fun funinfo)
                    (destructuring-bind (funobj lambda-list declarations body) funinfo
                      (let* ((fname       (env-function-name        funobj))
                             (ll          (env-function-lambda-list funobj))
                             (benv        (extend-environment-with-lambda-list
                                           (mapcar (lambda (parameter)
                                                     (env/make-variable :name parameter))
                                                   (lambda-list-parameters ll))
                                           env))
                             ;; TODO: process declarations; fundamentally
                             ;; special declaration -> dynamic-variable (and parameters).
                             ;; then declarations can be removed from the mc form.
                             (mc-body (mapcar (lambda (form) (minimal-compile benv form))
                                              body)))

                        (setf (env/function-environment funobj) env)
                        (setf (env/function-lambda-expression funobj)
                              `(lambda ,lambda-list
                                 ,@declarations
                                 (block ,fname ,@mc-body)))
                        `(,fname ,lambda-list ,@mc-body))))
                  bindings funinfos)))
      `(,flet ,funs
         ,@(mapcar (lambda (subform) (minimal-compile env subform))
                   body)))))

(cl:defun minimal-compile/setq (env form)
  (destructuring-bind (setq &rest var-val-pairs) form
    (declare (ignore setq))
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
                         `(setq ,(first var-val-pairs) ,(minimal-compile env (second var-val-pairs))))
                        (t
                         `(progn
                            ,@(loop
                                 :for (var val) :on var-val-pairs :by (function cddr)
                                 :collect `(setq ,var ,(minimal-compile env val))))))))
        'nil)))


(cl:defun minimal-compile/let (env form)
  (destructuring-bind (let bindings &body decl-and-body) form
    `(,let ,(mapcar (lambda (binding)
                      (if (atom binding)
                          `binding
                          `(,(first binding) ,(minimal-compile env (second binding)))))
             bindings)
       ,@(let ((env (extend-environment-with-variables
                     (mapcar (lambda (binding)
                               (env/make-variable :name (if (atom binding)
                                                            binding
                                                            (first binding))))
                             bindings)
                     env)))
           (multiple-value-bind (declarations docstrings body) (parse-body :locally decl-and-body)
             (declare (ignore docstrings))
             ;; TODO: process special declarations!
             (append declarations
                     (mapcar (lambda (subform) (minimal-compile env subform))
                             body)))))))


(cl:defun minimal-compile/let* (env form)
  (destructuring-bind (let* bindings &body body) form
    (minimal-compile
     env (labels ((wrap (bindings expr)
                    (if (null bindings)
                        expr
                        (wrap (cdr bindings) `(let (,(car bindings)) ,expr)))))
           (let ((bindings (reverse bindings)))
             (wrap (rest bindings) `(let (,(first bindings)) ,@body)))))))


(cl:defun minimal-compile/multiple-value-call (env form)
  (destructuring-bind (multiple-value-call function-form &rest forms) form
    `(,multiple-value-call
         ,@(mapcar (lambda (expr) (minimal-compile env expr))
                   (cons function-form forms)))))


(cl:defun minimal-compile/multiple-value-prog1 (env form)
  (destructuring-bind (multiple-value-prog1 first-form &body body) form
    `(,multiple-value-prog1
         ,@(mapcar (lambda (expr) (minimal-compile env expr))
                   (cons first-form body)))))


(cl:defun minimal-compile/progn (env form)
  (destructuring-bind (progn &body body) form
    `(,progn
       ,@(mapcar (lambda (expr) (minimal-compile env expr)) body))))


(cl:defun minimal-compile/progv (env form)
  (destructuring-bind (progv symbols values forms) form
    `(,progv
       ,@(mapcar (lambda (expr) (minimal-compile env expr))
                 (list* symbols values form)))))


(cl:defun minimal-compile/locally (env form)
  (destructuring-bind (locally &body body) form
    (multiple-value-bind (declarations forms) (split-declarations body)
      `(,locally ,declarations
         ,@(mapcar (lambda (expr) (minimal-compile env expr)) forms)))))


(cl:defun minimal-compile/the (env form)
  (destructuring-bind (the value-type expr) form
    `(,the ,value-type ,(minimap-compile env expr))))


(cl:defun minimal-compile/eval-when (env form)
  (destructuring-bind (eval-when situations &rest forms) form
    `(,eval-when ,situations
       ,@(mapcar (lambda (expr) (minimal-compile env expr)) forms))))


(cl:defun minimal-compile/funcall (env form)
  (destructuring-bind (funcall function &rest arguments) form
    `(,funcall ,@(mapcar (lambda (expr) (minimal-compile env expr))
                         (cons function arguments)))))


(cl:defun minimal-compile (env form)
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
             (let ((expansion (funcall (coerce-to-function *macroexpand-hook*)
                                       cmf form env)))
               (minimal-compile env expansion)))
            (mf
             (let ((expansion (funcall (coerce-to-function *macroexpand-hook*)
                                       mf form env)))
               (minimal-compile env expansion)))
            (t (minimal-compile/funcall env form)))))))))



#-(and)
(cl:defun macroexpand-all (form &optional env)
  "Recursivly expand all macros in FORM."
  ;; TODO: instanciated global environment
  (unless environment (setf environment *global-environment*))
  (flet ((mexpand (forms env)
           (mapcar (lambda (form) (macroexpand-all form env)) forms)))
    (cl:macrolet ((destructuring-bind-body (binds form &body body)
                 (if (eql '&body (first (last binds)))
                     (let ((&body (gensym "&BODY")))
                       `(destructuring-bind ,(append (butlast binds) (list '&body body)) ,form
                          (multiple-value-bind (declarations docstrings body) (parse-body :locally body)
                            ,@body)))
                     `(destructuring-bind ,binds ,form ,@body))))
      (multiple-value-bind (expansion win) (macroexpand-1 form env)
        (if win
            (macroexpand-all expansion env)
            (if (atom form)
                form
                (case (first form)

                  ((macrolet)
                   (destructuring-bind-body (macros &body) (rest form)
                                            (setf env (augment-environment env
                                                                           :macro (mapcar (lambda (macro)
                                                                                            (destructuring-bind
                                                                                                (name arglist &body body)
                                                                                                macro
                                                                                              (list name (enclose (parse-macro name arglist body env)))))
                                                                                          macros)
                                                                           :declare (decl-specs-from-declarations decls)))
                                            (let ((body (mexpand body env)))
                                              (if decls
                                                  `(locally ,@decls ,@body)
                                                  `(progn ,@body)))))

                  ((symbol-macrolet)
                   (destructuring-bind-body (symbol-macros &body) (rest form)
                                            (setf env (augment-environment env :symbol-macro symbol-macros :declare (decl-specs-from-declarations decls)))
                                            (let ((body (mexpand body env)))
                                              (if decls
                                                  `(locally ,@decls ,@body)
                                                  `(progn ,@body)))))

                  ((let let* compiler-let)
                   (destructuring-bind-body (bindings &body) (rest form)
                                            `(,(first form)
                                               ,(mapcar (lambda (binding)

                                                          (if (listp binding)
                                                              (list (first binding) (macroexpand-all (second binding) env))
                                                              binding))
                                                        bindings)
                                               ,@decls
                                               ,@(mexpand body env))))

                  ((flet labels)
                   (destructuring-bind-body (bindings &body) (rest form)
                                            (let ((augmented-env
                                                    (augment-environment env :function (mapcar #'car bindings))))
                                              `(,(first form)
                                                ,(mapcar (lambda (binding)
                                                           (list* (first binding)
                                                                  (cdr (macroexpand-all `(lambda ,@(rest binding))
                                                                                        (if (eq (first form) 'labels)
                                                                                            augmented-env
                                                                                            env)))))
                                                         bindings)
                                                ,@decls
                                                ,@(mexpand body augmented-env)))))
                  (function
                   (if (and (consp (second form))
                            (eql 'lambda (first (second form))))
                       (destructuring-bind (lambda arglist &body body&decls)
                           (second form)
                         (declare (ignore lambda))
                         (multiple-value-bind (body decls)
                             (parse-body body&decls env)
                           `(lambda ,arglist ,@decls ,@(mexpand body env))))
                       form))

                  ((eval-when the locally block return-from)
                   (list* (first form) (second form) (mexpand (cddr form) env)))

                  (setq
                   `(setq ,@(loop for (name value) on (rest form) by #'cddr
                                  collect name
                                  collect (macroexpand-all value env))))

                  ((go quote) form)

                  ((fbind with-c-frame with-variable-c-frame ppc-lap-function)
                   (error "Unable to macroexpand ~S." form))

                  ((catch if load-time-value multiple-value-call multiple-value-prog1 progn
                          progv tagbody throw unwind-protect)
                   (cons (first form) (mexpand (rest form) env)))

                  (t
                   ;; need to check that (first form) is either fboundp or a local function...
                   (cons (first form) (mexpand (rest form) env))))))))))


;;;
;;;
;;;


(cl:defmacro defun (&environment environment name lambda-list &body decl-doc-body)
  ;; TODO: instanciated global environment
  (unless environment (setf environment *global-environment*))
  (setf environment *global-environment*)
  (multiple-value-bind (declarations docstrings body) (parse-body :lambda decl-doc-body)
    `(progn
       ;; TODO: process declarations (special)
       (setf (symbol-function ',name)
             (lambda ,lambda-list
               ,@declarations
               ,@(mapcar (lambda (subform) (minimal-compile environment subform))
                         body))
             ,@(when docstrings
                 `((documentation ',name 'function) ',(first docstrings))))
       ',name)))


(cl:defun print-minimal-expansion (form)
  (let ((*print-circle* nil)
        (*PRINT-RIGHT-MARGIN* 80))
    (format t "~2%Source form:~%")
    (pprint form)
    (format t "~2%Minimal compilation:~%")
    (pprint (minimal-compile (make-environment) form))
    #+ccl   (format t "~2%CCL ccl::macroexpand-all:~%")
    #+ccl   (pprint (ccl::macroexpand-all form))
    #+clisp (format t "~2%CLISP ext:expand-form:~%")
    #+clisp (pprint (ext:expand-form form))
    (values)))

(defmacro dotimes ((var count-form &optional result-form) &body decl-tagbody)
  (destructuring-bind (declarations docstrings body) (parse-body :locally decl-tagbody)
    (declare (ignore docstrings))
    (let ((vcount (gensym "count"))
          (vvar   (gensym "var"))
          (vloop  (gensym "loop")))
      `(let ((,vcount ,count-form)
             (,vvar 0)
             (,var  0))
         ,@declarations
         (check-type ,vcount integer)
         (block nil
           (tagbody
              ,vloop
              (setf ,var ,vvar)
              (if (>= ,vvar ,vcount)
                  (return ,result-form))
              ,@body
              (incf ,vvar)
              (go ,vloop)))))))

(print-minimal-expansion '(dotimes (i 42) (print i)))
(print-minimal-expansion '(defun minimal-compile/function (env form)
                             (declare (ignore env))
                             (destructuring-bind (function fname) form
                               `(,function ,fname))))


(pprint (generate-environment-functions))



;;; TODO defgeneric, defmethod

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
