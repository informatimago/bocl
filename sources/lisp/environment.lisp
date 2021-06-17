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
  (:shadow "FUNCTION" "FUNCTIONP"
           "SPECIAL-OPERATOR-P"
           "COMPILER-MACRO-FUNCTION" "MACRO-FUNCTION" "FIND-RESTART"
           "BLOCK" "TAGBODY")
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
   "ENVIRONMENT-NEXT"

   "EXTEND-ENVIRONMENT-WITH-VARIABLES"       "PUSH-VARIABLE-ONTO-ENVIRONMENT"       "POP-VARIABLE-FROM-ENVIRONMENT"       "FIND-VARIABLE"
   "EXTEND-ENVIRONMENT-WITH-FUNCTIONS"       "PUSH-FUNCTION-ONTO-ENVIRONMENT"       "POP-FUNCTION-FROM-ENVIRONMENT"       "FIND-FUNCTION"
   "EXTEND-ENVIRONMENT-WITH-COMPILER-MACROS" "PUSH-COMPILER-MACRO-ONTO-ENVIRONMENT" "POP-COMPILER-MACRO-FROM-ENVIRONMENT" "FIND-COMPILER-MACRO"
   "EXTEND-ENVIRONMENT-WITH-DECLARATIONS"    "PUSH-DECLARATION-ONTO-ENVIRONMENT"    "POP-DECLARATION-FROM-ENVIRONMENT"    "FIND-DECLARATION"
   "EXTEND-ENVIRONMENT-WITH-BLOCKS"          "PUSH-BLOCK-ONTO-ENVIRONMENT"          "POP-BLOCK-FROM-ENVIRONMENT"          "FIND-BLOCK"
   "EXTEND-ENVIRONMENT-WITH-TAGS"            "PUSH-TAG-ONTO-ENVIRONMENT"            "POP-TAG-FROM-ENVIRONMENT"            "FIND-TAG"


   "AUTHENTIC-FUNCTION" "AUTHENTIC-FUNCTION-P" "AUTHENTIC-VARIABLE"
   "AUTHENTIC-VARIABLE-P" "BLOCK" "BLOCK-NAME" "BLOCKP" "COMPILER-MACRO"
   "COMPILER-MACRO-BODY" "COMPILER-MACRO-FUNCTION" "COMPILER-MACRO-P"
   "CONSTANT-VARIABLE" "CONSTANT-VARIABLE-P" "COPY-BLOCK"
   "COPY-COMPILER-MACRO" "COPY-CONSTANT-VARIABLE" "COPY-DECLARATION"
   "COPY-GLOBAL-FUNCTION" "COPY-GLOBAL-MACRO" "COPY-GLOBAL-SPECIAL-VARIABLE"
   "COPY-GLOBAL-SYMBOL-MACRO" "COPY-LEXICAL-VARIABLE" "COPY-LOCAL-FUNCTION"
   "COPY-LOCAL-MACRO" "COPY-LOCAL-SPECIAL-VARIABLE" "COPY-LOCAL-SYMBOL-MACRO"
   "COPY-SPECIAL-OPERATOR" "COPY-TAG" "COPY-TAGBODY" "DECLARATION"
   "DECLARATION-ARGUMENTS" "DECLARATION-NAME" "DECLARATIONP" "FUNCTION"
   "FUNCTION-BODY" "FUNCTION-FUNCTION" "FUNCTION-LAMBDA-LIST"
   "FUNCTION-NAME" "FUNCTIONP" "GLOBAL-FUNCTION" "GLOBAL-FUNCTION-P"
   "GLOBAL-MACRO" "GLOBAL-MACRO-P" "GLOBAL-SPECIAL-VARIABLE"
   "GLOBAL-SPECIAL-VARIABLE-P" "GLOBAL-SYMBOL-MACRO" "GLOBAL-SYMBOL-MACRO-P"
   "LEXICAL-VARIABLE" "LEXICAL-VARIABLE-P" "LOCAL-FUNCTION"
   "LOCAL-FUNCTION-ENVIRONMENT" "LOCAL-FUNCTION-P" "LOCAL-MACRO"
   "LOCAL-MACRO-ENVIRONMENT" "LOCAL-MACRO-P" "LOCAL-SPECIAL-VARIABLE"
   "LOCAL-SPECIAL-VARIABLE-P" "LOCAL-SYMBOL-MACRO" "LOCAL-SYMBOL-MACRO-P"
   "MACRO" "MACRO-BODY" "MACRO-FUNCTION" "MACROP" "MAKE-BLOCK"
   "MAKE-COMPILER-MACRO" "MAKE-CONSTANT-VARIABLE" "MAKE-DECLARATION"
   "MAKE-GLOBAL-FUNCTION" "MAKE-GLOBAL-MACRO" "MAKE-GLOBAL-SPECIAL-VARIABLE"
   "MAKE-GLOBAL-SYMBOL-MACRO" "MAKE-LEXICAL-VARIABLE" "MAKE-LOCAL-FUNCTION"
   "MAKE-LOCAL-MACRO" "MAKE-LOCAL-SPECIAL-VARIABLE" "MAKE-LOCAL-SYMBOL-MACRO"
   "MAKE-SPECIAL-OPERATOR" "MAKE-TAG" "MAKE-TAGBODY" "SPECIAL-OPERATOR"
   "SPECIAL-OPERATOR-P" "SYMBOL-MACRO" "SYMBOL-MACRO-EXPANSION"
   "SYMBOL-MACRO-P" "TAG" "TAG-INDEX" "TAG-NAME" "TAG-TAGBODY" "TAGBODY"
   "TAGBODY-TAGS" "TAGBODYP" "TAGP" "VARIABLE" "VARIABLE-NAME"
   "VARIABLE-VALUE" "VARIABLEP"

   ))
(in-package "COM.INFORMATIMAGO.BOCL.ENVIRONMENT")

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


(defstruct (variable
            (:constructor nil)
            (:copier nil)
            (:predicate variablep))
  name)

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
  value)

(defstruct (lexical-variable
            (:include authentic-variable)
            (:constructor make-lexical-variable)
            (:constructor lexical-variable (name &optional value))))

(defstruct (local-special-variable
            (:include authentic-variable)
            (:constructor make-local-special-variable)
            (:constructor local-special-variable (name &optional value))))

(defstruct (global-special-variable
            (:include authentic-variable)
            (:constructor make-global-special-variable)
            (:constructor global-special-variable (name &optional value))))

(defstruct (constant-variable
            (:include authentic-variable)
            (:constructor make-constant-variable)
            (:constructor constant-variable (name &optional value))))

(defstruct (function
            (:predicate functionp)
            (:constructor nil)
            (:copier nil))
  name
  lambda-list)

(defstruct (authentic-function
            (:include function)
            (:constructor nil)
            (:copier nil)
            (:conc-name function-))
  function                          ; the function object
  body) ; the body form to be interpreted.

(defstruct (global-function
            (:include authentic-function)
            (:constructor make-global-function)
            (:constructor global-function (name &optional lambda-list function body))))

(defstruct (local-function
            (:include authentic-function)
            (:constructor make-local-function)
            (:constructor local-function (name &optional lambda-list function body environment)))
  environment)

(defstruct (compiler-macro
            (:include function)
            (:constructor make-compiler-macro)
            (:constructor compiler-macro (name &optional lambda-list function body)))
  function ; the function object
  body) ; the body form to be interpreted.

(defstruct (macro
            (:include function)
            (:predicate macrop)
            (:constructor nil)
            (:copier nil))
  function ; the function object
  body) ; the body form to be interpreted.

(defstruct (global-macro
            (:include macro)
            (:constructor make-global-macro)
            (:constructor global-macro (name &optional lambda-list function body))))

(defstruct (local-macro
            (:include macro)
            (:constructor make-local-macro)
            (:constructor local-macro (name &optional lambda-list function body environment)))
  environment)

(defstruct (special-operator
            (:include function)
            (:constructor make-special-operator)
            (:constructor special-operator)))

(defstruct (declaration
            (:predicate declarationp)
            (:constructor make-declaration)
            (:constructor declaration (name &optional arguments)))
  name
  arguments)

(defstruct (block
            (:predicate blockp)
            (:constructor make-block)
            (:constructor block (name)))
  name)

(defstruct (tagbody
              (:predicate tagbodyp)
              (:constructor make-tagbody))
  tags)

(defstruct (tag
            (:predicate tagp)
            (:constructor make-tag)
            (:constructor tag (name &optional index tagbody)))
  name                                  ; symbol or integer
  index
  tagbody)



;;;
;;; Run-Time Environment
;;;

(defstruct runtime-environment
  next-environments          ; a list of environments in outer scopes.

  ;; The NEXT-ENVIRONMENTS contains the runtime-environments that are
  ;; used by the closures using this runtime-environment, for direct
  ;; access to their records. (It'll be stored as the first slots in
  ;; this environment record).
  ;;
  ;; Both variables and functions are a-list mapping names to offset
  ;; in the runtime environment record, counting after the
  ;; NEXT-ENVIRONMENTS.

  variables
  functions)

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
