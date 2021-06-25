(defpackage "COM.INFORMATIMAGO.BOCL.COMPILER"
  (:use "COMMON-LISP")
  (:shadowing-import-from "COMMON-LISP" "TAGBODY" "BLOCK")
  (:use "COM.INFORMATIMAGO.BOCL.ENVIRONMENT")
  (:shadowing-import-from
   "COM.INFORMATIMAGO.BOCL"
   "FUNCTION" "FUNCTIONP" "SPECIAL-OPERATOR-P"
   "COMPILER-MACRO-FUNCTION" "MACRO-FUNCTION" "FIND-RESTART"
   "LAMBDA" "FUNCALL"
   "DECLARE" "PROCLAIM" "DECLAIM"
   "DECLARATION" "IGNORE" "SPECIAL" "DYNAMIC-EXTENT" "INLINE" "TYPE"
   "FTYPE" "NOTINLINE" "IGNORABLE" "OPTIMIZE" "SAFETY")
  (:shadow "LOAD" "COMPILE" "COMPILE-FILE" "WITH-COMPILATION-UNIT")
  (:export "LOAD" "COMPILE" "COMPILE-FILE" "WITH-COMPILATION-UNIT")
  (:export
   "FUNCTION-NAME-P" "LAMBDA-EXPRESSION-P"
   "COERCE-TO-FUNCTION"  "COERCE-TO-CLOSURE"
   "GENERATE-DESTRUCTURING-LIST"))
(in-package "COM.INFORMATIMAGO.BOCL.COMPILER")

(defun function-name-p (object)
  (or (symbolp object)
      (and (consp object)
           (eql 'setf (car object))
           (cdr object)
           (symbolp (cadr object))
           (null (cddr object)))))

(defun lambda-expression-p (form)
  (and (consp form)
       (eql 'lambda (first form))
       (cdr form)
       (listp (cadr form))))


(defun undefined-function (name)
  (coerce-to-function
   `(lambda (&rest arguments)
      (block ,name
        (error "Undefined function ~S"
               (cons ',name arguments))))))

(defun make-function (name lambda-expression)
  (multiple-value-bind (nmandatory
                        noptional ioptional
                        nrest
                        nkey ikey kkey
                        naux iaux)
      (count-parameters (second lambda-expression))
    (kernel:make-function :name name
                          :nmandatory nmandatory
                          :noptional noptional
                          :ioptional ioptional
                          :nrest nrest
                          :nkey nkey :ikey ikey :kkey kkey
                          :naux naux :iaux iaux
                          :lambda-list (second lambda-expression)
                          :lambda-expression lambda-expression)))

(defun make-frame (environment)
  ;; TODO: process the environment.
  (kernel:make-frame :next kernel:*frame*
                     :bindings (kernel:make-bindings
                                (and kernel:*frame* (kernel:frame-bindings kernel:*frame*))
                                0 0)))

(defun coerce-to-function (lambda-expression &optional name)
  (cond
    ((function-name-p lambda-expression)

     ;; (eval `(function ,lambda-expression))
     ;; (compile nil lambda-expression)
     (let ((fun (env:find-function name (env:global-environment))))
       (if (env:authentic-function-p fun)
           (env:function-function fun)
           (undefined-function lambda-expression))))

    ((lambda-expression-p lambda-expression)

     ;; (eval lambda-expression)
     ;; (compile nil lambda-expression)
     (make-function name lambda-expression))

    (t
     (error "No such function ~S" lambda-expression))))

(defun coerce-to-closure (lambda-expression environment &optional name)
  (cond
    ((function-name-p lambda-expression)

     ;; (eval `(function ,lambda-expression))
     ;; (compile nil lambda-expression)
     (let ((fun (env:find-function lambda-expression environment)))
       (cond
         ((env:global-function-p fun)
          (env:function-function fun))
         ((env:local-function-p fun)
          (kernel:make-closure (env:function-function fun)
                               (env:local-function-environment fun)))
         (t
          (error "No such function ~S" lambda-expression)))))

    ((lambda-expression-p lambda-expression)
     ;; (eval lambda-expression)
     ;; (compile nil lambda-expression)
     (kernel:make-closure (make-function name lambda-expression)
                          (make-frame environment)))

    (t
     (error "No such function ~S" lambda-expression))))


(defun funcall (function &rest arguments)
  (let ((function (if (symbolp function)
                      (coerce-to-function function)
                      function)))
    (cond

      ((kernel:closurep function)
       (format t "~&:-- (~S)~%" (kernel:function-name function))
       (let ((kernel:*frame* (kernel:closure-environment function)))
         (kernel:call-function (kernel:closure-function function) arguments)))
      
      ((kernel:functionp function)
       (format t "~&:-- (~S)~%" (kernel:function-name function))
       (kernel:call-function function arguments))
      
      ((cl:functionp function)
       (cl:apply function arguments))
      
      (t
       (error "Calling not a function ~S" function)))))

#-(and)
(let ((function  (com.informatimago.bocl.kernel:make-function
                  nil
                  '(&rest com.informatimago.bocl.compiler::arguments)
                  '(lambda (&rest com.informatimago.bocl.compiler::arguments)
                    (block funcall
                      (error "Undefined function ~S" (cons 'funcall com.informatimago.bocl.compiler::arguments)))))))
  (list (symbolp function)
        (kernel:closurep function)
        (kernel:functionp function)
        (cl:functionp function)        
        ))




;;;; THE END ;;;;
