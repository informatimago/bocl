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
  (:export
   "REGISTER-COMPILER-MACRO"
   "REGISTER-MACRO"
   "REGISTER-SYMBOL-MACRO"
   "REGISTER-TYPE"
   "REGISTER-CONSTANT-VARIABLE"
   "REGISTER-DYNAMIC-VARIABLE" ))
(in-package "COM.INFORMATIMAGO.COMMON-LISP.LISP.CT-REGISTER")


(defun register-compiler-macro (name lambda-expression docstring)
  ;; TODO: Test for function in the global environment?
  (setf (compiler-macro-function name) lambda-expression)
  (if docstring
      (setf (documentation name 'compiler-macro) docstring)))

(defun register-macro (name lambda-expression docstring)
  ;; TODO: Test for function in the global environment?
  (setf (macro-function name) lambda-expression)
  (if docstring
      (setf (documentation name 'macro) docstring)))

(defun register-symbol-macro (name expansion)
  (let ((var (env:find-variable name (env:global-environment))))
    (if (and var (not (env:symbol-macro-p var)))
        (error 'variable-name-collision-error
               :name name :new-definition 'symbol-macro))
    (setf (symbol-macro-expansion name (env:global-environment)) expansion)))

(defun register-type (name lambda-expression docstring)
  ;; TODO register a type.
  (setf (getf name 'kernel:$type) lambda-expression)
  (if docstring
      (setf (documentation name 'type) docstring)))

(defun register-constant-variable (name initial-value docstring)
  (declare (ignore initial-value))
  ;; TODO register a variable in the environment
  (if (getf name 'kernel:$special)
      (error 'variable-name-collision-error
             :name name :new-definition 'constant-variable))
  (setf (getf name 'kernel:$constant-variable) t)
  (if docstring
      (setf (documentation name 'variable) docstring)))

(defun register-dynamic-variable (name initial-value docstring)
  ;; TODO register a variable in the environment
  (if (getf name 'kernel:$constant-variable)
      (error 'variable-name-collision-error
             :name name :new-definition 'dynamic-variable))
  (setf (getf name 'kernel:$special) t)
  (if docstring
      (setf (documentation name 'variable) docstring)))


;;;; THE END ;;;;
