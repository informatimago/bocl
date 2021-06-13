(in-package "COM.INFORMATIMAGO.COMMON-LISP.LISP.MINIMAL-COMPILER")

(cl:defmacro defmacro (&environment environment name lambda-list &body decl-doc-body)
  (let* ((ll            (parse-lambda-list lambda-list :macro))
         (whole         (if (lambda-list-whole-parameter-p ll)
                            (parameter-specifier (lambda-list-whole-parameter ll))
                            (gensym "whole")))
         (env-parameter (if (lambda-list-environment-parameter-p ll)
                            (parameter-specifier (lambda-list-environment-parameter ll))
                            (gensym "environment"))))
    (remove-whole-parameter ll)
    (remove-environment-parameter ll)
    (multiple-value-bind (docstrings declarations body) (parse-body :lambda decl-doc-body)
      `(progn
         (setf (env/macro-function ',name)
               (coerce-to-function
                ,(minimal-compile/lambda-expression
                  environment
                  `(lambda (,whole ,env-parameter)
                     (declare (ignorable ,env-parameter))
                     (block ,name
                       (destructuring-bind ,lambda-list ,whole
                         (locally ,@declarations
                           ,@body))))))
               ,@(when docstrings
                   `((documentation ',name 'function) ',(first docstrings))))
         ',name))))


(cl:defmacro defun (&environment environment name lambda-list &body decl-doc-body)
  ;; TODO: instanciated global environment
  (unless environment (setf environment *global-environment*))
  (setf environment *global-environment*)
  (multiple-value-bind (docstrings declarations body) (parse-body :lambda decl-doc-body)
    `(progn
       (setf (symbol-function ',name)
             (coerce-to-function
              ,(minimal-compile/lambda-expression
                environment
                `(lambda ,lambda-list
                   ,@declarations
                   (block ,name
                     ,@body))))
             ,@(when docstrings
                 `((documentation ',name 'function) ',(first docstrings))))
       ',name)))

;;; TODO defgeneric, defmethod
