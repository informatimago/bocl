(cl:in-package "COM.INFORMATIMAGO.BOCL.KERNEL")

(cl:defstruct (closure
               (:predicate closurep)
               (:constructor make-closure))
  function
  environment)

(cl:defstruct (function
               (:predicate functionp)
               (:constructor make-function))
  name
  lambda-list
  lambda-expression
  (ioptional   '())
  (ikey        '())
  (kkey        '())
  (iaux        '())
  (nmandatory  0)
  (noptional   0)
  (nrest       0)
  (nkey        0)
  (naux        0))

(cl:defstruct (frame
                (:predicate framep))
  next
  target
  bindings
  results
  non-local-exit)

(cl:defstruct (protect-frame
                (:include frame)))

(cl:defstruct (block-frame
                (:include frame))
  name)

(cl:defstruct (catch-frame
                (:include frame))
  object)

(cl:defstruct (tagbody-frame
                (:include frame))
  tags
  target-tag)

(cl:defstruct (handler-frame
                (:include frame))
  conditions)

(cl:defstruct (restart-frame
                (:include frame))
  restart
  arguments)


(cl:in-package "COMMON-LISP-USER")


(defun kernel:bindings-nbindings                      (bindings)             (aref bindings 0))
(defun kernel:bindings-nvars                          (bindings)             (aref bindings 1))
(defun kernel:bindings-nfuns                          (bindings)             (aref bindings 2))
(defun (setf kernel:bindings-nbindings)     (new-value bindings)       (setf (aref bindings 0) new-value))
(defun (setf kernel:bindings-nvars)         (new-value bindings)       (setf (aref bindings 1) new-value))
(defun (setf kernel:bindings-nfuns)         (new-value bindings)       (setf (aref bindings 2) new-value))
(defun kernel:bindings-aref                           (bindings index)       (aref bindings (+ 3                                     index)))
(defun kernel:bindings-vref                           (bindings index)       (aref bindings (+ 3 (aref bindings 0)                   index)))
(defun kernel:bindings-fref                           (bindings index)       (aref bindings (+ 3 (aref bindings 0) (aref bindings 1) index)))
(defun (setf kernel:bindings-aref)    (new-bindings    bindings index) (setf (aref bindings (+ 3                                     index)) new-bindings))
(defun (setf kernel:bindings-vref)    (new-value    bindings index) (setf (aref bindings (+ 3 (aref bindings 0)                      index)) new-value))
(defun (setf kernel:bindings-fref)    (new-function bindings index) (setf (aref bindings (+ 3 (aref bindings 0) (aref bindings 1)    index)) new-function))

(defun kernel:make-bindings (next-bindings nvars nfuns)
  (let ((nbindings (if next-bindings (kernel:bindings-nbindings next-bindings) 0)))
    (let ((bindings (make-array (+ 3 nbindings 1 nvars nfuns))))
      (dotimes (i nbindings)
        (setf (kernel:bindings-aref bindings i) (kernel:bindings-aref next-bindings i)))
      (setf (kernel:bindings-aref bindings nbindings) next-bindings
            (kernel:bindings-nbindings bindings) (+ nbindings 1)
            (kernel:bindings-nvars bindings) nvars
            (kernel:bindings-nfuns bindings) nfuns)
      bindings)))

(defparameter kernel:*frame* nil)

;;; ===============================================================================
;;; Function Call
;;;

(defun kernel::extract-keyword-argument (arguments key default function all-arguments)
  (let ((result default)
        (foundp nil)
        (current arguments))
    (tagbody
     :loop
       (cond
         ((null current))
         ((atom current)       (error "In call to ~S, argument list is a dotted-list ~S"
                                      (or (kernel:function-name function) "anonymous function")
                                      all-arguments))
         ((null (cdr current)) (error "In call to ~S, odd number of arguments in ~S"
                                      (or (kernel:function-name function) "anonymous function")
                                      all-arguments))
         ((eql key (car current))
          (setq foundp t
                result  (cadr current)
                current (cddr current)))
         (t
          (go :loop))))
    (values result foundp)))

(defun kernel:call-function (function arguments)
  (let ((nmandatory     (kernel:function-nmandatory function))
        (noptional      (kernel:function-noptional  function))
        (nrest          (kernel:function-nrest      function))
        (nkey           (kernel:function-nkey       function))
        (naux           (kernel:function-naux       function))
        (current        arguments))
    (let ((bindings (kernel:make-bindings (and kernel:*frame* (kernel:frame-bindings kernel:*frame*))
                                          (+ nmandatory noptional nrest nkey nkey naux) 0)))
      (dotimes (i nmandatory)
        (if (null current)
            (error "Missing arguments: expected ~A got ~A~%  calling ~:[an anonymous function~;~:*~A~]~%  with arguments: ~S"
                   nmandatory i (kernel:function-name function) arguments))
        (setf (kernel:bindings-vref bindings i) (pop current)))
      (when (plusp noptional)
        (let ((ioptional (kernel:function-ioptional function)))
          (dotimes (i noptional)
            (setf (kernel:bindings-vref bindings (+ i nmandatory))
                  (if current
                      (progn (pop ioptional)
                             (pop current))
                      (kernel:evale (pop ioptional) bindings))))))
      (if (plusp nrest)
          (setf (kernel:bindings-vref bindings (+ nmandatory noptional))
                current))
      (when (plusp nkey)
        (let ((ikey  (kernel:function-ikey function))
              (kkey  (kernel:function-kkey function))
              (base  (+ nmandatory noptional nrest)))
          (dotimes (i nkey)
            (multiple-value-bind (value foundp) (kernel::extract-keyword-argument current (pop kkey) (pop ikey) function arguments)
              (setf (kernel:bindings-vref bindings (+    (* i 2) base))
                    (if foundp value (kernel:evale value bindings))
                    (kernel:bindings-vref bindings (+  1 (* i 2) base))
                    foundp)))))
      (when (plusp naux)
        (let ((iaux (kernel:function-iaux function))
              (base  (+ nmandatory noptional nrest (* 2 nkey))))
          (dotimes (i naux)
            (setf (kernel:bindings-vref bindings (+ i base))
                  (kernel:evale (pop iaux) bindings)))))
      (kernel:evale (cddr (kernel:function-lambda-expression function)) bindings))))

(defun kernel:apply (function &rest arguments)
  (if (cdr arguments)
      (let ((all-arguments (list '())))
        (let ((tail all-arguments))
          (tagbody
           :loop 
             (if (cdr arguments)
                 (progn
                   (setf (cdr tail) (cons (car arguments) nil)
                         tail (cdr tail)
                         arguments (cdr arguments))
                   (go :loop))
                 (setf (cdr tail) (car arguments)))))
        (kernel:call-function function (cdr all-arguments)))
      (kernel:call-function function arguments)))


;;; ===============================================================================
;;; Variables
;;;

(defun kernel:variable-value (vindex findex)
  (if findex
      (aref (aref (kernel:frame-bindings kernel:*frame*) findex) vindex)
      (aref (kernel:frame-bindings kernel:*frame*) vindex)))

(defun kernel:set-variable-value (new-value vindex findex)
  (if findex
      (setf (aref (aref (kernel:frame-bindings kernel:*frame*) findex) vindex) new-value)
      (setf (aref (kernel:frame-bindings kernel:*frame*) vindex) new-value)))

(defun kernel:symbol-make-unbound              (symbol) (makunbound symbol))
(defun kernel:symbol-boundp                    (symbol) (boundp symbol))
(defun kernel:symbol-value                     (symbol) (symbol-value symbol))
(defun kernel:set-symbol-value       (new-value symbol) (setf (symbol-value symbol) new-value))
(defun kernel:symbol-make-funbound             (symbol) (fmakunbound symbol))
(defun kernel:symbol-fboundp                   (symbol) (fboundp symbol))
(defun kernel:symbol-function                  (symbol) (symbol-function symbol))
(defun kernel:set-symbol-function (new-function symbol) (setf (symbol-function symbol) new-function))


;;;; THE END ;;;;
