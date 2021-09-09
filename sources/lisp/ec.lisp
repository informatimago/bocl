
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
           (error "Compiler macros can be stored only in the global environment.")))))
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
  (if (null environment)
      (get symbol 'kernel:$symbol-macro-expansion)
      (let ((sm (env:find-variable symbol environment)))
        (when (env:symbol-macro-p sm)
          (env:symbol-macro-expansion sm)))))

(defun (cl:setf symbol-macro-expansion) (new-expansion symbol &optional environment)
  (if (null environment)
      (cl:setf (get symbol 'kernel:$symbol-macro-expansion) new-expansion)
      (let ((sm (env:find-variable symbol environment)))
        (cond
          ((null sm)
           (env:push-variable-onto-environment
            (if (eql environment (env:global-environment))
                (env:global-symbol-macro symbol new-expansion)
                (env:local-symbol-macro  symbol new-expansion))
            environment))
          ((env:symbol-macro-p sm)
           (cl:setf (env:symbol-macro-expansion sm) new-expansion))
          (t (error "There's already a ~A named ~S" (type-of sm) symbol)))))
  new-expansion)

;;;
;;; Normal macros
;;;

(defun macro-function (symbol &optional environment)
  (if (null environment)
      (cl:macro-function symbol nil)
      (let ((mac (env:find-function symbol environment)))
        (if (env:macrop mac)
            (env:macro-function mac)
            nil))))

(defun (cl:setf macro-function) (new-function symbol &optional environment)
  (if (null environment)
      (cl:setf (cl:macro-function symbol nil) new-function)
      (let ((mac (env:find-function symbol environment)))
        (cond
          ((null mac)
           (env:push-function-onto-environment
            (if (eql environment (env:global-environment))
                (env:make-global-macro :name symbol :function new-function)
                (env:make-local-macro  :name symbol :function new-function))
            environment))
          ((env:macrop mac)
           (cl:setf (env:macro-function mac) new-function))
          (t (error "There's already a ~A named ~S" (type-of mac) symbol)))))
  new-function)


