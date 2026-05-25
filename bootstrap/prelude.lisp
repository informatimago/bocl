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

(defun reduce (function sequence &key (initial-value nil iv-supplied) from-end)
  ;; FROM-END is honoured by reversing first; not the most efficient but
  ;; minimal-and-correct.
  (let ((seq (if from-end (reverse sequence) sequence)))
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

(defun find-if (predicate sequence &key (key #'identity))
  (dolist (x sequence nil)
    (when (funcall predicate (funcall key x))
      (return-from find-if x))))

(defun find (item sequence &key (test #'eql) (key #'identity))
  (find-if (lambda (x) (funcall test item x)) sequence :key key))

(defun position-if (predicate sequence &key (key #'identity))
  (let ((i 0))
    (dolist (x sequence nil)
      (when (funcall predicate (funcall key x))
        (return-from position-if i))
      (setq i (+ i 1)))))

(defun position (item sequence &key (test #'eql) (key #'identity))
  (position-if (lambda (x) (funcall test item x)) sequence :key key))

(defun count-if (predicate sequence &key (key #'identity))
  (let ((n 0))
    (dolist (x sequence n)
      (when (funcall predicate (funcall key x))
        (setq n (+ n 1))))))

(defun count (item sequence &key (test #'eql) (key #'identity))
  (count-if (lambda (x) (funcall test item x)) sequence :key key))

(defun remove-if (predicate sequence &key (key #'identity))
  (let ((acc nil))
    (dolist (x sequence (nreverse acc))
      (unless (funcall predicate (funcall key x))
        (push x acc)))))

(defun remove (item sequence &key (test #'eql) (key #'identity))
  (remove-if (lambda (x) (funcall test item x)) sequence :key key))

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
  ;; Minimal — list only. STRING / VECTOR specialisations later.
  (let ((rem (nthcdr start sequence))
        (acc nil)
        (count (if end (- end start) -1)))
    (do ((i 0 (+ i 1)))
        ((or (null rem) (and (>= count 0) (= i count))) (nreverse acc))
      (push (car rem) acc)
      (setq rem (cdr rem)))))

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

(defun sort (sequence less &key (key #'identity))
  ;; Destructive-style call signature (kept compatible — but we
  ;; allocate fresh cons cells, so SEQUENCE may or may not be reused).
  ;; KEY is applied via a wrapper that ignores it during comparison.
  (let ((cmp (if (eq key #'identity)
                 less
                 (lambda (x y) (funcall less (funcall key x) (funcall key y))))))
    (labels ((rec (lst)
               (cond ((null lst) nil)
                     ((null (cdr lst)) lst)
                     (t
                      (multiple-value-bind (l r) (%list-split lst)
                        (%merge cmp (rec l) (rec r)))))))
      (rec sequence))))

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

(defun write-to-string (obj)
  (with-output-to-string (s) (prin1 obj s)))

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

;;;; THE END
