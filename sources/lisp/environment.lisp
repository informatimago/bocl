;;;; -*- mode:lisp;coding:iso-8859-1 -*-

(defpackage "COM.INFORMATIMAGO.BOCL.ENVIRONMENT"
  (:use "COMMON-LISP")
  (:export "GLOBAL-ENVIRONMENT"))
(in-package "COM.INFORMATIMAGO.BOCL.ENVIRONMENT")


#|

# -*- mode:org -*-

Operators openning a new frame:

| Operator       | Scope           | Binding    | Exit                      |
|----------------+-----------------+------------+---------------------------|
| CATCH          | dynamic         | object     | Remove catch object       |
| PROGV          | dynamic         | variable   | Remove  scope (variables) |
| UNWIND-PROTECT | dynamic         | cleanup    | Execute cleanup           |
| BLOCK          | lexical         | blocks     | Remove scope (blocks)     |
| FLET           | lexical         | functions  | Remove scope (functions)  |
| LABELS         | lexical         | functions  | Remove scope (functions)  |
| TAGBODY        | lexical         | tags       | Remove scope (tags)       |
| LET            | lexical/dynamic | variables  | Remove scopes (variables) |
| LET*           | lexical/dynamic | variables  | Remove scopes (variables) |
| LAMBDA         | lexical/dynamic | parameters | Remove scopes (variables) |
|----------------+-----------------+------------+---------------------------|


#+BEGIN_EXAMPLE

(defun f () (declare (special goo)) goo)

(let ((goo 42))
  (list
   goo
   (let ((goo 33))
     (declare (special goo))
     (list
      goo
      (f)
      (let ((goo 22))
        (list goo
              (f)
              (locally
                  (declare (special goo))
                (f))
              (let ((goo 11))
                (declare (special goo))
                (f))))))))
;; --> (42 (33 33 (22 33 33 11)))

#+END_EXAMPLE


|#




(defvar *environment* '()
  "The global environment.")

(defun GLOBAL-ENVIRONMENT ()
  *environment*)

(defstruct (environment (:conc-name env-))
  lexical-variables
  lexical-functions
  lexical-blocks
  lexical-catchs
  lexical-declarations
  dynamic-variables
  next
  (frames :type c-pointer))



(defun env/add-lexical-variable (name value environment)
  (push (cons name value) (environment-lexical-variables environment))
  environment)

(defun env/add-lexical-function (name function environment)
  (push (cons name function) (environment-lexical-functions environment))
  environment)


(env/find-variable name environment) -> variable
(env/find-function name environment) -> function
(env/find-block    name environment) -> block
(env/find-tag      name environment) -> tag

env/get-variable-value
env/set-variable-value
env/get-function-value

variable-constantp

(add-lexical-variable    name environment)
(add-special-variable    name environment)
(add-local-symbol-macro  name environment)

(variable-type variable) -> type-sexp
(variable-ignore variable) -> boolean
(variable-dynamic-extend variable) -> boolean

(defstruct variable
  name
  value
  type
  ignore
  dynamic-extend)

(add-local-function  name environment)
(add-local-macro     name environment)

(function-type function) -> (function (…) …)
(function-ignore function) -> boolean
(function-dynamic-extend function) -> boolean
(function-inline function) -> boolean

(add-block name environment)
(add-tag   name environment)


(defclass variable ()
  ((name)))

(defclass symbol-macro (variable)
  ((expansion)))
(defclass local-symbol-macro (symbol-macro)
  ())
(defclass global-symbol-macro (symbol-macro)
  ())

(defclass authentic-variable (variable)
  ((value)))
(defclass lexical-variable (authentic-variable)
  ())
(defclass local-special-variable (authentic-variable)
  ())
(defclass global-special-variable (authentic-variable)
  ())
(defclass constant-variable (authentic-variable)
  ())

(defclass function ()
  ((name)
   (lambda-list)))

(defclass authentic-function (function)
  ((body)
   (compiler-macro)))

(defclass local-function (authentic-function)
  ())
(defclass global-function (authentic-function)
  ())

(defclass macro-function (function)
  ((body)))
(defclass local-macro (macro-function)
  ())
(defclass global-macro (macro-function)
  ())

(defclass special-operator (function)
  ())



(defclass block ()
  ((name)))

(defclass tag ()
  ((name)))


#|

# -*- mode:org -*-

| Class <- Subclass                             |
|-----------------------------------------------|
| variable <- symbol-macro                      |
| variable <- authentic-variable                |
| authentic-variable <- lexical-variable        |
| authentic-variable <- local-special-variable  |
| authentic-variable <- global-special-variable |
| authentic-variable <- constant-variable       |
| symbol-macro <- global-symbol-macro           |
| symbol-macro <- local-symbol-macro            |
|                                               |
| function <- authentic-function                |
| authentic-function <- local-function          |
| authentic-function <- global-function         |
| global-function <- generic-function           |
| function <- macro                             |
| macro <- local-macro                          |
| macro <- global-macro                         |
| function <- special-operator                  |
|                                               |
| block                                         |
|                                               |
| tag                                           |
|                                               |
| declaration <- special-declaration            |
|                                               |

|#


(defun env/add-lexical-frame (name environment)
  (make-environment :lexical-frames (list (cons name environment))
                    :next environment))

(defun env/add-dynamic-variable (name value environment)
  (push (cons name value) (environment-dynamic-variables environment))
  environment)

(defun env/add-dynamic-frame (name environment)
  (make-environment :dynamic-frames (list (cons name environment))
                    :next environment))



#|


(FUNCTION foo)
(FUNCTION (SETF foo))

Find function `foo` or `(SETF foo)` in the environment: global or lexical.
Return the function closure.


(FUNCTION (LAMBDA (…) …))

Create an interpreted anonymous function with the lambda expression.
Find the free variables and (local) free functions.
Create a closure with the function and its free bindings.
Signal an error if the closure is not complete (if no binding is found).


|#

(defun parse-tagbody (body)
  (flet ((go-tag-p (object) (or (symbolp object) (integerp object))))
    (loop
      with tag = "Initial"
      with section = '()
      with sections = '()
      for form in body
      do (if (go-tag-p form)
             (progn
               (push (cons tag (nreverse section)) sections)
               (setf tag form
                     section '()))
             (push form section))
      finally (push (cons tag (nreverse section)) sections)
              (return (nreverse sections)))))


(let ((*print-right-margin* 60))
  (pprint (parse-tagbody (cdr '(tagbody
                                (setq n (f2 flag (lambda () (go out))))
                                out
                                (prin1 n))))))

(("Initial" (setq n (f2 flag (lambda nil (go out)))))
 (out (prin1 n)))


(let ((*print-right-margin* 60))
  (pprint (parse-tagbody (cdr '(tagbody
                                (setq val 1)
                                (go point-a)
                                (incf val 16)
                                point-c
                                (incf val 04)
                                (go point-b)
                                (incf val 32)
                                point-a
                                (incf val 02)
                                (go point-c)
                                (incf val 64)
                                point-b
                                (incf val 08))))))

(("Initial" (setq val 1) (go point-a) (incf val 16))
 (point-c (incf val 4) (go point-b) (incf val 32))
 (point-a (incf val 2) (go point-c) (incf val 64))
 (point-b (incf val 8)))


(defmacro tagbody (&body body)
  )

#|

Interpretation of a tagbody consists in interpreting sequentially the
forms in the body.  GO may jump to another section of the same
tagbody.  But in any case, unless GO is a tagbody toplevel form, it
will involve a non-local exit, to find the tagbody concerned.

|#

(shadow '(go tagbody))



#-(and)
(pprint (macroexpand-1 '(tagbody
                         (setq val 1)
                         (go point-a)
                         (incf val 16)
                         point-c
                         (incf val 04)
                         (go point-b)
                         (incf val 32)
                         point-a
                         (incf val 02)
                         (go point-c)
                         (incf val 64)
                         point-b
                         (incf val 08))))






             (generate-kernel-function 'env/make-constant-variable           '(name))
             (generate-kernel-function 'env/make-global-special-variable     '(name))
             (generate-kernel-function 'env/make-local-special-variable      '(name))
             (generate-kernel-function 'env/make-lexical-variable            '(name))
             (generate-kernel-function 'env/make-global-symbol-macro         '(name))
             (generate-kernel-function 'env/make-local-symbol-macro          '(name))
             (generate-kernel-function 'env/make-global-function             '(name))
             (generate-kernel-function 'env/make-local-function              '(name))
             (generate-kernel-function 'env/make-global-macro                '(name))
             (generate-kernel-function 'env/make-local-macro                 '(name))

             (generate-kernel-function 'env/variablep                        '(object) :values '(boolean))
             (generate-kernel-function 'env/constant-variable-p              '(object) :values '(boolean))
             (generate-kernel-function 'env/special-variable-p               '(object) :values '(boolean))
             (generate-kernel-function 'env/variable-name                    '(variable))
             (generate-kernel-function 'env/variable-value                   '(variable))
             (generate-kernel-function 'env/variable-set-value               '(new-value variable))

             (generate-kernel-function 'env/symbol-macro-p                   '(object) :values '(boolean))
             (generate-kernel-function 'env/symbol-macro-name                '(symbol-macro))
             (generate-kernel-function 'env/symbol-macro-expansion           '(symbol-macro))
             (generate-kernel-function 'env/symbol-macro-set-expansion       '(new-expansion symbol-macro))

             (generate-kernel-function 'env/functionp                        '(object) :values '(boolean))
             (generate-kernel-function 'env/function-name                    '(function))
             (generate-kernel-function 'env/function-lambda-list             '(function))
             (generate-kernel-function 'env/function-body                    '(function))
             (generate-kernel-function 'env/function-compiler-macro          '(function))
             (generate-kernel-function 'env/function-set-lambda-list         '(new-lambda-list     function))
             (generate-kernel-function 'env/function-set-body                '(new-body            function))
             (generate-kernel-function 'env/function-set-compiler-macro      '(new-compiler-macro  function))

             (generate-kernel-function 'env/macro-function-p                 '(object) :values '(boolean))
             (generate-kernel-function 'env/macro-name                       '(macro))
             (generate-kernel-function 'env/macro-lambda-list                '(macro))
             (generate-kernel-function 'env/macro-body                       '(macro))
             (generate-kernel-function 'env/macro-set-lambda-list            '(new-lambda-list     macro))
             (generate-kernel-function 'env/macro-set-body                   '(new-body            macro))




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


(defparameter *environment-slots*
  '(variable function symbol-macro macro protect block
    catch handler restart))

(defmacro extend-environment (slot objects env)
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

(defun generate-environment-functions ()
  `(progn
     ,@(mapcar
        (lambda (slot)
          (let ((parameter      (scat slot 's))
                (accessor-form  `(,(scat 'environment- slot 's) env))
                (name-key-form  `(function ,(scat 'env/ slot '-name))))
            `(progn

               (defun ,(scat 'extend-environment-with- slot 's) (,parameter  env)
                 (extend-environment ,slot ,parameter env))

               (defun ,(scat 'push- slot '-onto-environment) (,slot env)
                 ;; TODO: instanciated global environment
                 (unless env (setf env *global-environment*))
                 (push ,slot ,accessor-form)
                 env)

               (defun ,(scat 'pop- slot '-from-environment) (name env)
                 ;; TODO: instanciated global environment
                 (unless env (setf env *global-environment*))
                 (setf ,accessor-form (remove name ,accessor-form :key ,name-key-form :count 1))
                 env)

               (defun ,(scat 'env/find- slot) (name env)
                 ;; TODO: instanciated global environment
                 (unless env (setf env *global-environment*))
                 (find name ,accessor-form :key ,name-key-form))

               )))
        *environment-slots*)))

(generate-environment-functions)

;;;; THE END ;;;;
