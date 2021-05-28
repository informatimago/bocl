;; Non-local exits are only initiated from error handlers, or from the
;; debugger in INVOKE-DEBUGGER.
;;
;; Catching Non-local exits is the job of handler-bind  handler-case,
;; restart-case, with-simple-restart, block, catch, tagbody, etc.



(defvar *break-on-signals* nil)

(defvar *handlers* '()
  ;; Note: we expect dynamic binding to deal with it.
  "A list of a-list mapping condition class names to handler function.")


(defvar *debugger-function* nil)
(defvar *debugger-hook* nil)
(defun invoke-debugger (condition)
  (when *debugger-hook*
    (let ((hook *debugger-hook*)
          (*debugger-hook* nil))
      (funcall hook condition)))
  (if *debugger-function*
      (funcall *debugger-function* condition)
      (progn
        (format *error-output* "~%FATAL ERROR: no debugger in ~A~%"
                (lisp-implementation-type))
        (fatal condition))))


(defun instancep (object class-designator)
  ;; We don't deal with types in the kernel; but we must deal with
  ;; class hierarchy (with possibly multiple-inheritance).  This
  ;; hierarchy must be declared in the C kernel.
  (typep objet class-designator))

(defun fatal (condition)
  ;; Note: instead of format, we can use printf, if we restrict
  ;; ourselves to simple format specifier that can easily be mapped to
  ;; printf format specifiers.
  (format *error-output* "~%FATAL ERROR: no handler for ~?~%"
          (simple-condition-format-control condition)
          (simple-condition-format-arguments condition))
  (finish-output *error-output*)
  (exit 1))

(defun signal (datum &rest arguments)
  (let ((condition
          (cond
            ((instancep datum 'condition)
             datum)
            ((stringp datum)
             (make-condition 'simple-error
                             :format-control datum
                             :format-arguments arguments)
             (t
              (apply (function make-condition) datum arguments))))))
    (when *break-on-signals*
      (invoke-debugger condition))
    (do ((handlers *handlers* (cdr handlers))
         (handler  (assoc-if (lambda (entry)
                               (instance-of condition (car entry)))
                             (car handlers))
                   (assoc-if (lambda (entry)
                               (instance-of condition (car entry)))
                             (car handlers))))
        ((or (null handlers)
             handler)
         (if handler
             (funcall handler condition)
             (fatal condition))))))

(defun error (datum &rest arguments)
  (let ((condition
          (if (stringp datum)
              (make-condition 'simple-error
                              :format-control datum
                              :format-arguments arguments)
              (apply (function make-condition) datum arguments))))
    (signal condition)
    (invoke-debugger condition)))



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defun gc (&optional command)
  (check-type command (member nil :on :off)))



(declaim

 (ftype (function (float) bool)
        floatp)

 (ftype (function (float) float)
        sin/f cos/f tan/f asin/f acos/f
        sinh/f cosh/f tanh/f asinh/f acosh/f atanh/f
        exp/f)

 (ftype (function (float &optional float) float)
        atan/f log/f)

 (ftype (function (float float) float)
        +/f -/f */f //f mod/f rem/f sqrt/f
        expt/f)

 (ftype (function (float float) bool)
        =/f /=/f </f >/f <=/ >=/f)

 (ftype (function (float &optional float) (values integer float))
        floor/f  ceiling/f truncate/f round/f)

 (ftype (function (float &optional float) (values float float))
        ffloor/f fceiling/f ftruncate/f fround/f)

 (ftype (function (float) (values float integer float))
        decode-float)
 (ftype (function (float integer) float)
        scale-float)
 (ftype (function (float) integer)
        float-radix)
 (ftype (function (float &optional float) float)
        float-sign)
 (ftype (function (float) integer)
        float-digits)
 (ftype (function (float) integer)
        float-precision)
 (ftype (function (float) (values float integer integer))
        integer-decode-float))

(declaim
 (ftype (function (integer integer) byte-specifier)
        byte)
 (ftype (function (byte-specifier) integer)
        byte-size byte-position))

(declaim

 (ftype (function (integer) bool)
        integerp)

 (ftype (function (integer) integer)
        exp/i integer-length logcount)

 (ftype (function (integer byte-specifier integer) integer)
        dpb deposit-field)

 (ftype (function (byte-specifier integer) integer)
        ldb mask-field)

 (ftype (function (integer integer) integer)
        +/i -/i */i //i mod/i rem/i sqrt/i
        expt/i  gcd/i lcm/i ash)

 (ftype (function (integer integer) bool)
        =/i /=/i </i >/i <=/ >=/i
        logbitp logtest)

 (ftype (function (integer) integer)
        isqrt/i)

 (ftype (function (integer &optional integer) (values integer integer))
        floor/i  ceiling/i truncate/i round/i)

 (ftype (function (integer &optional integer) (values integer integer))
        ffloor/i fceiling/i ftruncate/i fround/i)

 (ftype (function (string integer integer integer bool) (values integer integer))
        parse-integer/i))
