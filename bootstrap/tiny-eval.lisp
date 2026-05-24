;;;; -*- mode:lisp;coding:utf-8 -*-
;;;;**************************************************************************
;;;;FILE:               tiny-eval.lisp
;;;;LANGUAGE:           Common-Lisp
;;;;SYSTEM:             Common-Lisp
;;;;USER-INTERFACE:     NONE
;;;;DESCRIPTION
;;;;    
;;;;    The evaluator of a tiny subset of Common Lisp.
;;;;    
;;;;AUTHORS
;;;;    <PJB> Pascal J. Bourguignon <pjb@informatimago.com>
;;;;MODIFICATIONS
;;;;    2011-04-17 <PJB> Created.
;;;;BUGS
;;;;LEGAL
;;;;    GPL
;;;;    
;;;;    Copyright Pascal J. Bourguignon 2011 - 2011
;;;;    
;;;;    This program is free software; you can redistribute it and/or
;;;;    modify it under the terms of the GNU General Public License
;;;;    as published by the Free Software Foundation; either version
;;;;    2 of the License, or (at your option) any later version.
;;;;    
;;;;    This program is distributed in the hope that it will be
;;;;    useful, but WITHOUT ANY WARRANTY; without even the implied
;;;;    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
;;;;    PURPOSE.  See the GNU General Public License for more details.
;;;;    
;;;;    You should have received a copy of the GNU General Public
;;;;    License along with this program; if not, write to the Free
;;;;    Software Foundation, Inc., 59 Temple Place, Suite 330,
;;;;    Boston, MA 02111-1307 USA
;;;;**************************************************************************


(defvar *environment*
  '()
  "The global environment.")


#|



|#


(defstruct environment
  lexical-variables
  lexical-functions
  lexical-blocks
  dynamic-variables
  dynamic-blocks ; catch unwind-protect
  next)

(defun environment/add-lexical-variable (name value environment)
  (push (cons name value) (environment-lexical-variables environment))
  environment)

(defun environment/add-lexical-function (name function environment)
  (push (cons name function) (environment-lexical-functions environment))
  environment)

(defun environment/add-lexical-block (name environment)
  (make-environment :lexical-blocks (list (cons name environment))
                    :next environment))

(defun environment/add-dynamic-variable (name value environment)
  (push (cons name value) (environment-dynamic-variables environment))
  environment)

(defun environment/add-dynamic-block (name environment)
  (make-environment :dynamic-blocks (list (cons name environment))
                    :next environment))

(defun environment)







(defun tiny-eval (form)
  (evale form *environment*))


(defun check-arg-counts)
(defun evale (form environment)
  (cond
    ;; We don't implement symbol macros, so we only have variables:
    ((symbolp form)            (get-variable-value form environment))
    ;; The other atoms are unchanged:
    ((atom form)               form)
    ;; Now we have a list.  
    (t
     (case (first form)
       ;; First we check the special operators:
       ;; We don't implement (function (setf x)), only symbols and lambda:
       ((FUNCTION)
        (check-arg-count form 1 1)
        (if (symbolp (second form))
            (get-function-value (second form) *environment*)
            (make-closure (second form) *environment*)))
       ((QUOTE)
        (check-arg-count form 1 1)
        (second form))
       ((IF)
        (check-arg-count form 2 3)
        (if (tiny-eval (second form))
            (tiny-eval (third  form))
            (tiny-eval (fourth form))))
       ((BLOCK)
        (let ((*environment* (environment/block (second form) *environment*)))
          (tiny-eval `(progn ,(cddr form)))))
       ((RETURN-FROM)
        (check-arg-count form 1 2)
        (environment/return-from (second form) (tiny-eval (third form))))
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
