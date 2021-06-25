;;;; -*- mode:lisp;coding:iso-8859-1 -*-
;;;;**************************************************************************
;;;;FILE:               environment.lisp
;;;;LANGUAGE:           Common-Lisp
;;;;SYSTEM:             Common-Lisp
;;;;USER-INTERFACE:     NONE
;;;;DESCRIPTION
;;;;
;;;;    Defines Compilation-Time Common Lisp environments.
;;;;
;;;;AUTHORS
;;;;    <PJB> Pascal J. Bourguignon <pjb@informatimago.com>
;;;;MODIFICATIONS
;;;;    2021-06-17 <PJB> Created.
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

(defpackage "COM.INFORMATIMAGO.COMMON-LISP.LISP.ENVIRONMENT"
  (:nicknames "COM.INFORMATIMAGO.BOCL.ENVIRONMENT")
  (:nicknames "ENV")
  (:use "COMMON-LISP")
  (:use "COM.INFORMATIMAGO.COMMON-LISP.CESARUM.SYMBOL")
  (:shadow . #9=(
                 "FUNCTION" "FUNCTIONP"
                 "SPECIAL-OPERATOR-P"
                 "COMPILER-MACRO-FUNCTION" "MACRO-FUNCTION" "FIND-RESTART"
                 "BLOCK" "TAGBODY"))
  (:export . #9#)
  (:export
   "GLOBAL-ENVIRONMENT"
   "CURRENT-ENVIRONMENT"

   "ENVIRONMENT"
   "MAKE-ENVIRONMENT"
   "ENVIRONMENT-P"
   "COPY-ENVIRONMENT"
   "ENVIRONMENT-VARIABLES"
   "ENVIRONMENT-FUNCTIONS"
   "ENVIRONMENT-DECLARATIONS"
   "ENVIRONMENT-BLOCKS"
   "ENVIRONMENT-TAGS"
   "ENVIRONMENT-FRAME-INDEX"
   "ENVIRONMENT-NEXT"

   "EXTEND-ENVIRONMENT-WITH-VARIABLES"       "PUSH-VARIABLE-ONTO-ENVIRONMENT"       "POP-VARIABLE-FROM-ENVIRONMENT"       "FIND-VARIABLE"
   "EXTEND-ENVIRONMENT-WITH-FUNCTIONS"       "PUSH-FUNCTION-ONTO-ENVIRONMENT"       "POP-FUNCTION-FROM-ENVIRONMENT"       "FIND-FUNCTION"
   "EXTEND-ENVIRONMENT-WITH-COMPILER-MACROS" "PUSH-COMPILER-MACRO-ONTO-ENVIRONMENT" "POP-COMPILER-MACRO-FROM-ENVIRONMENT" "FIND-COMPILER-MACRO"
   "EXTEND-ENVIRONMENT-WITH-DECLARATIONS"    "PUSH-DECLARATION-ONTO-ENVIRONMENT"    "POP-DECLARATION-FROM-ENVIRONMENT"    "FIND-DECLARATION"
   "EXTEND-ENVIRONMENT-WITH-BLOCKS"          "PUSH-BLOCK-ONTO-ENVIRONMENT"          "POP-BLOCK-FROM-ENVIRONMENT"          "FIND-BLOCK"
   "EXTEND-ENVIRONMENT-WITH-TAGS"            "PUSH-TAG-ONTO-ENVIRONMENT"            "POP-TAG-FROM-ENVIRONMENT"            "FIND-TAG"

   "ENVIRONMENT-CURRENT-FRAME-INDEX")

  (:export

   "AUTHENTIC-FUNCTION-P" "AUTHENTIC-VARIABLE-P" "BLOCK" "BLOCK-NAME"
   "BLOCKP" "COMPILER-MACRO" "COMPILER-MACRO-FUNCTION"
   "COMPILER-MACRO-LAMBDA-LIST" "COMPILER-MACRO-NAME"
   "COMPILER-MACRO-P" "CONSTANT-VARIABLE" "CONSTANT-VARIABLE-NAME"
   "CONSTANT-VARIABLE-P" "CONSTANT-VARIABLE-VALUE" "COPY-BLOCK"
   "COPY-COMPILER-MACRO" "COPY-CONSTANT-VARIABLE" "COPY-DECLARATION"
   "COPY-GLOBAL-DYNAMIC-VARIABLE" "COPY-GLOBAL-FUNCTION"
   "COPY-GLOBAL-MACRO" "COPY-GLOBAL-SYMBOL-MACRO"
   "COPY-LEXICAL-VARIABLE" "COPY-LOCAL-DYNAMIC-VARIABLE"
   "COPY-LOCAL-FUNCTION" "COPY-LOCAL-MACRO" "COPY-LOCAL-SYMBOL-MACRO"
   "COPY-SPECIAL-OPERATOR" "COPY-TAG" "COPY-TAGBODY" "DECLARATION"
   "DECLARATION-ARGUMENTS" "DECLARATION-NAME" "DECLARATIONP"
   "FUNCTION-FUNCTION" "FUNCTION-LAMBDA-LIST" "FUNCTION-NAME"
   "FUNCTIONP" "GLOBAL-DYNAMIC-VARIABLE"
   "GLOBAL-DYNAMIC-VARIABLE-NAME" "GLOBAL-DYNAMIC-VARIABLE-P"
   "GLOBAL-DYNAMIC-VARIABLE-VALUE" "GLOBAL-FUNCTION"
   "GLOBAL-FUNCTION-FUNCTION" "GLOBAL-FUNCTION-LAMBDA-LIST"
   "GLOBAL-FUNCTION-NAME" "GLOBAL-FUNCTION-P" "GLOBAL-MACRO"
   "GLOBAL-MACRO-FUNCTION" "GLOBAL-MACRO-LAMBDA-LIST"
   "GLOBAL-MACRO-NAME" "GLOBAL-MACRO-P" "GLOBAL-SYMBOL-MACRO"
   "GLOBAL-SYMBOL-MACRO-EXPANSION" "GLOBAL-SYMBOL-MACRO-NAME"
   "GLOBAL-SYMBOL-MACRO-P" "LEXICAL-VARIABLE" "LEXICAL-VARIABLE-INDEX"
   "LEXICAL-VARIABLE-NAME" "LEXICAL-VARIABLE-P"
   "LEXICAL-VARIABLE-VALUE" "LOCAL-DYNAMIC-VARIABLE"
   "LOCAL-DYNAMIC-VARIABLE-INDEX" "LOCAL-DYNAMIC-VARIABLE-NAME"
   "LOCAL-DYNAMIC-VARIABLE-P" "LOCAL-DYNAMIC-VARIABLE-VALUE"
   "LOCAL-FUNCTION" "LOCAL-FUNCTION-ENVIRONMENT"
   "LOCAL-FUNCTION-FUNCTION" "LOCAL-FUNCTION-LAMBDA-LIST"
   "LOCAL-FUNCTION-NAME" "LOCAL-FUNCTION-INDEX"
   "LOCAL-FUNCTION-P" "LOCAL-MACRO"
   "LOCAL-MACRO-ENVIRONMENT" "LOCAL-MACRO-FUNCTION"
   "LOCAL-MACRO-LAMBDA-LIST" "LOCAL-MACRO-NAME" "LOCAL-MACRO-P"
   "LOCAL-SYMBOL-MACRO" "LOCAL-SYMBOL-MACRO-EXPANSION"
   "LOCAL-SYMBOL-MACRO-NAME" "LOCAL-SYMBOL-MACRO-P" "MACRO-FUNCTION"
   "MACRO-LAMBDA-LIST" "MACRO-NAME" "MACROP" "MAKE-BLOCK"
   "MAKE-COMPILER-MACRO" "MAKE-CONSTANT-VARIABLE" "MAKE-DECLARATION"
   "MAKE-GLOBAL-DYNAMIC-VARIABLE" "MAKE-GLOBAL-FUNCTION"
   "MAKE-GLOBAL-MACRO" "MAKE-GLOBAL-SYMBOL-MACRO"
   "MAKE-LEXICAL-VARIABLE" "MAKE-LOCAL-DYNAMIC-VARIABLE"
   "MAKE-LOCAL-FUNCTION" "MAKE-LOCAL-MACRO" "MAKE-LOCAL-SYMBOL-MACRO"
   "MAKE-SPECIAL-OPERATOR" "MAKE-TAG" "MAKE-TAGBODY"
   "SPECIAL-OPERATOR" "SPECIAL-OPERATOR-LAMBDA-LIST"
   "SPECIAL-OPERATOR-NAME" "SPECIAL-OPERATOR-P"
   "SYMBOL-MACRO-EXPANSION" "SYMBOL-MACRO-NAME" "SYMBOL-MACRO-P" "TAG"
   "TAG-INDEX" "TAG-NAME" "TAG-TAGBODY" "TAGBODY-TAGS" "TAGBODYP"
   "TAGP" "VARIABLE-INDEX" "VARIABLE-NAME" "VARIABLE-VALUE"
   "VARIABLEP"

   )
  (:export "DESCRIBE-ENVIRONMENT" "DUMP-ENVIRONMENT"))
(in-package "COM.INFORMATIMAGO.COMMON-LISP.LISP.ENVIRONMENT")

;;;
;;; Compilation-Time Environment
;;;

(defstruct environment
  variables            ; includes symbol-macro
  functions            ; includes macros
  compiler-macros      ; only global; either for a function or a macro
  declarations
  blocks
  tags                 ; tagbody tags
  frame-index
  next)

(defparameter *global-environment* (make-environment)
  "The global environment.")

(defparameter *environment* *global-environment*
  "The current environment.")

(defun global-environment  () *global-environment*)
(defun current-environment () *environment*)

(eval-when (:compile-toplevel :execute)
  (defparameter *environment-slots*
    '(variable
      function
      compiler-macro
      declaration
      block
      tag)))


(defmacro %extend-environment (slot objects env)
  `(progn
     (unless ,env (setf ,env *global-environment*))
     (make-environment
      :next ,env
      ,@(loop
          :for eslot :in *environment-slots*
          :for accessor-form := `(,(scat 'environment- eslot 's) ,env)
          :collect (keywordize eslot 's)
          :collect (if (eql slot eslot)
                       `(append ,objects ,accessor-form)
                       accessor-form)))))

(defmacro pass-values (form)
  `(multiple-value-call (cl:function values) ,form))

(defmacro generate-environment-functions ()
  `(list
    ,@(mapcar
       (lambda (slot)
         (let ((parameter      (scat slot 's))
               (accessor-form  `(,(scat 'environment- slot 's) env))
               (name-key-form  `(cl:function ,(scat slot '-name))))
           `(list
             (defun ,(scat 'extend-environment-with- slot 's) (,parameter  env)
               (unless env (setf env *global-environment*))
               (%extend-environment ,slot ,parameter env))
             (defun ,(scat 'push- slot '-onto-environment) (,slot env)
               (unless env (setf env *global-environment*))
               (push ,slot ,accessor-form)
               env)
             (defun ,(scat 'pop- slot '-from-environment) (name env)
               (unless env (setf env *global-environment*))
               (setf ,accessor-form (remove name ,accessor-form
                                            ;; We use :test equal to cater for (setf foo) for function names.
                                            ,@(when (eql slot 'function)
                                                '(:test (cl:function equal)))
                                            :key ,name-key-form :count 1))
               env)
             (defun ,(scat 'find- slot) (name env)
               (unless env (setf env *global-environment*))
               (let ((object (if (symbolp name)
                                 (find name ,accessor-form
                                       ;; We use :test equal to cater for (setf foo) for function names.
                                       ,@(when (eql slot 'function)
                                           '(:test (cl:function equal)))
                                       :key ,name-key-form)
                                 (find name ,accessor-form))))
                 (if object
                     (values object env)
                     (when (environment-next env)
                       (pass-values (,(scat 'find- slot) name (environment-next env))))))))))
       *environment-slots*)))


(generate-environment-functions)

;; ((extend-environment-with-variables       push-variable-onto-environment       pop-variable-from-environment       find-variable)
;;  (extend-environment-with-functions       push-function-onto-environment       pop-function-from-environment       find-function)
;;  (extend-environment-with-compiler-macros push-compiler-macro-onto-environment pop-compiler-macro-from-environment find-compiler-macro)
;;  (extend-environment-with-declarations    push-declaration-onto-environment    pop-declaration-from-environment    find-declaration)
;;  (extend-environment-with-blocks          push-block-onto-environment          pop-block-from-environment          find-block)
;;  (extend-environment-with-tags            push-tag-onto-environment            pop-tag-from-environment            find-tag))

(defun environment-current-frame-index (environment)
  (if (null environment)
      nil
      (let ((frame-index (environment-frame-index environment)))
        (if (null frame-index)
            (environment-current-frame-index (environment-next environment))
            frame-index))))

(defstruct (named-object
            (:constructor nil)
            (:copier nil))
  name)

(defstruct (variable
            (:include named-object)
            (:constructor nil)
            (:copier nil)
            (:predicate variablep)))

(defstruct (symbol-macro
            (:include variable)
            (:constructor nil)
            (:copier nil))
  expansion)

(defstruct (global-symbol-macro
            (:include symbol-macro)
            (:constructor make-global-symbol-macro)
            (:constructor global-symbol-macro (name &optional expansion))))

(defstruct (local-symbol-macro
            (:include symbol-macro)
            (:constructor make-local-symbol-macro)
            (:constructor local-symbol-macro (name &optional expansion))))

(defstruct (authentic-variable
            (:include variable)
            (:constructor nil)
            (:copier nil)
            (:conc-name variable-))
  index
  value)

(defstruct (lexical-variable
            (:include authentic-variable)
            (:constructor make-lexical-variable)
            (:constructor lexical-variable (name &optional index))))

(defstruct (local-dynamic-variable
            (:include authentic-variable)
            (:constructor make-local-dynamic-variable)
            (:constructor local-dynamic-variable (name &optional index))))

(defstruct (global-dynamic-variable
            (:include authentic-variable)
            (:constructor make-global-dynamic-variable)
            (:constructor global-dynamic-variable (name &optional value))))

(defstruct (constant-variable
            (:include authentic-variable)
            (:constructor make-constant-variable)
            (:constructor constant-variable (name &optional value))))

(defstruct (function
            (:include named-object)
            (:predicate functionp)
            (:constructor nil)
            (:copier nil))
  lambda-list)

(defstruct (authentic-function
            (:include function)
            (:constructor nil)
            (:copier nil)
            (:conc-name function-))
  function)

(defstruct (global-function
            (:include authentic-function)
            (:constructor make-global-function)
            (:constructor global-function (name &optional lambda-list function))))

(defstruct (local-function
            (:include authentic-function)
            (:constructor make-local-function)
            (:constructor local-function (name &optional lambda-list function environment)))
  index
  environment)

(defstruct (compiler-macro
            (:include function)
            (:constructor make-compiler-macro)
            (:constructor compiler-macro (name &optional lambda-list function)))
  function)

(defstruct (macro
            (:include function)
            (:predicate macrop)
            (:constructor nil)
            (:copier nil))
  function)

(defstruct (global-macro
            (:include macro)
            (:constructor make-global-macro)
            (:constructor global-macro (name &optional lambda-list function))))

(defstruct (local-macro
            (:include macro)
            (:constructor make-local-macro)
            (:constructor local-macro (name &optional lambda-list function environment)))
  environment)

(defstruct (special-operator
            (:include function)
            (:constructor make-special-operator)
            (:constructor special-operator)))

(defstruct (declaration
            (:include named-object)
            (:predicate declarationp)
            (:constructor make-declaration)
            (:constructor declaration (name &optional arguments)))
  arguments)

(defstruct (block
               (:include named-object)
             (:predicate blockp)
             (:constructor make-block)
             (:constructor block (name))))

(defstruct (tagbody
              (:predicate tagbodyp)
              (:constructor make-tagbody))
  tags)

(defstruct (tag
            (:include named-object)
            (:predicate tagp)
            (:constructor make-tag)
            (:constructor tag (name &optional index tagbody)))
  index
  tagbody)

#-(and)
(cl-user::export-symbols
 (mapcan (cl:function cl-user::defined-symbols)
         '()))


(defun describe-environment (&optional (env *global-environment*))
  (append
   (when (environment-variables env)
     (list (cons :variables
                 (mapcar (lambda (var)
                           (cond
                             ((global-symbol-macro-p     var) (list :global-symbol-macro     (variable-name var)))
                             ((local-symbol-macro-p      var) (list :local-symbol-macro      (variable-name var)))
                             ((lexical-variable-p        var) (list :lexical-variable        (variable-name var)))
                             ((local-dynamic-variable-p  var) (list :local-dynamic-variable  (variable-name var)))
                             ((global-dynamic-variable-p var) (list :global-dynamic-variable (variable-name var)))
                             ((constant-variable-p       var) (list :constant-variable       (variable-name var)))
                             (t (error "Unexpected variable ~S type ~S" (named-object-name var) (type-of var)))))
                         (environment-variables env)))))
   (when (environment-functions env)
     (list (cons :functions
                 (mapcar (lambda (fun)
                           (cond
                             ((global-function-p fun) (list :global-function (function-name fun)))
                             ((local-function-p  fun) (list :local-function  (function-name fun)))
                             ((macrop            fun) (list :macro           (function-name fun)))
                             ((global-macro-p    fun) (list :global-macro    (function-name fun)))
                             ((local-macro-p     fun) (list :local-macro     (function-name fun)))
                             (t (error "Unexpected function ~S type ~S" (named-object-name fun) (type-of fun)))))
                         (environment-functions env)))))
   (when (environment-compiler-macros env)
     (list (cons :compiler-macros
                 (mapcar (lambda (cm)
                           (list :compiler-macro (compiler-macro-name cm)))
                         (environment-compiler-macros env)))))
   (when (environment-declarations env)
     (list (cons :declarations
                 (environment-declarations env))))
   (when (environment-blocks env)
     (list (cons :blocks
                 (mapcar (lambda (block)
                           (block-name block))
                         (environment-blocks env)))))
   (when (environment-tags env)
     (list (cons :tags
                 (mapcar (lambda (tag)
                           (list (tag-name tag) (tag-index tag)))
                         (environment-tags env)))))
   (when (environment-next env)
     (list (list :next (describe-environment (environment-next env)))))))


(defun dump-environment (&optional (env (env:global-environment)))
  (let ((*package* (find-package :com.informatimago.bocl))
        (*print-right-margin* 40))
    (pprint (env:describe-environment env))))

;;;
;;; Run-Time Environment
;;;

(deftype frame () 'vector)

(defun build-runtime-environment (environment free-variables free-functions)
  (let* ((envs (coerce (loop :for env := environment :then (environment-next env)
                             :while env
                             :collect env)
                       'vector))
         (vmap (mapcar (lambda (variable)
                         (multiple-value-bind (var venv) (find-variable variable environment)
                           (list var
                                 (position var (environment-variables venv))
                                 (position venv envs))))
                       free-variables))
         (fmap (mapcar (lambda (function)
                         (multiple-value-bind (fun fenv) (find-function function environment)
                           (list fun
                                 (position fun (environment-functions fenv))
                                 (position fenv envs))))
                       free-functions)))

    (list envs vmap fmap)))


#|
(pprint

 (extend-environment-with-blocks
  (list (block 'foo))
  (extend-environment-with-variables
   (mapcar (cl:function lexical-variable) '(x y z))
   (extend-environment-with-functions
    (mapcar (cl:function local-function) '(f g h))
    (extend-environment-with-variables
     (mapcar (cl:function lexical-variable) '(a b c))
     (current-environment)))))
 )

(pprint
 (let ((*environment*
         (make-environment :blocks (list (block 'foo))
                           :next (make-environment :variables (mapcar (cl:function lexical-variable) '(a x y z) '(1 2 3 4))
                                                   :next (make-environment :functions (mapcar (cl:function local-function) '(f g h))
                                                                           :next (make-environment :variables (mapcar (cl:function lexical-variable) '(a b c) '(10 20 3))
                                                                                                   :next (current-environment)))))))
   (build-runtime-environment (current-environment)
                              (list (find-variable 'a (current-environment))
                                    (find-variable 'b (current-environment))
                                    (find-variable 'x (current-environment)))
                              (list (find-function 'g (current-environment))))))

(((#S(lexical-variable :name a :value 1) 0 1) (#S(lexical-variable :name b :value 20) 1 3)
  (#S(lexical-variable :name x :value 2) 1 1))
 ((#S(local-function :name g :lambda-list nil :body nil :compiler-macro nil :environment nil) 1 2)))

|#

;;;; THE END ;;;;
