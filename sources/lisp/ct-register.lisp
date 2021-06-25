;;;; -*- mode:lisp;coding:iso-8859-1 -*-
;;;;**************************************************************************
;;;;FILE:               ct-register.lisp
;;;;LANGUAGE:           Common-Lisp
;;;;SYSTEM:             Common-Lisp
;;;;USER-INTERFACE:     NONE
;;;;DESCRIPTION
;;;;
;;;;    This package provides an interface between macros and environments.
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

(defpackage "COM.INFORMATIMAGO.COMMON-LISP.LISP.CT-REGISTER"
  (:nicknames  "COM.INFORMATIMAGO.BOCL.MINIMAL-COMPILER.CT-REGISTER")
  (:use "COMMON-LISP")
  (:shadowing-import-from "COM.INFORMATIMAGO.BOCL"
                          "COMPILER-MACRO-FUNCTION" "MACRO-FUNCTION")
  (:export
   "COMPILER-MACRO-FUNCTION"
   "SYMBOL-MACRO-P"
   "SYMBOL-MACRO-EXPANSION"
   "MACRO-FUNCTION"
   ;; --------------------
   "REGISTER-COMPILER-MACRO"
   "REGISTER-SYMBOL-MACRO"
   "REGISTER-MACRO"
   "REGISTER-MACROLET"
   "REGISTER-FUNCTION"
   "REGISTER-TYPE"
   "REGISTER-CONSTANT-VARIABLE"
   "REGISTER-GLOBAL-PARAMETER"
   "REGISTER-GLOBAL-VARIABLE"
   ))
(in-package "COM.INFORMATIMAGO.COMMON-LISP.LISP.CT-REGISTER")


;;;
;;; Compiler macros
;;;

(defun compiler-macro-function (name &optional environment)
  (if (null environment)
      (get name 'kernel:$compiler-macro-function)
      (let ((cm (env:find-compiler-macro name environment)))
        (if (env:compiler-macro-p cm)
            (env:compiler-macro-function cm)))))

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
            environment))
          (t
           (error "Compiler macros (~S) can be stored only in the global environment."
                  name)))))
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
  (let* ((env (or environment (env:global-environment)))
         (sm (env:find-variable symbol env)))
    (when (env:symbol-macro-p sm)
      (env:symbol-macro-expansion sm))))

(defun (cl:setf symbol-macro-expansion) (new-expansion symbol &optional environment)
  (let* ((env (or environment (env:global-environment)))
         (sm (env:find-variable symbol env)))
    (cond
      ((null sm)
       (env:push-variable-onto-environment
        (if (eql environment (env:global-environment))
            (env:global-symbol-macro symbol new-expansion)
            (env:local-symbol-macro  symbol new-expansion))
        env))
      ((env:symbol-macro-p sm)
       (cl:setf (env:symbol-macro-expansion sm) new-expansion))
      (t (error "There's already a ~A named ~S" (type-of sm) symbol))))
  new-expansion)

;;;
;;; Normal macros
;;;

(defun macro-function (symbol &optional environment)
  (let* ((env (or environment (env:global-environment)))
         (mac (env:find-function symbol env)))
    (if (env:macrop mac)
        (env:macro-function mac)
        nil)))

(defun (cl:setf macro-function) (new-function symbol &optional environment)
  (let* ((env (or environment (env:global-environment)))
         (mac (env:find-function symbol env)))
    (cond
      ((null mac)
       (env:push-function-onto-environment
        (if (eql env (env:global-environment))
            (env:make-global-macro :name symbol :function new-function)
            (env:make-local-macro  :name symbol :function new-function))
        env))
      ((env:macrop mac)
       (cl:setf (env:macro-function mac) new-function))
      ((env:global-function-p mac)
       (env:pop-function-from-environment mac env)
       (env:push-function-onto-environment
        (env:make-global-macro :name symbol :function new-function)
        env))
      (t
       (error "There's something strange globally fbound to ~S, while defining a macro"
              symbol))))
  new-function)


;;; ------------------------------------------------------------

(defun register-compiler-macro (name lambda-list compiler-macro-function docstring &optional environment)
  (let* ((env (or environment (env:global-environment)))
         (mac (env:find-compiler-macro name env)))
    ;; TODO: provide restarts to redefine the function as a macro.
    (cond ((null mac)
           (if (eql env (env:global-environment))
               (env:push-compiler-macro-onto-environment
                (env:compiler-macro name lambda-list compiler-macro-function)
                env)
               (error "Cannot define a compiler-macro in a non-global environment")))
          ((env:compiler-macro-p mac)
           (setf (env:compiler-macro-lambda-list mac) lambda-list
                 (env:compiler-macro-function mac) compiler-macro-function))
          (t
           (error "There's already a ~:[macro~;function~] named ~S in the global environment, while defining a compiler-macro"
                  (env:functionp mac) name)))
    (setf (compiler-macro-function name) compiler-macro-function)
    (when docstring
      (setf (documentation name 'compiler-macro) docstring))
    name))

(defun register-symbol-macro (name expansion &optional environment)
  (let* ((env (or environment (env:global-environment)))
         (var (env:find-variable name env)))
    (if (and var (not (env:symbol-macro-p var)))
        (error 'variable-name-collision-error
               :name name :new-definition 'symbol-macro))
    (setf (symbol-macro-expansion name env) expansion)))

(defun register-macro (name lambda-list macro-function docstring)
  (cl:declare (cl:ignore lambda-list))
  ;; defmacro => global-environment
  #-(and) (let* ((env (env:global-environment))
                 (mac (env:find-function name env)))
            (cond ((null mac)
                   (env:push-function-onto-environment
                    (env:global-macro name lambda-list macro-function)
                    env))
                  ((env:macrop mac)
                   (setf (env:macro-lambda-list mac) lambda-list
                         (env:macro-function mac) macro-function))
                  ((env:global-function-p mac)
                   (env:pop-function-from-environment mac env)
                   (env:push-function-onto-environment
                    (env:global-macro name lambda-list macro-function)
                    env))
                  (t
                   (error "There's something strange globally fbound to ~S, while defining a macro"
                          name))))
  (setf (macro-function name (env:global-environment)) macro-function)
  (when docstring
    (setf (documentation name 'function) docstring))
  name)

(defun register-macrolet (name lambda-list macro-function docstring &optional environment)
  (cl:declare (cl:ignore lambda-list))
  #-(and) (let* ((env (or environment (env:global-environment)))
                 (mac (env:find-function name env)))
            (cond ((null mac)
                   (env:push-function-onto-environment
                    (env:local-macro name lambda-list macro-function environment)
                    env))
                  ((env:macrop mac)
                   (setf (env:macro-lambda-list mac) lambda-list
                         (env:macro-function mac) macro-function))
                  ((env:global-function-p mac)
                   (env:pop-function-from-environment mac env)
                   (env:push-function-onto-environment
                    (env:global-macro name lambda-list macro-function)
                    env))
                  (t
                   (error "There's something strange globally fbound to ~S, while defining a macro"
                          name))))
  (setf (macro-function name (or environment (env:global-environment))) macro-function)
  (when docstring
    (setf (documentation name 'function) docstring))
  name)

(defun register-function (name lambda-list function docstring &optional environment)
  (cl:declare (cl:ignore docstring))
  (let* ((env (or environment (env:global-environment)))
         (fun (env:find-function name env)))
    ;; TODO: provide restarts to redefine a macro as function.
    (cond ((null fun)
           (env:push-function-onto-environment
            (env:global-function name lambda-list function)
            env))
          ((env:global-function-p fun)
           (setf (env:function-lambda-list fun) lambda-list
                 (env:function-function fun) function))
          (t
           (error "There's already a function macro ~S in the global environment, while defining a function"
                  name)))))

(defun register-type (name lambda-expression docstring)
  (cl:declare (cl:ignore docstring))
  ;; TODO register a type.
  (setf (getf name 'kernel:$type) lambda-expression))

(defun register-constant-variable (name initial-value docstring)
  (cl:declare (cl:ignore docstring))
  (let* ((env (env:global-environment))
         (var (env:find-variable name env)))
    (cond
      ((null var)
       (env:push-variable-onto-environment
        (env:constant-variable name initial-value)
        env)
       (setf (getf name 'kernel:$constant-variable) t
             (symbol-value name) initial-value))
      ((env:constant-variable-p var)
       (if (not (eql (env:constant-variable-value var)
                     initial-value))
           (error "Trying to redefine the constant ~S with a different value ~S"
                  name initial-value)))
      (t
       (error 'variable-name-collision-error
              :name name :new-definition 'constant-variable)))))

(defun register-global-dynamic-variable (name initial-value docstring bindp overridep)
  (let* ((env (env:global-environment))
         (var (env:find-variable name env)))
    (cond
      ((null var)
       (env:push-variable-onto-environment
        (env:global-dynamic-variable name (if bindp initial-value 'kernel:$unbound))
        env)
       (setf (getf name 'kernel:$special) t)
       (if bindp
           (setf (symbol-value name) initial-value)))
      ((env:global-dynamic-variable-p var)
       (if (and overridep bindp)
           (setf (env:global-dynamic-variable-value var) initial-value))
       (setf (getf name 'kernel:$special) t)
       (if (and overridep bindp)
           (setf (symbol-value name) initial-value)))
      (t
       (error 'variable-name-collision-error
              :name name :new-definition 'dynamic-variable)))
    (when docstring
      (setf (documentation name 'variable) docstring))
    name))

(defun register-global-parameter (name initial-value documentation)
  (register-global-dynamic-variable name initial-value documentation t t))

(defun register-global-variable (name initial-value documentation bindp)
  (register-global-dynamic-variable name initial-value documentation bindp nil))



;;;; THE END ;;;;
