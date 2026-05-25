;;;; -*- mode:lisp -*-
;;;;
;;;; bocl prelude — the canonical small-CL macros that the kernel can't
;;;; ship as primitives but which DEFMACRO + backquote can express.
;;;;
;;;; Loaded after the C kernel boots; defines what's needed by the rest of
;;;; the Lisp library and by user code that expects standard CL surface.

;; -------------------------------------------------------------------- ;;
;;  Conditional macros.                                                  ;;
;; -------------------------------------------------------------------- ;;

(defmacro when (test &body body)
  `(if ,test (progn ,@body)))

(defmacro unless (test &body body)
  `(if ,test nil (progn ,@body)))

(defmacro and (&rest forms)
  (if (null forms)
      t
      (if (null (cdr forms))
          (car forms)
          `(if ,(car forms) (and ,@(cdr forms)) nil))))

(defmacro or (&rest forms)
  (if (null forms)
      nil
      (if (null (cdr forms))
          (car forms)
          `(let ((or-tmp ,(car forms)))
             (if or-tmp or-tmp (or ,@(cdr forms)))))))

(defmacro cond (&rest clauses)
  (if (null clauses)
      nil
      (let ((c (car clauses)))
        (if (null (cdr c))
            ;; (cond (test)) — return test if true
            `(or ,(car c) (cond ,@(cdr clauses)))
            (if (eq (car c) t)
                `(progn ,@(cdr c))
                `(if ,(car c)
                     (progn ,@(cdr c))
                     (cond ,@(cdr clauses))))))))

;; -------------------------------------------------------------------- ;;
;;  SETF — generic place assignment.                                     ;;
;;                                                                       ;;
;;  Each place form's operator is looked up in *SETF-EXPANDERS* (a       ;;
;;  hash table). The expander is a function of (place-args... value)     ;;
;;  that returns the equivalent setter call. Symbols use SETQ directly.  ;;
;;                                                                       ;;
;;  This is the simple form — place subforms may be evaluated more than  ;;
;;  once by macros like INCF when the value expression refers to the     ;;
;;  same place. Full GET-SETF-EXPANSION (with temp vars and             ;;
;;  guaranteed single evaluation) comes later.                          ;;
;; -------------------------------------------------------------------- ;;

(defparameter *setf-expanders* (make-hash-table :test 'eq))

(defmacro define-setf-expander (operator args value-args &body body)
  ;; (define-setf-expander op (a b ...) (val) body...)
  ;;   registers a lambda (a b ... val) → setter-form.
  `(sethash ',operator *setf-expanders*
            (lambda ,(append args value-args) ,@body)))

(defmacro setf (&rest pairs)
  (cond ((null pairs) nil)
        ((null (cdr pairs))
         (error "SETF: odd number of arguments"))
        ((null (cddr pairs))
         (let ((place (first pairs))
               (value (second pairs)))
           (cond ((symbolp place)
                  `(setq ,place ,value))
                 ((consp place)
                  (let ((expander (gethash (car place) *setf-expanders*)))
                    (if expander
                        (apply expander (append (cdr place) (list value)))
                        (error "SETF: no expander for place operator"))))
                 (t (error "SETF: invalid place")))))
        (t
         `(progn (setf ,(first pairs) ,(second pairs))
                 (setf ,@(cddr pairs))))))

;; Built-in place expanders. The argument-order quirks of the underlying
;; kernel setters are absorbed here, so SETF callers see the standard
;; (setf (op ...) value) shape.
(define-setf-expander car            (cell)         (val) `(rplaca ,cell ,val))
(define-setf-expander cdr            (cell)         (val) `(rplacd ,cell ,val))
(define-setf-expander first          (cell)         (val) `(rplaca ,cell ,val))
(define-setf-expander rest           (cell)         (val) `(rplacd ,cell ,val))
(define-setf-expander gethash        (k h)          (val) `(sethash ,k ,h ,val))
(define-setf-expander symbol-value   (s)            (val) `(set-symbol-value ,val ,s))
(define-setf-expander symbol-function (s)           (val) `(set-symbol-function ,val ,s))
(define-setf-expander symbol-plist   (s)            (val) `(set-symbol-plist ,val ,s))
(define-setf-expander svref          (vec idx)      (val) `(svref-set ,vec ,idx ,val))
(define-setf-expander bit            (vec idx)      (val) `(bit-set ,vec ,idx ,val))
(define-setf-expander nth            (n lst)        (val) `(rplaca (nthcdr ,n ,lst) ,val))

;; -------------------------------------------------------------------- ;;
;;  Assignment macros — now SETF-based, so they work on generic places.  ;;
;; -------------------------------------------------------------------- ;;

(defmacro push (item place)
  `(setf ,place (cons ,item ,place)))

(defmacro pop (place)
  `(let ((pop-tmp (car ,place)))
     (setf ,place (cdr ,place))
     pop-tmp))

(defmacro incf (place &optional (delta 1))
  `(setf ,place (+ ,place ,delta)))

(defmacro decf (place &optional (delta 1))
  `(setf ,place (- ,place ,delta)))

;; -------------------------------------------------------------------- ;;
;;  Accessors: FIRST..TENTH, NTH, REST.                                  ;;
;; -------------------------------------------------------------------- ;;

(defun first   (l) (car l))
(defun second  (l) (car (cdr l)))
(defun third   (l) (car (cdr (cdr l))))
(defun fourth  (l) (car (cdr (cdr (cdr l)))))
(defun fifth   (l) (car (cdr (cdr (cdr (cdr l))))))
(defun sixth   (l) (car (cdr (cdr (cdr (cdr (cdr l)))))))
(defun seventh (l) (car (cdr (cdr (cdr (cdr (cdr (cdr l))))))))
(defun eighth  (l) (car (cdr (cdr (cdr (cdr (cdr (cdr (cdr l)))))))))
(defun ninth   (l) (car (cdr (cdr (cdr (cdr (cdr (cdr (cdr (cdr l))))))))))
(defun tenth   (l) (car (cdr (cdr (cdr (cdr (cdr (cdr (cdr (cdr (cdr l)))))))))))
(defun rest    (l) (cdr l))

(defun nth (n list)
  (if (= n 0)
      (car list)
      (nth (- n 1) (cdr list))))

(defun nthcdr (n list)
  (if (= n 0)
      list
      (nthcdr (- n 1) (cdr list))))

;; -------------------------------------------------------------------- ;;
;;  Predicates and small helpers.                                        ;;
;; -------------------------------------------------------------------- ;;

(defun null  (x) (if x nil t))
(defun not   (x) (if x nil t))
(defun atom  (x) (if (consp x) nil t))
(defun endp  (x) (if (consp x) nil t))   ;; same as null, on a proper list
(defun listp (x) (or (null x) (consp x)))

(defun zerop  (n) (= n 0))
(defun plusp  (n) (> n 0))
(defun minusp (n) (< n 0))
(defun oddp   (n) (= (mod n 2) 1))
(defun evenp  (n) (= (mod n 2) 0))

;; -------------------------------------------------------------------- ;;
;;  Iteration macros.                                                    ;;
;; -------------------------------------------------------------------- ;;

(defmacro dolist (head &body body)
  ;; (dolist (var list-form [result-form]) body...) — minimal form.
  (let ((var       (first head))
        (list-form (second head))
        (result    (third head)))
    `(let ((dolist-list ,list-form))
       (block nil
         (let ((,var nil))
           (tagbody
              dolist-loop
              (if (null dolist-list)
                  (go dolist-done))
              (setq ,var (car dolist-list))
              (setq dolist-list (cdr dolist-list))
              ,@body
              (go dolist-loop)
              dolist-done))
         ,result))))

(defmacro dotimes (head &body body)
  ;; (dotimes (var count-form [result-form]) body...)
  (let ((var         (first head))
        (count-form  (second head))
        (result      (third head)))
    `(let ((dotimes-count ,count-form))
       (block nil
         (let ((,var 0))
           (tagbody
              dotimes-loop
              (if (>= ,var dotimes-count)
                  (go dotimes-done))
              ,@body
              (setq ,var (+ ,var 1))
              (go dotimes-loop)
              dotimes-done))
         ,result))))

;; -------------------------------------------------------------------- ;;
;;  Control macros (slice 3): CASE / ECASE / TYPECASE / ETYPECASE,       ;;
;;  MULTIPLE-VALUE-BIND / -LIST, DO / DO*, IGNORE-ERRORS, ASSERT,        ;;
;;  CHECK-TYPE, RETURN, WITH-OPEN-FILE.                                  ;;
;; -------------------------------------------------------------------- ;;

(defmacro case (keyform &rest clauses)
  ;; (case kf (keys body...) ... [(otherwise|t body...)])
  (let ((tmp (gensym "CASE-")))
    `(let ((,tmp ,keyform))
       (cond
         ,@(mapcar
            (lambda (clause)
              (let ((keys (car clause))
                    (body (cdr clause)))
                (cond ((or (eq keys 'otherwise) (eq keys 't))
                       `(t ,@body))
                      ((listp keys)
                       `((or ,@(mapcar (lambda (k) `(eql ,tmp ',k)) keys))
                         ,@body))
                      (t
                       `((eql ,tmp ',keys) ,@body)))))
            clauses)))))

(defmacro ecase (keyform &rest clauses)
  ;; Like CASE but signals if no clause matches; no OTHERWISE/T allowed.
  (let ((tmp (gensym "ECASE-")))
    `(let ((,tmp ,keyform))
       (cond
         ,@(mapcar
            (lambda (clause)
              (let ((keys (car clause))
                    (body (cdr clause)))
                (if (listp keys)
                    `((or ,@(mapcar (lambda (k) `(eql ,tmp ',k)) keys))
                      ,@body)
                    `((eql ,tmp ',keys) ,@body))))
            clauses)
         (t (error "ECASE: no matching clause"))))))

(defmacro typecase (keyform &rest clauses)
  (let ((tmp (gensym "TYPECASE-")))
    `(let ((,tmp ,keyform))
       (cond
         ,@(mapcar
            (lambda (clause)
              (let ((type (car clause))
                    (body (cdr clause)))
                (cond ((or (eq type 'otherwise) (eq type 't))
                       `(t ,@body))
                      (t
                       `((typep ,tmp ',type) ,@body)))))
            clauses)))))

(defmacro etypecase (keyform &rest clauses)
  (let ((tmp (gensym "ETYPECASE-")))
    `(let ((,tmp ,keyform))
       (cond
         ,@(mapcar
            (lambda (clause)
              `((typep ,tmp ',(car clause)) ,@(cdr clause)))
            clauses)
         (t (error "ETYPECASE: no matching type clause"))))))

;; -------------------------------------------------------------------- ;;
;;  Multiple values.                                                     ;;
;; -------------------------------------------------------------------- ;;

(defmacro multiple-value-bind (vars value-form &body body)
  ;; (multiple-value-bind (a b c) form body...) — bind a, b, c to the
  ;; first three values of `form`. Missing values bind to NIL via
  ;; &optional defaulting.
  `(multiple-value-call (lambda (&optional ,@vars) ,@body) ,value-form))

(defmacro multiple-value-list (form)
  `(multiple-value-call #'list ,form))

(defmacro multiple-value-setq (vars form)
  ;; (multiple-value-setq (a b c) form) — set a, b, c. Returns the
  ;; primary value of form.
  (let ((tmp-vars (mapcar (lambda (v) v (gensym "MVS-")) vars))
        (primary  (gensym "MVS-PRIMARY-")))
    `(let ((,primary nil))
       (multiple-value-bind ,tmp-vars ,form
         ,@(mapcar (lambda (v tv) `(setq ,v ,tv)) vars tmp-vars)
         (setq ,primary ,(car tmp-vars)))
       ,primary)))

(defmacro nth-value (n form)
  ;; Return the nth (0-based) value of FORM.
  `(nth ,n (multiple-value-list ,form)))

;; -------------------------------------------------------------------- ;;
;;  Iteration: DO / DO*. Minimal — no DECLARE handling.                  ;;
;; -------------------------------------------------------------------- ;;

(defmacro do (varlist endlist &body body)
  ;; (DO ((var init [step]) ...) (test result...) body...)
  (let ((loop (gensym "DO-LOOP-"))
        (done (gensym "DO-DONE-")))
    `(block nil
       (let ,(mapcar (lambda (v)
                       (if (consp v) (list (car v) (cadr v)) (list v nil)))
                     varlist)
         (tagbody
            ,loop
            (if ,(car endlist) (go ,done))
            ,@body
            ,@(mapcar
               (lambda (v)
                 (if (and (consp v) (consp (cddr v)))
                     `(setq ,(car v) ,(caddr v))
                     '(progn)))
               varlist)
            (go ,loop)
            ,done)
         ,@(cdr endlist)))))

(defmacro do* (varlist endlist &body body)
  ;; Same shape as DO but bindings are sequential and steps fire
  ;; sequentially too.
  (let ((loop (gensym "DO*-LOOP-"))
        (done (gensym "DO*-DONE-")))
    `(block nil
       (let* ,(mapcar (lambda (v)
                        (if (consp v) (list (car v) (cadr v)) (list v nil)))
                      varlist)
         (tagbody
            ,loop
            (if ,(car endlist) (go ,done))
            ,@body
            ,@(mapcar
               (lambda (v)
                 (if (and (consp v) (consp (cddr v)))
                     `(setq ,(car v) ,(caddr v))
                     '(progn)))
               varlist)
            (go ,loop)
            ,done)
         ,@(cdr endlist)))))

;; -------------------------------------------------------------------- ;;
;;  Error / assertion macros.                                            ;;
;; -------------------------------------------------------------------- ;;

(defmacro ignore-errors (&body body)
  ;; (handler-case (progn body) (error (c) (values nil c)))
  ;; Without VALUES syntax for multiple values returned, fall back to
  ;; (values nil c) which the kernel's VALUES built-in handles.
  `(handler-case (progn ,@body)
     (error (ignored-error-cond)
       (values nil ignored-error-cond))))

(defmacro assert (test-form &rest ignored)
  ;; (assert test [(place...) datum args...]) — minimal version,
  ;; just checks the test and errors with a default message. The
  ;; CL spec's optional (places-to-fix datum args...) tail is
  ;; accepted but ignored.
  ignored
  `(unless ,test-form
     (error "Assertion failed")))

(defmacro check-type (place type)
  ;; (check-type place type [type-string])
  `(unless (typep ,place ',type)
     (error "Value does not match expected type")))

(defmacro return (&optional value)
  `(return-from nil ,value))

;; -------------------------------------------------------------------- ;;
;;  File-stream convenience.                                             ;;
;; -------------------------------------------------------------------- ;;

(defmacro with-open-file (head &body body)
  ;; (WITH-OPEN-FILE (var filespec &rest open-args) body...)
  (let ((var       (first head))
        (filespec  (second head))
        (open-args (cddr head))
        (abnormal  (gensym "WOF-ABORT-")))
    `(let ((,var (open ,filespec ,@open-args))
           (,abnormal t))
       (unwind-protect
            (multiple-value-prog1
                (progn ,@body)
              (setq ,abnormal nil))
         (when ,var
           ;; CL: pass :abort t when body aborted.
           (close ,var))))))

;;;; THE END
