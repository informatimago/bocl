(cl:in-package "COM.INFORMATIMAGO.COMMON-LISP.LISP.MINIMAL-COMPILER")


(defun foo ()
  (let* ((env          (env:make-environment :next (env:global-environment)))
         (lambda-list '(a b &optional p (q 1 qp) &key k (l 2 lp)))
         (ll           (parse-lambda-list lambda-list :ordinary))
         (specials    '()))
    (loop
      ;; Open a new environment frame for the lambda-list:
      :with llenv := (env:make-environment
                      :next env
                      :frame-index (let ((cfi (env:environment-current-frame-index env)))
                                     (if cfi (1+ cfi) 0)))
      :for parameter :in (lambda-list-parameters ll)
      :do (if (parameter-initform-p parameter)
              (cl:setf (parameter-initform parameter)
                       (minimal-compile llenv (parameter-initform parameter))))
          (if (typep parameter 'optional-parameter)
              (if (parameter-indicator-p parameter)
                  (values)
                  (cl:setf (parameter-indicator parameter) (gensym))))
          ;; Add the new parameter variable bindings to the environment:
          (add-local-variable name specials llenv)
      :finally (return (values ll llenv)))))


(foo)

(nil nil nil qp nil lp)
nil 
(a)
(b)
(p #:g26121)
(q qp)
(k #:g26122)
(l lp)#<ordinary-lambda-list #x302002CDE2DD>
#S(com.informatimago.common-lisp.lisp.environment:environment :variables nil :functions nil :compiler-macros nil :declarations nil :blocks nil :tags nil :frame-index nil :next #S(com.informatimago.common-lisp.lisp.environment:environment :variables nil :functions nil :compiler-macros nil :declarations nil :blocks nil :tags nil :frame-index nil :next nil))
