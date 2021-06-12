(in-package "COM.INFORMATIMAGO.COMMON-LISP.LISP.MINIMAL-COMPILER")

(cl:defun print-minimal-expansion (form)
  (let ((*print-circle* nil)
        (*PRINT-RIGHT-MARGIN* 80))
    (format t "~2%Source form:~%")
    (pprint form)
    (format t "~2%Minimal compilation:~%")
    (pprint (minimal-compile (make-environment) form))
    #+ccl   (format t "~2%CCL ccl::macroexpand-all:~%")
    #+ccl   (pprint (ccl::macroexpand-all form))
    #+clisp (format t "~2%CLISP ext:expand-form:~%")
    #+clisp (pprint (ext:expand-form form))
    (values)))

(defmacro dotimes ((var count-form &optional result-form) &body decl-tagbody)
  (destructuring-bind (declarations docstrings body) (parse-body :locally decl-tagbody)
    (declare (ignore docstrings))
    (let ((vcount (gensym "count"))
          (vvar   (gensym "var"))
          (vloop  (gensym "loop")))
      `(let ((,vcount ,count-form)
             (,vvar 0)
             (,var  0))
         ,@declarations
         (check-type ,vcount integer)
         (block nil
           (tagbody
              ,vloop
              (setf ,var ,vvar)
              (if (>= ,vvar ,vcount)
                  (return ,result-form))
              ,@body
              (incf ,vvar)
              (go ,vloop)))))))

(print-minimal-expansion '(dotimes (i 42) (print i)))
(print-minimal-expansion '(defun minimal-compile/function (env form)
                             (declare (ignore env))
                             (destructuring-bind (function fname) form
                               `(,function ,fname))))


(pprint (generate-environment-functions))


