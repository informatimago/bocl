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
(define-setf-expander second         (cell)         (val) `(rplaca (cdr ,cell) ,val))
(define-setf-expander third          (cell)         (val) `(rplaca (cddr ,cell) ,val))
(define-setf-expander fourth         (cell)         (val) `(rplaca (cdddr ,cell) ,val))
(define-setf-expander rest           (cell)         (val) `(rplacd ,cell ,val))
(define-setf-expander caar           (cell)         (val) `(rplaca (car ,cell) ,val))
(define-setf-expander cadr           (cell)         (val) `(rplaca (cdr ,cell) ,val))
(define-setf-expander cdar           (cell)         (val) `(rplacd (car ,cell) ,val))
(define-setf-expander cddr           (cell)         (val) `(rplacd (cdr ,cell) ,val))
(define-setf-expander caddr          (cell)         (val) `(rplaca (cddr ,cell) ,val))
(define-setf-expander cadddr         (cell)         (val) `(rplaca (cdddr ,cell) ,val))
(define-setf-expander gethash        (k h)          (val) `(sethash ,k ,h ,val))
(define-setf-expander symbol-value   (s)            (val) `(set-symbol-value ,val ,s))
(define-setf-expander symbol-function (s)           (val) `(set-symbol-function ,val ,s))
(define-setf-expander symbol-plist   (s)            (val) `(set-symbol-plist ,val ,s))
(define-setf-expander svref          (vec idx)      (val) `(svref-set ,vec ,idx ,val))
(define-setf-expander bit            (vec idx)      (val) `(bit-set ,vec ,idx ,val))
(define-setf-expander aref           (array idx)    (val) `(aref-set ,val ,array ,idx))
(define-setf-expander char           (str idx)      (val) `(char-set ,str ,idx ,val))
(define-setf-expander schar          (str idx)      (val) `(char-set ,str ,idx ,val))
(define-setf-expander nth            (n lst)        (val) `(rplaca (nthcdr ,n ,lst) ,val))
;; ELT setter — dispatches on sequence type; uses gensyms to avoid
;; multiple evaluation of sub-forms.
(define-setf-expander elt (seq idx) (val)
  (let ((s (gensym "S")) (i (gensym "I")) (v (gensym "V")))
    `(let ((,s ,seq) (,i ,idx) (,v ,val))
       (cond ((consp ,s)        (rplaca (nthcdr ,i ,s) ,v))
             ((stringp ,s)      (char-set ,v ,s ,i))
             ((vectorp ,s)      (svref-set ,s ,i ,v))
             ((bit-vector-p ,s) (bit-set ,s ,i ,v))
             (t (error "SETF ELT: not a sequence"))))))

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

(defmacro prog1 (first-form &rest more-forms)
  "Evaluate FIRST-FORM and MORE-FORMS in order; return the value of FIRST-FORM."
  (let ((result (gensym "P1-")))
    `(let ((,result ,first-form))
       ,@more-forms
       ,result)))

(defmacro prog2 (first-form second-form &rest more-forms)
  "Evaluate all forms in order; return the value of SECOND-FORM."
  (let ((result (gensym "P2-")))
    `(progn
       ,first-form
       (let ((,result ,second-form))
         ,@more-forms
         ,result))))

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

;; Type predicates — delegate to kernel TYPEP
(defun numberp   (x) (typep x 'number))
(defun integerp  (x) (typep x 'integer))
(defun floatp    (x) (typep x 'float))
(defun rationalp (x) (typep x 'rational))
(defun realp     (x) (typep x 'real))
(defun complexp  (x) (typep x 'complex))
(defun characterp (x) (typep x 'character))
(defun vectorp   (x) (typep x 'vector))
(defun arrayp    (x) (typep x 'array))
(defun packagep  (x) (typep x 'package))
(defun pathnamep (x) (typep x 'pathname))
(defun streamp   (x) (typep x 'stream))
(defun hash-table-p (x) (typep x 'hash-table))
(defun random-state-p (x) (typep x 'random-state))

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

;; -------------------------------------------------------------------- ;;
;;  Sequence library (slice 4) — generic over LIST first; STRING and    ;;
;;  VECTOR cases use type-specific paths where the kernel supplies them ;;
;;  (LIST-LENGTH, STRING-LENGTH, VECTOR-LENGTH, NTH/CHAR-REF/SVREF).    ;;
;;                                                                       ;;
;;  Most CL sequence functions take :TEST and :KEY keyword args; we     ;;
;;  support those via &KEY and the new kernel default-evaluation path.  ;;
;; -------------------------------------------------------------------- ;;

(defun length (seq)
  (cond ((null seq)        0)
        ((consp seq)       (list-length seq))
        ((stringp seq)     (string-length seq))
        ((vectorp seq)     (vector-length seq))
        ((bit-vector-p seq)(bit-vector-length seq))
        (t (error "LENGTH: not a sequence"))))

(defun elt (seq i)
  (cond ((consp seq)        (nth i seq))
        ((stringp seq)      (char seq i))
        ((vectorp seq)      (svref seq i))
        ((bit-vector-p seq) (bit seq i))
        (t (error "ELT: not a sequence"))))

;; -------------------------------------------------------------------- ;;
;;  List-only versions of REDUCE, FIND, POSITION, COUNT, REMOVE, MAPCAN ;;
;;  + their IF variants. These are simple enough that supporting all    ;;
;;  sequence types right now is overkill; the LIST case unblocks 95% of ;;
;;  Lisp library code. Specialised string/vector paths can be slotted in;;
;;  later by changing these definitions.                                ;;
;; -------------------------------------------------------------------- ;;

(defun reduce (function sequence &key (initial-value nil iv-supplied) from-end (key #'identity))
  ;; Generic REDUCE: works on lists, strings, and vectors.
  ;; FROM-END reverses the element order; KEY is applied to each element
  ;; before it is passed to FUNCTION.
  (let* ((n   (length sequence))
         (lst (if (listp sequence)
                  sequence
                  ;; Collect elements via index (works for strings + vectors).
                  (let ((acc nil))
                    (dotimes (i n (nreverse acc))
                      (push (elt sequence i) acc)))))
         (seq (if from-end (reverse lst) lst))
         ;; Apply key to each element.
         (seq (mapcar (lambda (x) (funcall key x)) seq)))
    (cond
      ((null seq)
       (if iv-supplied initial-value (funcall function)))
      (iv-supplied
       (let ((acc initial-value))
         (dolist (x seq) (setq acc (if from-end (funcall function x acc)
                                                (funcall function acc x))))
         acc))
      (t
       (let ((acc (car seq)))
         (dolist (x (cdr seq)) (setq acc (if from-end (funcall function x acc)
                                                      (funcall function acc x))))
         acc)))))

(defun member (item list &key (test #'eql) (key #'identity))
  "Return the first cons in LIST whose CAR satisfies TEST with ITEM, or NIL."
  (do ((l list (cdr l)))
      ((null l) nil)
    (when (funcall test item (funcall key (car l)))
      (return l))))

(defun member-if (predicate list &key (key #'identity))
  "Return the first cons in LIST whose CAR satisfies PREDICATE, or NIL."
  (do ((l list (cdr l)))
      ((null l) nil)
    (when (funcall predicate (funcall key (car l)))
      (return l))))

(defun find-if (predicate sequence &key (key #'identity))
  (if (listp sequence)
      (dolist (x sequence nil)
        (when (funcall predicate (funcall key x))
          (return-from find-if x)))
      (dotimes (i (length sequence) nil)
        (let ((x (elt sequence i)))
          (when (funcall predicate (funcall key x))
            (return-from find-if x))))))

(defun find (item sequence &key (test #'eql) (key #'identity))
  (find-if (lambda (x) (funcall test item x)) sequence :key key))

(defun find-if-not (predicate sequence &key (key #'identity))
  (find-if (lambda (x) (not (funcall predicate x))) sequence :key key))

(defun position-if (predicate sequence &key (key #'identity))
  (if (listp sequence)
      (let ((i 0))
        (dolist (x sequence nil)
          (when (funcall predicate (funcall key x))
            (return-from position-if i))
          (setq i (+ i 1))))
      (dotimes (i (length sequence) nil)
        (when (funcall predicate (funcall key (elt sequence i)))
          (return-from position-if i)))))

(defun position (item sequence &key (test #'eql) (key #'identity))
  (position-if (lambda (x) (funcall test item x)) sequence :key key))

(defun position-if-not (predicate sequence &key (key #'identity))
  (position-if (lambda (x) (not (funcall predicate x))) sequence :key key))

(defun count-if (predicate sequence &key (key #'identity))
  (if (listp sequence)
      (let ((n 0))
        (dolist (x sequence n)
          (when (funcall predicate (funcall key x))
            (setq n (+ n 1)))))
      (let ((n 0))
        (dotimes (i (length sequence) n)
          (when (funcall predicate (funcall key (elt sequence i)))
            (setq n (+ n 1)))))))

(defun count (item sequence &key (test #'eql) (key #'identity))
  (count-if (lambda (x) (funcall test item x)) sequence :key key))

(defun count-if-not (predicate sequence &key (key #'identity))
  (count-if (lambda (x) (not (funcall predicate x))) sequence :key key))

(defun remove-if (predicate sequence &key (key #'identity))
  (cond
    ((listp sequence)
     (let ((acc nil))
       (dolist (x sequence (nreverse acc))
         (unless (funcall predicate (funcall key x))
           (push x acc)))))
    ((stringp sequence)
     (let ((acc nil))
       (dotimes (i (string-length sequence))
         (let ((x (char sequence i)))
           (unless (funcall predicate (funcall key x))
             (push x acc))))
       (coerce (nreverse acc) 'string)))
    ((vectorp sequence)
     (let ((acc nil))
       (dotimes (i (vector-length sequence))
         (let ((x (svref sequence i)))
           (unless (funcall predicate (funcall key x))
             (push x acc))))
       (coerce (nreverse acc) 'vector)))
    (t (error "REMOVE-IF: not a sequence"))))

(defun remove (item sequence &key (test #'eql) (key #'identity))
  (remove-if (lambda (x) (funcall test item x)) sequence :key key))

(defun remove-if-not (predicate sequence &key (key #'identity))
  (remove-if (lambda (x) (not (funcall predicate x))) sequence :key key))

(defun some (predicate sequence)
  "Return the first true value of (PREDICATE element) over SEQUENCE, or NIL."
  (if (listp sequence)
      (dolist (x sequence nil)
        (let ((v (funcall predicate x)))
          (when v (return v))))
      (dotimes (i (length sequence) nil)
        (let ((v (funcall predicate (elt sequence i))))
          (when v (return-from some v))))))

(defun every (predicate sequence)
  "Return T if (PREDICATE element) is true for every element of SEQUENCE."
  (if (listp sequence)
      (dolist (x sequence t)
        (unless (funcall predicate x) (return nil)))
      (dotimes (i (length sequence) t)
        (unless (funcall predicate (elt sequence i))
          (return-from every nil)))))

(defun notany (predicate sequence)
  "Return T if (PREDICATE element) is false for every element of SEQUENCE."
  (not (some predicate sequence)))

(defun notevery (predicate sequence)
  "Return T if (PREDICATE element) is false for at least one element."
  (not (every predicate sequence)))

(defun mapcan (function &rest lists)
  ;; (MAPCAN fn list...) — like MAPCAR but the results are NCONC'd.
  (let ((acc nil))
    (dolist (chunk (apply #'mapcar function lists))
      (setq acc (nconc acc chunk)))
    acc))

(defun maplist (function &rest lists)
  ;; (MAPLIST fn list...) — applies fn to successive sublists.
  (let ((acc nil))
    (do ((tails lists (mapcar #'cdr tails)))
        ((some #'null tails) (nreverse acc))
      (push (apply function tails) acc))))

(defun mapcon (function &rest lists)
  (let ((acc nil))
    (do ((tails lists (mapcar #'cdr tails)))
        ((some #'null tails) acc)
      (setq acc (nconc acc (apply function tails))))))

;; -------------------------------------------------------------------- ;;
;;  Sub-sequences.                                                       ;;
;; -------------------------------------------------------------------- ;;

(defun butlast (list &optional (n 1))
  ;; List minus the last n elements. Naive but correct.
  (let* ((len (length list))
         (keep (- len n)))
    (if (<= keep 0)
        nil
        (let ((acc nil)
              (rem list)
              (i 0))
          (dotimes (k keep (nreverse acc))
            k                                ;; suppress unused warnings
            (push (car rem) acc)
            (setq rem (cdr rem))
            (setq i (+ i 1)))))))

(defun subseq (sequence start &optional end)
  ;; Generic SUBSEQ: lists, strings, and vectors.
  (cond
    ((stringp sequence)
     (subseq-string sequence start (or end (string-length sequence))))
    ((vectorp sequence)
     (let* ((len (or end (vector-length sequence)))
            (count (- len start))
            (out (make-array count)))
       (dotimes (i count out)
         (setf (aref out i) (svref sequence (+ start i))))))
    (t
     ;; list path (original implementation)
     (let ((rem (nthcdr start sequence))
           (acc nil)
           (count (if end (- end start) -1)))
       (do ((i 0 (+ i 1)))
           ((or (null rem) (and (>= count 0) (= i count))) (nreverse acc))
         (push (car rem) acc)
         (setq rem (cdr rem)))))))

;; -------------------------------------------------------------------- ;;
;;  Concatenation — list result-type only for now.                       ;;
;; -------------------------------------------------------------------- ;;

(defun concatenate (result-type &rest sequences)
  ;; Currently only result-type LIST is supported.
  (case result-type
    ((list cl:list nil)
     (let ((acc nil))
       (dolist (seq sequences)
         ;; Walk seq generically.
         (cond ((null seq))
               ((consp seq)
                (dolist (x seq) (push x acc)))
               ((or (stringp seq) (vectorp seq) (bit-vector-p seq))
                (dotimes (i (length seq))
                  (push (elt seq i) acc)))
               (t (error "CONCATENATE: not a sequence"))))
       (nreverse acc)))
    (t (error "CONCATENATE: only :LIST result-type is supported"))))

;; -------------------------------------------------------------------- ;;
;;  Sort — merge sort on lists.                                          ;;
;; -------------------------------------------------------------------- ;;

(defun %merge (less a b)
  ;; Stable merge of two ordered lists. LESS is the comparator.
  (cond ((null a) b)
        ((null b) a)
        ((funcall less (car a) (car b))
         (cons (car a) (%merge less (cdr a) b)))
        (t
         (cons (car b) (%merge less a (cdr b))))))

(defun %list-split (list)
  ;; Tortoise/hare split. For (a b) returns (values (a) (b)).
  ;; Pre-fix the loop never entered for 2-element lists, returning
  ;; (NIL (a b)) and causing infinite recursion in SORT.
  (let ((slow list) (fast list) (acc nil))
    (do ()
        ((or (null fast) (null (cdr fast))))
      (push (car slow) acc)
      (setq slow (cdr slow))
      (setq fast (cddr fast)))
    (values (nreverse acc) slow)))

(defvar *%old-sort*
  (lambda (sequence less &key (key #'identity))
    ;; List-only merge sort (original implementation).
    (let ((cmp (if (eq key #'identity)
                   less
                   (lambda (x y) (funcall less (funcall key x) (funcall key y))))))
      (labels ((rec (lst)
                 (cond ((null lst) nil)
                       ((null (cdr lst)) lst)
                       (t
                        (multiple-value-bind (l r) (%list-split lst)
                          (%merge cmp (rec l) (rec r)))))))
        (rec sequence)))))

(defun sort (sequence less &key (key #'identity))
  ;; Generic SORT: handles lists and vectors.
  (cond
    ((listp sequence)
     (funcall *%old-sort* sequence less :key key))
    ((vectorp sequence)
     (let* ((n (vector-length sequence))
            (lst (coerce sequence 'list))
            (sorted (funcall *%old-sort* lst less :key key)))
       (dotimes (i n sequence)
         (setf (aref sequence i) (nth i sorted)))))
    (t (error "SORT: not a sequence"))))

(defun stable-sort (sequence less &key (key #'identity))
  ;; Merge sort is naturally stable.
  (sort sequence less :key key))

;; -------------------------------------------------------------------- ;;
;;  Set operations (on lists).                                           ;;
;; -------------------------------------------------------------------- ;;

(defun adjoin (item list &key (test #'eql) (key #'identity))
  (if (find item list :test test :key key) list (cons item list)))

(defun union (list1 list2 &key (test #'eql) (key #'identity))
  ;; Result contains every element of list2, plus every element of list1
  ;; not already in list2 (by key + test).
  (let ((acc (copy-list list2)))
    (dolist (x list1 acc)
      (unless (find (funcall key x) acc :test test :key key)
        (setq acc (cons x acc))))))

(defun intersection (list1 list2 &key (test #'eql) (key #'identity))
  (remove-if (lambda (x) (not (find (funcall key x) list2 :test test :key key)))
             list1))

(defun set-difference (list1 list2 &key (test #'eql) (key #'identity))
  (remove-if (lambda (x) (find (funcall key x) list2 :test test :key key))
             list1))

(defun pairlis (keys values &optional (alist nil))
  ;; (PAIRLIS '(a b c) '(1 2 3)) -> ((C . 3) (B . 2) (A . 1))
  ;; per CL: keys and values are pushed onto alist in order, so the
  ;; LAST pair becomes the FIRST in the result.
  (do ((ks keys (cdr ks))
       (vs values (cdr vs))
       (acc alist))
      ((or (null ks) (null vs)) acc)
    (setq acc (acons (car ks) (car vs) acc))))

(defun acons (key value alist)
  ;; (ACONS k v a) ≡ (CONS (CONS k v) a)
  (cons (cons key value) alist))

;; -------------------------------------------------------------------- ;;
;;  Association lists (slice 12).                                        ;;
;; -------------------------------------------------------------------- ;;

(defun assoc (item list &key (test #'eql) (key #'identity))
  "Return the first pair in LIST whose CAR satisfies TEST with ITEM."
  (dolist (pair list nil)
    (when (and (consp pair)
               (funcall test item (funcall key (car pair))))
      (return pair))))

(defun assoc-if (predicate list &key (key #'identity))
  "Return the first pair in LIST whose CAR satisfies PREDICATE."
  (dolist (pair list nil)
    (when (and (consp pair)
               (funcall predicate (funcall key (car pair))))
      (return pair))))

(defun assoc-if-not (predicate list &key (key #'identity))
  "Return the first pair in LIST whose CAR does not satisfy PREDICATE."
  (dolist (pair list nil)
    (when (and (consp pair)
               (not (funcall predicate (funcall key (car pair)))))
      (return pair))))

(defun rassoc (item list &key (test #'eql) (key #'identity))
  "Return the first pair in LIST whose CDR satisfies TEST with ITEM."
  (dolist (pair list nil)
    (when (and (consp pair)
               (funcall test item (funcall key (cdr pair))))
      (return pair))))

(defun rassoc-if (predicate list &key (key #'identity))
  "Return the first pair in LIST whose CDR satisfies PREDICATE."
  (dolist (pair list nil)
    (when (and (consp pair)
               (funcall predicate (funcall key (cdr pair))))
      (return pair))))

(defun rassoc-if-not (predicate list &key (key #'identity))
  "Return the first pair in LIST whose CDR does not satisfy PREDICATE."
  (dolist (pair list nil)
    (when (and (consp pair)
               (not (funcall predicate (funcall key (cdr pair)))))
      (return pair))))

;; -------------------------------------------------------------------- ;;
;;  List utilities (slice 12).                                           ;;
;; -------------------------------------------------------------------- ;;

(defun list* (&rest args)
  "Construct a list with the last arg as the final CDR.
   (LIST* a b c) => (CONS a (CONS b c)).  (LIST* x) => x."
  (cond ((null args) nil)
        ((null (cdr args)) (car args))
        (t (cons (car args) (apply #'list* (cdr args))))))

(defun copy-tree (tree)
  "Recursive copy of a tree (copies all cons cells)."
  (if (atom tree)
      tree
      (cons (copy-tree (car tree)) (copy-tree (cdr tree)))))

(defun tailp (object list)
  "T if OBJECT is EQ to some tail (including NIL) of LIST."
  (do ((l list (cdr l)))
      ((atom l) (eq object l))
    (when (eq l object) (return t))))

(defun nconc (&rest lists)
  "Destructively concatenate LISTS.  Non-list first args are an error."
  (cond ((null lists) nil)
        ((null (cdr lists)) (car lists))
        (t
         (let ((head (car lists))
               (rest (apply #'nconc (cdr lists))))
           (if (null head)
               rest
               (do ((tail head (cdr tail)))
                   ((null (cdr tail))
                    (rplacd tail rest)
                    head)))))))

(defun delete-if (predicate list &key (key #'identity))
  "Destructively remove elements satisfying PREDICATE."
  (let ((head (cons nil list)))
    (do ((prev head)
         (curr list (cdr curr)))
        ((null curr) (cdr head))
      (if (funcall predicate (funcall key (car curr)))
          (rplacd prev (cdr curr))
          (setq prev curr)))))

(defun delete-if-not (predicate list &key (key #'identity))
  "Destructively keep only elements satisfying PREDICATE."
  (delete-if (lambda (x) (not (funcall predicate x))) list :key key))

(defun remove-duplicates (list &key (test #'eql) (key #'identity))
  "Return a new list with duplicates removed (last occurrence kept)."
  (let ((acc nil))
    (dolist (x list (nreverse acc))
      (unless (find (funcall key x) acc :test test :key key)
        (push x acc)))))

(defun delete-duplicates (list &key (test #'eql) (key #'identity))
  "Destructive version of REMOVE-DUPLICATES."
  (remove-duplicates list :test test :key key))

;; -------------------------------------------------------------------- ;;
;;  Tree operations (slice 12).                                          ;;
;; -------------------------------------------------------------------- ;;

(defun subst (new old tree &key (test #'eql) (key #'identity))
  "Copy TREE substituting NEW for every subtree EQL to OLD."
  (cond ((funcall test old (funcall key tree)) new)
        ((atom tree) tree)
        (t (let ((new-car (subst new old (car tree) :test test :key key))
                 (new-cdr (subst new old (cdr tree) :test test :key key)))
             (if (and (eq new-car (car tree)) (eq new-cdr (cdr tree)))
                 tree
                 (cons new-car new-cdr))))))

(defun subst-if (new predicate tree &key (key #'identity))
  "Copy TREE substituting NEW for every subtree satisfying PREDICATE."
  (cond ((funcall predicate (funcall key tree)) new)
        ((atom tree) tree)
        (t (let ((new-car (subst-if new predicate (car tree) :key key))
                 (new-cdr (subst-if new predicate (cdr tree) :key key)))
             (if (and (eq new-car (car tree)) (eq new-cdr (cdr tree)))
                 tree
                 (cons new-car new-cdr))))))

(defun subst-if-not (new predicate tree &key (key #'identity))
  "Copy TREE substituting NEW for every subtree NOT satisfying PREDICATE."
  (subst-if new (lambda (x) (not (funcall predicate x))) tree :key key))

(defun tree-equal (tree1 tree2 &key (test #'eql))
  "T if TREE1 and TREE2 have the same shape and leaves (by TEST)."
  (cond ((and (atom tree1) (atom tree2))
         (funcall test tree1 tree2))
        ((or  (atom tree1) (atom tree2)) nil)
        (t (and (tree-equal (car tree1) (car tree2) :test test)
                (tree-equal (cdr tree1) (cdr tree2) :test test)))))

;; -------------------------------------------------------------------- ;;
;;  Property lists (slice 12).                                           ;;
;; -------------------------------------------------------------------- ;;

(defun get (symbol indicator &optional default)
  "Return the value for INDICATOR on SYMBOL's property list, or DEFAULT."
  (getf (symbol-plist symbol) indicator default))

(defun remprop (symbol indicator)
  "Remove INDICATOR and its value from SYMBOL's property list.
   Returns T if the indicator was present, NIL otherwise."
  ;; Build a fresh plist, skipping any (indicator value) pair found.
  ;; We avoid the kernel REMF because its destructive in-place mutation
  ;; doesn't update the symbol's plist pointer for single-pair plists.
  (let ((found nil)
        (acc   nil)
        (pl    (symbol-plist symbol)))
    (do ()
        ((null pl))
      (if (eq (car pl) indicator)
          (setq found t
                pl    (cddr pl))
          (progn
            (push (car  pl) acc)
            (push (cadr pl) acc)
            (setq pl (cddr pl)))))
    (when found
      (set-symbol-plist (nreverse acc) symbol))
    found))

;; (SETF (GET sym ind) val) — bocl simple-expander style.
;; Note: kernel SET-SYMBOL-PLIST arg order is (new-plist symbol).
(define-setf-expander get (symbol indicator) (val)
  `(set-symbol-plist (putf (symbol-plist ,symbol) ,indicator ,val) ,symbol))

;; -------------------------------------------------------------------- ;;
;;  String / character library (slice 5).                                ;;
;;                                                                       ;;
;;  The kernel supplies the raw character ops (CHAR-CODE / CODE-CHAR /   ;;
;;  CHAR / SCHAR / STRING= / STRING-EQUAL / STRING-LENGTH / MAKE-STRING  ;;
;;  / CHAR-SET) and we layer the case predicates, case conversion, the   ;;
;;  ordering predicates, and the common processing helpers on top.       ;;
;; -------------------------------------------------------------------- ;;

;; ---- Character predicates and case conversion. -----------------------

(defun upper-case-p (char)
  (let ((c (char-code char)))
    (and (>= c (char-code #\A)) (<= c (char-code #\Z)))))

(defun lower-case-p (char)
  (let ((c (char-code char)))
    (and (>= c (char-code #\a)) (<= c (char-code #\z)))))

(defun both-case-p (char)
  (or (upper-case-p char) (lower-case-p char)))

(defun alpha-char-p (char)
  (both-case-p char))

(defun digit-char-p (char &optional (radix 10))
  ;; Return the digit's integer value, or NIL if char isn't a digit in
  ;; the given radix.
  (let* ((c  (char-code char))
         (d0 (char-code #\0))
         (a  (char-code #\A))
         (la (char-code #\a))
         (v  (cond ((and (>= c d0) (<= c (+ d0 9))) (- c d0))
                   ((and (>= c a)  (<= c (+ a 25))) (+ 10 (- c a)))
                   ((and (>= c la) (<= c (+ la 25))) (+ 10 (- c la)))
                   (t nil))))
    (if (and v (< v radix)) v nil)))

(defun alphanumericp (char)
  (or (alpha-char-p char) (digit-char-p char)))

(defun digit-char (weight &optional (radix 10))
  ;; Return the character representing WEIGHT in RADIX, or NIL if impossible.
  (when (and (>= weight 0) (< weight radix) (<= radix 36))
    (if (< weight 10)
        (code-char (+ weight (char-code #\0)))
        (code-char (+ (- weight 10) (char-code #\A))))))

(defun char-upcase (char)
  (if (lower-case-p char)
      (code-char (- (char-code char) 32))
      char))

(defun char-downcase (char)
  (if (upper-case-p char)
      (code-char (+ (char-code char) 32))
      char))

(defun char-equal (c1 c2)
  ;; case-insensitive character compare.
  (eql (char-code (char-upcase c1)) (char-code (char-upcase c2))))

;; ---- Exact (case-sensitive) character comparisons. -------------------

(defun char= (c1 c2)   (eql (char-code c1) (char-code c2)))
(defun char/= (c1 c2)  (not (eql (char-code c1) (char-code c2))))
(defun char<  (c1 c2)  (< (char-code c1) (char-code c2)))
(defun char>  (c1 c2)  (> (char-code c1) (char-code c2)))
(defun char<= (c1 c2)  (<= (char-code c1) (char-code c2)))
(defun char>= (c1 c2)  (>= (char-code c1) (char-code c2)))

;; ---- Case-insensitive character comparisons. -------------------------

(defun char-lessp      (c1 c2) (char< (char-downcase c1) (char-downcase c2)))
(defun char-greaterp   (c1 c2) (char> (char-downcase c1) (char-downcase c2)))
(defun char-not-greaterp (c1 c2) (char<= (char-downcase c1) (char-downcase c2)))
(defun char-not-lessp    (c1 c2) (char>= (char-downcase c1) (char-downcase c2)))
(defun char-not-equal    (c1 c2) (char/= (char-downcase c1) (char-downcase c2)))

;; ---- STRING coercion designator. -------------------------------------

(defun string (designator)
  ;; CL: (string x) returns a string. If x is already a string, it's
  ;; returned as-is; symbol -> its name; character -> a 1-char string.
  (cond ((stringp designator) designator)
        ((symbolp designator) (symbol-name designator))
        ((characterp designator)
         (let ((s (make-string 1)))
           (char-set s 0 designator)
           s))
        (t (error "STRING: cannot coerce ~S to a string" designator))))

;; ---- String case conversion. -----------------------------------------

(defun %string-map (function s)
  ;; Build a fresh string by applying FUNCTION (char -> char) to each
  ;; position of S. S is run through STRING first so symbols/characters
  ;; coerce.
  (let* ((s   (string s))
         (n   (string-length s))
         (out (make-string n)))
    (dotimes (i n out)
      (char-set out i (funcall function (char s i))))))

(defun string-upcase   (s) (%string-map #'char-upcase   s))
(defun string-downcase (s) (%string-map #'char-downcase s))

(defun string-capitalize (s)
  ;; Upper-case the first letter of each maximal run of alphanumerics,
  ;; lower-case the rest.
  (let* ((s        (string s))
         (n        (string-length s))
         (out      (make-string n))
         (in-word  nil))
    (dotimes (i n out)
      (let ((c (char s i)))
        (cond ((alphanumericp c)
               (char-set out i
                         (if in-word (char-downcase c) (char-upcase c)))
               (setq in-word t))
              (t
               (char-set out i c)
               (setq in-word nil)))))))

;; ---- String ordering. ------------------------------------------------

(defun %string-compare (a b case-fold)
  ;; Return a < 0 / 0 / > 0 like memcmp; honour case-fold when t.
  (let* ((a  (string a))
         (b  (string b))
         (na (string-length a))
         (nb (string-length b))
         (n  (if (< na nb) na nb)))
    (do ((i 0 (1+ i)))
        ((= i n) (- na nb))
      (let* ((ca (char a i))
             (cb (char b i))
             (xa (if case-fold (char-code (char-upcase ca)) (char-code ca)))
             (xb (if case-fold (char-code (char-upcase cb)) (char-code cb))))
        (when (/= xa xb) (return (- xa xb)))))))

(defun string<  (a b) (< (%string-compare a b nil) 0))
(defun string>  (a b) (> (%string-compare a b nil) 0))
(defun string<= (a b) (<= (%string-compare a b nil) 0))
(defun string>= (a b) (>= (%string-compare a b nil) 0))
(defun string/= (a b) (not (zerop (%string-compare a b nil))))

(defun string-lessp    (a b) (<  (%string-compare a b t) 0))
(defun string-greaterp (a b) (>  (%string-compare a b t) 0))
(defun string-not-lessp    (a b) (>= (%string-compare a b t) 0))
(defun string-not-greaterp (a b) (<= (%string-compare a b t) 0))
(defun string-not-equal    (a b) (not (zerop (%string-compare a b t))))

;; ---- String trim. ----------------------------------------------------
;;
;; CHAR-BAG can be a string or a list of characters; we treat strings
;; as a sequence of characters for membership purposes.

(defun subseq-string (s start end)
  ;; Helper: copy s[start..end) into a fresh string.
  (let* ((len (- end start))
         (out (make-string len)))
    (dotimes (i len out)
      (char-set out i (char s (+ start i))))))

(defun %in-char-bag (char bag)
  (cond ((stringp bag)
         (dotimes (i (string-length bag) nil)
           (when (eql char (char bag i)) (return t))))
        ((listp bag)
         (find char bag))
        (t nil)))

(defun string-left-trim (char-bag s)
  (let* ((s (string s))
         (n (string-length s)))
    (do ((i 0 (1+ i)))
        ((or (= i n) (not (%in-char-bag (char s i) char-bag)))
         (subseq-string s i n)))))

(defun string-right-trim (char-bag s)
  (let* ((s (string s))
         (n (string-length s)))
    (do ((i n (1- i)))
        ((or (zerop i) (not (%in-char-bag (char s (1- i)) char-bag)))
         (subseq-string s 0 i)))))

(defun string-trim (char-bag s)
  (string-right-trim char-bag (string-left-trim char-bag s)))

;; ---- SEARCH (string-in-string only for now). -------------------------

(defun search (pattern target &key (test #'eql) (start1 0) end1 (start2 0) end2)
  ;; Return the position in TARGET where PATTERN first occurs as a
  ;; contiguous sub-sequence, or NIL.  We only handle string-in-string
  ;; here; list-in-list support can be added later.
  (let* ((pattern (string pattern))
         (target  (string target))
         (e1 (or end1 (string-length pattern)))
         (e2 (or end2 (string-length target)))
         (plen (- e1 start1))
         (tlen (- e2 start2)))
    (when (<= plen tlen)
      (do ((i start2 (1+ i))
           (limit (- e2 plen)))
          ((> i limit) nil)
        (let ((match t))
          (do ((j 0 (1+ j)))
              ((or (not match) (= j plen)))
            (unless (funcall test (char pattern (+ start1 j)) (char target (+ i j)))
              (setq match nil)))
          (when match (return i)))))))

;; ---- SUBSTITUTE (lists for now, like the rest of Slice 4). ----------

(defun substitute (new old sequence &key (test #'eql) (key #'identity))
  (cond ((listp sequence)
         (let ((acc '()))
           (dolist (x sequence (nreverse acc))
             (push (if (funcall test old (funcall key x)) new x) acc))))
        ((stringp sequence)
         (let* ((n   (string-length sequence))
                (out (make-string n)))
           (dotimes (i n out)
             (let ((c (char sequence i)))
               (char-set out i
                         (if (funcall test old (funcall key c)) new c))))))
        (t (error "SUBSTITUTE: unsupported sequence type"))))

;; -------------------------------------------------------------------- ;;
;;  Numeric extras (slice 6).                                            ;;
;;                                                                       ;;
;;  EXPT / SQRT / ISQRT / GCD / LCM / SIGNUM / LOGCOUNT and the float    ;;
;;  transcendentals are kernel-side; this layer adds the small helpers   ;;
;;  that benefit from Lisp definitions: PI, PARSE-INTEGER, and the byte  ;;
;;  field accessors LDB / DPB.                                           ;;
;; -------------------------------------------------------------------- ;;

(defparameter pi 3.141592653589793d0)

(defun parse-integer (string &key (start 0) end (radix 10) junk-allowed)
  ;; Return (values integer end-position). Skip leading whitespace; then
  ;; optional sign; then digits in the given radix. If JUNK-ALLOWED is
  ;; NIL and trailing junk is found, signal an error.
  (let* ((s   (string string))
         (e   (or end (string-length s)))
         (i   start)
         (sign 1)
         (any-digit nil)
         (val 0))
    ;; Skip leading whitespace.
    (do ()
        ((or (>= i e)
             (let ((c (char s i)))
               (not (or (eql c #\Space) (eql c #\Tab) (eql c #\Newline))))))
      (setq i (1+ i)))
    ;; Optional sign.
    (when (< i e)
      (let ((c (char s i)))
        (cond ((eql c #\+) (setq i (1+ i)))
              ((eql c #\-) (setq sign -1) (setq i (1+ i))))))
    ;; Digits.
    (do ()
        ((or (>= i e)
             (null (digit-char-p (char s i) radix))))
      (setq val       (+ (* val radix) (digit-char-p (char s i) radix))
            any-digit t
            i         (1+ i)))
    (unless any-digit
      (if junk-allowed
          (return-from parse-integer (values nil i))
          (error "PARSE-INTEGER: no digits in ~S" string)))
    ;; Trailing-junk check.
    (unless junk-allowed
      (do ()
          ((>= i e))
        (let ((c (char s i)))
          (unless (or (eql c #\Space) (eql c #\Tab) (eql c #\Newline))
            (error "PARSE-INTEGER: junk at position ~D of ~S" i string))
          (setq i (1+ i)))))
    (values (* sign val) i)))

;; ---- BYTE / LDB / DPB. -----------------------------------------------
;;
;; CL packages (size, position) into a single byte-specifier object.
;; We represent it as a cons (size . position) — opaque to user code.

(defun byte (size position) (cons size position))
(defun byte-size     (b) (car b))
(defun byte-position (b) (cdr b))

(defun ldb (bytespec integer)
  ;; Load byte: extract SIZE bits starting at POSITION.
  (logand (ash integer (- (byte-position bytespec)))
          (1- (ash 1 (byte-size bytespec)))))

(defun dpb (newbyte bytespec integer)
  ;; Deposit byte: replace the SIZE bits at POSITION in INTEGER with the
  ;; low SIZE bits of NEWBYTE.
  (let* ((mask (ash (1- (ash 1 (byte-size bytespec)))
                    (byte-position bytespec))))
    (logior (logand integer (lognot mask))
            (logand (ash newbyte (byte-position bytespec)) mask))))

(defun mask-field (bytespec integer)
  (logand integer
          (ash (1- (ash 1 (byte-size bytespec)))
               (byte-position bytespec))))

(defun deposit-field (newbyte bytespec integer)
  (let* ((mask (ash (1- (ash 1 (byte-size bytespec)))
                    (byte-position bytespec))))
    (logior (logand integer  (lognot mask))
            (logand newbyte  mask))))

;; -------------------------------------------------------------------- ;;
;;  DESTRUCTURING-BIND (slice 7).                                        ;;
;;                                                                       ;;
;;  Expands at macro-expansion time into a flat LET* binding list.       ;;
;;  The helper %DB-BINDINGS walks the pattern recursively and generates  ;;
;;  (VAR FORM) pairs; gensym'd position vars ensure shared subforms are  ;;
;;  evaluated only once. Supported lambda-list syntax:                   ;;
;;    required (possibly nested), &WHOLE, &OPTIONAL (with default and    ;;
;;    supplied-p), &REST / &BODY, &KEY (with :keyword rename and         ;;
;;    supplied-p), &ALLOW-OTHER-KEYS, &AUX.                              ;;
;; -------------------------------------------------------------------- ;;

(defun %db-bindings (pattern src)
  ;; Returns a flat list of (VAR ACCESS-FORM) pairs for use in LET*.
  ;; SRC is a Lisp form that evaluates to the object being destructured.
  (let ((binds '()))
    (labels
        ((add! (v f) (push (list v f) binds))
         ;; Create a new LET* position variable bound to FORM and return its name.
         (new-pos (form)
           (let ((g (gensym "P")))
             (add! g form)
             g))
         ;; Walk PAT against the list denoted by POS (a symbol in the LET*).
         (walk (pat pos)
           (cond
             ;; End of pattern list
             ((null pat) nil)
             ;; Atom: dotted-cdr rest variable
             ((symbolp pat) (add! pat pos))
             ;; List: walk each element
             ((consp pat)
              (let ((p pat)
                    (cur pos))  ;; cur tracks current-tail symbol
                ;; &WHOLE comes first if present
                (when (and (consp p) (eql (car p) '&whole))
                  (add! (cadr p) cur)
                  (setq p (cddr p)))
                (block walk-loop
                  (do ()
                      ((or (null p) (symbolp p))
                       ;; Dotted-cdr rest variable: (a b . rest)
                       (when (symbolp p) (add! p cur)))
                    (let ((elem (car p)))
                      (cond
                        ;; --- section markers ---
                        ((eql elem '&optional)
                         (walk-optional (cdr p) cur)
                         (return-from walk-loop nil))
                        ((or (eql elem '&rest) (eql elem '&body))
                         (let ((rp (cadr p)))
                           (if (symbolp rp)
                               (add! rp cur)
                             ;; Nested rest pattern — bind a temp first
                             (let ((tmp (gensym "R")))
                               (add! tmp cur)
                               (walk rp tmp))))
                         ;; Continue scanning for &key / &aux after &rest var
                         (walk-after-rest (cddr p) cur)
                         (return-from walk-loop nil))
                        ((eql elem '&key)
                         (walk-key (cdr p) cur)
                         (return-from walk-loop nil))
                        ((eql elem '&aux)
                         (walk-aux (cdr p))
                         (return-from walk-loop nil))
                        ((eql elem '&allow-other-keys)
                         (setq p (cdr p)))
                        ;; --- required element ---
                        (t
                         (let ((head `(car ,cur))
                               (next (new-pos `(cdr ,cur))))
                           (cond
                             ((symbolp elem) (add! elem head))
                             ((consp elem)
                              (let ((tmp (gensym "L")))
                                (add! tmp head)
                                (walk elem tmp)))
                             (t (error "DESTRUCTURING-BIND: bad pattern element ~S"
                                       elem)))
                           (setq cur next)
                           (setq p (cdr p))))))))))
             (t (error "DESTRUCTURING-BIND: bad pattern ~S" pat))))

         ;; &OPTIONAL section: SPECS is the tail of the pattern after &OPTIONAL.
         ;; CUR is the current-position symbol.
         (walk-optional (specs cur)
           (dolist (spec specs)
             (cond
               ;; Transition to another section
               ((eql spec '&rest) (return))
               ((eql spec '&body) (return))
               ((eql spec '&key)  (return))
               ((eql spec '&aux)  (return))
               ((eql spec '&allow-other-keys) nil)
               ;; Optional binding
               (t
                (let (var default svar)
                  (if (symbolp spec)
                      (setq var spec default nil svar nil)
                    (setq var     (car spec)
                          default (if (cdr spec) (cadr spec) nil)
                          svar    (if (cddr spec) (caddr spec) nil)))
                  (add! var `(if (consp ,cur) (car ,cur) ,default))
                  (when svar (add! svar `(consp ,cur)))
                  (setq cur (new-pos `(if (consp ,cur) (cdr ,cur) nil))))))))

         ;; After &REST we may have &KEY / &AUX
         (walk-after-rest (rest-of-pattern rest-pos)
           (when (consp rest-of-pattern)
             (cond
               ((eql (car rest-of-pattern) '&key)
                (walk-key (cdr rest-of-pattern) rest-pos))
               ((eql (car rest-of-pattern) '&allow-other-keys)
                (walk-after-rest (cdr rest-of-pattern) rest-pos))
               ((eql (car rest-of-pattern) '&aux)
                (walk-aux (cdr rest-of-pattern))))))

         ;; &KEY section: SPECS is the tail after &KEY; REST-POS is the
         ;; keyword plist (the whole remaining arg list).
         (walk-key (specs rest-pos)
           (dolist (spec specs)
             (cond
               ((eql spec '&allow-other-keys) nil)
               ((eql spec '&aux)
                (walk-aux (cdr (member '&aux specs)))
                (return))
               ((symbolp spec)
                ;; (foo) — key is :foo, variable is foo
                (let ((kw `',(intern-keyword (symbol-name spec))))
                  (add! spec `(getf ,rest-pos ,kw))))
               ((consp spec)
                (let* ((head    (car spec))
                       (default (if (cdr spec) (cadr spec) nil))
                       (svar    (if (cddr spec) (caddr spec) nil))
                       kw var)
                  (if (consp head)
                      ;; ((:keyword var) default svar)
                      (setq kw `',(car head) var (cadr head))
                    ;; (var default svar) — keyword inferred from var name
                    (setq kw `',(intern-keyword (symbol-name head)) var head))
                  (add! var `(getf ,rest-pos ,kw ,default))
                  (when svar
                    ;; Supplied-p: T if the key actually appeared in the plist.
                    ;; Use a unique sentinel to distinguish missing from NIL default.
                    (let ((sentinel (gensym "KP")))
                      (add! svar
                            `(let ((,sentinel (list nil)))
                               (not (eq (getf ,rest-pos ,kw ,sentinel)
                                        ,sentinel))))))))
               (t (error "DESTRUCTURING-BIND: bad &key spec ~S" spec)))))

         ;; &AUX section: just bind without pulling from the form.
         (walk-aux (specs)
           (dolist (spec specs)
             (if (symbolp spec)
                 (add! spec nil)
               (add! (car spec) (if (cdr spec) (cadr spec) nil))))))

      (walk pattern (new-pos src)))
    (nreverse binds)))

(defmacro destructuring-bind (pattern form &body body)
  ;; Expand to (LET* <bindings> <body>) where bindings are generated
  ;; by the compile-time helper %DB-BINDINGS.
  (let ((tmp (gensym "DSRC")))
    `(let ((,tmp ,form))
       (let* ,(%db-bindings pattern tmp)
         ,@body))))

;;;; -------------------------------------------------------------------- ;;;;
;;;;  Slice 8 — DEFSTRUCT                                                 ;;;;
;;;;  Kernel primitives used:                                             ;;;;
;;;;    make-structure-class  structure-class-instance-slots              ;;;;
;;;;    make-structure-instance  structure-get  structure-set  typep      ;;;;
;;;;    set-symbol-function  sethash  gethash                             ;;;;
;;;; -------------------------------------------------------------------- ;;;;

;;; Private string-concatenation helper (no standard fn for this yet).
(defun %string-append (&rest strings)
  (let ((total 0))
    (dolist (s strings) (setq total (+ total (string-length s))))
    (let ((result (make-string total))
          (pos 0))
      (dolist (s strings result)
        (dotimes (i (string-length s))
          (char-set result pos (char s i))
          (setq pos (1+ pos)))))))

;;; Registries keyed by struct name (symbol).
;;;   *defstruct-classes*    name → class object
;;;   *defstruct-slot-specs* name → list of (slot-name initform read-only-p)
;;;                                  for ALL slots (inherited + own)
(defvar *defstruct-classes*    (make-hash-table :test 'eq))
(defvar *defstruct-slot-specs* (make-hash-table :test 'eq))

;;; Create a SETF-expander closure for a structure slot.
;;; Called at load time; the lambda captures SLOT-NAME.
(defun %make-struct-setter (slot-name)
  (lambda (s v) (list 'structure-set v s (list 'quote slot-name))))

;;; Parse one slot specification.
;;; Returns (name initform read-only-p).
(defun %parse-defstruct-slot (spec)
  (cond ((symbolp spec)    (list spec nil nil))
        ((null (cdr spec)) (list (car spec) nil nil))
        (t (list (car spec)
                 (cadr spec)
                 (if (cddr spec) (getf (cddr spec) :read-only nil) nil)))))

(defmacro defstruct (name-and-options &rest slot-specs)
  ;; Supported structure options: :conc-name, :constructor, :copier,
  ;;   :predicate, :include.
  ;; Supported slot options: initform (positional), :read-only.
  ;; NOTE: uses (set-symbol-function (lambda ...) 'name) rather than
  ;; (defun name ...) so that the generated functions close over the
  ;; lexically-bound class object.  (bocl DEFUN does not capture the
  ;; lexical env; LAMBDA + SET-SYMBOL-FUNCTION does.)
  (let* ((name     (if (consp name-and-options) (car name-and-options) name-and-options))
         (options  (if (consp name-and-options) (cdr name-and-options) nil))
         ;; :conc-name — default "<NAME>-"; nil means no prefix
         (conc-str (let ((o (assq :conc-name options)))
                     (cond ((null o)  (%string-append (symbol-name name) "-"))
                           ((cadr o)  (symbol-name (cadr o)))
                           (t ""))))
         ;; :include parent
         (include  (let ((o (assq :include options))) (if o (cadr o) nil)))
         ;; :constructor — default MAKE-<NAME>; nil to suppress
         (ctor-sym (let ((o (assq :constructor options)))
                     (if o (cadr o)
                         (intern (%string-append "MAKE-" (symbol-name name))))))
         ;; :predicate — default <NAME>-P; nil to suppress
         (pred-sym (let ((o (assq :predicate options)))
                     (if o (cadr o)
                         (intern (%string-append (symbol-name name) "-P")))))
         ;; :copier — default COPY-<NAME>; nil to suppress
         (copy-sym (let ((o (assq :copier options)))
                     (if o (cadr o)
                         (intern (%string-append "COPY-" (symbol-name name))))))
         ;; Parse own slot specs; retrieve inherited slot specs
         (own-specs (mapcar #'%parse-defstruct-slot slot-specs))
         (inh-specs (if include (gethash include *defstruct-slot-specs*) nil))
         ;; All slot specs: inherited first, then own
         (all-specs (append inh-specs own-specs))
         ;; New slot names only (for make-structure-class)
         (own-names (mapcar #'car own-specs))
         ;; (slot-name accessor-name initform read-only-p) for all slots
         (acc-info  (mapcar (lambda (sp)
                              (list (car sp)
                                    (intern (%string-append conc-str
                                                            (symbol-name (car sp))))
                                    (cadr sp)
                                    (caddr sp)))
                            all-specs))
         (cls-var   (gensym "SC")))
    `(let* ((,cls-var (make-structure-class
                        ',name
                        ,(if include `(gethash ',include *defstruct-classes*) nil)
                        0 ',own-names)))
       ;; Register for future :include uses.
       (sethash ',name *defstruct-classes*    ,cls-var)
       (sethash ',name *defstruct-slot-specs* ',all-specs)
       ;; Constructor
       ,@(when ctor-sym
           `((set-symbol-function
              (lambda (&key ,@(mapcar (lambda (ai)
                                        (if (caddr ai) (list (car ai) (caddr ai)) (car ai)))
                                      acc-info))
                (let ((%i (make-structure-instance ,cls-var)))
                  ,@(mapcar (lambda (ai) `(structure-set ,(car ai) %i ',(car ai)))
                            acc-info)
                  %i))
              ',ctor-sym)))
       ;; Predicate
       ,@(when pred-sym
           `((set-symbol-function (lambda (o) (typep o ,cls-var)) ',pred-sym)))
       ;; Accessors + mutable-slot setf expanders
       ,@(let ((forms nil))
           (dolist (ai acc-info)
             (let ((sname (car ai))
                   (aname (cadr ai))
                   (ro    (cadddr ai)))
               (push `(set-symbol-function
                        (lambda (s) (structure-get s ',sname)) ',aname)
                     forms)
               (unless ro
                 (push `(sethash ',aname *setf-expanders*
                                 (%make-struct-setter ',sname))
                       forms))))
           (nreverse forms))
       ;; Copier — copies all slots generically via the class slot alist
       ,@(when copy-sym
           `((set-symbol-function
              (lambda (s)
                (let ((%c (make-structure-instance ,cls-var)))
                  (dolist (e (structure-class-instance-slots ,cls-var) %c)
                    (structure-set (structure-get s (car e)) %c (car e)))))
              ',copy-sym)))
       ',name)))

;;;; -----------------------------------------------------------------------
;;;;  Slice 9b — CERROR / WITH-SIMPLE-RESTART
;;;; -----------------------------------------------------------------------

;; CERROR: signal a correctable error. Establishes a CONTINUE restart so
;; a HANDLER-BIND can invoke it to recover.  `continue-string` names the
;; restart's report; bocl does not display it yet.
(defmacro cerror (continue-string datum &rest args)
  ;; continue-string names the restart's report action; accepted for CL
  ;; compatibility but not used by bocl yet.
  `(restart-case (error ,datum ,@args)
     (continue () nil)))

;; WITH-SIMPLE-RESTART: shorthand for RESTART-CASE with one named restart.
(defmacro with-simple-restart (restart-spec &body body)
  (let ((name (if (consp restart-spec) (car restart-spec) restart-spec)))
    `(restart-case (progn ,@body)
       (,name () nil))))

;;;; -----------------------------------------------------------------------
;;;; Slice 13 — printer control variables, WRITE, PPRINT, stream predicates
;;;;
;;;; Kernel built-ins added in this slice (C side):
;;;;   close               — close a file stream
;;;;   write-byte          — write a byte to a binary stream
;;;;   read-byte           — read a byte from a binary stream
;;;;   input-stream-p      — is stream readable?
;;;;   output-stream-p     — is stream writable?
;;;; -----------------------------------------------------------------------

;; ---- *print-* control variables (defaults per CLHS) --------------------

(defvar *print-escape*   t)
(defvar *print-readably* nil)
(defvar *print-length*   nil)
(defvar *print-level*    nil)
(defvar *print-pretty*   nil)
(defvar *print-base*     10)
(defvar *print-radix*    nil)
(defvar *print-circle*   nil)
(defvar *print-case*     :upcase)
(defvar *print-gensym*   t)
(defvar *print-array*    t)

;; ---- *read-* control variables -----------------------------------------

(defvar *read-base*                 10)
(defvar *read-default-float-format* 'single-float)
(defvar *read-eval*                 t)
(defvar *read-suppress*             nil)

;; ---- WRITE — full CL printer entry point (simplified) ------------------
;;
;; Bocl's printer currently ignores *print-level* / *print-length* / etc.,
;; so WRITE is just a thin dispatch on :escape.

(defun write (object &key (stream    *standard-output*)
                          (escape    *print-escape*)
                          (readably  *print-readably*)
                          ;; honoured in future; accepted now so callers compile
                          (radix     *print-radix*)
                          (base      *print-base*)
                          (circle    *print-circle*)
                          (pretty    *print-pretty*)
                          (level     *print-level*)
                          (length    *print-length*)
                          (case      *print-case*)
                          (gensym    *print-gensym*)
                          (array     *print-array*))
  ;; radix/base/circle/pretty/level/length/case/gensym/array not yet wired up
  (if (or readably escape)
      (prin1 object stream)
      (princ object stream))
  object)

;; ---- PPRINT — pretty-print (simplified: prin1 + terpri) ----------------

(defun pprint (object &optional (stream *standard-output*))
  (terpri stream)
  (prin1 object stream)
  (values))                          ; CL says no values

;; ---- PRINT and PRIN1 already exist as kernel built-ins.
;;      Expose PRINT as a proper Lisp function (optional stream). ----------

;; (PRINT object &optional stream) is already a kernel built-in; it calls
;; terpri then prin1 and returns OBJECT.  The kernel registration has it
;; take exactly two args (object stream), but the stream defaults to
;; *standard-output* at the C level.  No wrapper needed.

;; ---- Prin1-to-string is a kernel built-in; ensure write-to-string
;;      and princ-to-string honour *print-escape*. --------------------------

;; Already defined in Slice 10 section; redefined here to use WRITE.
;; These definitions shadow the earlier ones.
(defun write-to-string (object &rest keys)
  (with-output-to-string (s)
    (apply #'write object :stream s keys)))

;;;; -----------------------------------------------------------------------
;;;; Slice 14 — MAKE-ARRAY, RANDOM, file-system ops, timing, FILL/REPLACE
;;;;
;;;; Kernel built-ins added in this slice (C side):
;;;;   make-array             — create 1-D vector (simple-vector)
;;;;   array-rank             — always 1
;;;;   array-dimensions       — (list size)
;;;;   array-total-size       — element count
;;;;   array-in-bounds-p      — bounds check
;;;;   adjustable-array-p     — always nil
;;;;   array-has-fill-pointer-p — always nil
;;;;   simple-vector-p        — alias for vectorp
;;;;   make-list              — build a fresh list of n elements
;;;;   copy-seq               — shallow copy of any sequence
;;;;   random                 — splitmix64 PRNG
;;;;   make-random-state      — re-seed / return T
;;;;   probe-file             — pathname or nil
;;;;   delete-file            — remove(2)
;;;;   rename-file            — rename(2), returns 3 values
;;;;   get-universal-time     — seconds since 1900-01-01
;;;;   get-internal-real-time — microseconds wall time
;;;;   get-internal-run-time  — microseconds CPU time
;;;; -----------------------------------------------------------------------

;; ---- Timing constant ----------------------------------------------------

(defconstant internal-time-units-per-second 1000000)

;; ---- Random state -------------------------------------------------------

(defvar *random-state* t)   ; T = the global kernel PRNG state

;; ---- FILL ---------------------------------------------------------------
;; (fill sequence item &key start end) → sequence (destructively modified)

(defun fill (sequence item &key (start 0) end)
  (let ((stop (or end (length sequence))))
    (dotimes (i (- stop start) sequence)
      (setf (elt sequence (+ start i)) item))))

;; ---- REPLACE ------------------------------------------------------------
;; (replace seq1 seq2 &key start1 end1 start2 end2) → seq1

(defun replace (sequence1 sequence2
                &key (start1 0) end1 (start2 0) end2)
  (let* ((len1  (length sequence1))
         (len2  (length sequence2))
         (stop1 (min (or end1 len1) len1))
         (stop2 (min (or end2 len2) len2))
         (n     (min (- stop1 start1) (- stop2 start2))))
    (dotimes (i n sequence1)
      (setf (elt sequence1 (+ start1 i))
            (elt sequence2 (+ start2 i))))))

;; ---- MISMATCH -----------------------------------------------------------
;; (mismatch seq1 seq2 &key test start1 end1 start2 end2) → index or nil

(defun mismatch (sequence1 sequence2
                 &key (test #'eql) (key #'identity)
                      (start1 0) end1
                      (start2 0) end2)
  (let* ((len1  (length sequence1))
         (len2  (length sequence2))
         (stop1 (min (or end1 len1) len1))
         (stop2 (min (or end2 len2) len2))
         (n     (min (- stop1 start1) (- stop2 start2))))
    (do ((i 0 (1+ i)))
        ((= i n)
         ;; Reached end of shorter range; mismatch if lengths differ
         (if (= (- stop1 start1) (- stop2 start2))
             nil
             (+ start1 n)))
      (unless (funcall test
                       (funcall key (elt sequence1 (+ start1 i)))
                       (funcall key (elt sequence2 (+ start2 i))))
        (return (+ start1 i))))))

;; ---- CONCATENATE (already in prelude but ensure vector and string support) ---
;; We shadow it here with a version that also handles VECTOR and STRING
;; result-types.  Store the old binding in a global to work around bocl's
;; lack of closure capture over LET bindings.
(defvar *%old-concatenate* (symbol-function 'concatenate))
(defun concatenate (result-type &rest sequences)
  (cond
    ((member result-type '(vector simple-vector))
     (let* ((all (apply *%old-concatenate* 'list sequences))
            (v   (make-array (length all))))
       (dotimes (i (length all) v)
         (setf (aref v i) (elt all i)))))
    ((member result-type '(string cl:string simple-string))
     ;; Flatten all sequences to a list of characters, then build a string.
     (let* ((all (apply *%old-concatenate* 'list sequences))
            (len (length all))
            (s   (make-string len)))
       (dotimes (i len s)
         (char-set s i (nth i all)))))
    (t (apply *%old-concatenate* result-type sequences))))

;; ---- POSITION-IF / FIND-IF with :from-end (additional keyword) ---------
;; The prelude versions above don't support :from-end; don't need to yet.

;; ---- Duplicate write-to-string fix:
;; The simple version defined in Slice 10 (which doesn't honour :escape etc.)
;; is superseded by the Slice 13 version. Remove the stale shadow.
;; Nothing to do here — DEFUN rewrites the binding, and the correct version
;; is the one with &rest keys defined in the Slice 13 section above.

;;;; -----------------------------------------------------------------------
;;;; Slice 10 — FORMAT, I/O library
;;;;
;;;; Kernel built-ins added in this slice (C side):
;;;;   write-char           — write a character to a stream
;;;;   kernel-write-string  — write a string region to a stream (positional args)
;;;;   fresh-line           — emit a newline (simplified: always emits one)
;;;;   unread-char          — push a character back onto an input stream
;;;;
;;;; Kernel built-ins already available that we wrap here:
;;;;   terpri  prin1  princ  prin1-to-string
;;;;   kernel-read  kernel-read-char  kernel-peek-char  kernel-read-from-string
;;;;   make-string-output-stream  get-output-stream-string
;;;;   make-string-input-stream
;;;; -----------------------------------------------------------------------

;; ---- Output primitives --------------------------------------------------

(defun write-string (string &optional (stream *standard-output*)
                     &key (start 0) end)
  ;; Write STRING[START..END) to STREAM; return STRING.
  (kernel-write-string string stream start end))

(defun write-line (string &optional (stream *standard-output*)
                   &key (start 0) end)
  ;; Like WRITE-STRING but appends a newline.
  (write-string string stream :start start :end end)
  (terpri stream)
  string)

;; ---- Input wrappers -----------------------------------------------------

(defun read (&optional (stream *standard-input*)
                       (eof-error-p t) eof-value recursive-p)
  (kernel-read stream eof-error-p eof-value recursive-p))

(defun read-char (&optional (stream *standard-input*)
                             (eof-error-p t) eof-value recursive-p)
  (kernel-read-char stream eof-error-p eof-value recursive-p))

(defun peek-char (&optional (peek-type nil)
                             (stream *standard-input*)
                             (eof-error-p t) eof-value recursive-p)
  (kernel-peek-char peek-type stream eof-error-p eof-value recursive-p))

(defun read-from-string (string &optional (eof-error-p t) eof-value
                         &key (start 0) end preserve-whitespace)
  ;; Returns the first Lisp object read from STRING.
  ;; (The second CL return value — the end position — is not yet returned.)
  (kernel-read-from-string string eof-error-p eof-value start end
                            preserve-whitespace))

;; ---- String-stream macros -----------------------------------------------

(defmacro with-output-to-string (var-spec &body body)
  ;; VAR-SPEC is (VAR) — a one-element list holding the stream variable name.
  ;; (Bocl does not support destructuring macro lambda lists, so we accept
  ;; the whole spec as one argument and extract the variable manually.)
  (let ((var (if (consp var-spec) (car var-spec) var-spec)))
    `(let ((,var (make-string-output-stream)))
       ,@body
       (get-output-stream-string ,var))))

(defmacro with-input-from-string (spec &body body)
  ;; SPEC is (VAR STRING) or (VAR STRING :START start :END end).
  ;; Extract the variable and string; start/end default to 0/nil.
  ;; (Bocl does not support destructuring macro lambda lists.)
  (let ((var    (car  spec))
        (string (cadr spec)))
    `(let ((,var (make-string-input-stream ,string nil nil)))
       ,@body)))

;; ---- String coercions ---------------------------------------------------

(defun princ-to-string (obj)
  (with-output-to-string (s) (princ obj s)))

;; write-to-string is defined in the Slice 13 section above (with &rest keys).
;; The simpler version that was here has been removed.

;; ---- READ-LINE ----------------------------------------------------------

(defun read-line (&optional (stream *standard-input*)
                             (eof-error-p t) eof-value recursive-p)
  ;; Return (values string missing-newline-p).
  ;; missing-newline-p is T when the line ended at EOF rather than #\Newline.
  (let ((out       (make-string-output-stream))
        (eof-box   (list nil))        ; unique sentinel — cannot be a character
        (saw-any   nil))
    (block read-line-block
      (do ()
          (nil)
        (let ((c (kernel-read-char stream nil eof-box recursive-p)))
          (cond
            ((eq c eof-box)
             (when (and (not saw-any) eof-error-p)
               (error "READ-LINE: end of file on stream"))
             (return-from read-line-block
               (if saw-any
                   (values (get-output-stream-string out) t)
                   (values eof-value t))))
            ((eql c #\Newline)
             (return-from read-line-block
               (values (get-output-stream-string out) nil)))
            (t
             (setq saw-any t)
             (write-char c out))))))))

;; ---- FORMAT -------------------------------------------------------------
;;
;; The core format runner is implemented in C (kernel-format-run) to avoid
;; allocation pressure from the Lisp interpreter expanding do-loop gensyms
;; on every call.
;;
;; Supported directives:
;;   ~A ~S ~W  PRINC / PRIN1 the next argument
;;   ~D ~B ~O ~X ~nR   integer in decimal / binary / octal / hex / radix-n
;;   ~C     print a character
;;   ~%     newline(s)              ~&    fresh-line(s)
;;   ~~     literal tilde
;;   ~P     plural suffix ("s" or "")
;;   ~nT    emit n spaces (tab — simplified)
;;   ~n*    skip n arguments (default 1)
;;   ~{…~}  iterate over a list argument
;;   ~F ~E ~G   float (simplified: behaves like ~A)
;;
;; Numeric prefix:  ~nX  (decimal integer)  ~vX (from next arg)  ~#X (count)
;; Modifiers:       : and @ (honoured where noted above)
;;
;; Destination:  T  → *standard-output*, returns NIL
;;               NIL → returns a fresh string
;;               stream → writes to stream, returns NIL

(defun format (destination control-string &rest args)
  ;; Delegate to the C kernel runner to avoid GC-pressure from Lisp do-loops.
  (let ((stream (cond ((eq destination t)   *standard-output*)
                      ((eq destination nil) (make-string-output-stream))
                      (t                    destination))))
    (kernel-format-run stream control-string 0 (string-length control-string) args)
    (if (null destination)
        (get-output-stream-string stream)
        nil)))

;; ---- LOOP ------------------------------------------------------------------
;;
;; Supported clauses:
;;   named, with, for/as ... in/on/from/upfrom/downfrom/to/upto/below/
;;     downto/above/by/across/=.../then..., repeat,
;;   while, until, do/doing,
;;   collect/collecting, append/appending, nconc/nconcing,
;;   sum/summing, count/counting, maximize/maximizing, minimize/minimizing,
;;   always, never, thereis, return, initially, finally.
;;
;; Simple (keyword-free) form: (loop body...) → infinite loop, (return x) exits.

(defun %lkw (sym &rest names)
  "True if SYM is a symbol whose STRING-NAME matches any of NAMES."
  (and (symbolp sym)
       (some (lambda (n) (string= (symbol-name sym) n)) names)))

(defun %loop-kw-p (x)
  (%lkw x "NAMED" "WITH" "FOR" "AS" "REPEAT" "WHILE" "UNTIL"
          "WHEN" "UNLESS" "IF" "AND" "ELSE" "END"
          "DO" "DOING"
          "COLLECT" "COLLECTING" "APPEND" "APPENDING"
          "NCONC" "NCONCING" "SUM" "SUMMING" "COUNT" "COUNTING"
          "MAXIMIZE" "MAXIMIZING" "MINIMIZE" "MINIMIZING"
          "ALWAYS" "NEVER" "THEREIS" "RETURN" "INITIALLY" "FINALLY"))

(defun %loop-simple-p (clauses)
  (not (some #'%loop-kw-p clauses)))

(defun %loop-accum-init (kind)
  ;; :collect-body / :append-body / etc. are conditionally updated in body;
  ;; their init is still nil (0 for sum/count).
  (if (member kind '(:sum :count :sum-body :count-body)) 0 nil))

(defun %loop-accum-step (kind var expr)
  ;; Kinds ending in -body are handled inline in the loop body (via WHEN/UNLESS);
  ;; returning nil here suppresses double-generation.
  (case kind
    (:collect  `(setq ,var (cons ,expr ,var)))
    (:append   `(setq ,var (append ,var ,expr)))
    (:nconc    `(setq ,var (nconc ,var ,expr)))
    (:sum      `(incf ,var ,expr))
    (:count    `(when ,expr (incf ,var)))
    (:maximize `(let ((%v ,expr)) (when (or (null ,var) (> %v ,var)) (setq ,var %v))))
    (:minimize `(let ((%v ,expr)) (when (or (null ,var) (< %v ,var)) (setq ,var %v))))
    ;; -body variants: their update is already in body; return nil to skip here.
    ((:collect-body :append-body :nconc-body :sum-body :count-body
      :maximize-body :minimize-body)
     nil)))

(defun %loop-expand (all-clauses)
  (let ((rest     all-clauses)
        (name     nil)
        (letb     nil)    ; ((var init) ...) pushed in parse order; nreversed → LET*
        (tests    nil)    ; pushed; nreversed → top-of-loop exit conditions
        (setups   nil)    ; pushed; nreversed → run after tests, before body
        (body     nil)    ; do/doing forms (append, already in order)
        (steps    nil)    ; pushed; nreversed → bottom-of-loop advances
        (initially nil)
        (finally   nil)
        (accums    nil)   ; (kind result-var expr) — first in = last pushed; nreverse
        (always-f  nil)
        (always-set nil) ; T if an ALWAYS clause was seen (always-f may be NIL)
        (never-f   nil)
        (never-set  nil) ; T if a NEVER clause was seen (never-f may be NIL)
        (thereis-f nil)
        (return-f  nil)
        (return-set nil))
    (flet ((pk  ()     (car rest))
           (pop! ()    (prog1 (car rest) (setq rest (cdr rest))))
           (try! (nm)  (when (%lkw (car rest) nm) (pop!) t))
           (take-body ()
             (let ((acc nil))
               (do () ((or (null rest) (%loop-kw-p (pk))))
                 (push (pop!) acc))
               (nreverse acc))))
      (do () ((null rest))
        (let ((kw (pop!)))
          (cond
            ;; NAMED
            ((%lkw kw "NAMED")
             (setq name (pop!)))
            ;; WITH var [= init] [AND ...]
            ((%lkw kw "WITH")
             (let ((v (pop!)))
               (try! "=")
               (push (list v (if (and rest (not (%loop-kw-p (pk))))
                                 (pop!) nil))
                     letb))
             (do () ((not (try! "AND")))
               (let ((v (pop!)))
                 (try! "=")
                 (push (list v (if (and rest (not (%loop-kw-p (pk))))
                                   (pop!) nil))
                       letb))))
            ;; FOR / AS
            ((%lkw kw "FOR" "AS")
             (let ((v (pop!)))
               (cond
                 ;; FOR v IN list
                 ((%lkw (pk) "IN")
                  (pop!)
                  (let ((lv (gensym "LV-")))
                    (push (list lv (pop!)) letb)   ; lv bound first (LET* order)
                    (push (list v  nil)    letb)
                    (push `(null ,lv)            tests)
                    (push `(setq ,v (car ,lv))   setups)
                    (push `(setq ,lv (cdr ,lv))  steps)))
                 ;; FOR v ON list
                 ((%lkw (pk) "ON")
                  (pop!)
                  (push (list v (pop!)) letb)
                  (push `(null ,v)      tests)
                  (push `(setq ,v (cdr ,v)) steps))
                 ;; FOR v ACROSS vector/string
                 ((%lkw (pk) "ACROSS")
                  (pop!)
                  (let ((sv  (gensym "SV-"))
                        (lnv (gensym "LN-"))
                        (iv  (gensym "IV-")))
                    ;; push sv first → ends up first in LET* after nreverse
                    (push (list sv  (pop!))        letb)
                    (push (list lnv `(length ,sv)) letb)
                    (push (list iv  0)             letb)
                    (push (list v   nil)           letb)
                    (push `(>= ,iv ,lnv)             tests)
                    (push `(setq ,v (elt ,sv ,iv))   setups)
                    (push `(incf ,iv)                steps)))
                 ;; FOR v FROM start [TO/BELOW/DOWNTO/ABOVE end] [BY step]
                 ((%lkw (pk) "FROM" "UPFROM" "DOWNFROM")
                  (let* ((dir   (pop!))
                         (start (pop!))
                         (downp (%lkw dir "DOWNFROM"))
                         (end-f nil)
                         (incl  t)
                         (by-supplied nil)
                         (step  (if downp -1 1)))
                    (push (list v start) letb)
                    (do () ((or (null rest)
                                (not (%lkw (pk) "TO" "UPTO" "BELOW"
                                                "DOWNTO" "ABOVE" "BY"))))
                      (let ((sub (pop!)))
                        (cond
                          ((%lkw sub "TO" "UPTO")  (setq end-f (pop!) incl  t))
                          ((%lkw sub "BELOW")       (setq end-f (pop!) incl  nil))
                          ((%lkw sub "DOWNTO")      (setq end-f (pop!) incl  t  downp t))
                          ((%lkw sub "ABOVE")       (setq end-f (pop!) incl  nil downp t))
                          ((%lkw sub "BY")          (setq step  (pop!) by-supplied t)))))
                    ;; If DOWNTO/ABOVE was found but no BY was given, default step = -1
                    (unless by-supplied
                      (setq step (if downp -1 1)))
                    (when end-f
                      (push (if downp
                                (if incl `(< ,v ,end-f)  `(<= ,v ,end-f))
                                (if incl `(> ,v ,end-f)  `(>= ,v ,end-f)))
                            tests))
                    (push `(incf ,v ,step) steps)))
                 ;; FOR v = init [THEN update]
                 ((%lkw (pk) "=")
                  (pop!)
                  (let ((init (pop!)))
                    (if (%lkw (pk) "THEN")
                        (progn (pop!)
                               (push (list v init) letb)
                               (push `(setq ,v ,(pop!)) steps))
                        (push (list v init) letb))))
                 (t
                  (error "LOOP FOR: unknown sub-clause ~S" (pk))))))
            ;; REPEAT n
            ((%lkw kw "REPEAT")
             (let ((rv (gensym "R-")))
               (push (list rv (pop!)) letb)
               (push `(<= ,rv 0)  tests)
               (push `(decf ,rv)  steps)))
            ;; WHILE cond
            ((%lkw kw "WHILE")
             (push `(not ,(pop!)) tests))
            ;; UNTIL cond
            ((%lkw kw "UNTIL")
             (push (pop!) tests))
            ;; DO / DOING
            ((%lkw kw "DO" "DOING")
             (setq body (append body (take-body))))
            ;; Accumulation clauses
            ((%lkw kw "COLLECT" "COLLECTING")
             (let ((e (pop!)) (iv nil))
               (when (try! "INTO") (setq iv (pop!)))
               (push (list :collect (or iv (gensym "C-")) e) accums)))
            ((%lkw kw "APPEND" "APPENDING")
             (let ((e (pop!)) (iv nil))
               (when (try! "INTO") (setq iv (pop!)))
               (push (list :append (or iv (gensym "A-")) e) accums)))
            ((%lkw kw "NCONC" "NCONCING")
             (let ((e (pop!)) (iv nil))
               (when (try! "INTO") (setq iv (pop!)))
               (push (list :nconc (or iv (gensym "NC-")) e) accums)))
            ((%lkw kw "SUM" "SUMMING")
             (let ((e (pop!)) (iv nil))
               (when (try! "INTO") (setq iv (pop!)))
               (push (list :sum (or iv (gensym "S-")) e) accums)))
            ((%lkw kw "COUNT" "COUNTING")
             (let ((e (pop!)) (iv nil))
               (when (try! "INTO") (setq iv (pop!)))
               (push (list :count (or iv (gensym "K-")) e) accums)))
            ((%lkw kw "MAXIMIZE" "MAXIMIZING")
             (let ((e (pop!)) (iv nil))
               (when (try! "INTO") (setq iv (pop!)))
               (push (list :maximize (or iv (gensym "MX-")) e) accums)))
            ((%lkw kw "MINIMIZE" "MINIMIZING")
             (let ((e (pop!)) (iv nil))
               (when (try! "INTO") (setq iv (pop!)))
               (push (list :minimize (or iv (gensym "MN-")) e) accums)))
            ;; ALWAYS / NEVER / THEREIS
            ((%lkw kw "ALWAYS")   (setq always-f  (pop!) always-set  t))
            ((%lkw kw "NEVER")    (setq never-f   (pop!) never-set   t))
            ((%lkw kw "THEREIS")  (setq thereis-f (pop!)))
            ;; RETURN
            ((%lkw kw "RETURN")
             (setq return-f (pop!) return-set t))
            ;; INITIALLY / FINALLY
            ((%lkw kw "INITIALLY")
             (setq initially (append initially (take-body))))
            ((%lkw kw "FINALLY")
             (setq finally (append finally (take-body))))
            ;; WHEN / UNLESS / IF cond sub-clause [AND sub-clause ...]
            ;; The sub-clause may be: DO, RETURN, COLLECT, APPEND, SUM,
            ;; COUNT, MAXIMIZE, MINIMIZE.  AND chains more sub-clauses
            ;; under the same condition.  ELSE introduces an alternative.
            ((%lkw kw "WHEN" "UNLESS" "IF")
             (let* ((cond-expr (pop!))
                    (negate (%lkw kw "UNLESS")))
               (when (%lkw (pk) "THEN") (pop!)) ; optional THEN after IF
               ;; Inner helper: parse one sub-clause guarded by cond-expr.
               ;; Returns a list of body forms.
               ;; shared-var: when parsing IF ... collect ... ELSE collect ...,
               ;; both sides share the same accumulation variable so the loop
               ;; builds one unified result list.
               (let ((shared-var nil))
                 (labels
                   ((parse-guarded-sub (neg cond reuse-var)
                      ;; REUSE-VAR: if non-nil, the ELSE collect shares this var.
                      (let ((sub (pop!)))
                        (cond
                          ;; RETURN
                          ((%lkw sub "RETURN")
                           (let ((val (pop!)))
                             (list (if neg
                                       `(unless ,cond (return-from ,(or name nil) ,val))
                                       `(when ,cond (return-from ,(or name nil) ,val))))))
                          ;; DO / DOING
                          ((%lkw sub "DO" "DOING")
                           (let ((forms (take-body)))
                             (list (if neg
                                       `(unless ,cond ,@forms)
                                       `(when ,cond ,@forms)))))
                          ;; COLLECT / COLLECTING
                          ((%lkw sub "COLLECT" "COLLECTING")
                           (let* ((e  (pop!))
                                  (iv (when (try! "INTO") (pop!)))
                                  ;; Reuse shared var for ELSE branch so both
                                  ;; halves accumulate into the same list.
                                  (v  (or reuse-var iv (gensym "C-"))))
                             (setq shared-var v) ; remember for possible ELSE
                             (unless reuse-var
                               (push (list :collect-body v e) accums))
                             (list (if neg
                                       `(unless ,cond (setq ,v (cons ,e ,v)))
                                       `(when ,cond (setq ,v (cons ,e ,v)))))))
                          ;; APPEND / APPENDING
                          ((%lkw sub "APPEND" "APPENDING")
                           (let* ((e (pop!))
                                  (iv (when (try! "INTO") (pop!)))
                                  (v  (or reuse-var iv (gensym "A-"))))
                             (setq shared-var v)
                             (unless reuse-var
                               (push (list :append-body v e) accums))
                             (list (if neg
                                       `(unless ,cond (setq ,v (append ,v ,e)))
                                       `(when ,cond (setq ,v (append ,v ,e)))))))
                          ;; SUM / SUMMING
                          ((%lkw sub "SUM" "SUMMING")
                           (let* ((e (pop!))
                                  (iv (when (try! "INTO") (pop!)))
                                  (v  (or reuse-var iv (gensym "S-"))))
                             (setq shared-var v)
                             (unless reuse-var
                               (push (list :sum-body v e) accums))
                             (list (if neg
                                       `(unless ,cond (incf ,v ,e))
                                       `(when ,cond (incf ,v ,e))))))
                          ;; COUNT / COUNTING
                          ((%lkw sub "COUNT" "COUNTING")
                           (let* ((e (pop!))
                                  (iv (when (try! "INTO") (pop!)))
                                  (v  (or reuse-var iv (gensym "K-"))))
                             (setq shared-var v)
                             (unless reuse-var
                               (push (list :count-body v e) accums))
                             (list (if neg
                                       `(unless ,cond (when ,e (incf ,v)))
                                       `(when ,cond (when ,e (incf ,v)))))))
                          ;; MAXIMIZE / MAXIMIZING
                          ((%lkw sub "MAXIMIZE" "MAXIMIZING")
                           (let* ((e (pop!))
                                  (iv (when (try! "INTO") (pop!)))
                                  (v  (or reuse-var iv (gensym "MX-"))))
                             (setq shared-var v)
                             (unless reuse-var
                               (push (list :maximize-body v e) accums))
                             (list (if neg
                                       `(unless ,cond
                                          (let ((%mv ,e))
                                            (when (or (null ,v) (> %mv ,v)) (setq ,v %mv))))
                                       `(when ,cond
                                          (let ((%mv ,e))
                                            (when (or (null ,v) (> %mv ,v)) (setq ,v %mv))))))))
                          ;; MINIMIZE / MINIMIZING
                          ((%lkw sub "MINIMIZE" "MINIMIZING")
                           (let* ((e (pop!))
                                  (iv (when (try! "INTO") (pop!)))
                                  (v  (or reuse-var iv (gensym "MN-"))))
                             (setq shared-var v)
                             (unless reuse-var
                               (push (list :minimize-body v e) accums))
                             (list (if neg
                                       `(unless ,cond
                                          (let ((%nv ,e))
                                            (when (or (null ,v) (< %nv ,v)) (setq ,v %nv))))
                                       `(when ,cond
                                          (let ((%nv ,e))
                                            (when (or (null ,v) (< %nv ,v)) (setq ,v %nv))))))))
                          (t
                           (error "LOOP WHEN/UNLESS: unhandled sub-clause ~S" sub))))))
                   ;; Parse first sub-clause (no reuse-var for the IF/WHEN branch)
                   (setq body (append body (parse-guarded-sub negate cond-expr nil)))
                   ;; AND chains more sub-clauses under the same condition.
                   (do () ((not (%lkw (pk) "AND")))
                     (pop!) ; consume AND
                     (setq body (append body (parse-guarded-sub negate cond-expr nil))))
                   ;; ELSE introduces a negated branch. For COLLECT/SUM etc.,
                   ;; reuse the shared-var so both halves fill one accumulator.
                   (when (%lkw (pk) "ELSE")
                     (pop!) ; consume ELSE
                     (setq body (append body (parse-guarded-sub (not negate) cond-expr shared-var)))
                     (do () ((not (%lkw (pk) "AND")))
                       (pop!)
                       (setq body (append body (parse-guarded-sub (not negate) cond-expr shared-var)))))
                   ;; END terminates a WHEN/UNLESS/IF block.
                   (when (%lkw (pk) "END") (pop!))))))
            ;; AND at top level (outside WHEN) — absorb so it doesn't error
            ((%lkw kw "AND") nil) ; parallel FOR clauses already handle AND
            ;; END at top level — ignore (close of a WHEN block)
            ((%lkw kw "END") nil)
            (t
             (error "LOOP: unknown clause keyword: ~S" kw))))))
    ;; --- code generation ---
    ;; accums: last pushed = first defined; nreverse so primary = first defined
    (setq accums (nreverse accums))
    (let* ((top-tag      (gensym "TOP-"))
           (end-tag      (gensym "END-"))
           (let*-binds   (nreverse letb))
           (accum-binds  (mapcar (lambda (a)
                                   (list (second a) (%loop-accum-init (first a))))
                                 accums))
           (test-forms   (mapcar (lambda (tst) `(when ,tst (go ,end-tag)))
                                 (nreverse tests)))
           (setup-forms  (nreverse setups))
           (step-forms   (nreverse steps))
           ;; accum-forms: skip nil results from -body kinds (their updates
           ;; are already in `body` from the WHEN/UNLESS clause handling).
           (accum-forms  (let ((fs nil))
                           (dolist (a accums (nreverse fs))
                             (let ((f (%loop-accum-step (first a) (second a) (third a))))
                               (when f (push f fs))))))
           (always-chk   (when always-f
                           `((unless ,always-f (return-from ,(or name nil) nil)))))
           (never-chk    (when never-f
                           `((when   ,never-f  (return-from ,(or name nil) nil)))))
           (thereis-chk  (when thereis-f
                           (let ((tv (gensym "TV-")))
                             `((let ((,tv ,thereis-f))
                                 (when ,tv (return-from ,(or name nil) ,tv)))))))
           (primary      (first accums))
           (result-expr  (cond (return-set return-f)
                               (primary
                                (let ((k (first primary)) (rv (second primary)))
                                  ;; both :collect and :collect-body use nreverse
                                  (if (member k '(:collect :collect-body))
                                      `(nreverse ,rv)
                                      rv)))
                               (always-set  t) ; vacuously T when loop never runs
                               (never-set   t) ; vacuously T when loop never runs
                               (t           nil))))
      `(block ,(or name nil)
         (let* (,@let*-binds ,@accum-binds)
           (tagbody
             ,@initially
             ,top-tag
             ,@test-forms
             ,@setup-forms
             ,@always-chk
             ,@never-chk
             ,@thereis-chk
             ,@body
             ,@accum-forms
             ,@step-forms
             (go ,top-tag)
             ,end-tag
             ,@finally
             (return-from ,(or name nil) ,result-expr)))))))

(defmacro loop (&rest clauses)
  (if (%loop-simple-p clauses)
      (let ((top (gensym "LOOP-")))
        `(block nil
           (tagbody
             ,top
             ,@clauses
             (go ,top))))
      (%loop-expand clauses)))

;; -------------------------------------------------------------------- ;;
;;  Slice 15: MAP, MAPL, NBUTLAST, LDIFF, GENTEMP,                      ;;
;;            SUBTYPEP, WITH-OPEN-STREAM.                               ;;
;; -------------------------------------------------------------------- ;;

(defmacro with-open-stream (head &body body)
  ;; (WITH-OPEN-STREAM (var stream) body...) — execute BODY with VAR
  ;; bound to STREAM; always close the stream on exit.
  (let ((var    (first  head))
        (stream (second head)))
    `(let ((,var ,stream))
       (unwind-protect (progn ,@body)
         (when ,var (close ,var))))))

(defun subtypep (type1 type2)
  ;; Basic type-hierarchy query for bocl's type lattice.
  ;; Returns (values subtypep certainp).
  (cond
    ((eq type1 type2)
     (values t t))
    ((eq type2 t)
     (values t t))
    ((eq type1 nil)
     (values t t))
    ((member type2 '(number cl:number))
     (values (not (null (member type1 '(integer fixnum bignum
                                        float ratio rational real)))) t))
    ((member type2 '(real cl:real))
     (values (not (null (member type1 '(integer fixnum bignum
                                        float ratio rational)))) t))
    ((member type2 '(rational cl:rational))
     (values (not (null (member type1 '(integer fixnum bignum ratio)))) t))
    ((member type2 '(integer cl:integer))
     (values (not (null (member type1 '(fixnum bignum)))) t))
    ((member type2 '(sequence cl:sequence))
     (values (not (null (member type1 '(list cons null
                                        string vector bit-vector
                                        simple-vector)))) t))
    ((member type2 '(list cl:list))
     (values (not (null (member type1 '(cons null)))) t))
    (t (values nil nil))))

(defun map (result-type function &rest sequences)
  ;; (MAP result-type function sequence+) — CL:MAP.
  ;; RESULT-TYPE: NIL (side-effects only), LIST, VECTOR, STRING.
  (let* ((len  (reduce #'min (mapcar #'length sequences)))
         (vals (let ((acc nil))
                 (dotimes (i len (nreverse acc))
                   (push (apply function
                                (mapcar (lambda (s) (elt s i)) sequences))
                         acc)))))
    (cond
      ((null result-type) nil)
      ((member result-type '(list cl:list))
       vals)
      ((member result-type '(string cl:string))
       (let ((s (make-string len)))
         (dotimes (i len s)
           (setf (char s i) (nth i vals)))))
      (t   ;; VECTOR, SIMPLE-VECTOR, etc.
       (let ((v (make-array len)))
         (dotimes (i len v)
           (setf (aref v i) (nth i vals))))))))

(defun mapl (function &rest lists)
  ;; (MAPL function list+) — apply FUNCTION to successive sublists;
  ;; return the original first list (like MAPC but sublist-based).
  (let ((result (car lists)))
    (do ((tails lists (mapcar #'cdr tails)))
        ((some #'null tails) result)
      (apply function tails))))

(defun nbutlast (list &optional (n 1))
  ;; Destructive BUTLAST: trim the last N elements from LIST in place.
  (let* ((len  (length list))
         (keep (- len n)))
    (if (<= keep 0)
        nil
        (progn
          (setf (cdr (nthcdr (1- keep) list)) nil)
          list))))

(defun ldiff (list object)
  ;; Return a fresh list of LIST's elements that precede the tail OBJECT.
  ;; If OBJECT is not eq to any tail of LIST, a copy of LIST is returned.
  (let ((acc nil))
    (do ((l list (cdr l)))
        ((or (null l) (eq l object)) (nreverse acc))
      (push (car l) acc))))

(defvar *gentemp-counter* 0)

(defun gentemp (&optional (prefix "T") (package *package*))
  ;; Intern a unique symbol in PACKAGE.  Unlike GENSYM the symbol is
  ;; interned; its name is PREFIX concatenated with a counter.
  (intern (format nil "~A~D" prefix (incf *gentemp-counter*))
          package))

;;;; -----------------------------------------------------------------------
;;;; Slice 16 — coerce sequence types
;;;;
;;;; The C kernel_COERCE handles numeric types only.  We wrap it here with
;;;; a Lisp-level COERCE that handles sequence conversions.
;;;;

;; Store old coerce in a global to avoid closure-capture issues in bocl.
(defvar *%old-coerce* (symbol-function 'coerce))
(defun coerce (object result-type)
  ;; Extend the C kernel's numeric COERCE with sequence conversions.
  ;; NOTE: in bocl, (vectorp string) is T, so string branches MUST precede
  ;; vector branches.
  (cond
    ;; String identity
    ((and (stringp object) (member result-type '(string cl:string simple-string)))
     object)
    ;; String → list
    ((and (stringp object) (member result-type '(list cl:list)))
     (let ((acc nil))
       (dotimes (i (string-length object) (nreverse acc))
         (push (char object i) acc))))
    ;; String → vector
    ((and (stringp object) (member result-type '(vector simple-vector)))
     (let* ((len (string-length object))
            (v   (make-array len)))
       (dotimes (i len v)
         (setf (aref v i) (char object i)))))
    ;; List → vector
    ((and (listp object) (member result-type '(vector simple-vector)))
     (let* ((len (length object))
            (v   (make-array len)))
       (dotimes (i len v)
         (setf (aref v i) (nth i object)))))
    ;; List → string
    ((and (listp object) (member result-type '(string cl:string simple-string)))
     (let* ((len (length object))
            (s   (make-string len)))
       (dotimes (i len s)
         (char-set s i (nth i object)))))
    ;; List identity
    ((and (listp object) (member result-type '(list cl:list)))
     object)
    ;; Vector identity
    ((and (vectorp object) (member result-type '(vector simple-vector)))
     object)
    ;; Vector → list
    ((and (vectorp object) (member result-type '(list cl:list)))
     (let ((acc nil))
       (dotimes (i (vector-length object) (nreverse acc))
         (push (svref object i) acc))))
    ;; Vector → string
    ((and (vectorp object) (member result-type '(string cl:string simple-string)))
     (let* ((len (vector-length object))
            (s   (make-string len)))
       (dotimes (i len s)
         (char-set s i (svref object i)))))
    ;; Fall through to kernel for numeric coercions
    (t (funcall *%old-coerce* object result-type))))

;;;; Slice 17 — generic sequences, -if-not variants, pathname accessors, map-into
;;;;
;;;; Generic sequence functions (every, some, find-if, position-if, count-if,
;;;; remove-if and their siblings) were already updated in-place above to
;;;; handle lists, strings, and vectors.  This section adds:
;;;;   • pathname component accessors (pure Lisp string parsing of POSIX paths)
;;;;   • SORT / STABLE-SORT extended to handle vectors (also updated in-place above)
;;;;   • MAP-INTO
;;;;   • WITH-STANDARD-IO-SYNTAX macro
;;;;

;; -------------------------------------------------------------------- ;;
;;  Pathname component accessors (POSIX path string parsing).           ;;
;; -------------------------------------------------------------------- ;;

(defun %path-dir-end (s)
  ;; Return the index of the last #\/ in S, or -1 if none.
  (let ((last -1))
    (dotimes (i (string-length s) last)
      (when (eql (char s i) #\/) (setq last i)))))

(defun %path-dot-pos (base)
  ;; Position of the last #\. in BASE after position 0, or -1 if none.
  ;; Ignores a leading dot (hidden files like ".hidden").
  (let ((last -1))
    (do ((i 1 (+ i 1)))
        ((>= i (string-length base)) last)
      (when (eql (char base i) #\.) (setq last i)))))

(defun pathname-directory (pathname)
  ;; Return the directory as a string like "/usr/local/" or "".
  (let* ((s (if (stringp pathname) pathname (namestring pathname)))
         (pos (%path-dir-end s)))
    (if (< pos 0) "" (subseq s 0 (+ pos 1)))))

(defun file-namestring (pathname)
  ;; Return the base-name part (name + type, e.g. "foo.lisp").
  (let* ((s (if (stringp pathname) pathname (namestring pathname)))
         (pos (%path-dir-end s)))
    (subseq s (+ pos 1))))

(defun pathname-name (pathname)
  ;; Return the name component (without extension), e.g. "foo".
  (let* ((base (file-namestring pathname))
         (dot  (%path-dot-pos base)))
    (if (< dot 0)
        (if (string= base "") nil base)
        (subseq base 0 dot))))

(defun pathname-type (pathname)
  ;; Return the type (extension) component, e.g. "lisp", or NIL.
  (let* ((base (file-namestring pathname))
         (dot  (%path-dot-pos base)))
    (if (< dot 0)
        nil
        (subseq base (+ dot 1)))))

(defun pathname-host    (p) (setq p nil) nil)
(defun pathname-device  (p) (setq p nil) nil)
(defun pathname-version (p) (setq p nil) nil)

(defun directory-namestring (pathname)
  (pathname-directory pathname))

(defun host-namestring (p)
  (setq p nil) "")

(defun enough-namestring (pathname &optional defaults)
  (setq defaults nil)
  (if (stringp pathname) pathname (namestring pathname)))

(defun merge-pathnames (pathname &optional (defaults "") default-version)
  ;; If PATHNAME is absolute (starts with /), return it unchanged.
  ;; Otherwise prepend DEFAULTS's directory.
  (setq default-version nil)
  (let ((s (if (stringp pathname) pathname (namestring pathname)))
        (d (if (stringp defaults) defaults (namestring defaults))))
    (if (and (> (string-length s) 0) (eql (char s 0) #\/))
        (pathname s)
        (pathname (concatenate 'string (pathname-directory d) s)))))

(defun make-pathname (&key host device directory name type version defaults)
  ;; Build a pathname string from components.
  (setq host nil device nil version nil defaults nil)
  (let* ((dir-s
          (cond ((null directory) "")
                ((stringp directory) directory)
                ((listp directory)
                 ;; CL form: (:absolute "usr" "local") -> "/usr/local/"
                 ;;           (:relative "foo" "bar")  -> "foo/bar/"
                 (let* ((abs (eq (car directory) :absolute))
                        (parts (cdr directory))
                        (result (if abs "/" "")))
                   (dolist (part parts result)
                     (setq result (concatenate 'string result part "/")))))
                (t "")))
         (name-s (or name ""))
         (type-s (if type (concatenate 'string "." type) "")))
    (pathname (concatenate 'string dir-s name-s type-s))))

;; -------------------------------------------------------------------- ;;
;;  MAP-INTO                                                             ;;
;; -------------------------------------------------------------------- ;;

(defun map-into (result-sequence function &rest sequences)
  ;; Write (apply function (nth-element-of-each-sequence i)) into
  ;; result-sequence[i].  Returns result-sequence.  Stops at the
  ;; shortest sequence (including result-sequence itself).
  (let ((n (reduce #'min (cons (length result-sequence)
                               (mapcar #'length sequences)))))
    (dotimes (i n result-sequence)
      (setf (elt result-sequence i)
            (apply function (mapcar (lambda (s) (elt s i)) sequences))))))

;; -------------------------------------------------------------------- ;;
;;  WITH-STANDARD-IO-SYNTAX                                             ;;
;; -------------------------------------------------------------------- ;;

(defmacro with-standard-io-syntax (&body body)
  ;; Bind all *print-* variables to their CL standard initial values.
  `(let ((*print-base*     10)
         (*print-radix*    nil)
         (*print-escape*   t)
         (*print-readably* nil)
         (*print-pretty*   nil)
         (*print-level*    nil)
         (*print-length*   nil)
         (*print-circle*   nil)
         (*print-gensym*   t)
         (*print-array*    t)
         (*print-case*     :upcase))
     ,@body))

;;;; -----------------------------------------------------------------------
;;;; Slice 18 — LOOP WHEN/UNLESS/IF, generic REDUCE, stream extras,
;;;;            DO-SYMBOLS / DO-ALL-SYMBOLS / DO-EXTERNAL-SYMBOLS
;;;;

;; ---- FRESH-LINE (kernel registers it; this is a thin Lisp alias) ----------
;; The kernel built-in FRESH-LINE is already registered; nothing to add here.
;; Verify it's accessible:
;;   (fresh-line) → T (newline was output) or NIL (already at column 0)

;; ---- UNREAD-CHAR (kernel built-in — already registered) -------------------
;; (unread-char char &optional stream) — push CHAR back onto STREAM.
;; The kernel already registers it; nothing more needed here.

;; ---- LISTEN ---------------------------------------------------------------
;; LISTEN checks whether a character is immediately available on STREAM.
;; We implement it via PEEK-CHAR: if PEEK-CHAR returns a character (eof-value
;; NIL, eof-error-p NIL) the stream has input; if it returns NIL we're at EOF
;; or there is no character ready.  For string-input-streams this is exact;
;; for interactive/file streams this may block briefly but is correct.

(defun listen (&optional (stream *standard-input*))
  (not (null (peek-char nil stream nil nil))))

;; ---- READ-CHAR-NO-HANG / CHAR-READY-P ------------------------------------
;; Implement in terms of LISTEN (defined just above).

(defun read-char-no-hang (&optional (stream *standard-input*)
                           (eof-error-p t) eof-value recursive-p)
  ;; Return a character if one is immediately available; NIL otherwise;
  ;; or EOF-VALUE (or error) at end-of-file.
  (if (listen stream)
      (kernel-read-char stream eof-error-p eof-value recursive-p)
      nil))

(defun char-ready-p (&optional (stream *standard-input*))
  ;; T if a character is available on STREAM without blocking.
  (listen stream))

;; ---- DO-SYMBOLS, DO-ALL-SYMBOLS, DO-EXTERNAL-SYMBOLS ---------------------

(defmacro do-symbols (varspec &body body)
  ;; (DO-SYMBOLS (var [package [result]]) body...)
  ;; Bind VAR to each symbol accessible in PACKAGE (default *package*).
  ;; Returns RESULT (default NIL).
  (let ((var     (first  varspec))
        (package (or (second varspec) '*package*))
        (result  (third  varspec)))
    `(dolist (,var (package-symbols ,package) ,result)
       ,@body)))

(defmacro do-external-symbols (varspec &body body)
  ;; (DO-EXTERNAL-SYMBOLS (var [package [result]]) body...)
  ;; Bind VAR to each symbol exported from PACKAGE.
  (let ((var     (first  varspec))
        (package (or (second varspec) '*package*))
        (result  (third  varspec)))
    `(dolist (,var (package-exported-symbols ,package) ,result)
       ,@body)))

(defmacro do-all-symbols (varspec &body body)
  ;; (DO-ALL-SYMBOLS (var [result]) body...)
  ;; Bind VAR to every symbol accessible in every package.
  (let ((var    (first  varspec))
        (result (second varspec))
        (pkg%   (gensym "PKG-")))
    `(dolist (,pkg% (list-all-packages) ,result)
       (do-symbols (,var ,pkg%)
         ,@body))))

;;;; THE END
