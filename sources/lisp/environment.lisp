;;;; -*- mode:lisp;coding:iso-8859-1 -*-

(defpackage "COM.INFORMATIMAGO.BOCL.ENVIRONMENT"
  (:nicknames "ENV")
  (:use "COMMON-LISP")
  (:use "COM.INFORMATIMAGO.COMMON-LISP.CESARUM.SYMBOL")
  (:shadow "FUNCTION" "FUNCTIONP"
           "SPECIAL-OPERATOR-P"
           "MACRO-FUNCTION" "FIND-RESTART"
           "BLOCK")
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

   "EXTEND-ENVIRONMENT-WITH-VARIABLES"    "PUSH-VARIABLE-ONTO-ENVIRONMENT"    "POP-VARIABLE-FROM-ENVIRONMENT"    "FIND-VARIABLE"
   "EXTEND-ENVIRONMENT-WITH-FUNCTIONS"    "PUSH-FUNCTION-ONTO-ENVIRONMENT"    "POP-FUNCTION-FROM-ENVIRONMENT"    "FIND-FUNCTION"
   "EXTEND-ENVIRONMENT-WITH-DECLARATIONS" "PUSH-DECLARATION-ONTO-ENVIRONMENT" "POP-DECLARATION-FROM-ENVIRONMENT" "FIND-DECLARATION"
   "EXTEND-ENVIRONMENT-WITH-BLOCKS"       "PUSH-BLOCK-ONTO-ENVIRONMENT"       "POP-BLOCK-FROM-ENVIRONMENT"       "FIND-BLOCK"
   "EXTEND-ENVIRONMENT-WITH-TAGS"         "PUSH-TAG-ONTO-ENVIRONMENT"         "POP-TAG-FROM-ENVIRONMENT"         "FIND-TAG"

   "VARIABLE" "VARIABLEP" "VARIABLE-NAME"

   "SYMBOL-MACRO" "SYMBOL-MACRO-P" "SYMBOL-MACRO-EXPANSION"

   "LOCAL-SYMBOL-MACRO" "MAKE-LOCAL-SYMBOL-MACRO"
   "LOCAL-SYMBOL-MACRO-P" "COPY-LOCAL-SYMBOL-MACRO"

   "GLOBAL-SYMBOL-MACRO" "MAKE-GLOBAL-SYMBOL-MACRO"
   "GLOBAL-SYMBOL-MACRO-P" "COPY-GLOBAL-SYMBOL-MACRO"

   "AUTHENTIC-VARIABLE" "AUTHENTIC-VARIABLE-P" "VARIABLE-VALUE"

   "LEXICAL-VARIABLE" "MAKE-LEXICAL-VARIABLE" "LEXICAL-VARIABLE-P"
   "COPY-LEXICAL-VARIABLE"

   "LOCAL-SPECIAL-VARIABLE" "MAKE-LOCAL-SPECIAL-VARIABLE"
   "LOCAL-SPECIAL-VARIABLE-P" "COPY-LOCAL-SPECIAL-VARIABLE"

   "GLOBAL-SPECIAL-VARIABLE" "MAKE-GLOBAL-SPECIAL-VARIABLE"
   "GLOBAL-SPECIAL-VARIABLE-P" "COPY-GLOBAL-SPECIAL-VARIABLE"

   "CONSTANT-VARIABLE" "MAKE-CONSTANT-VARIABLE" "CONSTANT-VARIABLE-P"
   "COPY-CONSTANT-VARIABLE"

   "FUNCTION" "FUNCTION-P" "FUNCTION-NAME" "FUNCTION-LAMBDA-LIST"

   "AUTHENTIC-FUNCTION" "AUTHENTIC-FUNCTION-P"
   "AUTHENTIC-FUNCTION-BODY" "AUTHENTIC-FUNCTION-COMPILER-MACRO"

   "LOCAL-FUNCTION" "MAKE-LOCAL-FUNCTION" "LOCAL-FUNCTION-P"
   "COPY-LOCAL-FUNCTION" "LOCAL-FUNCTION-ENVIRONMENT"

   "GLOBAL-FUNCTION" "MAKE-GLOBAL-FUNCTION" "GLOBAL-FUNCTION-P"
   "COPY-GLOBAL-FUNCTION"

   "MACRO-FUNCTION" "MACRO-FUNCTION-P" "MACRO-FUNCTION-BODY"

   "LOCAL-MACRO" "MAKE-LOCAL-MACRO" "LOCAL-MACRO-P" "COPY-LOCAL-MACRO"
   "LOCAL-MACRO-ENVIRONMENT"

   "GLOBAL-MACRO" "MAKE-GLOBAL-MACRO" "GLOBAL-MACRO-P"
   "COPY-GLOBAL-MACRO"

   "SPECIAL-OPERATOR" "MAKE-SPECIAL-OPERATOR" "SPECIAL-OPERATOR-P"
   "COPY-SPECIAL-OPERATOR"

   "DECLARATION" "MAKE-DECLARATION" "DECLARATION-P" "COPY-DECLARATION"
   "DECLARATION-NAME"

   "BLOCK" "MAKE-BLOCK" "BLOCK-P" "COPY-BLOCK" "BLOCK-NAME"

   "TAG" "MAKE-TAG" "TAG-P" "COPY-TAG" "TAG-NAME" "TAG-INDEX"

   ))
(in-package "COM.INFORMATIMAGO.BOCL.ENVIRONMENT")


#|

# -*- mode:org -*-

** Environment Kinds

There are two kinds of environments:

- compilation-time (minimal-compilation) ie. macroexpansion-time
  &environment environments.  The compilation-time environments are
  used while analysing (macroexpanding) the source code, to record the
  lexical objects used by inner macros and minimal-compilation.

- run-time environments.  The run-time environments are parts of the
  =Frame= that is stored on the heap, to be referenced by closure, and
  that contain the local lexical variable and function bindings.


This package implements compilation-time environments, and produces
from them the information required to generate the run-time
environment meta-information.  The =Frames= are allocated by the
interpreter =KERNEL:EVAL= function for the special-operators requiring
them.

The minimal-compilation process should store with the generated
special-operator forms, the run-time environment meta-information.

Operators openning a new =Frame=:

| Operator       | CT/RT             | Scope           | Binding    | Exit                       |
|----------------+-------------------+-----------------+------------+----------------------------|
| UNWIND-PROTECT | Frame             | dynamic         | cleanup    | Execute cleanup            |
| HANDLER        | Frame             | dynamic         | handlers   | Remove handlers            |
| RESTART        | Frame             | dynamic         | restarts   | Remove restarts            |
| CATCH          | Frame             | dynamic         | object     | Remove catch object        |
| BLOCK          | CT, Frame         | lexical         | blocks     | Remove scope (blocks)      |
| TAGBODY        | CT, Frame         | lexical         | tags       | Remove scope (tags)        |
| PROGV          | CT, Frame         | dynamic         | variable   | Remove scope (variables)   |
| LET            | CT, RT, Frame(*)  | lexical/dynamic | variables  | Remove scopes (variables)  |
| LET*           | CT, RT, Frame(*)¥ | lexical/dynamic | variables  | Remove scopes (variables)  |
| FLET           | CT, RT            | lexical         | functions  | Remove scope (functions)   |
| LABELS         | CT, RT            | lexical         | functions  | Remove scope (functions)   |
| LAMBDA         | CT, RT            | lexical/dynamic | parameters | Remove scopes (parameters) |
|----------------+-------------------+-----------------+------------+----------------------------|

(*): only when there's a dynamic binding.
¥: normally LET* is minimal-compiled into embedded LETs.

UNWIND-PROTECT, HANDLER-CASE, HANDLER-BIND, RESTART-CASE,
RESTART-BIND, and CATCH don't need to be recorded in the
compilation-time environment, since they're purely dynamic constructs,
without any lexical dependency.

#+BEGIN_EXAMPLE

(defun f () (declare (special goo)) goo)

(let ((goo 42))
  (list
   goo
   (let ((goo 33))
     (declare (special goo))color-picker--previous-channel
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

** Declarations

Declarations to process:
- DECLARATION
  (declaim (declaration foo)) --> push foo on environment-lexical-declarations of the (global-environment)
  (declare (declaration foo)) --> push foo on environment-lexical-declarations
- NOTINLINE (but then, our interprete doesn't inline anything, so we're good)
- SPECIAL

** Global Environment

The global environment is reifed as an =ENVIRONMENT= object.

The defining standard macros registers the definitions in this global
environment object.  (When not running in bocl, the host environment
is not used).

** Special Operator FUNCTION

`(FUNCTION foo)`
`(FUNCTION (SETF foo))`

Find function `foo` or `(SETF foo)` in the environment: global or lexical.
Return the function closure.

`(FUNCTION (LAMBDA (...) ...))`

Create an interpreted anonymous function with the lambda expression.
Find the free variables and (local) free functions.
Create a closure with the function and its free bindings.
Signal an error if the closure is not complete (if no binding is found).

** Special Operator TAGBODY

Interpretation of a tagbody consists in interpreting sequentially the
forms in the body.  GO may jump to another section of the same
tagbody.  But in any case, unless GO is a tagbody-toplevel form, it
will involve a non-local exit, to find the tagbody concerned.

The minimal-compilation can compute a table mapping tags to rest of
tagbody forms. this table can be stored in a vector, so that the
search for the tags doesn't need to be performed by GO at run-time,
but is performed at compilation-time.

#+BEGIN_SRC lisp

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

#+END_SRC

Also, tagbody-toplevel GO forms can be interpreted directly by the
TAGBODY special operator, without going thru the Frame unwinding
mechanism.

#+BEGIN_SRC c
body=tags[go_tag_index];
#+END_SRC


|#



;;;
;;; Compilation-Time Environment
;;;

(defstruct environment
  variables                             ; includes symbol-macro
  functions                             ; includes macros
  declarations
  blocks
  tags                                  ; tagbody tags
  next)

(eval-when (:compile-toplevel :execute)
  (defparameter *environment-slots*
    '(variable
      function
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
               (setf ,accessor-form (remove name ,accessor-form :key ,name-key-form :count 1))
               env)
             (defun ,(scat 'find- slot) (name env)
               (unless env (setf env *global-environment*))
               (let ((object (if (symbolp name)
                                 (find name ,accessor-form :key ,name-key-form)
                                 (find name ,accessor-form))))
                 (if object
                     (values object env)
                     (when (environment-next env)
                       (pass-values (,(scat 'find- slot) name (environment-next env))))))))))
       *environment-slots*)))


(generate-environment-functions)

;; ((extend-environment-with-variables    push-variable-onto-environment    pop-variable-from-environment    find-variable)
;;  (extend-environment-with-functions    push-function-onto-environment    pop-function-from-environment    find-function)
;;  (extend-environment-with-declarations push-declaration-onto-environment pop-declaration-from-environment find-declaration)
;;  (extend-environment-with-blocks       push-block-onto-environment       pop-block-from-environment       find-block)
;;  (extend-environment-with-tags         push-tag-onto-environment         pop-tag-from-environment         find-tag))

(defvar *global-environment* (make-environment)
  "The global environment.")

(defvar *environment* *global-environment*
  "The current environment.")

(defun GLOBAL-ENVIRONMENT  () *global-environment*)
(defun CURRENT-ENVIRONMENT () *environment*)

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

(defstruct (local-symbol-macro
            (:include symbol-macro)
            (:constructor make-local-symbol-macro)
            (:constructor local-symbol-macro (name &optional expansion))))

(defstruct (global-symbol-macro
            (:include symbol-macro)
            (:constructor make-global-symbol-macro)
            (:constructor global-symbol-macro (name &optional expansion))))

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
            (:constructor nil)
            (:copier nil))
  name
  lambda-list)

(defstruct (authentic-function
            (:include function)
            (:constructor nil)
            (:copier nil))
  body
  compiler-macro)

(defstruct (local-function
            (:include authentic-function)
            (:constructor make-local-function)
            (:constructor local-function (name &optional lambda-list body compiler-macro environment)))
  environment)

(defstruct (global-function
            (:include authentic-function)
            (:constructor make-global-function)
            (:constructor global-function (name &optional lambda-list body compiler-macro))))

(defstruct (macro-function
            (:include function)
            (:constructor nil)
            (:copier nil))
  body)

(defstruct (local-macro
            (:include macro-function)
            (:constructor make-local-macro)
            (:constructor local-macro (name &optional lambda-list body environment)))
  environment)

(defstruct (global-macro
            (:include macro-function)
            (:constructor make-global-macro)
            (:constructor global-macro (name &optional lambda-list body))))

(defstruct (special-operator
            (:include function)
            (:constructor make-special-operator)
            (:constructor special-operator)))

(defstruct (declaration
            (:constructor make-declaration)
            (:constructor declaration (name &optional arguments)))
  name
  arguments)

(defstruct (block
            (:constructor make-block)
            (:constructor block (name)))
  name)

(defstruct (tag
            (:constructor make-tag)
            (:constructor tag (name)))
  name                                  ; symbol or integer
  index)



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
