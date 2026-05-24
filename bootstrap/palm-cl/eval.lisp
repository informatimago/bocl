;;;;  -*- mode:lisp;coding:utf-8 -*-
;;;;**************************************************************************
;;;; FILE:               eval.lisp
;;;; LANGUAGE:           Common-Lisp
;;;; SYSTEM:             Common-Lisp
;;;; USER-INTERFACE:     NONE
;;;; DESCRIPTION
;;;;     
;;;;     The Common Lisp interpreter.
;;;;     
;;;; AUTHORS
;;;;     <PJB> Pascal Bourguignon <pjb@informatimago.com>
;;;; MODIFICATIONS
;;;;     2007-06-16 <PJB> Created.
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

(defun not-implemented-yet () (error "Not implemented yet."))


;;;----------------------------------------------------------------------
;;; ENVIRONMENT
;;;----------------------------------------------------------------------

;;; Environments are composed of two parts: dynamic environment and
;;; lexical environment.  Each part consists in a set of lists
;;; containing the name or bindings.

;;; When a new scope (of whatever kind) is opened, a new environment is
;;; created, with either a new dynamic or a new lexical part, shallow
;;; copy of the parent environment: the lists are the same.  Local
;;; names are prepended to these lists, possibly shadowing same names
;;; accessible from the parent environments.



;; dynamic environment:
;; # bindings for dynamic variables.
;; # information about active catch tags.
;; # information about exit points established by unwind-protect.
;; # information about active handlers and restarts.

(defstruct dynamic
  variables   ;; dynamic variables and constant variables
  protectors  ;; (unwind-protect)
  catchers    ;; (catch throw)
  handlers    ;; (handler-case handler-bind signal error cerror ...)
  restarts)   ;; (restart-case ...)

;; lexical environment:
;; # bindings of lexical variables and symbol macros.
;; # bindings of functions and macros. (Implicit in this is information
;;   about those compiler macros that are locally disabled.)
;; # bindings of block tags.
;; # bindings of go tags.
;; # information about declarations.

(defstruct lexical
  variables ; lexical variables (setq on toplevel in global, let/let*/lambda)
  functions ; functions and compiler-macros
  macros
  symbol-macros 
  blocks
  tags
  declarations)


;; global environment:
;; # bindings of dynamic variables and constant variables.
;; # bindings of functions, macros, and special operators.
;; # bindings of compiler macros.
;; # bindings of type and class names
;; # information about proclamations.

(defstruct environment
  dynamic
  lexical)

(defparameter *global-environment*
  (make-environment :dynamic (make-dynamic) :lexical (make-lexical))
  "Also known as the null-environment, denoted by NIL.")


(defun environment-push-block (name env)
  "Push a new lexical frame with a  block named NAME (BLOCK NAME ...)."
  (let ((lenv (environment-lexical env)))
    (make-environment
     :dynamic (environment-dynamic env)
     :lexical (make-lexical :variables         (lexical-variables     lenv)
                            :functions         (lexical-functions     lenv)
                            :macros            (lexical-macros        lenv)
                            :symbol-macros     (lexical-symbol-macros lenv)
                            :blocks (cons name (lexical-blocks        lenv))
                            :catches           (lexical-catches       lenv)
                            :tags              (lexical-tags          lenv)
                            :declarations      (lexical-declarations  lenv)))))



(defvar *unbound* (gensym) "The 'unbound' value.")

(defstruct (binding (:type list)) name value)



(defun dynamic-variable-p              (env name)
  "Return the binding of NAME as a dynamic variable, or NIL."
  (find name (dynamic-variables (environment-dynamic env))
        :key (function binding-name)))

;; protectors have no name.

(defun catcher-p                       (env value)
  "Return the binding of VALUE as a catcher, or NIL."
  (find name (dynamic-catchers (environment-dynamic env))
        :key (function binding-name)))

(defun handler-p                       (env condition-name)
  "Return the binding of CONDITION-NAME as a handler, or NIL."
  (find condition-name (dynamic-handlers (environment-dynamic env))
        :key (function binding-name)
        :test (function subtypep)))

(defun restart-p                       (env restart-name)
  "Return the binding of CONDITION-NAME as a handler, or NIL."
  (find restart-name (dynamic-restarts (environment-dynamic env))
        :key (function binding-name)))



(defun lexical-variable-p              (env name)
  "Return the binding of NAME as a lexical variable, or NIL."
  (find name (lexical-variables (environment-lexical env))
        :key (function binding-name)))

(defun lexical-function-p              (env name)
  "Return the binding of NAME as a lexical function, or NIL."
  (find name (lexical-functions (environment-lexical env))
        :key (function binding-name)))

(defun symbol-macro-p                  (env name)
  "Return the binding of NAME as a symbol-macro, or NIL."
  (find name (lexical-symbol-macros (environment-lexical env))
        :key (function binding-name)))

(defun block-p                         (env name)
  "Return the binding of NAME as a block, or NIL."
  (find name (lexical-blocks (environment-lexical env))
        :key (function binding-name)))

(defun tags-p                          (env name)
  "Return the binding of NAME as a tag, or NIL."
  (find name (lexical-tags (environment-lexical env))
        :key (function binding-name)))

(defun declaration-p                   (env name)
  "Return the binding of NAME as a declaration, or NIL."
  (find name (lexical-declarations (environment-lexical env))
        :key (function binding-name)))






(defun macro-p                         (env name) (not-implemented-yet))
(defun function-p                      (env name) (not-implemented-yet))



(defun make-global-variable            (env name)
  "
Insert in the environment ENV a new unbound dynamic variable named NAME.
"
  (push (cons name *unbound*) (environment-global-variables      env)))


(defun global-variable-p               (env name)
  (assoc name (environment-global-variables      env)))


(defun global-variable-value           (env name)
  (let ((ass (assoc name (environment-global-variables      env))))
    (cond ((null ass)               (result-error "Undefined variable ~S" name))
          ((eq *unbound* (cdr ass)) (result-error "Unbound variable ~S" name))
          (t                        (result-values env (cdr ass))))))


(defun make-global-symbol-macro        (env name)
  (push (cons name *unbound*) (environment-global-symbol-macros  env)))

(defun global-symbol-macro-variable-p  (env name)
  (assoc name (environment-global-symbol-macros  env)))


(defun bind-lexical-variable           (env name value)
  (push (cons name value) (environment-lexical-variables     env)))
(defun lexical-variable-p              (env name)
  (assoc name (environment-lexical-variables     env)))
(defun lexical-variable-value          (env name)
  (let ((ass (assoc name (environment-lexical-variables      env))))
    (if (null ass)
        (result-error "Undefined variable ~S" name)
        (result-values env (cdr ass)))))

(defun bind-lexical-symbol-macro       (env name expansion)
  (push (cons name expansion) (environment-lexical-symbol-macros env)))
(defun lexical-symbol-macro-p          (env name)
  (assoc name (environment-lexical-symbol-macros env)))
(defun lexical-symbol-macro-expansion  (env name)
  (let ((ass (assoc name (environment-lexical-symbol-macros      env))))
    (if (null ass)
        (result-error "Undefined symbol-macro ~S" name)
        (result-values env (cdr ass)))))




(defun find-block-named (env name)
  (find name        (environment-blocks   env) :test (function eq)))

(defun find-catch-value (env catch-value)
  (find catch-value (environment-catches  env) :test (function eq)))

(defun find-go-tag      (env tag)
  (find tag         (environment-tags     env) :test (function eql)))





(defun result-error           (env condition &rest arguments)
  ;; (apply (function error) condition arguments)
  (cons :error (apply (function make-condition) condition arguments)))

(defun result-values (env &rest multiple-values)
  (declare (ignore env))
  (cons :values multiple-values))

(defun result-values-list  (env values-list)
  (declare (ignore env))
  (cons :values values-list))

(defun result-values-p (result)
  (and (consp result) (eq :values (first result))))

(defun result-multiple-value-list (result) (rest result))

(defun result-nth-value (n result)
  (nth (1+ n) result))

(defun result-block-exit      (env block-name result)
  (if (find-block-named env block-name)
      (list :block block-name result)
      (result-error env "No block named ~S" block-name)))

(defun result-block-p (result name)
  (and (consp result) (eq :block  (first result)) (eq  name (second result))))

(defun result-throw-exit      (env catch-value result)
  (if (find-catch-value env catch-value)
      (list :throw catch-value result)
      (result-error env "No catch for ~S" catch-value)))

(defun result-throw-p (result value)
  (and (consp result) (eq :throw  (first result)) (eql value (second result))))

(defun result-go              (env tag)
  (if (find-go-tag env tag)
      (list :go tag)
      (result-error env "No GO tag ~S" tag)))

(defun result-go-p (result tag)
  (and (consp result) (eq :go     (first result)) (eql tag   (second result))))


(defun check-result-exceptional (result)
  (if (result-values-p result)
      nil
      result))

(defun check-symbolp (object &optional message)
  (if (symbolp object)
      nil
      (result-error "~:[~;~:*~A: ~]~S should be a symbol," object)))

(defun length>= (list mini)
  (loop :named nil :do
     (cond ((zerop mini) (return (values t   list)))
           ((endp list)  (return (values nil list)))
           (t            (pop list) (decf mini)))))

(defun length<= (list maxi)
  (loop :named nil :do
     (cond ((zerop maxi) (return (values (endp list) list)))
           ((endp list)  (return (values t           list)))
           (t            (pop list) (decf maxi)))))

(defun length-between-p (list mini maxi)
  (multiple-value-bind (p r) (length>= list mini)
        (and p (length<= r  (- maxi mini)))))

(defun test-lengths ()
  (loop
     :for i :from 0 :to 10
     :do (print `(,i ,(length>= '(a b c d e) i)
                     ,(length<= '(a b c d e) i)
                     ,(length-between-p '(a b c d e) (1- i) (1+ i))))))

(defun check-argument-count (env form mini maxi)
  (if (if (null maxi)
          (length>= form (1+ mini))
          (length-between-p form (1+ mini) (1+ maxi)))
      nil
      (result-error env (if (<= (length form) maxi)
                            "Not enough argument given to ~S in ~S"
                            "Too many argument given to ~S in ~S")
                    (first form) form)))






(defun eval/symbol (env form)
  (cond ((lexical-variable-p     env form)
         (result-values env (lexical-variable-value env form)))
        ((global-variable-p      env form)
         (global-variable-value env form))
        ((result-error env "Symbol ~S is not bound." form))))

(defun eval/function (env form)
  (or (check-argument-count env form 1 1)
      (cond
        ((and (consp (second form)) (eq 'lambda (first (second form))))
         (return-value env (%compile nil (second form))))
        ((and (symbolp (second form))) (function-bound-p env (second form))
         (result-values env (function-value env (second form))))
        (t
         (result-error env "~S is not a function name" (second form))))))

(defun eval/quote (env form)
  (or (check-argument-count env form 1 1)
      (second form)))

(defun eval/if (env form)
  (or (check-argument-count env form 2 3)
      (let ((test (eval-in-environment env (second form))))
        (if (result-values-p test)
            (eval-in-environment env (if (result-nth-value 0 test)
                                         (third  form)
                                         (fourth form)))
            test))))

(defun eval/block (env form)
  (or (check-argument-count env form 1 nil)
      (check-symbolp (second form) "The BLOCK name")
      (let ((env  (environment-push-block (second form) env)))
        (loop
           :for expression :in (cddr form)
           :for result = (eval-in-environment env expression)
           :until (check-result-exceptional result)
           :finally (if (result-block-p result (second form))
                        (return (third result))
                        (return result))))))

(defun eval/return-from (env form)
  (or (check-argument-count env form 1 2)
      (let ((result (eval-in-environment env (third form))))
        (or (check-result-exceptional result)
            (result-block-exit env (second form) result)))))

(DEFUN EVAL/CATCH (ENV FORM) (NOT-IMPLEMENTED-YET))
(DEFUN EVAL/THROW (ENV FORM) (NOT-IMPLEMENTED-YET))
(DEFUN EVAL/UNWIND-PROTECT (ENV FORM) (NOT-IMPLEMENTED-YET))
(DEFUN EVAL/TAGBODY (ENV FORM) (NOT-IMPLEMENTED-YET))
(DEFUN EVAL/GO (ENV FORM) (NOT-IMPLEMENTED-YET))
(DEFUN EVAL/FLET (ENV FORM) (NOT-IMPLEMENTED-YET))
(DEFUN EVAL/LABELS (ENV FORM) (NOT-IMPLEMENTED-YET))
(DEFUN EVAL/SETQ (ENV FORM) (NOT-IMPLEMENTED-YET))
(DEFUN EVAL/LET (ENV FORM) (NOT-IMPLEMENTED-YET))
(DEFUN EVAL/LET* (ENV FORM) (NOT-IMPLEMENTED-YET))

(DEFUN EVAL/MULTIPLE-VALUE-CALL (ENV FORM)
  (loop
     ;; evaluate the arguments
     :for arg :in (rest form)
     :for result = (eval-in-environment env arg)
     :while (result-values-p result)
     :nconc (result-multiple-value-list result) :into arguments 
     :finally (return (if arg ; if there was an error evaluating some argument
                          result        ; then return the errror
                                        ; else apply the function
                          (apply-in-environment env
                                                (get-function (first form))
                                                arguments)))))

(defun eval/funcall (env form)
  (loop
     ;; evaluate the arguments
     :for arg :in (rest form)
     :for result = (eval-in-environment env arg)
     :while (result-values-p result)
     :collect (result-nth-value 0 result) :into arguments
     :finally (return (if arg ; if there was an error evaluating some argument
                          result        ; then return the errror
                                        ; else apply the function
                          (apply-in-environment env
                                                (get-function (first form))
                                                arguments)))))

(DEFUN EVAL/MULTIPLE-VALUE-PROG1 (ENV FORM) (NOT-IMPLEMENTED-YET))

(defun do-progn (env forms)
  (loop
     :for expression :in forms
     :for result = (eval-in-environment env expression)
     :until (check-result-exceptional result)
     :finally (return result)))

(defun eval/progn (env form)
  (do-progn env (cdr form)))


(DEFUN EVAL/PROGV (ENV FORM) (NOT-IMPLEMENTED-YET))


(defun split-declarations (forms)
  (loop
     :for current :on forms
     :while (and (consp (car current)) (eq 'declare (caar current)))
     :collect (car current) :into declarations
     :finally (return (values declarations current))))

(DEFUN EVAL/LOCALLY (ENV FORM)
  (warn "LOCALLY not implemented yet.")
  (do-progn env  (nth-value 1 (split-declarations (cdr form)))))


(DEFUN EVAL/THE (ENV FORM)
  ;; We do nothing, since we can. :-(
  (or (check-argument-count env form 2 2)
      (eval-in-environment env (third form))))

(defvar *current-situation* :execute)

(defun normalize-situations (situations)
  (mapcar (lambda (situation)
            (case situation
              ((:compile-toplevel :load-toplevel :execute) situation)
              ((compile)                                   :compile-toplevel)
              ((load)                                      :load-toplevel)
              ((eval)                                      :execute)
              (otherwise                                   situation)))
          situations))


(DEFUN EVAL/EVAL-WHEN (ENV FORM)
  (warn "EVAL-WHEN not implemented yet.")
  (or (check-argument-count env form 1 nil)
      (if (or t (member *current-situation*
                        (normalize-situations (second form))))
          (do-progn env (cddr form))
          (result-values nil))))


(defun eval-in-environment (env form)
  (cond
    ((symbolp form)              (eval/symbol               env form))
    ;; The other atoms are self evaluating:
    ((atom form)                 (result-values             env form))
    ;; Now we have a list.  
    (t (case (first form)
         ;; First we check the special operators:
         ((FUNCTION)             (eval/function             env form))
         ((QUOTE)                (eval/quote                env form))
         ((IF)                   (eval/if                   env form))
         ((BLOCK)                (eval/block                env form))
         ((RETURN-FROM)          (eval/return-from          env form))
         ((CATCH)                (eval/catch                env form))
         ((THROW)                (eval/throw                env form))
         ((UNWIND-PROTECT)       (eval/unwind-protect       env form))
         ((TAGBODY)              (eval/tagbody              env form))
         ((GO)                   (eval/go                   env form))
         ((FLET)                 (eval/flet                 env form))
         ((LABELS)               (eval/labels               env form))
         ((SETQ)                 (eval/setq                 env form))
         ((LET)                  (eval/let                  env form))
         ((LET*)                 (eval/let*                 env form))
         ((MULTIPLE-VALUE-CALL)  (eval/multiple-value-call  env form))
         ((MULTIPLE-VALUE-PROG1) (eval/multiple-value-prog1 env form))
         ((PROGN)                (eval/progn                env form))
         ((PROGV)                (eval/progv                env form))
         ((LOCALLY)              (eval/locally              env form))
         ((THE)                  (eval/the                  env form))
         ((EVAL-WHEN)            (eval/eval-when            env form))
         ((SYMBOL-MACROLET MACROLET LOAD-TIME-VALUE)
          (return-error env
                        "~S shouldn't occur after minimal compilation"
                        (first form)))
         (otherwise
          ;; We don't have macros anymore, since we do minimal compilation
          ;; before calling the interpreter.
          ;; It must be a function call.
          (eval/funcall env form))))))


(defun apply-in-environment (env function arguments)
  (if (functionp function)
       ;; a primitive function:
      (handler-case
          (result-values-list env (multiple-value-list (apply function arguments)))
        (error (err) (return-error env "~A" err)))
      ;; an interpreted function:
      ()))



(defstruct function
  lambda-expression
  compiled)

(defstruct (closure (:include function))
  environment)

(defun %compile (name &optional definition)
  (result-values
   )
  )

(defun %eval (form)
  (eval-in-environment *global-environment*
                       `(funcall (compile nil '(lambda () ,form)))))


;; (handler-case form
;;   (type1 (var1) . body1)
;;   (type2 (var2) . body2) ...)
;; 
;; (block #1=#:g0001
;;   (let ((#2=#:g0002 nil))
;;     (tagbody
;;        (handler-bind ((type1 #'(lambda (temp)
;;                                  (setq #1# temp)
;;                                  (go #3=#:g0003)))
;;                       (type2 #'(lambda (temp)
;;                                  (setq #2# temp)
;;                                  (go #4=#:g0004))) ...)
;;          (return-from #1# form))
;;        #3# (return-from #1# (let ((var1 #2#)) . body1))
;;        #4# (return-from #1# (let ((var2 #2#)) . body2)) ...)))
;; 
;; (BLOCK #:G0001
;;   (LET ((#:G0002 NIL))
;;     (TAGBODY
;;        (HANDLER-BIND
;;            ((TYPE1 #'(LAMBDA (TEMP) (SETQ #:G0001 TEMP) (GO #:G0003)))
;;             (TYPE2 #'(LAMBDA (TEMP) (SETQ #:G0002 TEMP) (GO #:G0004))) |...|)
;;          (RETURN-FROM #:G0001 FORM))
;;        #:G0003
;;        (RETURN-FROM #:G0001 (LET ((VAR1 #:G0002)) . BODY1))
;;        #:G0004
;;        (RETURN-FROM #:G0001 (LET ((VAR2 #:G0002)) . BODY2)) |...|)))
;; 
;; (handler-case form
;;   (type1 (var1) . body1)
;;   ...
;;   (:no-error (varN-1 varN-2 ...) . bodyN))
;; 
;; (block #1=#:error-return
;;   (multiple-value-call #'(lambda (varN-1 varN-2 ...) . bodyN)
;;     (block #2=#:normal-return
;;       (return-from #1#
;;         (handler-case (return-from #2# form)
;;           (type1 (var1) . body1) ...)))))
;; 
;; (BLOCK #:ERROR-RETURN
;;   (MULTIPLE-VALUE-CALL #'(LAMBDA (VARN-1 VARN-2 |...|) . BODYN)
;;     (BLOCK #:NORMAL-RETURN
;;       (RETURN-FROM #:ERROR-RETURN
;;         (HANDLER-CASE (RETURN-FROM #:NORMAL-RETURN FORM)
;;           (TYPE1 (VAR1) . BODY1)
;;           |...|)))))


(defun test-eval ()
  (assert (equal 
           (eval-in-environment *global-environment*
                                '(progn (progn 2) (progn 3 4 5)))
           '(:values 5)))
  (assert (equal 
           (eval-in-environment *global-environment*
                                '(block :name (progn 2) (progn 3 4 5)))
           '(:values 5)))
  (assert (equal
           (eval-in-environment *global-environment*
                                '(block :name (progn 2)
                                  (return-from :name 3)
                                  (progn 4 5)))
           '(:VALUES 3)))
  (assert (equal
            (eval-in-environment *global-environment*
                                '(block :name (progn 2)
                                  (progn 3
                                         (block :other
                                           (progn 4)
                                           (return-from :name 5)
                                           (progn 6))
                                         (progn 7))
                                  (progn 8)))
            '(:VALUES 5)))

  )



