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

;; SETF-functions: (defun (setf foo) (val arg...) ...) registers a function here
;; under FOO; the kernel DEFUN / FUNCTION forms and the SETF macro consult it so
;; `(setf (foo a) v)` becomes `(funcall #'(setf foo) v a)` (CLHS 5.1.2.3).
(defparameter *%setf-functions* (make-hash-table :test 'eq))
(defun %register-setf-function (sym fn) (sethash sym *%setf-functions* fn) sym)
(defun %setf-function (sym) (gethash sym *%setf-functions*))

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
                 ((and (consp place) (eq (car place) 'values))
                  ;; (setf (values p1 p2 ...) form): bind FORM's multiple values to
                  ;; temporaries, store each into the corresponding place (a place
                  ;; may itself be (values ...), nesting naturally), and return the
                  ;; stored values.  Extra values are discarded; missing ones are NIL.
                  (let* ((places (cdr place))
                         (temps (mapcar (lambda (p) p (gensym "SETF-VALUES-")) places)))
                    `(multiple-value-bind ,temps ,value
                       ,@(mapcar (lambda (p tv) (list 'setf p tv)) places temps)
                       (values ,@temps))))
                 ((consp place)
                  (let ((expander (gethash (car place) *setf-expanders*)))
                    (cond
                      ;; GETF is place-recursive — its expander re-SETFs its first
                      ;; subform, so binding that subform to a temp would write the
                      ;; temp, not the place.  Keep the (value-first) expansion.
                      ((eq (car place) 'getf)
                       (let ((g (gensym "SETF-V")))
                         `(let ((,g ,value))
                            ,(apply expander (append (cdr place) (list g)))
                            ,g)))
                      ;; A simple place: evaluate the place's subforms left-to-right,
                      ;; THEN the value, THEN store (CLHS 5.1.1.1).  Subforms are
                      ;; bound to temps so they are evaluated exactly once and before
                      ;; the value (the kernel setters return the cons / object, so
                      ;; SETF yields the saved value G itself).
                      (expander
                       (let* ((args  (cdr place))
                              (temps (mapcar (lambda (a) a (gensym "SETF-T-")) args))
                              (g     (gensym "SETF-V")))
                         `(let* (,@(mapcar #'list temps args) (,g ,value))
                            ,(apply expander (append temps (list g)))
                            ,g)))
                      ;; No expander, but FOO has a (SETF FOO) function:
                      ;; (setf (foo a b) v) -> (funcall #'(setf foo) v a b).
                      ((%setf-function (car place))
                       (let ((g (gensym "SETF-V")))
                         `(let ((,g ,value))
                            (funcall (%setf-function ',(car place)) ,g ,@(cdr place))
                            ,g)))
                      (t (error "SETF: no expander for place operator ~S" (car place))))))
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
(define-setf-expander fifth          (cell)         (val) `(rplaca (nthcdr 4 ,cell) ,val))
(define-setf-expander sixth          (cell)         (val) `(rplaca (nthcdr 5 ,cell) ,val))
(define-setf-expander seventh        (cell)         (val) `(rplaca (nthcdr 6 ,cell) ,val))
(define-setf-expander eighth         (cell)         (val) `(rplaca (nthcdr 7 ,cell) ,val))
(define-setf-expander ninth          (cell)         (val) `(rplaca (nthcdr 8 ,cell) ,val))
(define-setf-expander tenth          (cell)         (val) `(rplaca (nthcdr 9 ,cell) ,val))
(define-setf-expander rest           (cell)         (val) `(rplacd ,cell ,val))
(define-setf-expander caar           (cell)         (val) `(rplaca (car ,cell) ,val))
(define-setf-expander cadr           (cell)         (val) `(rplaca (cdr ,cell) ,val))
(define-setf-expander cdar           (cell)         (val) `(rplacd (car ,cell) ,val))
(define-setf-expander cddr           (cell)         (val) `(rplacd (cdr ,cell) ,val))
(define-setf-expander caddr          (cell)         (val) `(rplaca (cddr ,cell) ,val))
(define-setf-expander cadddr         (cell)         (val) `(rplaca (cdddr ,cell) ,val))
;; GETHASH may carry an optional DEFAULT in the place: (setf (gethash k h d) v).
;; SETF appends the new value last, so this expander is called with (k h v) or
;; (k h default v); a supplied default is evaluated (kept in the expansion for its
;; side effects / evaluation order) but not stored.
(sethash 'gethash *setf-expanders*
         (lambda (k h &rest more)
           (let ((val (car (last more))))
             (if (cdr more)
                 `(sethash ,k ,h (progn ,(car more) ,val))
                 `(sethash ,k ,h ,val)))))
(define-setf-expander symbol-value   (s)            (val) `(set-symbol-value ,val ,s))
(define-setf-expander symbol-function (s)           (val) `(set-symbol-function ,val ,s))
(define-setf-expander fdefinition    (name)         (val) `(set-symbol-function ,val ,name))
(define-setf-expander symbol-plist   (s)            (val) `(set-symbol-plist ,val ,s))
(define-setf-expander svref          (vec idx)      (val) `(svref-set ,vec ,idx ,val))
(define-setf-expander bit            (vec idx)      (val) `(bit-set ,vec ,idx ,val))
(define-setf-expander aref           (array idx)    (val) `(aref-set ,val ,array ,idx))
(define-setf-expander char           (str idx)      (val) `(char-set ,str ,idx ,val))
(define-setf-expander schar          (str idx)      (val) `(char-set ,str ,idx ,val))
(define-setf-expander readtable-case (rt)           (val) `(%set-readtable-case ,rt ,val))
(define-setf-expander nth            (n lst)        (val) `(rplaca (nthcdr ,n ,lst) ,val))
;; ELT setter — dispatches on sequence type; uses gensyms to avoid
;; multiple evaluation of sub-forms.
(define-setf-expander elt (seq idx) (val)
  ;; bit-vector-p and stringp before vectorp (subtypes of vector in bocl)
  (let ((s (gensym "S")) (i (gensym "I")) (v (gensym "V")))
    `(let ((,s ,seq) (,i ,idx) (,v ,val))
       (cond ((consp ,s)        (rplaca (nthcdr ,i ,s) ,v))
             ((stringp ,s)      (char-set ,s ,i ,v))  ; kernel_CHAR_SET: (string index char)
             ((bit-vector-p ,s) (bit-set ,s ,i ,v))
             ((vectorp ,s)      (svref-set ,s ,i ,v))
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
  ;; Iterative: a recursive walk has no TCO here, so (nth i list) for a large
  ;; index would recurse i frames deep and overflow the C stack — and since ELT
  ;; on a list calls NTH, that took out FIND/POSITION over long lists.
  (let ((k n) (l list))
    (tagbody
     top
       (when (= k 0) (return-from nth (car l)))
       (setq k (- k 1))
       (setq l (cdr l))
       (go top))))

(defun nthcdr (n list)
  (let ((k n) (l list))
    (tagbody
     top
       (when (= k 0) (return-from nthcdr l))
       (setq k (- k 1))
       (setq l (cdr l))
       (go top))))

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
(defun vectorp   (x)
  ;; A 1-D array is a vector — including a displaced/adjustable one, which bocl
  ;; represents as a %md-array.  (Sequence functions that follow VECTORP with
  ;; SVREF guard %md-array-p first, so they stay correct.)
  (or (typep x 'vector)
      (and (%md-array-p x) (= (length (%md-array-dimensions x)) 1))))
(defun arrayp    (x) (typep x 'array))
(defun packagep  (x) (typep x 'package))
(defun pathnamep (x) (typep x 'pathname))
(defun streamp   (x) (typep x 'stream))
(defun hash-table-p (x) (typep x 'hash-table))
(defun random-state-p (x) (typep x 'random-state))

;; The kernel RANDOM handles only fixnum and float limits.  Extend it to BIGNUM
;; integer limits (the ANSI tests use e.g. (random (* big big))) by assembling a
;; random integer from 30-bit words and reducing modulo LIMIT.  Built with only
;; * / + / FLOOR / MOD, since the kernel's bignum INTEGER-LENGTH/ASH/LOGIOR are
;; incomplete.
(defparameter *%kernel-random* (symbol-function 'random))
(defun random (limit &optional state)
  (declare (ignore state))
  (if (and (integerp limit) (> limit most-positive-fixnum))
      (let ((base (ash 1 30)) (words 0) (m limit) (r 0))
        (do () ((zerop m)) (setq m (floor m base)) (setq words (+ words 1)))
        (dotimes (i (+ words 1))
          (setq r (+ (* r base) (funcall *%kernel-random* base))))
        (mod r limit))
      (funcall *%kernel-random* limit)))

(defun zerop  (n) (= n 0))
(defun plusp  (n) (> n 0))
(defun minusp (n) (< n 0))
(defun oddp   (n) (= (mod n 2) 1))
(defun evenp  (n) (= (mod n 2) 0))

;; -------------------------------------------------------------------- ;;
;;  Iteration macros.                                                    ;;
;; -------------------------------------------------------------------- ;;

(defun %strip-declares (body)
  "Drop leading (declare ...) forms from an implicit-progn BODY (bocl ignores
   declaration content but must not evaluate `declare` as a function)."
  (if (and (consp body) (consp (car body)) (eq (caar body) 'declare))
      (%strip-declares (cdr body))
      body))

(defmacro dolist (head &body body)
  ;; (dolist (var list-form [result-form]) declaration* body...)
  ;; The result-form is evaluated INSIDE the var's binding (var bound to NIL,
  ;; per CLHS), not outside it.
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
              ,@(%strip-declares body)
              (go dolist-loop)
              dolist-done)
           (setq ,var nil)
           ,result)))))

(defmacro dotimes (head &body body)
  ;; (dotimes (var count-form [result-form]) declaration* body...)
  ;; The result-form is evaluated INSIDE the var's binding, with var = count.
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
              ,@(%strip-declares body)
              (setq ,var (+ ,var 1))
              (go dotimes-loop)
              dotimes-done)
           ,result)))))

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
  ;; Like CASE but signals a TYPE-ERROR if no clause matches; no OTHERWISE/T.
  (let ((tmp (gensym "ECASE-"))
        (all-keys (apply #'append
                         (mapcar (lambda (c) (if (listp (car c)) (car c) (list (car c))))
                                 clauses))))
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
         (t (error 'type-error :datum ,tmp
                   :expected-type '(member ,@all-keys)))))))

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
         (t (error 'type-error :datum ,tmp
                   :expected-type '(or ,@(mapcar #'car clauses))))))))

;; -------------------------------------------------------------------- ;;
;;  Multiple values.                                                     ;;
;; -------------------------------------------------------------------- ;;

(defmacro multiple-value-bind (vars value-form &body body)
  ;; (multiple-value-bind (a b c) form body...) — bind a, b, c to the first three
  ;; values of `form`.  Missing values bind to NIL (&optional); EXTRA values are
  ;; discarded by the trailing &rest (CLHS), so more values than vars is not an
  ;; error and an empty VARS list is fine.
  (let ((rest (gensym "MVB-REST-")))
    `(multiple-value-call
         (lambda (&optional ,@vars &rest ,rest)
           (declare (ignore ,rest))
           ,@body)
       ,value-form)))

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
  ;; DO steps in PARALLEL (unlike DO*): every step-form is evaluated using the
  ;; OLD variable values, then all variables are assigned at once.  Emitting a
  ;; sequence of (setq var step) would be DO* semantics and breaks idioms like
  ;; (do ((i .. (1+ i)) (j .. i)) ...) and in-place NREVERSE.  We instead bind a
  ;; fresh temp to each step-form, then assign.
  (let ((loop (gensym "DO-LOOP-"))
        (done (gensym "DO-DONE-")))
    (labels ((triples (vl)            ; -> ((TEMP VAR STEP) ...) for stepped vars
               (if (null vl)
                   nil
                   (let ((v (car vl)))
                     (if (and (consp v) (consp (cddr v)))
                         (cons (list (gensym "DO-STEP-") (car v) (caddr v))
                               (triples (cdr vl)))
                         (triples (cdr vl)))))))
      (let ((trs (triples varlist)))
        `(block nil
           (let ,(mapcar (lambda (v)
                           (if (consp v) (list (car v) (cadr v)) (list v nil)))
                         varlist)
             (tagbody
                ,loop
                (if ,(car endlist) (go ,done))
                ,@(%strip-declares body)
                (let ,(mapcar (lambda (tr) (list (car tr) (caddr tr))) trs)
                  ,@(mapcar (lambda (tr) (list 'setq (cadr tr) (car tr))) trs))
                (go ,loop)
                ,done)
             ,@(cdr endlist)))))))

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
            ,@(%strip-declares body)
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

(defmacro assert (test-form &optional places datum &rest args)
  ;; (assert test [({place}*) [datum {arg}*]]) — CLHS ASSERT.  When DATUM is given
  ;; it denotes the condition to signal ((error datum . args)); otherwise a default
  ;; SIMPLE-ERROR.  A CONTINUE restart allows the caller to retry (PLACES would be
  ;; re-set interactively; bocl just re-tests TEST-FORM).
  (declare (ignore places))
  `(do () (,test-form nil)
     (restart-case
         ,(if datum
              `(error ,datum ,@args)
              `(error 'simple-error
                      :format-control "The assertion ~S failed."
                      :format-arguments (list ',test-form)))
       (continue () :report "Retry the assertion." nil))))

(defmacro check-type (place type &optional type-string)
  ;; (check-type place type [type-string]) — CLHS CHECK-TYPE.  Signals a
  ;; correctable TYPE-ERROR (a SIMPLE-TYPE-ERROR) with a STORE-VALUE restart that
  ;; stores a new value into PLACE and re-tests.
  (declare (ignore type-string))
  (let ((v (gensym "CT-")))
    `(do () ((typep ,place ',type) nil)
       (restart-case
           (error 'simple-type-error
                  :format-control "The value ~S is not of type ~S."
                  :format-arguments (list ,place ',type))
         (store-value (,v)
           :report "Supply a new value."
           :interactive (lambda () (list (eval (read))))
           (setf ,place ,v))))))

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

;; -------------------------------------------------------------------- ;;
;;  Fill-pointer arrays.  The fill pointer is stored INLINE in the        ;;
;;  vector's kernel slots block (an untraced word on Array for VECTOR and ;;
;;  OctetArray for STRING / BIT-VECTOR), reached via the kernel           ;;
;;  primitives %ARRAY-FILL-POINTER / %SET-ARRAY-FILL-POINTER.  Storing it ;;
;;  on the object — rather than an external table — means a fill-pointer  ;;
;;  vector is collected normally once dead, with no pinning.  The array   ;;
;;  itself stays a real bocl string/vector, so AREF/CHAR/SVREF/STRINGP/   ;;
;;  VECTORP keep working natively and correctly *ignore* the fill pointer ;;
;;  (per CLHS aref/char); only the length-sensitive operators — LENGTH,   ;;
;;  the STRING coercion, and the VECTOR-PUSH/POP family — consult it.     ;;
;;  %ARRAY-FILL-POINTER returns NIL for any non-fill-pointer object, so   ;;
;;  %FILL-POINTER-OF is safe to call on arbitrary values (e.g. in LENGTH).;;
;; -------------------------------------------------------------------- ;;

;; Forward placeholder: the real predicate comes from the %md-array DEFSTRUCT
;; (much later in this file).  Several early functions — %FILL-POINTER-OF,
;; STRING, LENGTH — must consult it, but no %md-array exists until MAKE-ARRAY
;; runs (well after the defstruct), so NIL is the correct answer during load.
(defun %md-array-p (x) x nil)

;; A complex array (adjustable / displaced / multi-dimensional) is represented
;; by a %md-array struct and keeps its fill pointer in a struct slot; a simple
;; vector/string/bit-vector keeps it in the kernel object field.
(defun %fill-pointer-of (array)
  "The current fill pointer of ARRAY (an integer), or NIL if it has none."
  (if (%md-array-p array) (%md-array-fill-pointer array) (%array-fill-pointer array)))

(defun %set-fill-pointer (array fp)
  "Set ARRAY's fill pointer to FP (registers ARRAY as fill-pointered)."
  (if (%md-array-p array) (setf (%md-array-fill-pointer array) fp) (%set-array-fill-pointer array fp)))

(defun length (seq)
  (cond ((null seq)        0)
        ((consp seq)       (list-length seq))
        ((%fill-pointer-of seq))            ; fill-pointer array: active length
        ((%md-array-p seq)                  ; adjustable/displaced 1-D vector
         (let ((d (%md-array-dimensions seq)))
           (if (and d (null (cdr d))) (car d)
               (error "LENGTH: ~S is not a sequence" seq))))
        ((stringp seq)     (string-length seq))
        ((vectorp seq)     (vector-length seq))
        ((bit-vector-p seq)(bit-vector-length seq))
        (t (error "LENGTH: not a sequence"))))

(defun elt (seq i)
  ;; NOTE: bit-vector-p and stringp must come BEFORE vectorp because
  ;; bit-vectors and strings are subtypes of vector in bocl.
  (cond ((consp seq)        (nth i seq))
        ((%md-array-p seq)  (row-major-aref seq i)) ; 1-D adjustable/displaced vector
        ((stringp seq)      (char seq i))
        ((bit-vector-p seq) (bit seq i))
        ((vectorp seq)      (svref seq i))
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

(defun %xtest (test test-not)
  "Collapse :TEST / :TEST-NOT into one 2-arg predicate (TEST-NOT wins)."
  (cond (test-not (lambda (a b) (not (funcall test-not a b))))
        (test test)
        (t #'eql)))

(defun member (item list &key (test #'eql) test-not (key #'identity))
  "Return the first cons in LIST whose CAR satisfies TEST with ITEM, or NIL."
  (unless key (setq key #'identity))     ; :KEY NIL means identity
  (setq test (%xtest test test-not))
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

(defun %cons-length (list)
  "Number of cons cells in LIST (works on proper AND dotted lists)."
  (let ((n 0))
    (do ((x list (cdr x))) ((atom x) n) (setq n (1+ n)))))

(defun butlast (list &optional (n 1))
  ;; LIST minus its last N cons cells (works on dotted lists too — count conses,
  ;; not elements, since LENGTH rejects an improper list).
  (let* ((len (%cons-length list))
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
    ;; BIT-VECTOR before VECTORP: a bit-vector is also a VECTORP, but SVREF on it
    ;; reads garbage — copy bits with BIT/BIT-SET into a fresh bit-vector.
    ((bit-vector-p sequence)
     (let* ((len (or end (bit-vector-length sequence)))
            (count (- len start))
            (out (make-bit-vector count 0)))
       (dotimes (i count out)
         (bit-set out i (bit sequence (+ start i))))))
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

;; (SETF (SUBSEQ seq start [end]) new): replace seq[start:end] with elements of
;; NEW, copying the shorter of the subsequence length and (length NEW) (CLHS).
(defun %set-subseq (sequence start end new)
  (let* ((slen (length sequence))
         (stop (min (or end slen) slen))
         (n    (min (- stop start) (length new))))
    (dotimes (i n new)
      (setf (elt sequence (+ start i)) (elt new i)))))

(sethash 'subseq *setf-expanders*
  (lambda (sequence start &rest end-and-val)
    (let ((val (car (last end-and-val)))
          (end (if (cdr end-and-val) (car end-and-val) nil)))
      `(%set-subseq ,sequence ,start ,end ,val))))

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
               ((or (stringp seq) (vectorp seq) (bit-vector-p seq) (%md-array-p seq))
                (dotimes (i (length seq))
                  (push (elt seq i) acc)))
               (t (error "CONCATENATE: not a sequence"))))
       (nreverse acc)))
    (t (error "CONCATENATE: only :LIST result-type is supported"))))

;; -------------------------------------------------------------------- ;;
;;  Sort — stable merge sort whose loop runs in C (%KERNEL-SORT!).       ;;
;; -------------------------------------------------------------------- ;;
;;
;; The merge loop lives in the %KERNEL-SORT! kernel primitive (sources/bocl.c),
;; which reads the list/vector into a pinned scratch vector, bottom-up merge
;; sorts it in C — calling back into Lisp only for the comparator — and writes
;; the result back destructively.  A Lisp-level SORT is bounded by the
;; interpreter's ~150 us-per-op cost; doing the element moves in C and paying it
;; once per *comparison* is what makes SORT fast (an array merge sort written in
;; Lisp was ~5x slower than the old list merge sort, because bocl AREF/MAKE-ARRAY
;; cost far more than CONS car/cdr).  The primitive is stable, so STABLE-SORT is
;; the same call.

(defun sort (sequence less &key (key #'identity))
  "Destructively sort SEQUENCE (a list or vector) by the strict comparator LESS
applied to the KEYs of elements.  Stable."
  (%kernel-sort! sequence
                 (if (eq key #'identity)
                     less
                     (lambda (x y) (funcall less (funcall key x) (funcall key y))))))

(defun stable-sort (sequence less &key (key #'identity))
  ;; %KERNEL-SORT! is a stable merge sort.
  (sort sequence less :key key))

;; -------------------------------------------------------------------- ;;
;;  Set operations (on lists).                                           ;;
;; -------------------------------------------------------------------- ;;

(defun adjoin (item list &key (test #'eql) test-not (key #'identity))
  ;; Per CLHS, the KEY is applied to ITEM as well as to each list element when
  ;; testing membership.  Self-contained so it needs neither FIND's :TEST-NOT
  ;; support nor a non-NIL KEY.
  (let* ((kfn (or key #'identity))
         (ki  (funcall kfn item)))
    (if (do ((l list (cdr l)))
            ((null l) nil)
          (let ((ke (funcall kfn (car l))))
            (when (if test-not
                      (not (funcall test-not ki ke))
                      (funcall test ki ke))
              (return t))))
        list
        (cons item list))))

(defun %std-hash-test (test)
  "The HASH-TABLE test name (EQ/EQL/EQUAL/EQUALP) equivalent to the standard test
function TEST, or NIL if TEST is not one of those — so a set operation can use an
O(n+m) hash table instead of an O(n*m) MEMBER scan."
  (cond ((eq test #'eq)     'eq)
        ((eq test #'eql)    'eql)
        ((eq test #'equal)  'equal)
        ((eq test #'equalp) 'equalp)
        (t nil)))

(defun %set-op-hash (list test key)
  "A hash table (under TEST's hashable equivalent) of (KEY x) for x in LIST, or
NIL when TEST has no hashable equivalent."
  (let ((ht-test (%std-hash-test test)))
    (when ht-test
      (let ((h (make-hash-table :test ht-test)))
        (dolist (x list h) (setf (gethash (funcall key x) h) t))))))

(defun union (list1 list2 &key (test #'eql) (key #'identity))
  ;; Result contains every element of list2, plus every element of list1 not
  ;; already present (by key + test).
  (let ((h (%set-op-hash list2 test key)))
    (if h
        (let ((acc (reverse list2)))
          (dolist (x list1 (nreverse acc))
            (let ((k (funcall key x)))
              (unless (gethash k h) (setf (gethash k h) t) (push x acc)))))
        (let ((acc (copy-list list2)))
          (dolist (x list1 acc)
            (unless (find (funcall key x) acc :test test :key key)
              (setq acc (cons x acc))))))))

(defun intersection (list1 list2 &key (test #'eql) (key #'identity))
  (let ((h (%set-op-hash list2 test key)))
    (if h
        (let ((acc nil))
          (dolist (x list1 (nreverse acc))
            (when (gethash (funcall key x) h) (push x acc))))
        (remove-if (lambda (x) (not (find (funcall key x) list2 :test test :key key)))
                   list1))))

(defun set-difference (list1 list2 &key (test #'eql) test-not (key #'identity))
  (let ((h (and (null test-not) (%set-op-hash list2 test key))))
    (if h
        (let ((acc nil))
          (dolist (x list1 (nreverse acc))
            (unless (gethash (funcall key x) h) (push x acc))))
        (let ((tf (%xtest test test-not)) (acc nil))
          (dolist (x list1 (nreverse acc))
            (unless (member (funcall key x) list2 :test tf :key key) (push x acc)))))))

(defun subsetp (list1 list2 &key (test #'eql) (key #'identity))
  "True if every element of LIST1 appears (under TEST/KEY) in LIST2."
  (let ((h (%set-op-hash list2 test key)))
    (if h
        (dolist (x list1 t)
          (unless (gethash (funcall key x) h) (return nil)))
        (dolist (x list1 t)
          (unless (find (funcall key x) list2 :test test :key key)
            (return nil))))))

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

(defun assoc (item list &key (test #'eql test-p) test-not (key #'identity))
  "Return the first pair in LIST whose CAR satisfies TEST (or, if :TEST-NOT is
   supplied, fails it) with ITEM."
  (when (and test-p test-not) (error "ASSOC: both :TEST and :TEST-NOT supplied"))
  (dolist (pair list nil)
    (when (and (consp pair)
               (if test-not
                   (not (funcall test-not item (funcall key (car pair))))
                   (funcall test item (funcall key (car pair)))))
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

(defun rassoc (item list &key (test #'eql test-p) test-not (key #'identity))
  "Return the first pair in LIST whose CDR satisfies TEST (or, if :TEST-NOT is
   supplied, fails it) with ITEM."
  (when (and test-p test-not) (error "RASSOC: both :TEST and :TEST-NOT supplied"))
  (dolist (pair list nil)
    (when (and (consp pair)
               (if test-not
                   (not (funcall test-not item (funcall key (cdr pair))))
                   (funcall test item (funcall key (cdr pair)))))
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

(defun subst (new old tree &key (test #'eql) test-not (key #'identity))
  "Copy TREE substituting NEW for every subtree EQL to OLD."
  (setq test (%xtest test test-not))
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

;; Destructive substitution: like SUBST/SUBST-IF/... but modify TREE in place
;; (RPLACA/RPLACD) and return the same object when the top isn't replaced.
(defun nsubst (new old tree &key (test #'eql) test-not (key #'identity))
  "Like SUBST but destructive."
  (setq test (%xtest test test-not))
  (cond ((funcall test old (funcall key tree)) new)
        ((atom tree) tree)
        (t (rplaca tree (nsubst new old (car tree) :test test :key key))
           (rplacd tree (nsubst new old (cdr tree) :test test :key key))
           tree)))

(defun nsubst-if (new predicate tree &key (key #'identity))
  "Like SUBST-IF but destructive."
  (cond ((funcall predicate (funcall key tree)) new)
        ((atom tree) tree)
        (t (rplaca tree (nsubst-if new predicate (car tree) :key key))
           (rplacd tree (nsubst-if new predicate (cdr tree) :key key))
           tree)))

(defun nsubst-if-not (new predicate tree &key (key #'identity))
  "Like SUBST-IF-NOT but destructive."
  (nsubst-if new (lambda (x) (not (funcall predicate x))) tree :key key))

(defun sublis (alist tree &key (test #'eql) test-not (key #'identity))
  "Copy TREE substituting, for every subtree found as a key (under TEST/KEY) in
ALIST, the corresponding value.  Subtrees with no substitution keep their
identity."
  (setq test (%xtest test test-not))
  (let ((pair (assoc (funcall key tree) alist :test test)))
    (cond (pair (cdr pair))
          ((atom tree) tree)
          (t (let ((new-car (sublis alist (car tree) :test test :key key))
                   (new-cdr (sublis alist (cdr tree) :test test :key key)))
               (if (and (eq new-car (car tree)) (eq new-cdr (cdr tree)))
                   tree
                   (cons new-car new-cdr)))))))

(defun nsublis (alist tree &key (test #'eql) test-not (key #'identity))
  "Like SUBLIS but destructive."
  (setq test (%xtest test test-not))
  (let ((pair (assoc (funcall key tree) alist :test test)))
    (cond (pair (cdr pair))
          ((atom tree) tree)
          (t (rplaca tree (nsublis alist (car tree) :test test :key key))
             (rplacd tree (nsublis alist (cdr tree) :test test :key key))
             tree))))

(defun revappend (list tail)
  "(append (reverse LIST) TAIL); LIST is not modified, TAIL is shared."
  (nconc (reverse list) tail))

(defun nreconc (list tail)
  "(nconc (nreverse LIST) TAIL) — destructive REVAPPEND."
  (nconc (nreverse list) tail))

;; (SETF (GETF place indicator) value): update the indicator's value in the plist
;; held by PLACE, extending it (and updating PLACE) if the indicator is absent.
(defun %put-getf (plist indicator value)
  (labels ((scan (tail)
             (cond ((null tail) (list* indicator value plist))
                   ((eq (car tail) indicator) (setf (cadr tail) value) plist)
                   (t (scan (cddr tail))))))
    (scan plist)))

;; (SETF (GETF place indicator [default]) value) — DEFAULT is accepted (CL allows
;; it in the place) and ignored.  Registered directly because the value is the
;; LAST argument and an optional default sits between indicator and value.
(sethash 'getf *setf-expanders*
  (lambda (place indicator &rest default-and-val)
    (let ((val (car (last default-and-val))))
      `(setf ,place (%put-getf ,place ,indicator ,val)))))

(defun last (list &optional (n 1))
  "Return the last N cons cells of LIST (default 1).  Works on dotted lists."
  (unless (listp list)
    (error 'type-error :datum list :expected-type 'list))
  (let ((lead list))
    (dotimes (i n) (when (consp lead) (setq lead (cdr lead))))
    (let ((trail list))
      (loop (if (consp lead)
                (setq lead (cdr lead) trail (cdr trail))
                (return trail))))))

(defun tree-equal (tree1 tree2 &key (test #'eql) test-not)
  "T if TREE1 and TREE2 have the same shape and leaves (by TEST)."
  (setq test (%xtest test test-not))
  (cond ((and (atom tree1) (atom tree2))
         (funcall test tree1 tree2))
        ((or  (atom tree1) (atom tree2)) nil)
        (t (and (tree-equal (car tree1) (car tree2) :test test)
                (tree-equal (cdr tree1) (cdr tree2) :test test)))))

;; -------------------------------------------------------------------- ;;
;;  Property lists (slice 12).                                           ;;
;; -------------------------------------------------------------------- ;;

;; SYMBOL-VALUE of the self-valued constants NIL, T, and keywords returns the
;; symbol itself — the kernel value cell of NIL/T holds the unbound marker.
(let ((kernel-symbol-value (symbol-function 'symbol-value)))
  (set-symbol-function
   (lambda (symbol)
     (cond ((null symbol)      nil)
           ((eq symbol t)      t)
           ((keywordp symbol)  symbol)
           (t (funcall kernel-symbol-value symbol))))
   'symbol-value))

(defun get (symbol indicator &optional default)
  "Return the value for INDICATOR on SYMBOL's property list, or DEFAULT."
  (getf (symbol-plist symbol) indicator default))

;; REMF must be a place-modifying macro: removing the first pair (or the sole
;; pair, leaving NIL) requires updating PLACE — which the kernel REMF function
;; cannot do.  %REMF returns (new-plist . removed-p); the macro stores the new
;; plist back into PLACE and yields the boolean.
(defun %remf (plist indicator)
  (cond ((null plist) (cons nil nil))
        ((eql (car plist) indicator) (cons (cddr plist) t))
        (t (do ((prev plist (cddr prev)))
               ((null (cddr prev)) (cons plist nil))
             (when (eql (caddr prev) indicator)
               (rplacd (cdr prev) (cddddr prev))
               (return (cons plist t)))))))

(defmacro remf (place indicator)
  (let ((g (gensym "REMF-")))
    `(let ((,g (%remf ,place ,indicator)))
       (setf ,place (car ,g))
       (cdr ,g))))

(defun remprop (symbol indicator)
  "Remove the FIRST INDICATOR/value pair from SYMBOL's property list (CLHS:
   only the first such property is removed).  Returns T if present, else NIL."
  ;; Removing only the first occurrence matters when the plist has duplicate
  ;; indicators: (remprop sym 'a) on (a 1 b 2 a 9) must leave (b 2 a 9), so a
  ;; subsequent (get sym 'a) sees 9.  The head case updates the symbol's plist
  ;; pointer; an interior pair is spliced out in place.
  (let ((pl (symbol-plist symbol)))
    (cond ((null pl) nil)
          ((eq (car pl) indicator)
           (set-symbol-plist (cddr pl) symbol) t)
          (t (do ((prev pl (cddr prev)))
                 ((null (cddr prev)) nil)
               (when (eq (caddr prev) indicator)
                 (rplacd (cdr prev) (cddddr prev))
                 (return t)))))))

;; (SETF (GET sym ind) val) — bocl simple-expander style.
;; Note: kernel SET-SYMBOL-PLIST arg order is (new-plist symbol).
(define-setf-expander get (symbol indicator &rest more) ()
  ;; (setf (get sym ind [default]) val).  MORE is (val) or (default val): CLHS
  ;; says the optional DEFAULT place argument is still evaluated (for effect) but
  ;; otherwise ignored by the store.  Value is always the last subform.
  (let* ((has-default (cdr more))
         (value       (if has-default (cadr more) (car more)))
         (default     (if has-default (car more))))
    (if has-default
        `(progn ,default
                (set-symbol-plist (putf (symbol-plist ,symbol) ,indicator ,value) ,symbol))
        `(set-symbol-plist (putf (symbol-plist ,symbol) ,indicator ,value) ,symbol))))

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
  ;; Canonical boolean: DIGIT-CHAR-P returns the digit's weight, which must not
  ;; leak out (e.g. as a REMOVE-DUPLICATES :key value).
  (if (or (alpha-char-p char) (digit-char-p char)) t nil))

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
  (cond ((and (stringp designator) (%fill-pointer-of designator))
         ;; Char array with a fill pointer: its string value is the active
         ;; prefix only, so string functions see the correct length.
         (subseq designator 0 (%fill-pointer-of designator)))
        ((stringp designator) designator)
        ;; A character %md-array (an adjustable or displaced string): build a
        ;; fresh simple string from its active elements in row-major order.
        ((and (%md-array-p designator)
              (eq (%md-array-element-type designator) 'character))
         (let* ((n (or (%fill-pointer-of designator) (array-total-size designator)))
                (s (make-string n)))
           (dotimes (i n s) (char-set s i (row-major-aref designator i)))))
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
  ;; Return < 0 / 0 / > 0 (sign only) like memcmp; honour CASE-FOLD when true.
  ;; The scan runs in C via the %STRING-CMP2 kernel primitive rather than a
  ;; CHAR-by-CHAR interpreter loop (each CHAR is a ~15us funcall), so every
  ;; STRING</STRING>/... and any SORT with a string comparator is fast.
  (nth-value 1 (%string-cmp2 (string a) (string b) case-fold 0 nil 0 nil)))

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

;; The kernel LOGAND/LOGIOR/LOGXOR take exactly two arguments; CL makes them
;; variadic (with identities -1 / 0 / 0).  Wrap them to fold pairwise.
(defvar *%logand2* (symbol-function 'logand))
(defvar *%logior2* (symbol-function 'logior))
(defvar *%logxor2* (symbol-function 'logxor))
(defun logand (&rest integers)
  (if (null integers) -1
      (let ((acc (car integers)))
        (dolist (x (cdr integers) acc) (setq acc (funcall *%logand2* acc x))))))
(defun logior (&rest integers)
  (if (null integers) 0
      (let ((acc (car integers)))
        (dolist (x (cdr integers) acc) (setq acc (funcall *%logior2* acc x))))))
(defun logxor (&rest integers)
  (if (null integers) 0
      (let ((acc (car integers)))
        (dolist (x (cdr integers) acc) (setq acc (funcall *%logxor2* acc x))))))

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
  ;; CLHS WITH-SIMPLE-RESTART: if the restart is invoked, return two values NIL and T.
  (let ((name   (if (consp restart-spec) (car restart-spec) restart-spec))
        (report (if (consp restart-spec) (cadr restart-spec) nil)))
    `(restart-case (progn ,@body)
       (,name () ,@(when report `(:report ,report)) (values nil t)))))

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
(defvar *print-lines*        nil)   ; pretty-printer line cap (CLHS: default NIL)
(defvar *print-right-margin* nil)   ; pretty-printer right margin (default NIL)
(defvar *print-miser-width*  nil)   ; pretty-printer miser-mode width (default NIL)

;; ---- *read-* control variables -----------------------------------------

(defvar *read-base*                 10)
(defvar *read-default-float-format* 'single-float)
(defvar *read-eval*                 t)
;; *QUERY-IO* — the bidirectional stream for interactive queries (Y-OR-N-P etc.);
;; default it to the terminal, like the other interaction streams.
(defvar *query-io* *terminal-io*)
(defvar *read-suppress*             nil)

;; ---- WRITE — full CL printer entry point (simplified) ------------------
;;
;; Bocl's printer currently ignores *print-level* / *print-length* / etc.,
;; so WRITE is just a thin dispatch on :escape.

(defun write (object &key (stream    *standard-output*)
                          (escape    *print-escape*)
                          (readably  *print-readably*)
                          (radix     *print-radix*)
                          (base      *print-base*)
                          (circle    *print-circle*)
                          (pretty    *print-pretty*)
                          (level     *print-level*)
                          (length    *print-length*)
                          (case      *print-case*)
                          (gensym    *print-gensym*)
                          (array     *print-array*))
  ;; Bind the *PRINT-…* variables to the keyword arguments for the dynamic extent
  ;; of the output, so the (C) printer honours them — :CIRCLE / :LENGTH / :LEVEL
  ;; etc. now take effect (the printer reads the specials).
  (let ((*print-escape*   escape)   (*print-readably* readably)
        (*print-radix*    radix)    (*print-base*     base)
        (*print-circle*   circle)   (*print-pretty*   pretty)
        (*print-level*    level)    (*print-length*   length)
        (*print-case*     case)     (*print-gensym*   gensym)
        (*print-array*    array))
    (if (or readably escape)
        (prin1 object stream)
        (princ object stream)))
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

;; SETF expanders for every cXr accessor (cAR/cDR access paths of length 2-4):
;; (setf (c<L><rest>r cell) v) == (rpla<L> (c<rest>r cell) v).  All 28 written as
;; explicit top-level SETHASH forms (the proven pattern, cf. the GETF expander
;; above) — string-capable CONCATENATE is defined later in the prelude, so a
;; name-generating loop cannot run here.  Each closure rewrites the place into a
;; destructive RPLACA/RPLACD on the appropriate inner accessor.
(sethash 'caar   *setf-expanders* (lambda (c v) (list 'rplaca (list 'car   c) v)))
(sethash 'cadr   *setf-expanders* (lambda (c v) (list 'rplaca (list 'cdr   c) v)))
(sethash 'cdar   *setf-expanders* (lambda (c v) (list 'rplacd (list 'car   c) v)))
(sethash 'cddr   *setf-expanders* (lambda (c v) (list 'rplacd (list 'cdr   c) v)))
(sethash 'caaar  *setf-expanders* (lambda (c v) (list 'rplaca (list 'caar  c) v)))
(sethash 'caadr  *setf-expanders* (lambda (c v) (list 'rplaca (list 'cadr  c) v)))
(sethash 'cadar  *setf-expanders* (lambda (c v) (list 'rplaca (list 'cdar  c) v)))
(sethash 'caddr  *setf-expanders* (lambda (c v) (list 'rplaca (list 'cddr  c) v)))
(sethash 'cdaar  *setf-expanders* (lambda (c v) (list 'rplacd (list 'caar  c) v)))
(sethash 'cdadr  *setf-expanders* (lambda (c v) (list 'rplacd (list 'cadr  c) v)))
(sethash 'cddar  *setf-expanders* (lambda (c v) (list 'rplacd (list 'cdar  c) v)))
(sethash 'cdddr  *setf-expanders* (lambda (c v) (list 'rplacd (list 'cddr  c) v)))
(sethash 'caaaar *setf-expanders* (lambda (c v) (list 'rplaca (list 'caaar c) v)))
(sethash 'caaadr *setf-expanders* (lambda (c v) (list 'rplaca (list 'caadr c) v)))
(sethash 'caadar *setf-expanders* (lambda (c v) (list 'rplaca (list 'cadar c) v)))
(sethash 'caaddr *setf-expanders* (lambda (c v) (list 'rplaca (list 'caddr c) v)))
(sethash 'cadaar *setf-expanders* (lambda (c v) (list 'rplaca (list 'cdaar c) v)))
(sethash 'cadadr *setf-expanders* (lambda (c v) (list 'rplaca (list 'cdadr c) v)))
(sethash 'caddar *setf-expanders* (lambda (c v) (list 'rplaca (list 'cddar c) v)))
(sethash 'cadddr *setf-expanders* (lambda (c v) (list 'rplaca (list 'cdddr c) v)))
(sethash 'cdaaar *setf-expanders* (lambda (c v) (list 'rplacd (list 'caaar c) v)))
(sethash 'cdaadr *setf-expanders* (lambda (c v) (list 'rplacd (list 'caadr c) v)))
(sethash 'cdadar *setf-expanders* (lambda (c v) (list 'rplacd (list 'cadar c) v)))
(sethash 'cdaddr *setf-expanders* (lambda (c v) (list 'rplacd (list 'caddr c) v)))
(sethash 'cddaar *setf-expanders* (lambda (c v) (list 'rplacd (list 'cdaar c) v)))
(sethash 'cddadr *setf-expanders* (lambda (c v) (list 'rplacd (list 'cdadr c) v)))
(sethash 'cdddar *setf-expanders* (lambda (c v) (list 'rplacd (list 'cddar c) v)))
(sethash 'cddddr *setf-expanders* (lambda (c v) (list 'rplacd (list 'cdddr c) v)))


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

;; The kernel NREVERSE is list-only (returns NIL on a vector/string).  Make it
;; generic: lists go to the kernel; vectors/strings/bit-vectors are reversed in
;; place by swapping ends.
(defvar *%kernel-nreverse* (symbol-function 'nreverse))
(defun nreverse (sequence)
  (if (listp sequence)
      (funcall *%kernel-nreverse* sequence)
      (let ((len (length sequence)))
        (dotimes (i (truncate len 2) sequence)
          (let ((tmp (elt sequence i)) (j (- len 1 i)))
            (setf (elt sequence i) (elt sequence j))
            (setf (elt sequence j) tmp))))))

(defun replace (sequence1 sequence2
                &key (start1 0) end1 (start2 0) end2)
  "Destructively replace SEQUENCE1[START1,END1) with SEQUENCE2[START2,END2),
copying min of the two lengths.  Snapshot the source first so an overlapping
self-replace (SEQUENCE1 eq SEQUENCE2) copies as if through a temporary (CLHS)."
  (let* ((len1  (length sequence1))
         (len2  (length sequence2))
         (stop1 (min (or end1 len1) len1))
         (stop2 (min (or end2 len2) len2))
         (n     (max 0 (min (- stop1 start1) (- stop2 start2))))
         (tmp   (make-array n)))
    (dotimes (i n) (setf (aref tmp i) (elt sequence2 (+ start2 i))))
    (dotimes (i n sequence1) (setf (elt sequence1 (+ start1 i)) (aref tmp i)))))

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
;; Map a sequence type-specifier (atom OR compound like (VECTOR T 3),
;; (STRING 5), (ARRAY CHARACTER (3))) to a base kind.  MAKE-SEQUENCE, MAP,
;; CONCATENATE and COERCE all dispatch on this so they accept the full range of
;; sequence type specifiers, not just the bare symbols.
(defun %seq-type-kind (type)
  "Base kind of a sequence type-spec, or NIL if TYPE is not a sequence type."
  (let ((head (if (consp type) (car type) type))
        (elt  (and (consp type) (cdr type) (cadr type))))
    (cond ((member head '(list cons null)) :list)
          ((member head '(string simple-string base-string simple-base-string)) :string)
          ((member head '(bit-vector simple-bit-vector)) :bit-vector)
          ((member head '(vector simple-vector array simple-array sequence))
           (cond ((member elt '(character base-char standard-char)) :string)
                 ((eq elt 'bit) :bit-vector)
                 (t :vector)))
          (t nil))))   ; not a sequence type (e.g. SYMBOL, HASH-TABLE) -> caller errors

(defun concatenate (result-type &rest sequences)
  (let ((kind (%seq-type-kind result-type)))
    (cond
      ((eq kind :string)
       (let* ((all (apply *%old-concatenate* 'list sequences))
              (len (length all)) (s (make-string len)))
         (dotimes (i len s) (char-set s i (nth i all)))))
      ((eq kind :bit-vector)
       (let* ((all (apply *%old-concatenate* 'list sequences))
              (len (length all)) (v (make-bit-vector len 0)))
         (dotimes (i len v) (bit-set v i (nth i all)))))
      ((eq kind :list)
       (apply *%old-concatenate* 'list sequences))
      ((eq kind :vector)
       (let* ((all (apply *%old-concatenate* 'list sequences))
              (v   (make-array (length all))))
         (dotimes (i (length all) v) (setf (aref v i) (elt all i)))))
      (t (error 'type-error :datum result-type :expected-type 'sequence)))))

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

(defun %input-stream-designator (stream)
  ;; An input-stream designator: NIL denotes *STANDARD-INPUT*, T denotes
  ;; *TERMINAL-IO*, a stream denotes itself (CLHS Glossary).
  (cond ((null stream) *standard-input*)
        ((eq stream t)  *terminal-io*)
        (t stream)))

(defun read (&optional (stream *standard-input*)
                       (eof-error-p t) eof-value recursive-p)
  (kernel-read (%input-stream-designator stream) eof-error-p eof-value recursive-p))

(defun read-preserving-whitespace (&optional (stream *standard-input*)
                                             (eof-error-p t) eof-value recursive-p)
  (kernel-read-preserving-whitespace (%input-stream-designator stream)
                                     eof-error-p eof-value recursive-p))

(defun read-delimited-list (char &optional (stream *standard-input*) recursive-p)
  ;; Read objects from STREAM until the (unescaped, non-quoted) CHAR is reached,
  ;; returning them as a list (CHAR is consumed).  Delegates to the kernel reader.
  (kernel-read-delimited-list char stream recursive-p))

(defun read-char (&optional (stream *standard-input*)
                             (eof-error-p t) eof-value recursive-p)
  (kernel-read-char stream eof-error-p eof-value recursive-p))

(defun peek-char (&optional (peek-type nil)
                             (stream *standard-input*)
                             (eof-error-p t) eof-value recursive-p)
  (kernel-peek-char peek-type stream eof-error-p eof-value recursive-p))

(defun read-from-string (string &optional (eof-error-p t) eof-value
                         &key (start 0) end preserve-whitespace)
  ;; Returns two values: the first Lisp object read from STRING, and the index of
  ;; the first character not read (the kernel primitive supplies both).
  (kernel-read-from-string string eof-error-p eof-value start end
                            preserve-whitespace))

;; ---- SET-SYNTAX-FROM-CHAR (with behaviour transfer for the standard macros) --
;; The kernel reader dispatches the standard macro characters ( ' etc. by a
;; hardcoded switch keyed on the character, so %SET-SYNTAX-FROM-CHAR (which copies
;; the syntax-table entry) alone does not make a new character read like one of
;; them.  These reader-macro FUNCTIONS reproduce the behaviour, and the wrapper
;; installs them on TO-CHAR via SET-MACRO-CHARACTER when FROM-CHAR is one of them.

(defun %rm-quote (stream char)
  (declare (ignore char))
  (list 'quote (read stream t nil t)))

(defun %rm-open-paren (stream char)
  (declare (ignore char))
  (read-delimited-list #\) stream t))

(defun set-syntax-from-char (to-char from-char
                             &optional (to-readtable *readtable*) from-readtable)
  (%set-syntax-from-char to-char from-char to-readtable from-readtable)
  (cond ((char= from-char #\') (set-macro-character to-char #'%rm-quote      nil to-readtable))
        ((char= from-char #\() (set-macro-character to-char #'%rm-open-paren nil to-readtable)))
  t)

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
    ;; List accumulation builds REVERSED (cons / revappend / nreconc) so that
    ;; collect/append/nconc can share one variable; collect-reverses flips it
    ;; once at loop end.
    (:collect  `(setq ,var (cons ,expr ,var)))
    (:append   `(setq ,var (revappend ,expr ,var)))
    (:nconc    `(setq ,var (nreconc ,expr ,var)))
    (:sum      `(incf ,var ,expr))
    (:count    `(when ,expr (incf ,var)))
    (:maximize `(let ((%v ,expr)) (when (or (null ,var) (> %v ,var)) (setq ,var %v))))
    (:minimize `(let ((%v ,expr)) (when (or (null ,var) (< %v ,var)) (setq ,var %v))))
    ;; -body variants: their update is already in body; return nil to skip here.
    ((:collect-body :append-body :nconc-body :sum-body :count-body
      :maximize-body :minimize-body)
     nil)))

(defun %loop-accum-body-kind (kind)
  "Map an accumulation KIND to its -body marker (whose per-iteration update lives
inline in the loop body, so accum-forms skips it but binding/result still apply)."
  (case kind
    (:collect  :collect-body)  (:append :append-body)  (:nconc :nconc-body)
    (:sum      :sum-body)      (:count  :count-body)
    (:maximize :maximize-body) (:minimize :minimize-body)
    (t kind)))

(defun %loop-pattern-vars (pattern)
  "Symbol leaves of a LOOP destructuring pattern, in order (NIL = ignore)."
  (cond ((null pattern)    nil)
        ((symbolp pattern) (list pattern))
        ((consp pattern)   (append (%loop-pattern-vars (car pattern))
                                   (%loop-pattern-vars (cdr pattern))))
        (t                 nil)))

(defun %loop-numeric-type-p (type)
  "True if TYPE (a LOOP OF-TYPE spec) names a numeric type — so an uninitialised
variable of that type defaults to 0 rather than NIL (CLHS 6.1.1.7)."
  (let ((head (if (consp type) (car type) type)))
    (and (symbolp head)
         (member head '(number real rational integer fixnum bignum bit
                        float single-float double-float short-float long-float
                        ratio signed-byte unsigned-byte mod)))))

(defun %loop-typed-default (type)
  "Initial value for an uninitialised LOOP variable declared OF-TYPE TYPE:
0.0 for a float type, 0 for another numeric type, NIL otherwise (CLHS 6.1.1.7)."
  (let ((head (if (consp type) (car type) type)))
    (cond ((member head '(float single-float double-float short-float long-float)) 0.0)
          ((%loop-numeric-type-p type) 0)
          (t nil))))

(defun %loop-destr-form (pattern access)
  "List of (SETQ var <car/cdr access>) forms binding PATTERN's leaves from the
value reached by ACCESS (a form).  NIL leaves are skipped; a dotted/atom tail
binds the remaining CDR."
  (cond ((null pattern)    nil)
        ((symbolp pattern) (list (list 'setq pattern access)))
        ((consp pattern)   (append (%loop-destr-form (car pattern) (list 'car access))
                                   (%loop-destr-form (cdr pattern) (list 'cdr access))))
        (t                 nil)))

(defun %loop-parallel-steps (forms)
  "Fuse a FOR/AND group's step forms (each (SETQ v e) / (INCF v d) / (DECF v d))
into one form that evaluates every new value with the iteration's CURRENT
bindings, then assigns — so AND-joined steppers see each other's old values
(CLHS 6.1.2.2).  0 or 1 form needs no temporaries; anything not a simple
SETQ/INCF/DECF is left in place (sequential)."
  (cond ((null forms) nil)
        ((null (cdr forms)) (car forms))
        (t (let ((tmps nil) (assigns nil))
             (dolist (f forms)
               (let ((op (and (consp f) (car f))))
                 (cond ((eq op 'setq)
                        (let ((g (gensym "PS-")))
                          (push (list g (third f)) tmps)
                          (push (list 'setq (second f) g) assigns)))
                       ((eq op 'incf)
                        (let ((g (gensym "PS-")))
                          (push (list g (list '+ (second f) (or (third f) 1))) tmps)
                          (push (list 'setq (second f) g) assigns)))
                       ((eq op 'decf)
                        (let ((g (gensym "PS-")))
                          (push (list g (list '- (second f) (or (third f) 1))) tmps)
                          (push (list 'setq (second f) g) assigns)))
                       (t (push f assigns)))))
             (cons 'let (cons (nreverse tmps) (nreverse assigns)))))))

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
        (into-vars nil)   ; accumulation vars named by INTO — never the loop result
        (anon-list nil)   ; shared var for anonymous collect/append/nconc
        (anon-sum  nil)   ; shared var for anonymous sum/count
        (anon-max  nil)   ; shared var for anonymous maximize
        (anon-min  nil)   ; shared var for anonymous minimize
        (always-set nil) ; T if an ALWAYS clause was seen (for result + conflict check)
        (never-set  nil) ; T if a NEVER clause was seen
        (thereis-set nil) ; T if a THEREIS clause was seen
        (return-f  nil)
        (return-set nil)
        (end-tag   (gensym "END-")))  ; created here so WHILE/UNTIL can target it
    (flet ((pk  ()     (car rest))
           (pop! ()    (prog1 (car rest) (setq rest (cdr rest))))
           (try! (nm)  (when (%lkw (car rest) nm) (pop!) t))
           (take-body ()
             (let ((acc nil))
               (do () ((or (null rest) (%loop-kw-p (pk))))
                 (push (pop!) acc))
               (nreverse acc)))
           (skip-type-spec ()
             ;; Consume an optional LOOP type specifier after a variable: either
             ;; "OF-TYPE <type>" or a bare type spec (FIXNUM, FLOAT, T, a cons
             ;; like (INTEGER 0 9)).  bocl ignores the declaration.  Stop before
             ;; the keywords that legitimately follow the variable.
             (when (%lkw (pk) "OF-TYPE") (pop!) (pop!))
             (when (and rest
                        (not (%lkw (pk) "IN" "ON" "ACROSS" "FROM" "UPFROM"
                                     "DOWNFROM" "BEING" "=" "TO" "UPTO" "BELOW"
                                     "DOWNTO" "ABOVE" "BY" "THEN" "AND"))
                        (not (%loop-kw-p (pk))))
               (pop!)))
           (add-accum (kind var e)
             ;; Emit the per-iteration accumulation update in the loop BODY at
             ;; this textual position (so it interleaves with DO/WHILE/UNTIL per
             ;; CLHS 6.1.1.4), and register the result variable (as its -body
             ;; kind) for binding + the final result form — but only ONCE per var,
             ;; so several clauses sharing a var (anonymous or same INTO) bind and
             ;; reverse it a single time.
             (setq body (append body (list (%loop-accum-step kind var e))))
             (unless (dolist (a accums nil) (when (eq (second a) var) (return t)))
               (push (list (%loop-accum-body-kind kind) var e) accums)))
           (anon-var (kind)
             ;; The shared accumulation variable for an anonymous clause of KIND's
             ;; category, created on first use: like-typed anonymous accumulations
             ;; all merge into one place (CLHS 6.1.3).
             (cond ((member kind '(:collect :append :nconc))
                    (or anon-list (setq anon-list (gensym "ACC-L-"))))
                   ((member kind '(:sum :count))
                    (or anon-sum (setq anon-sum (gensym "ACC-N-"))))
                   ((eq kind :maximize)
                    (or anon-max (setq anon-max (gensym "ACC-MX-"))))
                   ((eq kind :minimize)
                    (or anon-min (setq anon-min (gensym "ACC-MN-")))))))
      (do () ((null rest))
        (let ((kw (pop!)))
          (cond
            ;; NAMED
            ((%lkw kw "NAMED")
             (setq name (pop!)))
            ;; WITH var [OF-TYPE type] [= init] [AND var ...]
            ;; VAR may be a destructuring pattern.  Variables joined by AND are
            ;; bound in PARALLEL (CLHS 6.1.2.2): every init is evaluated in the
            ;; outer scope (into a temp) BEFORE any of the group's variables is
            ;; bound, so `with a = 1 and b = (+ a 2)` sees the outer A, not 1.
            ((%lkw kw "WITH")
             (let ((pairs nil))
               (flet ((grab-type ()
                        ;; OF-TYPE <type>, or a bare type spec; NIL if neither.
                        (cond ((%lkw (pk) "OF-TYPE") (pop!) (pop!))
                              ((and rest (not (%lkw (pk) "=")) (not (%loop-kw-p (pk))))
                               (pop!))
                              (t nil)))
                      (one ()
                        (let* ((v    (pop!))
                               (type (grab-type))
                               (eq?  (try! "="))
                               (init (if (and eq? rest (not (%loop-kw-p (pk))))
                                         (pop!) nil)))
                          (push (list v init eq? type) pairs))))   ; (var init had-init type)
                 (one)
                 (do () ((not (try! "AND"))) (one)))
               (let ((temps (mapcar (lambda (p) (cons (gensym "WITH-") p))
                                    (nreverse pairs))))
                 ;; All inits first (evaluated in the outer scope -> parallel) ...
                 (dolist (tp temps)
                   (when (fourth tp) (push (list (first tp) (third tp)) letb)))
                 ;; ... then bind each variable / pattern.
                 (dolist (tp temps)
                   (let ((g (first tp)) (v (second tp))
                         (eq? (fourth tp)) (type (fifth tp)))
                     (cond
                       ;; initialised: bind from the temp (destructure if a pattern)
                       (eq?
                        (if (consp v)
                            (dolist (b (%loop-destr-form v g))
                              (push (list (second b) (third b)) letb))
                            (push (list v g) letb)))
                       ;; uninitialised pattern: each leaf takes its type's default
                       ;; (a per-leaf type list mirrors the pattern; a lone type
                       ;; applies to every leaf).
                       ((consp v)
                        (let ((i 0))
                          (dolist (leaf (%loop-pattern-vars v))
                            (push (list leaf (%loop-typed-default
                                              (if (consp type) (nth i type) type)))
                                  letb)
                            (setq i (+ i 1)))))
                       ;; uninitialised single variable
                       (t (push (list v (%loop-typed-default type)) letb))))))))
            ;; FOR / AS
            ((%lkw kw "FOR" "AS")
             ;; A FOR clause may bind several variables joined by AND; they step
             ;; in PARALLEL (CLHS 6.1.2.2) — each AND-clause's stepper sees the
             ;; others' values from the start of the iteration.  Process each
             ;; variable, then (if AND was used) fuse this group's steps.
             (let ((%old-steps steps) (%old-letb letb) (%nvars 0)
                   (%eq-then-info nil)    ; (var step-form) of a lone `= init then step`
                   (%destr-pairs nil))    ; (pattern . gensym) of destructured FOR vars
              (flet ((%one-for ()
               (setq %nvars (+ %nvars 1))
               (let* ((fvar  (pop!))
                    ;; A cons FOR variable is a destructuring pattern: iterate a
                    ;; hidden gensym and destructure it into the pattern's leaves
                    ;; each iteration (after the sub-clause sets the gensym).
                    (destr (consp fvar))
                    (v     (if destr (gensym "DSTR-") fvar)))
               (when destr
                 ;; Bind the pattern's leaves; the destructuring of the hidden gensym
                 ;; V into them is pushed to SETUPS *after* the sub-clause below (so it
                 ;; runs right after V is assigned, and crucially BEFORE any later
                 ;; FOR x = ... clause that references the pattern's variables —
                 ;; setups run, in push order, before the body).
                 (dolist (pv (%loop-pattern-vars fvar)) (push (list pv nil) letb)))
               (skip-type-spec)   ; optional OF-TYPE / bare type spec (ignored)
               (cond
                 ;; FOR v IN list [BY step-fn]
                 ((%lkw (pk) "IN")
                  (pop!)
                  (let ((lv (gensym "LV-")) (bv nil))
                    (push (list lv (pop!)) letb)   ; lv bound first (LET* order)
                    (when (%lkw (pk) "BY")         ; step-fn (e.g. #'cddr), eval'd once
                      (pop!) (setq bv (gensym "BY-"))
                      (push (list bv (pop!)) letb))
                    (push (list v  nil)    letb)
                    (push `(null ,lv)            tests)
                    (push `(setq ,v (car ,lv))   setups)
                    (push (if bv `(setq ,lv (funcall ,bv ,lv))
                                 `(setq ,lv (cdr ,lv))) steps)))
                 ;; FOR v ON list [BY step-fn]
                 ((%lkw (pk) "ON")
                  (pop!)
                  ;; Evaluate the list into an aux temp (an uninterned gensym), then
                  ;; bind V to it, so a parallel AND-group keeps every init's
                  ;; evaluation in source order (the temp sorts with the other
                  ;; auxiliaries, V with the loop variables).
                  (let ((bv nil) (og (gensym "ON-")))
                    (push (list og (pop!)) letb)
                    (push (list v og)      letb)
                    (when (%lkw (pk) "BY")
                      (pop!) (setq bv (gensym "BY-"))
                      (push (list bv (pop!)) letb))
                    (push `(atom ,v)      tests)   ; ON ends by ENDP/ATOM, not NULL
                    (push (if bv `(setq ,v (funcall ,bv ,v))
                                 `(setq ,v (cdr ,v))) steps)))
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
                 ((%lkw (pk) "FROM" "UPFROM" "DOWNFROM"
                              "TO" "UPTO" "BELOW" "DOWNTO" "ABOVE" "BY")
                  ;; Arithmetic stepping.  The prepositions FROM/UPFROM/DOWNFROM,
                  ;; TO/UPTO/BELOW/DOWNTO/ABOVE and BY may appear in ANY order, and
                  ;; FROM may be omitted (START defaults to 0).
                  (let ((start 0) (downp nil) (end-f nil) (incl t)
                        (by-supplied nil) (step nil) (once nil))
                    ;; Each of FROM/TO/BY is evaluated exactly ONCE, in the order
                    ;; written (CLHS 6.1.2.1.1): bind each to a gensym in source
                    ;; order, so a side-effecting or sibling-referencing bound is
                    ;; not re-run in the per-iteration test/step.
                    (flet ((bind1 (form)
                             (let ((g (gensym "AR-"))) (push (cons g form) once) g)))
                      (do () ((or (null rest)
                                  (not (%lkw (pk) "FROM" "UPFROM" "DOWNFROM" "TO"
                                                  "UPTO" "BELOW" "DOWNTO" "ABOVE" "BY"))))
                        (let ((sub (pop!)))
                          (cond
                            ((%lkw sub "FROM" "UPFROM") (setq start (bind1 (pop!))))
                            ((%lkw sub "DOWNFROM")      (setq start (bind1 (pop!)) downp t))
                            ((%lkw sub "TO" "UPTO")     (setq end-f (bind1 (pop!)) incl  t))
                            ((%lkw sub "BELOW")         (setq end-f (bind1 (pop!)) incl  nil))
                            ((%lkw sub "DOWNTO")        (setq end-f (bind1 (pop!)) incl  t  downp t))
                            ((%lkw sub "ABOVE")         (setq end-f (bind1 (pop!)) incl  nil downp t))
                            ((%lkw sub "BY")            (setq step  (bind1 (pop!)) by-supplied t))))))
                    ;; Bind the once-evaluated bounds (in source order) before V.
                    (dolist (pr (reverse once)) (push (list (car pr) (cdr pr)) letb))
                    ;; BY supplies a POSITIVE step magnitude; DOWNTO/ABOVE/DOWNFROM
                    ;; count downward, so the effective step is negated.  Without
                    ;; BY, default to -1 (down) or 1 (up).
                    (cond ((not by-supplied) (setq step (if downp -1 1)))
                          (downp             (setq step (list '- step))))
                    (push (list v start) letb)
                    (when end-f
                      (push (if downp
                                (if incl `(< ,v ,end-f)  `(<= ,v ,end-f))
                                (if incl `(> ,v ,end-f)  `(>= ,v ,end-f)))
                            tests))
                    (push `(incf ,v ,step) steps)))
                 ;; FOR v = init [THEN update]
                 ;; Without THEN, INIT is re-evaluated at the TOP of every
                 ;; iteration (CLHS 6.1.2.1.2) — including the first — so it must
                 ;; run in SETUPS (before the body), not STEPS (after).  Otherwise
                 ;; `for a in l for x = a` sees a stale x on the first pass.
                 ((%lkw (pk) "=")
                  (pop!)
                  (let ((init (pop!)))
                    (if (%lkw (pk) "THEN")
                        (progn (pop!)
                               (let ((stepform (pop!)))
                                 (push (list v init) letb)
                                 ;; Push the step at the bottom (so an AND group can
                                 ;; fuse it); also record it so a STANDALONE `= then`
                                 ;; can be hoisted to the loop top afterwards.
                                 (push `(setq ,v ,stepform) steps)
                                 (setq %eq-then-info (list v stepform))))
                        (progn
                          (push (list v nil) letb)
                          (push `(setq ,v ,init) setups)))))
                 ;; FOR v BEING [EACH|THE] HASH-KEY[S] OF ht [USING (HASH-VALUE vv)]
                 ;; FOR v BEING [EACH|THE] HASH-VALUE[S] OF ht [USING (HASH-KEY kk)]
                 ;; FOR v BEING [EACH|THE] symbol[s]/present-symbol[s]/
                 ;; external-symbol[s] [OF|IN] package  — iterate a package's symbols.
                 ((and (%lkw (pk) "BEING")
                       (let ((r2 (cdr rest)))
                         (when (or (%lkw (car r2) "EACH") (%lkw (car r2) "THE"))
                           (setq r2 (cdr r2)))
                         (%lkw (car r2) "SYMBOL" "SYMBOLS" "PRESENT-SYMBOL"
                                        "PRESENT-SYMBOLS" "EXTERNAL-SYMBOL"
                                        "EXTERNAL-SYMBOLS")))
                  (pop!)  ; BEING
                  (when (or (%lkw (pk) "EACH") (%lkw (pk) "THE")) (pop!))
                  (let* ((variant  (pop!))
                         (extp     (%lkw variant "EXTERNAL-SYMBOL" "EXTERNAL-SYMBOLS"))
                         ;; PRESENT-SYMBOLS iterates only the package's OWN symbols
                         ;; (internal + external), excluding those inherited from
                         ;; used packages; plain SYMBOLS includes inherited.
                         (presentp (%lkw variant "PRESENT-SYMBOL" "PRESENT-SYMBOLS"))
                         (lv       (gensym "SYMS-")))
                    (when (or (%lkw (pk) "OF") (%lkw (pk) "IN")) (pop!))
                    (let ((pkg-form (pop!)))
                      (push (list lv
                                  (cond
                                    (extp
                                     `(let ((%a nil))
                                        (do-external-symbols (%s ,pkg-form) (push %s %a)) %a))
                                    (presentp
                                     `(let ((%a nil) (%pk ,pkg-form))
                                        (do-symbols (%s %pk)
                                          (unless (eq (second (multiple-value-list
                                                               (find-symbol (symbol-name %s) %pk)))
                                                      :inherited)
                                            (push %s %a)))
                                        %a))
                                    (t
                                     `(let ((%a nil))
                                        (do-symbols (%s ,pkg-form) (push %s %a)) %a))))
                            letb)
                      (push (list v nil)        letb)
                      (push `(null ,lv)             tests)
                      (push `(setq ,v (car ,lv))    setups)
                      (push `(setq ,lv (cdr ,lv))   steps)
                      (push `(setq ,v (car ,lv))    steps))))
                 ((%lkw (pk) "BEING")
                  (pop!)  ; consume BEING
                  (when (or (%lkw (pk) "EACH") (%lkw (pk) "THE")) (pop!))
                  (let* ((variant  (pop!))
                         (key-p    (%lkw variant "HASH-KEY" "HASH-KEYS"))
                         (ht-var   (gensym "HT-"))
                         (both-var (gensym "BOTH-"))   ; (cons keys vals) computed once
                         (keys-var (gensym "HKEYS-"))
                         (vals-var (gensym "HVALS-"))
                         (other-v  nil))
                    (when (%lkw (pk) "OF" "IN") (pop!))   ; OF or IN (CLHS 6.1.2.1.4)
                    (let ((ht-form (pop!)))
                      (when (%lkw (pk) "USING")
                        (pop!)
                        (let ((spec (pop!)))
                          (when (consp spec) (setq other-v (cadr spec)))))
                      ;; Evaluate hash-table once in LET*, collect keys+values once.
                      (push (list ht-var    ht-form) letb)
                      (push (list both-var
                                  `(let ((%ks nil) (%vs nil))
                                     (maphash (lambda (%k %v)
                                                (push %k %ks)
                                                (push %v %vs))
                                              ,ht-var)
                                     (cons (nreverse %ks) (nreverse %vs))))
                            letb)
                      (push (list keys-var `(car ,both-var)) letb)
                      (push (list vals-var `(cdr ,both-var)) letb)
                      (push (list v nil)                     letb)
                      ;; The USING co-variable may itself be a destructuring
                      ;; pattern (e.g. USING (HASH-VALUE (v vv))): step a gensym
                      ;; OV and destructure it into the leaves each iteration.
                      (let ((ov (if (consp other-v) (gensym "USING-") other-v)))
                       (when other-v (push (list ov nil) letb))
                       (when (consp other-v)
                         (dolist (pv (%loop-pattern-vars other-v)) (push (list pv nil) letb))
                         (setq body (append body
                                            (list (cons 'progn (%loop-destr-form other-v ov))))))
                       ;; Terminate when keys list is exhausted.
                       (push `(null ,keys-var) tests)
                       ;; Per-iteration setup: bind v (and co-var) from head of lists.
                       (if key-p
                           (progn
                             (push `(setq ,v (car ,keys-var)) setups)
                             (when other-v
                               (push `(setq ,ov (car ,vals-var)) setups)))
                           (progn
                             (push `(setq ,v (car ,vals-var)) setups)
                             (when other-v
                               (push `(setq ,ov (car ,keys-var)) setups))))
                       ;; Per-iteration step: advance both lists, then rebind v.
                       (push `(progn (setq ,keys-var (cdr ,keys-var))
                                     (setq ,vals-var (cdr ,vals-var)))
                              steps)
                       (if key-p
                           (progn
                             (push `(setq ,v (car ,keys-var)) steps)
                             (when other-v
                               (push `(setq ,ov (car ,vals-var)) steps)))
                           (progn
                             (push `(setq ,v (car ,vals-var)) steps)
                             (when other-v
                               (push `(setq ,ov (car ,keys-var)) steps))))))))
                 (t
                  (error "LOOP FOR: unknown sub-clause ~S" (pk))))
               ;; Record (pattern . gensym) so the destructuring of V into the
               ;; pattern's leaves is emitted into SETUPS *after* this for-clause's
               ;; post-processing (which, for `= init then step`, hoists the step into
               ;; SETUPS) — so destructuring sees V's final per-iteration value, and
               ;; runs before any subsequent FOR x = ... clause reads the leaves.
               (when destr
                 (setq %destr-pairs (cons (cons fvar v) %destr-pairs))))))
              (%one-for)
              (do () ((not (%lkw (pk) "AND"))) (pop!) (%one-for)))
             ;; AND joined >1 variable: fuse this group's steps into one parallel
             ;; assignment (the new steps are at the front of STEPS, before
             ;; %OLD-STEPS) so each stepper sees the others' old values.
             (cond
              ;; A STANDALONE `for v = init then step` (no AND): hoist its step from
              ;; the loop BOTTOM to the TOP — evaluated after the exit test and
              ;; skipped on the first iteration — so a REPEAT/UNTIL terminator does
              ;; not force one extra step (CLHS 6.1.2.1.2).  (An AND-grouped `= then`
              ;; keeps its fused bottom-of-loop step, below, for parallel old-value
              ;; semantics.)
              ((and (= %nvars 1) %eq-then-info)
               (do () ((eq steps %old-steps)) (pop steps))   ; drop the bottom step
               (let ((vv (first %eq-then-info)) (sf (second %eq-then-info))
                     (ff (gensym "LOOP-FIRST-")))
                 (push (list ff t) letb)
                 (push `(if ,ff (setq ,ff nil) (setq ,vv ,sf)) setups)))
              ((> %nvars 1)
               (let ((%new nil))
                 (do () ((eq steps %old-steps)) (push (pop steps) %new))
                 (push (%loop-parallel-steps (nreverse %new)) steps))
               ;; Parallel binding: an AND-group's iteration-variable INITs are
               ;; evaluated in the surrounding scope, before any of the group's
               ;; variables is bound (CLHS 6.1.2.2) — so `for a from 0 and b from
               ;; a` sees the outer A.  Reorder this group's LETB so the init /
               ;; auxiliary bindings (uninterned gensyms: the once-eval'd bounds,
               ;; list/vector holders) precede the loop-variable bindings
               ;; (interned), which reference them.
               (let ((grp nil))
                 (do () ((eq letb %old-letb)) (push (pop letb) grp))  ; grp: source order
                 (let ((auxs nil) (vars nil))
                   (dolist (b grp)
                     (if (and (symbolp (car b)) (symbol-package (car b)))
                         (push b vars)
                         (push b auxs)))
                   (dolist (b (nreverse auxs)) (push b letb))
                   (dolist (b (nreverse vars)) (push b letb))))))
               ;; Emit the destructuring of each destructured FOR var into SETUPS,
               ;; AFTER the post-processing above (so it runs after V's final
               ;; per-iteration value is assigned — including a hoisted `= then`
               ;; step — and before any later FOR x = ... clause reads the leaves).
               (dolist (%dp (reverse %destr-pairs))
                 (push (cons 'progn (%loop-destr-form (car %dp) (cdr %dp))) setups))))
            ;; REPEAT n
            ((%lkw kw "REPEAT")
             (let ((rv (gensym "R-")))
               (push (list rv (pop!)) letb)
               (push `(<= ,rv 0)  tests)
               (push `(decf ,rv)  steps)))
            ;; WHILE / UNTIL are body clauses tested at their TEXTUAL position
            ;; (CLHS 6.1.9), not hoisted to the loop top with the FOR/REPEAT
            ;; termination tests — so `do X while Y` tests Y after X, and
            ;; `collect X while Y` collects before testing.
            ((%lkw kw "WHILE")
             (setq body (append body (list `(when (not ,(pop!)) (go ,end-tag))))))
            ((%lkw kw "UNTIL")
             (setq body (append body (list `(when ,(pop!) (go ,end-tag))))))
            ;; DO / DOING
            ((%lkw kw "DO" "DOING")
             (setq body (append body (take-body))))
            ;; Accumulation clauses
            ((%lkw kw "COLLECT" "COLLECTING")
             (let ((e (pop!)) (iv nil))
               (when (try! "INTO") (setq iv (pop!)) (push iv into-vars))
               (when (try! "OF-TYPE") (pop!))   ; optional accumulator type (ignored)
               (when (and rest (not (%loop-kw-p (pk)))) (pop!))   ; bare accumulator type
               (add-accum :collect (or iv (anon-var :collect)) e)))
            ((%lkw kw "APPEND" "APPENDING")
             (let ((e (pop!)) (iv nil))
               (when (try! "INTO") (setq iv (pop!)) (push iv into-vars))
               (when (try! "OF-TYPE") (pop!))   ; optional accumulator type (ignored)
               (when (and rest (not (%loop-kw-p (pk)))) (pop!))   ; bare accumulator type
               (add-accum :append (or iv (anon-var :append)) e)))
            ((%lkw kw "NCONC" "NCONCING")
             (let ((e (pop!)) (iv nil))
               (when (try! "INTO") (setq iv (pop!)) (push iv into-vars))
               (when (try! "OF-TYPE") (pop!))   ; optional accumulator type (ignored)
               (when (and rest (not (%loop-kw-p (pk)))) (pop!))   ; bare accumulator type
               (add-accum :nconc (or iv (anon-var :nconc)) e)))
            ((%lkw kw "SUM" "SUMMING")
             (let ((e (pop!)) (iv nil))
               (when (try! "INTO") (setq iv (pop!)) (push iv into-vars))
               (when (try! "OF-TYPE") (pop!))   ; optional accumulator type (ignored)
               (when (and rest (not (%loop-kw-p (pk)))) (pop!))   ; bare accumulator type
               (add-accum :sum (or iv (anon-var :sum)) e)))
            ((%lkw kw "COUNT" "COUNTING")
             (let ((e (pop!)) (iv nil))
               (when (try! "INTO") (setq iv (pop!)) (push iv into-vars))
               (when (try! "OF-TYPE") (pop!))   ; optional accumulator type (ignored)
               (when (and rest (not (%loop-kw-p (pk)))) (pop!))   ; bare accumulator type
               (add-accum :count (or iv (anon-var :count)) e)))
            ((%lkw kw "MAXIMIZE" "MAXIMIZING")
             (let ((e (pop!)) (iv nil))
               (when (try! "INTO") (setq iv (pop!)) (push iv into-vars))
               (when (try! "OF-TYPE") (pop!))   ; optional accumulator type (ignored)
               (when (and rest (not (%loop-kw-p (pk)))) (pop!))   ; bare accumulator type
               (add-accum :maximize (or iv (anon-var :maximize)) e)))
            ((%lkw kw "MINIMIZE" "MINIMIZING")
             (let ((e (pop!)) (iv nil))
               (when (try! "INTO") (setq iv (pop!)) (push iv into-vars))
               (when (try! "OF-TYPE") (pop!))   ; optional accumulator type (ignored)
               (when (and rest (not (%loop-kw-p (pk)))) (pop!))   ; bare accumulator type
               (add-accum :minimize (or iv (anon-var :minimize)) e)))
            ;; ALWAYS / NEVER / THEREIS — boolean termination clauses tested at
            ;; their TEXTUAL position in the body (CLHS 6.1.2.3 / 6.1.9), so a
            ;; preceding WHILE/UNTIL or stepped FOR can terminate the loop before
            ;; the test runs (e.g. `for p = x then (next p) while p thereis (f p)`
            ;; must not evaluate (f NIL)).  The *-SET flags drive the result form
            ;; and the accumulation-conflict check below.
            ((%lkw kw "ALWAYS")
             (setq always-set t)
             (setq body (append body
                          (list `(unless ,(pop!) (return-from ,(or name nil) nil))))))
            ((%lkw kw "NEVER")
             (setq never-set t)
             (setq body (append body
                          (list `(when ,(pop!) (return-from ,(or name nil) nil))))))
            ((%lkw kw "THEREIS")
             (setq thereis-set t)
             (let ((tv (gensym "TV-")))
               (setq body (append body
                            (list `(let ((,tv ,(pop!)))
                                     (when ,tv (return-from ,(or name nil) ,tv))))))))
            ;; RETURN
            ((%lkw kw "RETURN")
             ;; RETURN form terminates the loop IMMEDIATELY (CLHS 6.1.5), so emit
             ;; (return-from nil form) into the body — NOT a deferred post-loop
             ;; result-expr, which a loop with no iteration clause never reaches.
             (setq body (append body
                                 (list `(return-from ,(or name nil) ,(pop!))))))
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
             ;; IT is anaphoric only in the FIRST clause after the test/THEN (and
             ;; after ELSE); in an AND-chained clause IT is an ordinary variable
             ;; (CLHS 6.1.2.1.4).  IT-LIVE gates the substitution per sub-clause.
             (let ((it-live t))
               (labels
                   ((pop-it (c)
                      ;; Pop an accumulation expression, resolving the anaphoric
                      ;; IT to the guard's test form C only while IT-LIVE.
                      (let ((x (pop!))) (if (and it-live (%lkw x "IT")) c x)))
                    (parse-guarded-sub (neg cond reuse-var shared-box)
                      ;; REUSE-VAR: if non-nil, the ELSE branch shares this var.
                      ;; SHARED-BOX: a 1-cons cell recording the accumulation var
                      ;; created here, so a following ELSE at this level reuses it.
                      (let ((sub (pop!)))
                        (cond
                          ;; Nested conditional as the sub-clause (CLHS 6.1.2.1.4):
                          ;; parse it (its own ELSE/END), then guard its forms by
                          ;; COND and propagate its accumulator up to SHARED-BOX so
                          ;; an enclosing ELSE collects into the same list.
                          ((%lkw sub "WHEN" "UNLESS" "IF")
                           (let ((forms (parse-cond sub shared-box)))
                             (list (if neg `(unless ,cond ,@forms)
                                           `(when ,cond ,@forms)))))
                          ;; RETURN
                          ((%lkw sub "RETURN")
                           (let ((val (pop-it cond)))   ; IT -> the guard's value
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
                           (let* ((e  (pop-it cond))
                                  (iv (when (try! "INTO") (pop!)))
                                  (%ot (when (try! "OF-TYPE") (pop!)))
                                  (%bt (when (and rest (not (%loop-kw-p (pk)))) (pop!)))
                                  ;; Reuse shared var for ELSE branch so both
                                  ;; halves accumulate into the same list.
                                  (v  (or iv reuse-var (anon-var :collect))))
                             (setf (car shared-box) v) (when iv (push iv into-vars)) ; remember for possible ELSE
                             (unless (dolist (%a accums nil) (when (eq (second %a) v) (return t)))
                               (push (list :collect-body v e) accums))
                             (list (if neg
                                       `(unless ,cond (setq ,v (cons ,e ,v)))
                                       `(when ,cond (setq ,v (cons ,e ,v)))))))
                          ;; APPEND / APPENDING
                          ((%lkw sub "APPEND" "APPENDING")
                           (let* ((e (pop-it cond))
                                  (iv (when (try! "INTO") (pop!)))
                                  (%ot (when (try! "OF-TYPE") (pop!)))
                                  (%bt (when (and rest (not (%loop-kw-p (pk)))) (pop!)))
                                  (v  (or iv reuse-var (anon-var :append))))
                             (setf (car shared-box) v) (when iv (push iv into-vars))
                             (unless (dolist (%a accums nil) (when (eq (second %a) v) (return t)))
                               (push (list :append-body v e) accums))
                             (list (if neg
                                       `(unless ,cond (setq ,v (revappend ,e ,v)))
                                       `(when ,cond (setq ,v (revappend ,e ,v)))))))
                          ;; NCONC / NCONCING
                          ((%lkw sub "NCONC" "NCONCING")
                           (let* ((e (pop-it cond))
                                  (iv (when (try! "INTO") (pop!)))
                                  (%ot (when (try! "OF-TYPE") (pop!)))
                                  (%bt (when (and rest (not (%loop-kw-p (pk)))) (pop!)))
                                  (v  (or iv reuse-var (anon-var :nconc))))
                             (setf (car shared-box) v) (when iv (push iv into-vars))
                             (unless (dolist (%a accums nil) (when (eq (second %a) v) (return t)))
                               (push (list :nconc-body v e) accums))
                             (list (if neg
                                       `(unless ,cond (setq ,v (nreconc ,e ,v)))
                                       `(when ,cond (setq ,v (nreconc ,e ,v)))))))
                          ;; SUM / SUMMING
                          ((%lkw sub "SUM" "SUMMING")
                           (let* ((e (pop-it cond))
                                  (iv (when (try! "INTO") (pop!)))
                                  (%ot (when (try! "OF-TYPE") (pop!)))
                                  (%bt (when (and rest (not (%loop-kw-p (pk)))) (pop!)))
                                  (v  (or iv reuse-var (anon-var :sum))))
                             (setf (car shared-box) v) (when iv (push iv into-vars))
                             (unless (dolist (%a accums nil) (when (eq (second %a) v) (return t)))
                               (push (list :sum-body v e) accums))
                             (list (if neg
                                       `(unless ,cond (incf ,v ,e))
                                       `(when ,cond (incf ,v ,e))))))
                          ;; COUNT / COUNTING
                          ((%lkw sub "COUNT" "COUNTING")
                           (let* ((e (pop-it cond))
                                  (iv (when (try! "INTO") (pop!)))
                                  (%ot (when (try! "OF-TYPE") (pop!)))
                                  (%bt (when (and rest (not (%loop-kw-p (pk)))) (pop!)))
                                  (v  (or iv reuse-var (anon-var :count))))
                             (setf (car shared-box) v) (when iv (push iv into-vars))
                             (unless (dolist (%a accums nil) (when (eq (second %a) v) (return t)))
                               (push (list :count-body v e) accums))
                             (list (if neg
                                       `(unless ,cond (when ,e (incf ,v)))
                                       `(when ,cond (when ,e (incf ,v)))))))
                          ;; MAXIMIZE / MAXIMIZING
                          ((%lkw sub "MAXIMIZE" "MAXIMIZING")
                           (let* ((e (pop-it cond))
                                  (iv (when (try! "INTO") (pop!)))
                                  (%ot (when (try! "OF-TYPE") (pop!)))
                                  (%bt (when (and rest (not (%loop-kw-p (pk)))) (pop!)))
                                  (v  (or iv reuse-var (anon-var :maximize))))
                             (setf (car shared-box) v) (when iv (push iv into-vars))
                             (unless (dolist (%a accums nil) (when (eq (second %a) v) (return t)))
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
                           (let* ((e (pop-it cond))
                                  (iv (when (try! "INTO") (pop!)))
                                  (%ot (when (try! "OF-TYPE") (pop!)))
                                  (%bt (when (and rest (not (%loop-kw-p (pk)))) (pop!)))
                                  (v  (or iv reuse-var (anon-var :minimize))))
                             (setf (car shared-box) v) (when iv (push iv into-vars))
                             (unless (dolist (%a accums nil) (when (eq (second %a) v) (return t)))
                               (push (list :minimize-body v e) accums))
                             (list (if neg
                                       `(unless ,cond
                                          (let ((%nv ,e))
                                            (when (or (null ,v) (< %nv ,v)) (setq ,v %nv))))
                                       `(when ,cond
                                          (let ((%nv ,e))
                                            (when (or (null ,v) (< %nv ,v)) (setq ,v %nv))))))))
                          (t
                           (error "LOOP WHEN/UNLESS: unhandled sub-clause ~S" sub)))))
                    (parse-cond (ckw out-box)
                      ;; CKW (WHEN/UNLESS/IF) already consumed.  Parse the test,
                      ;; the guarded sub-clause(s), an optional ELSE branch and a
                      ;; closing END; return the list of body forms.  OUT-BOX, when
                      ;; given, receives this conditional's accumulation var so an
                      ;; enclosing ELSE collects into the same list.
                      (let* ((cond-expr (pop!))
                             (negate (%lkw ckw "UNLESS"))
                             (sbox   (list nil))
                             (forms  nil))
                        (when (%lkw (pk) "THEN") (pop!)) ; optional THEN after IF
                        (setq it-live t)
                        (setq forms (parse-guarded-sub negate cond-expr nil sbox))
                        (setq it-live nil)
                        ;; AND chains more sub-clauses under the same condition.
                        (do () ((not (%lkw (pk) "AND")))
                          (pop!)
                          (setq forms (append forms
                                              (parse-guarded-sub negate cond-expr nil sbox))))
                        ;; ELSE: negated branch; reuse the THEN accumulator so both
                        ;; halves fill one list.
                        (when (%lkw (pk) "ELSE")
                          (pop!)
                          (setq it-live t)
                          (setq forms (append forms
                                              (parse-guarded-sub (not negate) cond-expr (car sbox) sbox)))
                          (setq it-live nil)
                          (do () ((not (%lkw (pk) "AND")))
                            (pop!)
                            (setq forms (append forms
                                                (parse-guarded-sub (not negate) cond-expr (car sbox) sbox)))))
                        ;; END terminates a WHEN/UNLESS/IF block.
                        (when (%lkw (pk) "END") (pop!))
                        (when out-box (setf (car out-box) (car sbox)))
                        forms)))
                 (setq body (append body (parse-cond kw nil))))))
            ;; AND at top level (outside WHEN) — absorb so it doesn't error
            ((%lkw kw "AND") nil) ; parallel FOR clauses already handle AND
            ;; END at top level — ignore (close of a WHEN block)
            ((%lkw kw "END") nil)
            (t
             (error "LOOP: unknown clause keyword: ~S" kw))))))
    ;; A value-accumulation clause and a boolean ALWAYS/NEVER/THEREIS clause have
    ;; conflicting result semantics and may not be combined (CLHS 6.1.2.3) — a
    ;; PROGRAM-ERROR, signalled here so it surfaces at macro-expansion time.
    (when (and accums (or always-set never-set thereis-set))
      (error 'program-error))
    ;; A variable bound by WITH/FOR twice, or an iteration variable also named by
    ;; INTO, is a PROGRAM-ERROR (CLHS 6.1.1.7).  An INTO name MAY be shared by
    ;; several accumulation clauses, so duplicates among INTO names alone are
    ;; fine — only collisions with a binding variable count.  Gensyms are
    ;; uninterned, so SYMBOL-PACKAGE filters out the macro's own temporaries.
    (let ((seen nil) (dup nil))
      (dolist (b letb)
        (let ((v (car b)))
          (when (and v (symbolp v) (symbol-package v))
            (if (member v seen) (setq dup t) (push v seen)))))
      (dolist (v (remove-duplicates into-vars))
        (when (and v (symbolp v) (symbol-package v) (member v seen))
          (setq dup t)))
      (when dup (error 'program-error)))
    ;; --- code generation ---
    ;; accums: last pushed = first defined; nreverse so primary = first defined
    (setq accums (nreverse accums))
    (let* ((top-tag      (gensym "TOP-"))
           ;; end-tag is created at the top of %loop-expand so WHILE/UNTIL can
           ;; target it while parsing.
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
           ;; COLLECT builds its list reversed (cons); reverse each collect var
           ;; ONCE at loop end so the value is forward-order for FINALLY, for an
           ;; INTO variable, and for the loop result alike.  (APPEND/NCONC/SUM/
           ;; COUNT/MAX/MIN already hold their final value.)
           (collect-reverses
                         (let ((rs nil))
                           (dolist (a accums (nreverse rs))
                             (when (member (first a) '(:collect :collect-body
                                                       :append :append-body
                                                       :nconc :nconc-body))
                               (push `(setq ,(second a) (nreverse ,(second a))) rs)))))
           ;; The loop's default result is the first ANONYMOUS accumulation; an
           ;; INTO-named accumulation goes only to its variable (CLHS 6.1.3), so
           ;; `loop ... append a into x` returns NIL unless FINALLY/RETURN says so.
           (primary      (dolist (a accums nil)
                           (unless (member (second a) into-vars) (return a))))
           (result-expr  (cond (return-set return-f)
                               (primary     (second primary))  ; already forward-order
                               (always-set  t) ; vacuously T when loop never runs
                               (never-set   t) ; vacuously T when loop never runs
                               (t           nil))))
      ;; LOOP-FINISH jumps to the loop epilogue (END-TAG) so the collect-reversal
      ;; and FINALLY still run — shadow the global LOOP-FINISH within this loop.
      `(block ,(or name nil)
         (macrolet ((loop-finish () (list 'go ',end-tag)))
           (let* (,@let*-binds ,@accum-binds)
             (tagbody
               ,@initially
               ,top-tag
               ,@test-forms
               ,@setup-forms
               ,@body
               ,@accum-forms
               ,@step-forms
               (go ,top-tag)
               ,end-tag
               ,@collect-reverses
               ,@finally
               (return-from ,(or name nil) ,result-expr))))))))

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
  ;; Destructive BUTLAST: trim the last N cons cells from LIST in place.
  (let* ((len  (%cons-length list))
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
  (when (member result-type '(cons null)) (setq result-type 'list))  ; CONS/NULL == LIST
  (cond
    ;; String identity
    ((and (stringp object) (member result-type '(string cl:string simple-string base-string simple-base-string)))
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
    ((and (listp object) (member result-type '(string cl:string simple-string base-string simple-base-string)))
     (let* ((len (length object))
            (s   (make-string len)))
       (dotimes (i len s)
         (char-set s i (nth i object)))))
    ;; List identity
    ((and (listp object) (member result-type '(list cl:list)))
     object)
    ;; ---- BIT-VECTOR branches MUST precede the VECTOR branches: a bit-vector
    ;; is a vector subtype, but its storage is octets, so SVREF would misread it
    ;; (reading octet bytes as object references).  Use the BIT accessor instead.
    ((and (bit-vector-p object) (member result-type '(bit-vector simple-bit-vector)))
     object)
    ((and (bit-vector-p object) (member result-type '(list cl:list)))
     (let ((acc nil))
       (dotimes (i (length object) (nreverse acc))
         (push (bit object i) acc))))
    ((and (bit-vector-p object) (member result-type '(vector simple-vector)))
     (let* ((len (length object)) (v (make-array len)))
       (dotimes (i len v) (setf (svref v i) (bit object i)))))
    ;; LIST / VECTOR / STRING → BIT-VECTOR
    ((and (or (listp object) (vectorp object))
          (member result-type '(bit-vector simple-bit-vector)))
     (let* ((len (length object)) (b (make-bit-vector len 0)))
       (dotimes (i len b) (setf (bit b i) (elt object i)))))
    ;; Vector identity
    ((and (vectorp object) (member result-type '(vector simple-vector)))
     object)
    ;; Vector → list
    ((and (vectorp object) (member result-type '(list cl:list)))
     (let ((acc nil))
       (dotimes (i (vector-length object) (nreverse acc))
         (push (svref object i) acc))))
    ;; Vector → string
    ((and (vectorp object) (member result-type '(string cl:string simple-string base-string simple-base-string)))
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

(defun %parse-physical-directory (s)
  "Parse the directory portion of a POSIX namestring S into a CL directory
   component: NIL when there is no directory part, otherwise the list
   (:ABSOLUTE|:RELATIVE . components), where each component is a string, :UP
   (\"..\"), :WILD (\"*\") or :WILD-INFERIORS (\"**\").  \".\" segments are dropped."
  (let ((dir-end (%path-dir-end s)))
    (if (< dir-end 0)
        nil                                       ; no #\/ at all => no directory
        (let* ((abs  (and (> (string-length s) 0) (eql (char s 0) #\/)))
               (body (subseq s 0 (+ dir-end 1)))  ; through the final #\/
               (segs (%split-path body))          ; drops empty segments
               (comps '()))
          (dolist (seg segs)
            (cond ((string= seg ".")  nil)
                  ((string= seg "..") (push :up comps))
                  ((string= seg "**") (push :wild-inferiors comps))
                  ((string= seg "*")  (push :wild comps))
                  (t                  (push seg comps))))
          (cons (if abs :absolute :relative) (nreverse comps))))))

(defun pathname-directory (pathname)
  ;; CLHS 19.2: the directory component is NIL or (:ABSOLUTE|:RELATIVE . parts).
  (%parse-physical-directory (if (stringp pathname) pathname (namestring pathname))))

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

(defun user-homedir-pathname (&optional host)
  "Return the pathname of the user's home directory ($HOME).  A leading ~/ in a
   namestring abbreviates this (e.g. (PATHNAME \"~/x\") = (MERGE-PATHNAMES \"x\"
   (USER-HOMEDIR-PATHNAME)))."
  (declare (ignore host))
  (pathname "~/"))

(defun directory-namestring (pathname)
  ;; The directory portion of the namestring AS A STRING (e.g. "/usr/local/"),
  ;; not the structured list that PATHNAME-DIRECTORY returns.
  (let* ((s (if (stringp pathname) pathname (namestring pathname)))
         (pos (%path-dir-end s)))
    (if (< pos 0) "" (subseq s 0 (+ pos 1)))))

(defun host-namestring (p)
  (setq p nil) "")

(defun enough-namestring (pathname &optional defaults)
  (setq defaults nil)
  (if (stringp pathname) pathname (namestring pathname)))

(defun %dir-merge (pdir ddir)
  ;; Merge a directory component PDIR over the defaults DDIR (CLHS 19.2.3):
  ;; an absent or :ABSOLUTE PDIR is taken as-is; a :RELATIVE PDIR has its
  ;; components appended onto DDIR (keeping DDIR's :ABSOLUTE/:RELATIVE head).
  (cond ((null pdir)                  ddir)
        ((eq (car pdir) :absolute)    pdir)
        ((null ddir)                  pdir)
        (t                            (cons (car ddir)
                                            (append (cdr ddir) (cdr pdir))))))

(defun merge-pathnames (pathname &optional (defaults "") default-version)
  ;; Fill in unsupplied components of PATHNAME from DEFAULTS (CLHS 19.2.3),
  ;; using the structured directory component.  (DEFAULTS defaults to "" here so
  ;; this physical version is safe to call before *DEFAULT-PATHNAME-DEFAULTS* is
  ;; defined; the logical-aware wrapper below supplies the proper default.)
  (declare (ignore default-version))
  (let* ((p    (pathname pathname))
         (d    (pathname defaults))
         (dir  (%dir-merge (pathname-directory p) (pathname-directory d)))
         (name (or (pathname-name p) (pathname-name d)))
         (type (or (pathname-type p) (pathname-type d))))
    (make-pathname :directory dir :name name :type type
                   :device (pathname-device p) :host (pathname-host p))))

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
  ;; Write (apply function (i-th of each SEQUENCE)) into result-sequence[i].
  ;; For a fill-pointered result the limit is ARRAY-TOTAL-SIZE (not the current
  ;; fill pointer) and the fill pointer is then set to the number stored (CLHS).
  (let* ((fp?  (and (vectorp result-sequence)
                    (array-has-fill-pointer-p result-sequence)))
         (cap  (if fp? (array-total-size result-sequence) (length result-sequence)))
         ;; The count is the minimum of the result capacity and the source
         ;; lengths; with NO source sequences there is nothing to map, so the
         ;; result is left untouched (SACLA must-sequence form-272/274).
         (n    (if sequences
                   (reduce #'min (cons cap (mapcar #'length sequences)))
                   0)))
    (when fp? (setf (fill-pointer result-sequence) n))
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

(defun %pkg-accessible-symbols (package)
  "All symbols accessible in PACKAGE: its present symbols plus the external
   symbols inherited (and not shadowed) from the packages it uses."
  (let* ((pkg (find-package package))
         (result (copy-list (package-symbols pkg)))
         ;; EQ hash set of symbols already collected, so the inherited-symbol
         ;; dedup is O(1) per symbol rather than a linear MEMBER over RESULT —
         ;; the latter makes DO-SYMBOLS over a CL-inheriting package O(n^2)
         ;; (n ~ 1000), which dominates UIOP/ASDF's DEFINE-PACKAGE load time.
         (seen (make-hash-table :test 'eq)))
    (dolist (s result) (setf (gethash s seen) t))
    (dolist (used (package-use-list pkg) result)
      (dolist (s (package-exported-symbols used))
        (when (and (not (gethash s seen))
                   (eq s (find-symbol (symbol-name s) pkg)))
          (setf (gethash s seen) t)
          (push s result))))))

(defmacro do-symbols (varspec &body body)
  ;; (DO-SYMBOLS (var [package [result]]) body...)
  ;; Bind VAR to each symbol accessible in PACKAGE (default *package*) — its own
  ;; present symbols and those inherited from used packages.  Returns RESULT.
  (let ((var     (first  varspec))
        (package (or (second varspec) '*package*))
        (result  (third  varspec)))
    `(dolist (,var (%pkg-accessible-symbols ,package) ,result)
       ,@body)))

(defmacro do-external-symbols (varspec &body body)
  ;; (DO-EXTERNAL-SYMBOLS (var [package [result]]) body...)
  ;; Bind VAR to each symbol exported from PACKAGE.
  (let ((var     (first  varspec))
        (package (or (second varspec) '*package*))
        (result  (third  varspec)))
    `(dolist (,var (package-exported-symbols ,package) ,result)
       ,@body)))

(defun %all-symbols-list ()
  "Every symbol present in every package (a symbol present in several packages may
   appear more than once — permitted by CLHS DO-ALL-SYMBOLS)."
  (let ((acc '()))
    (dolist (p (list-all-packages) acc)
      (dolist (s (package-symbols p)) (push s acc)))))

(defmacro do-all-symbols (varspec &body body)
  ;; (DO-ALL-SYMBOLS (var [result]) declaration* {tag|form}*)
  ;; CLHS: an implicit BLOCK NIL surrounds it (RETURN exits), the body is an implicit
  ;; TAGBODY (GO/tags work), and VAR is bound to NIL when RESULT is evaluated.  We do
  ;; NOT iterate with DOLIST: its own BLOCK NIL would capture the body's RETURN.
  (let ((var       (first  varspec))
        (result    (second varspec))
        (rest%     (gensym "REST-"))
        (top%      (gensym "TOP-"))
        (end%      (gensym "END-"))
        (real-body (%strip-leading-declares body)))
    `(block nil
       (let ((,var nil) (,rest% (%all-symbols-list)))
         (tagbody
            ,top%
            (when (null ,rest%) (go ,end%))
            (setq ,var (car ,rest%) ,rest% (cdr ,rest%))
            (tagbody ,@real-body)
            (go ,top%)
            ,end%)
         (setq ,var nil)
         ,result))))

(defun %make-package-iterator (packages symbol-types)
  "Return a thunk that yields (values found-p symbol accessibility package) for
   each requested symbol across PACKAGES, then (values nil nil nil nil)."
  (let ((items '()))
    (dolist (pd packages)
      (let ((p (find-package pd)))
        (when p
          (when (member :external symbol-types)
            (dolist (s (package-exported-symbols p)) (push (list s :external p) items)))
          (when (member :internal symbol-types)
            (dolist (s (package-symbols p))
              (multiple-value-bind (sym st) (find-symbol (symbol-name s) p)
                (declare (ignore sym))
                (when (eq st :internal) (push (list s :internal p) items)))))
          (when (member :inherited symbol-types)
            (dolist (u (package-use-list p))
              (dolist (e (package-exported-symbols u))
                (when (eq e (find-symbol (symbol-name e) p))
                  (push (list e :inherited p) items))))))))
    (lambda ()
      (if items
          (let ((triple (pop items)))
            (values t (first triple) (second triple) (third triple)))
          (values nil nil nil nil)))))

(defmacro with-package-iterator (spec &body body)
  ;; (WITH-PACKAGE-ITERATOR (name package-list . symbol-types) body...)
  ;; Within BODY, (name) returns (values found-p symbol accessibility package).
  (let ((name         (first spec))
        (packages     (second spec))
        (symbol-types (cddr spec))
        (stepper      (gensym "STEP-")))
    `(let ((,stepper (%make-package-iterator
                       (if (listp ,packages) ,packages (list ,packages))
                       ',symbol-types)))
       (macrolet ((,name () (list 'funcall ',stepper)))
         ,@body))))

;;;; ========================================================================
;;;; Slice 19 prerequisites — operators needed by CLOS
;;;; ========================================================================

(defmacro rotatef (&rest places)
  ;; (rotatef p1 p2 ... pn) — rotate values left: p1←p2, ..., pn←old-p1.
  ;; With 0 or 1 places: no-op.  Returns NIL.  Uses GET-SETF-EXPANSION so each
  ;; place's subforms are evaluated exactly once, in order, before any store
  ;; (correct even when places share structure).
  (if (< (length places) 2)
      nil
      (let ((sub-binds nil) (olds nil) (old-binds nil) (svars nil) (sforms nil))
        (dolist (p places)
          (multiple-value-bind (vars vals stores store-form access-form)
              (get-setf-expansion p)
            (setq sub-binds (append sub-binds (mapcar #'list vars vals)))
            (let ((og (gensym "ROT-OLD-")))
              (setq olds      (append olds (list og)))
              ;; capture ALL of the place's current values (a (VALUES ...) place
              ;; has several), so a multi-value place rotates correctly.
              (setq old-binds (append old-binds
                                      (list (list og `(multiple-value-list ,access-form))))))
            (setq svars  (append svars  (list stores)))
            (setq sforms (append sforms (list store-form)))))
        ;; place_i receives old_{i+1}; the last place receives old_1.
        (let ((newvals (append (cdr olds) (list (car olds)))))
          `(let* (,@sub-binds            ; capture every place subform once, in order
                  ,@old-binds)           ; read every old value before any store
             ,@(mapcar (lambda (sv val sf) `(multiple-value-bind ,sv (values-list ,val) ,sf))
                       svars newvals sforms)
             nil)))))

(defmacro pushnew (item place &rest keys)
  ;; (pushnew item place keys...) == (setf place (adjoin item place keys...)).
  ;; ADJOIN applies :KEY to ITEM and honours :TEST/:TEST-NOT, as CL requires.
  `(setf ,place (adjoin ,item ,place ,@keys)))

(defun get-properties (plist indicator-list)
  ;; Search PLIST for any key in INDICATOR-LIST.
  ;; Returns (values indicator value tail) for first match, else (nil nil nil).
  (loop
    (when (null plist) (return (values nil nil nil)))
    (let ((key (car plist)) (val (cadr plist)))
      (when (member key indicator-list :test #'eq)
        (return (values key val plist)))
      (setq plist (cddr plist)))))

(defun mapappend (fn list)
  ;; (mapappend fn list) = (apply #'append (mapcar fn list))
  (apply #'append (mapcar fn list)))

;; -------------------------------------------------------------------- ;;
;;  PROG / PROG* and the parallel/shifting assignment macros.           ;;
;; -------------------------------------------------------------------- ;;

(defmacro prog (varlist &body body)
  ;; (prog (vars) decl* {tag|statement}*) — parallel (LET) binding inside an
  ;; implicit BLOCK NIL and TAGBODY.  Returns NIL unless RETURN is used.
  ;; (bocl ignores declaration content, so leading declares are just dropped.)
  `(block nil
     (let ,varlist
       (tagbody ,@(%strip-declares body)))))

(defmacro prog* (varlist &body body)
  ;; Like PROG but sequential (LET*) binding.
  `(block nil
     (let* ,varlist
       (tagbody ,@(%strip-declares body)))))

(defmacro psetq (&rest pairs)
  ;; (psetq var1 val1 var2 val2 ...) — evaluate every value, THEN assign in
  ;; parallel.  Returns NIL.
  (let ((vars nil) (vals nil) (p pairs))
    (loop (when (null p) (return))
          (push (car p) vars) (push (cadr p) vals) (setq p (cddr p)))
    (setq vars (nreverse vars) vals (nreverse vals))
    (let ((temps (mapcar (lambda (v) v (gensym "PSET-")) vars)))
      `(let ,(mapcar #'list temps vals)
         ,@(mapcar (lambda (var tmp) `(setq ,var ,tmp)) vars temps)
         nil))))

(defmacro psetf (&rest pairs)
  ;; (psetf place1 val1 place2 val2 ...) — like PSETQ but for places.  Uses
  ;; GET-SETF-EXPANSION so each place's subforms are captured once (in order),
  ;; then every value is evaluated, then the stores run in parallel.  Returns NIL.
  (let ((places nil) (vals nil) (p pairs))
    (loop (when (null p) (return))
          (push (car p) places) (push (cadr p) vals) (setq p (cddr p)))
    (setq places (nreverse places) vals (nreverse vals))
    (let ((binds nil) (sforms nil))
      ;; Interleave per PAIR (CLHS left-to-right): place1 subforms, val1, place2
      ;; subforms, val2, ...  The store-form (run after all binds) refers only to
      ;; the captured temps, so the assignments are still parallel.
      (loop for place in places for val in vals do
        (multiple-value-bind (vars vs stores store-form access-form)
            (get-setf-expansion place)
          access-form
          (setq binds  (append binds (mapcar #'list vars vs)
                                     (list (list (car stores) val))))
          (setq sforms (append sforms (list store-form)))))
      `(let* (,@binds)
         ,@sforms
         nil))))

(defmacro shiftf (&rest args)
  ;; (shiftf place1 place2 ... placeN newvalue) — store place2's old value into
  ;; place1, place3's into place2, ..., NEWVALUE into placeN; return the OLD
  ;; value of place1.  Uses GET-SETF-EXPANSION so each place's subforms are
  ;; evaluated exactly once (correct even when places share structure, e.g.
  ;; (shiftf y x (cdr x) ...)).
  (let ((places (butlast args))
        (newval (car (last args)))
        (sub-binds nil)    ; (temp val) subform bindings, in place order
        (olds nil)         ; gensyms holding each place's old value
        (old-binds nil)    ; (old-gensym access-form)
        (svars nil)        ; per-place list of store variables
        (sforms nil))      ; per-place store-form
    (dolist (p places)
      (multiple-value-bind (vars vals stores store-form access-form)
          (get-setf-expansion p)
        (setq sub-binds (append sub-binds (mapcar #'list vars vals)))
        (let ((og (gensym "SHIFTF-OLD-")))
          (setq olds      (append olds (list og)))
          ;; capture ALL of the place's current values (a (VALUES ...) place has
          ;; several) so multi-value places shift correctly.
          (setq old-binds (append old-binds
                                  (list (list og `(multiple-value-list ,access-form))))))
        (setq svars  (append svars  (list stores)))
        (setq sforms (append sforms (list store-form)))))
    (let ((nv (gensym "SHIFTF-NEW-")))
      ;; place_i receives old_{i+1}; the last place receives NEWVALUE.
      (let ((newvals (append (cdr olds) (list nv))))
        `(let* (,@sub-binds            ; capture every place subform once
                ,@old-binds            ; read every old value before any store
                (,nv (multiple-value-list ,newval)))
           ,@(mapcar (lambda (sv val sf) `(multiple-value-bind ,sv (values-list ,val) ,sf))
                     svars newvals sforms)
           (values-list ,(car olds)))))))   ; return old value(s) of place 1

(defun %case-key-tests (tmp clause)
  ;; The COND test for a (CASE/CCASE) clause: (k ...) keys -> (or (eql tmp 'k) ...).
  (let ((keys (car clause)))
    (if (listp keys)
        `(or ,@(mapcar (lambda (k) `(eql ,tmp ',k)) keys))
        `(eql ,tmp ',keys))))

(defun %case-all-keys (clauses)
  ;; Flatten every key across CCASE clauses (for the TYPE-ERROR expected-type).
  (mapappend (lambda (c) (let ((k (car c))) (if (listp k) k (list k)))) clauses))

(defmacro ccase (keyplace &rest clauses)
  ;; Like CASE but: KEYPLACE is a place, it is exhaustive (no OTHERWISE), and a
  ;; non-match signals a correctable TYPE-ERROR with a STORE-VALUE restart that
  ;; sets KEYPLACE and retries.
  (let ((blk (gensym "CCASE-")) (tag (gensym "CCASE-TAG-")) (tmp (gensym "CCASE-VAL-")))
    `(block ,blk
       (tagbody
          ,tag
          (return-from ,blk
            (let ((,tmp ,keyplace))
              (cond
                ,@(mapcar (lambda (c) `(,(%case-key-tests tmp c) ,@(cdr c))) clauses)
                (t (restart-case
                       (error 'type-error :datum ,tmp
                              :expected-type '(member ,@(%case-all-keys clauses)))
                     (store-value (,tmp) (setf ,keyplace ,tmp) (go ,tag)))))))))))

(defmacro ctypecase (keyplace &rest clauses)
  ;; Like TYPECASE but exhaustive and correctable (TYPE-ERROR + STORE-VALUE).
  (let ((blk (gensym "CTYPECASE-")) (tag (gensym "CTC-TAG-")) (tmp (gensym "CTC-VAL-")))
    `(block ,blk
       (tagbody
          ,tag
          (return-from ,blk
            (let ((,tmp ,keyplace))
              (cond
                ,@(mapcar (lambda (c) `((typep ,tmp ',(car c)) ,@(cdr c))) clauses)
                (t (restart-case
                       (error 'type-error :datum ,tmp
                              :expected-type '(or ,@(mapcar #'car clauses)))
                     (store-value (,tmp) (setf ,keyplace ,tmp) (go ,tag)))))))))))

(defparameter lambda-list-keywords
  '(&optional &rest &key &allow-other-keys &aux &body &whole &environment))

;;; Minimal MAKE-ARRAY — creates a 1-D simple vector of SIZE elements.
;;; Uses the VECTOR built-in which takes &rest elements.
;;; Full MAKE-ARRAY (multi-dimensional, displaced, fill-pointer) is deferred.
(defun make-array (size &key (initial-element nil) initial-contents
                             element-type adjustable fill-pointer)
  "Create a 1-D simple vector of SIZE elements.
  Supports :initial-element and :initial-contents.
  :element-type, :adjustable and :fill-pointer are accepted but ignored."
  element-type adjustable fill-pointer   ; accepted, ignored
  (if initial-contents
      (apply #'vector (coerce initial-contents 'list))
      (let ((elts nil))
        (dotimes (i size)
          (push initial-element elts))
        (apply #'vector (nreverse elts)))))

;;;; ========================================================================
;;;; Slice 19 — Minimal CLOS
;;;; ========================================================================
;;;; An interpreter-level CLOS implementation — no compiler needed.
;;;; Classes, generic functions, and methods are all represented as
;;;; ordinary Lisp objects (DEFSTRUCTs) and closures.
;;;;
;;;; Supported:
;;;;   DEFCLASS, MAKE-INSTANCE, SLOT-VALUE / (SETF SLOT-VALUE),
;;;;   SLOT-BOUNDP, SLOT-MAKUNBOUND, DEFGENERIC, DEFMETHOD,
;;;;   CALL-NEXT-METHOD, NEXT-METHOD-P, CLASS-OF (extended),
;;;;   FIND-CLASS (extended), TYPEP (extended), CLASS-NAME,
;;;;   STANDARD-OBJECT-P, WITH-SLOTS, WITH-ACCESSORS, PRINT-OBJECT.
;;;; ========================================================================

;;; ---- 1. Unbound-slot sentinel -------------------------------------------

(defparameter *%unbound* (list '%clos-unbound)
  "Sentinel stored in slot vectors for slots that have not been initialised.")

;;; ---- 2. Internal representation structs ----------------------------------

(defstruct (%obj (:predicate %obj-p) (:copier nil))
  class    ; %cls object — the runtime class
  slots)   ; simple-vector; *%unbound* marks each unbound slot

(defstruct (%cls (:predicate %cls-p) (:copier nil))
  name          ; symbol
  dsupers       ; list of %cls objects (direct superclasses)
  dslots        ; list of slot-spec plists from DEFCLASS
  eslots        ; list of effective slot plists: (:name :location :initargs
                ;    :initform :initfunction :allocation :readers :writers)
  cpl           ; class precedence list (list of %cls, most-specific first)
  dsubs)        ; list of %cls objects (direct subclasses)

(defstruct (%mth (:predicate %mth-p) (:copier nil))
  qualifiers    ; list of qualifier symbols, nil for primary
  specializers  ; list, one per required arg: %cls | kernel-class-symbol | t
  function)     ; the underlying lambda

(defstruct (%gf (:predicate %gf-p) (:copier nil))
  name          ; symbol
  lambda-list   ; lambda-list as stored by DEFGENERIC
  methods)      ; list of %mth objects

;;; ---- 3. Registries -------------------------------------------------------

(defparameter *%classes* (make-hash-table))  ; symbol -> %cls

(defparameter *%gfs* (make-hash-table :test 'equal))  ; name -> %gf
;; EQUAL (not EQL) so a (SETF FOO) cons function-name keys consistently across
;; the freshly-read conses produced by DEFGENERIC / DEFMETHOD.

;;; Save kernel versions before overriding.
(defparameter *%kernel-find-class* (symbol-function 'find-class))
(defparameter *%kernel-class-of*   (symbol-function 'class-of))
(defparameter *%kernel-typep*      (symbol-function 'typep))

;;; Structure class objects for %cls and %obj — needed so we can check types
;;; WITHOUT going through our overridden TYPEP (which would cause infinite recursion,
;;; since DEFSTRUCT predicates call TYPEP internally).
;;; We get the class by looking up *defstruct-classes* which DEFSTRUCT populates.
(defparameter *%cls-sc* (gethash '%cls *defstruct-classes*)
  "Kernel structure class for CLOS class objects (%cls instances).")
(defparameter *%obj-sc* (gethash '%obj *defstruct-classes*)
  "Kernel structure class for CLOS instances (%obj instances).")

;;; Safe predicates that use the KERNEL typep — no circularity.
(defun %is-cls (x) (funcall *%kernel-typep* x *%cls-sc*))
(defun %is-obj (x) (funcall *%kernel-typep* x *%obj-sc*))

;;; Register the CLOS type bridge so the C kernel can dispatch CLOS conditions.
;;; After this, C class_of(clos-instance) returns the CLOS class, and
;;; C subclassp(clos-class, other) walks the CPL.
;;; IMPORTANT: After registration, %is-obj and %is-cls must use the bypass
;;; predicates (%clos-obj-p, %clos-cls-p) because *%kernel-typep* calls
;;; class_of which now returns the CLOS class, not the struct class.
(%clos-register *%obj-sc* *%cls-sc*)

;;; Switch to bypass predicates — these check x->class directly, not class_of.
(set-symbol-function (lambda (x) (%clos-obj-p x)) '%is-obj)
(set-symbol-function (lambda (x) (%clos-cls-p x)) '%is-cls)

;;; ---- 4. FIND-CLASS override ----------------------------------------------

(defun find-class (name &optional errorp environment)
  "Return the class named NAME.
  Checks user-defined CLOS classes first, then kernel built-in classes.
  Returns NIL if not found and ERRORP is nil (default)."
  environment  ; ignored
  (let ((user-cls (gethash name *%classes*)))
    (if user-cls
        user-cls
        ;; Try the kernel registry.  find_class(C) returns C NULL (not NIL_Symbol)
        ;; when the name is unknown — use %clos-null-p (a C helper) to detect it.
        (let ((k (funcall *%kernel-find-class* name)))
          (if (%clos-null-p k)
              (when errorp (error "No class named ~S" name))
              k)))))

(define-setf-expander find-class (name) (new-class)
  `(%set-find-class ,new-class ,name))

(defun %set-find-class (new-class name &optional errorp environment)
  errorp environment  ; ignored
  (if new-class
      (sethash name *%classes* new-class)
      (remhash name *%classes*))
  new-class)

;;; ---- 5. CLASS-OF override -----------------------------------------------

(defun class-of (object)
  "Return the class of OBJECT; handles CLOS instances and kernel primitives."
  (if (%is-obj object)
      (%obj-class object)
      (funcall *%kernel-class-of* object)))

;;; ---- 6. CLASS-NAME -------------------------------------------------------

(defun class-name (class)
  "Return the name of CLASS."
  (if (%is-cls class)
      (%cls-name class)
      nil))

(define-setf-expander class-name (class) (new-name)
  `(progn (when (%is-cls ,class) (setf (%cls-name ,class) ,new-name)) ,new-name))

;;; ---- 7. Predicates -------------------------------------------------------

(defun standard-object-p (object)
  "Return T if OBJECT is an instance of a user-defined CLOS class."
  (%is-obj object))

(defun standard-class-p (class)
  "Return T if CLASS is a CLOS class object created by DEFCLASS."
  (%is-cls class))

;;; ---- 8. TYPEP override ---------------------------------------------------

(defun typep (object type &optional environment)
  "Extended TYPEP: also handles user-defined CLOS classes.
  Uses *%kernel-typep* for the circularity-safe checks."
  environment  ; ignored
  (cond
    ((eq type t) t)
    ((eq type nil) nil)
    ;; Direct %cls object as type — use kernel typep to detect it (no circularity)
    ((%is-cls type)
     (and (%is-obj object)
          (not (null (member type (%cls-cpl (%obj-class object)))))))
    ;; Symbol — check user classes first, then kernel
    ((symbolp type)
     (let ((cls (gethash type *%classes*)))
       (if (%is-cls cls)
           (and (%is-obj object)
                (not (null (member cls (%cls-cpl (%obj-class object))))))
           (funcall *%kernel-typep* object type))))
    ;; Compound type specifier — delegate to kernel
    (t (funcall *%kernel-typep* object type))))

;;; X coerced to a CLOS class object: X itself if it is one, the class named X if
;;; X is a symbol naming a DEFCLASS'd class, else NIL.  Used by the CLOS-aware
;;; SUBTYPEP / coerce-class logic (the active SUBTYPEP is defined in Slice 30).
(defun %as-clos-class (x)
  (cond ((%is-cls x) x)
        ((symbolp x) (let ((c (gethash x *%classes*))) (and (%is-cls c) c)))
        (t nil)))

;;; ---- 9. SLOT-VALUE and friends ------------------------------------------

(defun %find-eslot (cls slot-name)
  "Find an effective slot plist for SLOT-NAME in class CLS, or NIL."
  (find slot-name (%cls-eslots cls) :key (lambda (s) (getf s :name))))

;; Read/write a slot's storage given its effective-slot plist.  An :INSTANCE slot
;; lives in the instance's slot vector at :LOCATION; a :CLASS slot is shared by all
;; instances and lives in a one-element box (:CELL) held in the (per-class) eslot.
(defun %eslot-ref (object eslot)
  (if (eq (getf eslot :allocation) :class)
      (car (getf eslot :cell))
      (svref (%obj-slots object) (getf eslot :location))))

(defun %eslot-set (object eslot value)
  (if (eq (getf eslot :allocation) :class)
      (setf (car (getf eslot :cell)) value)
      (setf (svref (%obj-slots object) (getf eslot :location)) value)))

(defun slot-value (object slot-name)
  "Return the value of slot SLOT-NAME in OBJECT."
  (unless (%is-obj object)
    (error "SLOT-VALUE: ~S is not a CLOS instance" object))
  (let* ((cls (%obj-class object))
         (eslot (%find-eslot cls slot-name)))
    (unless eslot
      (error "SLOT-VALUE: class ~S has no slot named ~S" (%cls-name cls) slot-name))
    (let ((val (%eslot-ref object eslot)))
      (if (eq val *%unbound*)
          (error "Unbound slot ~S in object of class ~S" slot-name (%cls-name cls))
          val))))

(defun %set-slot-value (new-value object slot-name)
  "Set slot SLOT-NAME of OBJECT to NEW-VALUE."
  (unless (%is-obj object)
    (error "(SETF SLOT-VALUE): ~S is not a CLOS instance" object))
  (let* ((cls (%obj-class object))
         (eslot (%find-eslot cls slot-name)))
    (unless eslot
      (error "(SETF SLOT-VALUE): class ~S has no slot named ~S" (%cls-name cls) slot-name))
    (%eslot-set object eslot new-value)
    new-value))

(define-setf-expander slot-value (object slot-name) (new-value)
  `(%set-slot-value ,new-value ,object ,slot-name))

(defun slot-boundp (object slot-name)
  "Return T if slot SLOT-NAME in OBJECT has been bound."
  (unless (%is-obj object)
    (error "SLOT-BOUNDP: ~S is not a CLOS instance" object))
  (let* ((cls (%obj-class object))
         (eslot (%find-eslot cls slot-name)))
    (unless eslot
      (error "SLOT-BOUNDP: class ~S has no slot ~S" (%cls-name cls) slot-name))
    (not (eq (%eslot-ref object eslot) *%unbound*))))

(defun slot-makunbound (object slot-name)
  "Make slot SLOT-NAME in OBJECT unbound."
  (unless (%is-obj object)
    (error "SLOT-MAKUNBOUND: ~S is not a CLOS instance" object))
  (let* ((cls (%obj-class object))
         (eslot (%find-eslot cls slot-name)))
    (unless eslot
      (error "SLOT-MAKUNBOUND: class ~S has no slot ~S" (%cls-name cls) slot-name))
    (%eslot-set object eslot *%unbound*)
    object))

(defun slot-exists-p (object slot-name)
  "Return T if OBJECT's class defines a slot named SLOT-NAME."
  (and (%is-obj object)
       (not (null (%find-eslot (%obj-class object) slot-name)))))

;;; ---- 10. Class Precedence List computation (AMOP algorithm) -------------

(defun %cls-all-supers (cls)
  "Return a list of all superclasses of CLS (including indirect) without duplicates."
  (let ((result nil))
    (labels ((collect (c)
               (dolist (super (%cls-dsupers c))
                 (when (%is-cls super)
                   (unless (member super result)
                     (push super result)
                     (collect super))))))
      (collect cls))
    result))

(defun %compute-cpl (cls)
  "Compute the class precedence list for CLS using the AMOP linearisation algorithm."
  (let* ((all-classes (cons cls (%cls-all-supers cls)))
         ;; Local-precedence ordering pairs: (A B) means A must precede B.
         (pairs (delete-duplicates
                 (mapappend (lambda (c)
                              (let ((ds (remove-if-not #'%is-cls (%cls-dsupers c))))
                                (mapcar #'list (cons c ds) ds)))
                            all-classes)
                 :test #'equal))
         (result nil)
         (remaining all-classes)
         (remaining-pairs pairs))
    (loop
      (when (null remaining) (return (nreverse result)))
      ;; Candidates: classes with no predecessor in remaining-pairs
      (let ((candidates (remove-if
                          (lambda (c) (find c remaining-pairs :key #'cadr))
                          remaining)))
        (when (null candidates)
          (error "DEFCLASS ~S: circular class hierarchy" (%cls-name cls)))
        ;; Prefer the class that appears first in the last-added class's dsupers
        (let ((next
                (or (and result
                         (find-if (lambda (c) (member c candidates))
                                  (remove-if-not #'%is-cls (%cls-dsupers (car result)))))
                    (car candidates))))
          (push next result)
          (setq remaining (remove next remaining))
          (setq remaining-pairs
                (remove-if (lambda (p) (eq (car p) next)) remaining-pairs)))))))

;;; ---- 11. Effective slot computation -------------------------------------

(defun %compute-eslots (cls)
  "Compute the effective slot definitions for CLS from its CPL.
  Each effective slot is a plist:
    (:name :location :initargs :initform :initfunction :allocation :readers :writers)
  Per CLHS 7.5.3, a slot defined by several classes in the CPL is MERGED into one
  effective slot: the union of :INITARGS (and :READERS / :WRITERS), and the
  most-specific (first in CPL) :INITFORM / :INITFUNCTION / :ALLOCATION.  (Taking
  only the first class's definition silently drops the others' :INITARGs — e.g. a
  slot given :INITARG by one superclass and re-declared bare by another would lose
  the initarg, leaving the slot unbindable via MAKE-INSTANCE.)"
  (let ((order nil)                       ; slot names, first (most-specific) first
        (groups (make-hash-table)))       ; name -> list of direct slot plists
    (dolist (c (%cls-cpl cls))
      (dolist (dslot (%cls-dslots c))
        (let ((name (getf dslot :name)))
          (multiple-value-bind (existing present) (gethash name groups)
            (declare (ignore existing))
            (unless present (push name order)))
          (sethash name groups (cons dslot (gethash name groups))))))
    (flet ((dedup (list test)            ; keep first occurrence, preserve order
             (let ((acc nil))
               (dolist (x list (nreverse acc))
                 (unless (member x acc :test test) (push x acc))))))
      (let ((result nil) (loc 0))
        (dolist (name (nreverse order))
          ;; GROUPS[name] was consed in CPL order, so its head is the LEAST specific;
          ;; reverse to visit most-specific first.
          (let ((initargs nil) (readers nil) (writers nil)
                (initform nil) (initfunction nil) (allocation nil))
            (dolist (d (reverse (gethash name groups)))
              (setq initargs (append initargs (getf d :initargs)))
              (setq readers  (append readers  (getf d :readers)))
              (setq writers  (append writers  (getf d :writers)))
              (when (and (null initfunction) (getf d :initfunction))
                (setq initform     (getf d :initform)
                      initfunction (getf d :initfunction)))
              (when (null allocation) (setq allocation (getf d :allocation))))
            (unless allocation (setq allocation :instance))
            (push (list :name name
                        :location loc
                        :initargs (dedup initargs #'eq)
                        :initform initform
                        :initfunction initfunction
                        :allocation allocation
                        ;; A :CLASS slot's single shared value lives in this box
                        ;; (one per class); :INSTANCE slots use :LOCATION instead.
                        :cell (when (eq allocation :class) (list *%unbound*))
                        :readers (dedup readers #'eq)
                        :writers (dedup writers #'equal))
                  result)
            (when (eq allocation :instance) (incf loc))))
        (nreverse result)))))

;;; ---- 12. Slot spec parsing -----------------------------------------------

(defun %parse-slot-spec (spec)
  "Parse a slot spec from DEFCLASS into a normalised plist.
  spec ::= symbol | (symbol [:option value]*)"
  (if (symbolp spec)
      (list :name spec :initargs nil :initform nil :initfunction nil
            :allocation :instance :readers nil :writers nil)
      ;; Use COND instead of CASE to avoid the macro-expansion cons-sharing
      ;; issue that corrupts %parse-slot-spec's body after GC.
      (let ((name (car spec))
            (initargs nil) (readers nil) (writers nil)
            (initform nil) (initfunction nil) (allocation :instance)
            (rest (cdr spec)))
        (loop
          (when (null rest) (return nil))
          (let ((key (car rest)) (val (cadr rest)))
            (cond ((eq key :initarg)    (push val initargs))
                  ((eq key :reader)     (push val readers))
                  ((eq key :writer)     (push val writers))
                  ((eq key :accessor)   (push val readers)
                                        (push (list 'setf val) writers))
                  ((eq key :initform)   (setq initform val
                                               ;; Capture the initform value in a closure (no EVAL needed)
                                               initfunction (let ((v val)) (lambda () v))))
                  ((eq key :allocation) (setq allocation val))))
          (setq rest (cddr rest)))
        (list :name name
              :initargs (nreverse initargs)
              :readers  (nreverse readers)
              :writers  (nreverse writers)
              :initform initform
              :initfunction initfunction
              :allocation allocation))))

;;; ---- 13. Class creation --------------------------------------------------

(defun ensure-clos-class (name dsuper-names dslot-specs)
  "Create or update the CLOS class named NAME.
  DSUPER-NAMES: list of symbols.  DSLOT-SPECS: list of parsed slot plists."
  ;; Resolve direct superclasses — default to standard-object if empty
  (let* ((supers
           (mapcar (lambda (sn)
                     (or (gethash sn *%classes*)
                         (error "DEFCLASS ~S: superclass ~S not found" name sn)))
                   dsuper-names))
         ;; Get or create the class object
         (cls (or (gethash name *%classes*)
                  (let ((c (make-%cls :name name :dsupers nil :dslots nil
                                      :eslots nil :cpl nil :dsubs nil)))
                    (sethash name *%classes* c)
                    c))))
    (setf (%cls-dsupers cls) supers)
    (setf (%cls-dslots  cls) dslot-specs)
    ;; Compute CPL and effective slots
    (setf (%cls-cpl   cls) (%compute-cpl cls))
    (setf (%cls-eslots cls) (%compute-eslots cls))
    ;; Register as subclass of each direct superclass
    (dolist (super supers)
      (when (%is-cls super)
        (pushnew cls (%cls-dsubs super))))
    cls))

;;; ---- 14. DEFCLASS macro --------------------------------------------------

(defmacro defclass (name direct-superclasses direct-slots &rest options)
  "Define a new CLOS class.
  Supports slot options: :initarg :initform :accessor :reader :writer :allocation.
  Class option :documentation is accepted but ignored."
  options  ; ignored
  (let* ((dsuper-names (if (null direct-superclasses) '(standard-object) direct-superclasses))
         ;; Pre-expand slot specs to extract accessor/reader/writer definitions
         (parsed-slots (mapcar (lambda (s) (%parse-slot-spec s)) direct-slots))
         (reader-forms nil)
         (writer-forms nil))
    ;; Collect reader/writer method definitions
    (dolist (pslot parsed-slots)
      (let ((sname (getf pslot :name)))
        (dolist (r (getf pslot :readers))
          (push `(defmethod ,r ((self ,name))
                   (slot-value self ',sname))
                reader-forms))
        (dolist (w (getf pslot :writers))
          (if (and (consp w) (eq (car w) 'setf))
              ;; Install a setf expander for the accessor name
              (push `(%install-clos-setf-expander ',(cadr w) ',sname)
                    writer-forms)
              ;; Named writer method: (writer new-val instance)
              (push `(defmethod ,w ((new-value t) (self ,name))
                       (setf (slot-value self ',sname) new-value))
                    writer-forms)))))
    `(progn
       (ensure-clos-class
        ',name ',dsuper-names
        (list ,@(mapcar
                 (lambda (ps)
                   ;; Build the slot plist with a REAL :initfunction closure that
                   ;; EVALUATES the initform per instance — so computed initforms
                   ;; like (MAKE-HASH-TABLE) or (LIST ...) work, not just self-
                   ;; evaluating literals.  (PARSED-SLOTS carries a non-NIL
                   ;; :initfunction iff the slot supplied an :initform.)
                   `(list :name ',(getf ps :name)
                          :initargs ',(getf ps :initargs)
                          :readers ',(getf ps :readers)
                          :writers ',(getf ps :writers)
                          :initform ',(getf ps :initform)
                          :initfunction ,(when (getf ps :initfunction)
                                           `(lambda () ,(getf ps :initform)))
                          :allocation ',(getf ps :allocation)))
                 parsed-slots)))
       ,@(nreverse reader-forms)
       ,@(nreverse writer-forms)
       (find-class ',name))))

(defun %install-clos-setf-expander (accessor-name slot-name)
  "Install (setf accessor-name) so (setf (accessor-name obj) val) works."
  (sethash accessor-name *setf-expanders*
           (let ((sn slot-name))
             (lambda (obj-form new-val-form)
               `(%set-slot-value ,new-val-form ,obj-form ',sn)))))

;;; ---- 15. Instance allocation --------------------------------------------

(defun %alloc-instance (cls)
  "Allocate an unbound CLOS instance of class CLS."
  (let* ((n (count-if (lambda (s) (eq (getf s :allocation) :instance))
                      (%cls-eslots cls)))
         (slots (make-array n :initial-element *%unbound*)))
    (make-%obj :class cls :slots slots)))

;;; ---- 16. CALL-NEXT-METHOD infrastructure --------------------------------

(defvar *%cnm-args*  nil "Current argument list for CALL-NEXT-METHOD.")
(defvar *%cnm-nexts* nil "Remaining next-method closures (callable, take &rest args).")
(defvar *%cnm-on*    nil "T when executing inside a method body.")

;; Helper: set the three CNM globals and call THUNK, restoring afterwards.
;; Uses explicit setq since bocl's LET creates lexical (not dynamic) bindings.
(defun %cnm-call (on nexts args thunk)
  (let ((old-on    *%cnm-on*)
        (old-nexts *%cnm-nexts*)
        (old-args  *%cnm-args*))
    (setq *%cnm-on* on *%cnm-nexts* nexts *%cnm-args* args)
    ;; MULTIPLE-VALUE-PROG1 so a method body's full values propagate through the
    ;; dispatch (CLHS 7.6.6 — a generic function returns the values of the method);
    ;; a plain (let ((r (funcall thunk))) r) would truncate to the primary value
    ;; (e.g. breaks GETHASH-returning methods like ASDF COMPONENT-OPERATION-TIME).
    (multiple-value-prog1 (funcall thunk)
      (setq *%cnm-on* old-on *%cnm-nexts* old-nexts *%cnm-args* old-args))))

(defun call-next-method (&rest explicit-args)
  "Within a method body: call the next applicable method.
  With no args: passes the original arguments.  With args: passes those instead."
  (unless *%cnm-on*
    (error "CALL-NEXT-METHOD: called outside a method body"))
  (let ((next (car *%cnm-nexts*)))
    (unless next
      (error "CALL-NEXT-METHOD: no next method"))
    (let ((actual-args (if explicit-args explicit-args *%cnm-args*)))
      (%cnm-call t (cdr *%cnm-nexts*) actual-args
                 (lambda () (apply next actual-args))))))

(defun next-method-p ()
  "Return T if CALL-NEXT-METHOD would find a method."
  (and *%cnm-on* (not (null *%cnm-nexts*))))

;;; ---- 17. Method applicability and specificity ---------------------------

(defun %applicable-p (specializer arg)
  "Return T if ARG matches SPECIALIZER."
  (cond
    ((eq specializer t) t)
    ;; CLOS class — use %is-cls (kernel typep, no circularity)
    ((%is-cls specializer)
     (and (%is-obj arg)
          (not (null (member specializer (%cls-cpl (%obj-class arg)))))))
    ;; Built-in type symbol
    ((symbolp specializer)
     (funcall *%kernel-typep* arg specializer))
    (t nil)))

(defun %all-applicable-p (specs args)
  "Return T if every specializer in SPECS matches the corresponding arg in ARGS."
  (let ((ss specs) (as args))
    (loop
      (when (null ss) (return t))
      (unless (%applicable-p (car ss) (car as)) (return nil))
      (setq ss (cdr ss) as (cdr as)))))

(defun %applicable-methods (gf args)
  "Return all methods of GF whose specializers match ARGS."
  (remove-if-not
   (lambda (m)
     (let* ((specs (%mth-specializers m))
            (n (length specs))
            (req-args (if (> (length args) n) (subseq args 0 n) args)))
       (%all-applicable-p specs req-args)))
   (%gf-methods gf)))

(defun %specializer-position (spec arg)
  "Return the CPL position of SPEC for ARG (lower = more specific), or 9999."
  (cond
    ((eq spec t) 9999)
    ((%is-cls spec)
     (if (and (%is-obj arg) (%cls-cpl (%obj-class arg)))
         (let ((pos (position spec (%cls-cpl (%obj-class arg)))))
           (or pos 9999))
         9999))
    ((symbolp spec)
     ;; Built-in type — use subtype relationships to rank (approx)
     (if (funcall *%kernel-typep* arg spec) 1 9999))
    (t 9999)))

(defun %spec-more-specific-p (s1 s2 arg)
  "Return T if specializer S1 is strictly more specific than S2 for ARG."
  (cond
    ((eq s1 s2) nil)
    ((eq s1 t)  nil)                 ; T is least specific
    ((eq s2 t)  t)
    ((and (%is-cls s1) (%is-cls s2))
     (< (%specializer-position s1 arg) (%specializer-position s2 arg)))
    ((and (symbolp s1) (symbolp s2))
     ;; Built-in / condition type names: rank by the kernel class hierarchy, so
     ;; e.g. a method on SIMPLE-ERROR overrides one on CONDITION.
     (let ((c1 (funcall *%kernel-find-class* s1))
           (c2 (funcall *%kernel-find-class* s2)))
       (and (not (%clos-null-p c1)) (not (%clos-null-p c2))
            (not (eq c1 c2))
            (subclassp c1 c2))))
    ((%is-cls s1) t)                 ; a CLOS class beats a bare built-in symbol
    ((%is-cls s2) nil)
    (t nil)))

(defun %method-more-specific-p (m1 m2 args)
  "Return T if M1 is more specific than M2 for ARGS."
  (loop for s1 in (%mth-specializers m1)
        for s2 in (%mth-specializers m2)
        for arg in args
        unless (eq s1 s2)
        do (return (%spec-more-specific-p s1 s2 arg))
        finally (return nil)))

(defun %sort-methods (methods args)
  "Sort METHODS by specificity for ARGS (most specific first)."
  (sort (copy-list methods)
        (lambda (m1 m2) (%method-more-specific-p m1 m2 args))))

;;; ---- 18. Standard method combination / dispatch -------------------------

(defun %invoke-effective-method (sorted-methods args)
  "Implement standard method combination for SORTED-METHODS and ARGS."
  (let ((arounds  (remove-if-not
                   (lambda (m) (equal (%mth-qualifiers m) '(:around))) sorted-methods))
        (befores  (remove-if-not
                   (lambda (m) (equal (%mth-qualifiers m) '(:before))) sorted-methods))
        (primaries (remove-if
                    (lambda (m) (%mth-qualifiers m)) sorted-methods))
        (afters   (nreverse
                   (remove-if-not
                    (lambda (m) (equal (%mth-qualifiers m) '(:after))) sorted-methods))))

    (unless primaries
      (error "No applicable primary method for args ~S" args))

    ;; Build the primary chain: most-specific primary calls less specific via
    ;; *%cnm-nexts*.  Use %cnm-call (setq-based) since bocl LET = lexical only.
    (let* ((primary-fns (mapcar #'%mth-function primaries))

           ;; Core thunk: runs before*, primary chain, after*
           (core-thunk
             (lambda (&rest core-args)
               (let ((actual (if core-args core-args args)))
                 ;; Run befores (no cnm)
                 (dolist (m befores)
                   (%cnm-call t nil actual
                               (let ((mf (%mth-function m)))
                                 (lambda () (apply mf actual)))))
                 ;; Run primary chain, preserving ALL its values (CLHS 7.6.6); the
                 ;; afters run for effect afterwards, then the primary values are
                 ;; returned — MULTIPLE-VALUE-PROG1 captures them across the afters.
                 (multiple-value-prog1
                     (%cnm-call t (cdr primary-fns) actual
                                (let ((fn (car primary-fns)))
                                  (lambda () (apply fn actual))))
                   ;; Run afters (reversed; no cnm)
                   (dolist (m afters)
                     (%cnm-call t nil actual
                                 (let ((mf (%mth-function m)))
                                   (lambda () (apply mf actual))))))))))

      ;; Wrap with arounds: each around's next is the following around or core
      (if (null arounds)
          (apply core-thunk args)
          (labels ((make-chain (around-fns)
                     (if (null around-fns)
                         core-thunk
                         (let* ((next    (make-chain (cdr around-fns)))
                                (this-fn (%mth-function (car around-fns))))
                           (lambda (&rest a)
                             (let ((actual (if a a args)))
                               (%cnm-call t (list next) actual
                                           (lambda () (apply this-fn actual)))))))))
            (apply (make-chain arounds) args))))))

;;; ---- 19. Generic function installation ----------------------------------

(defparameter *%gf-by-fn* (make-hash-table :test 'eq)) ; discriminating-fn -> %gf

(defun %install-discriminating-function (name gf)
  "Install a discriminating function for GF as the global function binding of NAME."
  (let ((df (lambda (&rest args)
              (let* ((applicable (%applicable-methods gf args))
                     (sorted     (%sort-methods applicable args)))
                (when (null sorted)
                  (error "No applicable method for generic function ~S with arguments ~S"
                         name args))
                (%invoke-effective-method sorted args)))))
    (sethash df *%gf-by-fn* gf)   ; so FIND-METHOD can map #'gf back to its %gf
    ;; A (SETF accessor) generic function is installed as ACCESSOR's setf-function
    ;; (consulted by the SETF macro), not as a symbol-function.
    (if (and (consp name) (eq (car name) 'setf))
        (%register-setf-function (cadr name) df)
        (set-symbol-function df name))))

;;; ---- 20. ENSURE-GENERIC-FUNCTION and ENSURE-METHOD ---------------------

(defun ensure-generic-function (name &key lambda-list)
  "Return the generic function named NAME, creating it if needed."
  (let ((existing (gethash name *%gfs*)))
    (if existing
        (progn
          (when lambda-list (setf (%gf-lambda-list existing) lambda-list))
          existing)
        (let ((gf (make-%gf :name name :lambda-list lambda-list :methods nil)))
          (sethash name *%gfs* gf)
          (%install-discriminating-function name gf)
          gf))))

(defun ensure-method (gf-name qualifiers specializer-names fn)
  "Add a method to the generic function GF-NAME.
  QUALIFIERS: list of qualifier keywords.
  SPECIALIZER-NAMES: list of class names (symbols), class objects, or T.
  FN: the method lambda."
  ;; Resolve specializer names to class objects
  (let* ((specializers
           (mapcar (lambda (s)
                     (cond ((eq s t) t)
                           ((%is-cls s) s)
                           ((symbolp s)
                            (or (gethash s *%classes*)
                                s))          ; keep the symbol so %applicable-p
                                             ; dispatches via the kernel TYPEP
                                             ; (handles built-in & condition classes)
                           (t s)))
                   specializer-names))
         (method (make-%mth :qualifiers qualifiers
                             :specializers specializers
                             :function fn))
         (gf (or (gethash gf-name *%gfs*)
                 (let ((new (make-%gf :name gf-name :lambda-list nil :methods nil)))
                   (sethash gf-name *%gfs* new)
                   new))))
    ;; Remove any existing method with the same qualifiers + specializers
    (setf (%gf-methods gf)
          (remove-if (lambda (m)
                       (and (equal (%mth-qualifiers m) qualifiers)
                            (equal (%mth-specializers m) specializers)))
                     (%gf-methods gf)))
    (push method (%gf-methods gf))
    (%install-discriminating-function gf-name gf)
    gf-name))

;;; ---- 21. Lambda list parsing helpers ------------------------------------

(defun %parse-spec-ll (specialized-ll)
  "Parse a specialised lambda list from DEFMETHOD.
  Returns (values plain-lambda-list specializer-names).
  Only required parameters (before any lambda-list keyword) are specialised."
  (let ((plain-ll nil) (spec-names nil) (in-required t))
    (dolist (param specialized-ll)
      (cond
        ((member param lambda-list-keywords)
         (push param plain-ll)
         (setq in-required nil))
        (in-required
         (if (consp param)
             (progn (push (car param) plain-ll)
                    (push (cadr param) spec-names))
             (progn (push param plain-ll)
                    (push t spec-names))))
        (t (push param plain-ll))))
    (values (nreverse plain-ll) (nreverse spec-names))))

;;; ---- 22. DEFGENERIC and DEFMETHOD macros --------------------------------

(defmacro defgeneric (name lambda-list &rest options)
  "Define a generic function NAME with LAMBDA-LIST.  (:method qualifiers*
specialized-lambda-list body...) options define methods; other options (documen-
tation, :method-combination, ...) are ignored."
  (let ((forms (list `(ensure-generic-function ',name :lambda-list ',lambda-list))))
    (dolist (opt options)
      (when (and (consp opt) (eq (car opt) :method))
        (push `(defmethod ,name ,@(cdr opt)) forms)))
    `(progn ,@(nreverse forms) ',name)))

(defun %method-ll-allow-other-keys (ll)
  "If method lambda-list LL has &KEY but not &ALLOW-OTHER-KEYS, insert
   &ALLOW-OTHER-KEYS (before any &AUX).  CLHS 7.6.5: a method must accept the
   keyword arguments accepted by the generic function as a whole — i.e. those
   named by ANY applicable method — even ones it does not itself name.  bocl
   validates a method lambda's keywords individually, so without this an
   otherwise-valid call (e.g. ASDF passing :VERBOSE to a PERFORM method whose
   sibling method declares :VERBOSE) is wrongly rejected as an unknown keyword."
  (if (and (member '&key ll) (not (member '&allow-other-keys ll)))
      (let ((aux (position '&aux ll)))
        (if aux
            (append (subseq ll 0 aux) '(&allow-other-keys) (subseq ll aux))
            (append ll '(&allow-other-keys))))
      ll))

(defmacro defmethod (name &rest args)
  "Define a method for generic function NAME.
  Syntax: (defmethod name [qualifier*] (specialised-lambda-list) body...)"
  ;; Collect qualifiers (keywords before the lambda list)
  (let ((qualifiers nil)
        (rest args))
    (loop while (and (consp rest) (keywordp (car rest)))
          do (push (pop rest) qualifiers))
    (setq qualifiers (nreverse qualifiers))
    (let* ((specialized-ll (car rest))
           (body (cdr rest)))
      (multiple-value-bind (plain-ll spec-names)
          (%parse-spec-ll specialized-ll)
        `(ensure-method ',name ',qualifiers ',spec-names
                         (lambda ,(%method-ll-allow-other-keys plain-ll) ,@body))))))

;;; DEFINE-METHOD-COMBINATION — bocl supports only the standard method
;;; combination, so this records nothing operational; it accepts the syntax and
;;; returns the name so programs that define (but don't rely on) custom
;;; combinations still load.
(defmacro define-method-combination (name &rest args)
  (declare (ignore args))
  `',name)

;;; FIND-METHOD — locate a method of GF with the given QUALIFIERS and
;;; SPECIALIZERS (class objects, as from FIND-CLASS).  GF may be a generic
;;; function object, its name, or the %gf.  Specializers are matched by name so a
;;; class object and the symbol a method was specialised on compare equal.
(defun %spec-as-class (s)
  "Normalise a method specializer (a class object, a class-name symbol, or T) to
the class object it denotes, so two specializers compare by class identity."
  (cond ((eq s t)     t)
        ((symbolp s)  (or (find-class s nil) s))
        (t            s)))
(defun find-method (gf qualifiers specializers &optional (errorp t))
  (let ((g (cond ((%gf-p gf)    gf)
                 ((symbolp gf)  (gethash gf *%gfs*))
                 (t             (gethash gf *%gf-by-fn*)))))
    (if (null g)
        (if errorp (error "FIND-METHOD: ~S is not a generic function" gf) nil)
        (let ((m (find-if
                  (lambda (mth)
                    (and (equal (%mth-qualifiers mth) qualifiers)
                         (let ((ms (%mth-specializers mth)))
                           (and (= (length ms) (length specializers))
                                (every (lambda (a b) (eq (%spec-as-class a) (%spec-as-class b)))
                                       ms specializers)))))
                  (%gf-methods g))))
          (or m (if errorp (error "FIND-METHOD: no such method for ~S" gf) nil))))))

;;; ---- 23. SHARED-INITIALIZE and MAKE-INSTANCE ---------------------------

(defun shared-initialize (instance slot-names &rest initargs)
  "Initialise slots of INSTANCE from INITARGS and slot initforms.
  SLOT-NAMES: T = all slots, or a list of slot names to initialise."
  (unless (%is-obj instance)
    (error "SHARED-INITIALIZE: ~S is not a CLOS instance" instance))
  (let* ((cls (%obj-class instance))
         (eslots (%cls-eslots cls)))
    (dolist (eslot eslots instance)
      (let ((name (getf eslot :name)))
        (multiple-value-bind (ikey ivalue found)
            (get-properties initargs (or (getf eslot :initargs) nil))
          ikey  ; not used
          (cond
            ;; A supplied initarg ALWAYS sets the slot, regardless of SLOT-NAMES
            ;; (CLHS 7.1.3 step 1).
            (found (%eslot-set instance eslot ivalue))
            ;; Otherwise fill from the initform only when the slot is named by
            ;; SLOT-NAMES (T = all) and is currently unbound (step 2/3).
            ((and (or (eq slot-names t) (member name slot-names))
                  (eq (%eslot-ref instance eslot) *%unbound*)
                  (getf eslot :initfunction))
             (%eslot-set instance eslot
                         (funcall (getf eslot :initfunction))))))))
    instance))

(defun make-instance (class-designator &rest initargs)
  "Create and return a new instance of CLASS-DESIGNATOR.
  INITARGS are keyword-value pairs for slot initialisation."
  (let* ((cls (cond ((%is-cls class-designator) class-designator)
                    ((symbolp class-designator)
                     (or (gethash class-designator *%classes*)
                         (error "MAKE-INSTANCE: no class ~S" class-designator)))
                    (t (error "MAKE-INSTANCE: ~S is not a class designator"
                              class-designator))))
         (instance (%alloc-instance cls)))
    (apply #'initialize-instance instance initargs)
    instance))

;;; ---- 24. WITH-SLOTS and WITH-ACCESSORS ----------------------------------

(defmacro with-slots (slot-specs instance-form &body body)
  "Execute BODY with slots bound as symbol-macros to slot-value accessors.
  Each element of SLOT-SPECS is either a slot-name symbol or (var slot-name)."
  (let ((inst (gensym "INST-")))
    `(let ((,inst ,instance-form))
       (symbol-macrolet
           ,(mapcar (lambda (spec)
                      (if (symbolp spec)
                          `(,spec (slot-value ,inst ',spec))
                          `(,(car spec) (slot-value ,inst ',(cadr spec)))))
                    slot-specs)
         ,@body))))

(defmacro with-accessors (accessor-specs instance-form &body body)
  "Execute BODY with accessors bound as symbol-macros.
  Each spec is (var accessor-name) or accessor-name (using itself as var)."
  (let ((inst (gensym "INST-")))
    `(let ((,inst ,instance-form))
       (symbol-macrolet
           ,(mapcar (lambda (spec)
                      (if (symbolp spec)
                          `(,spec (,spec ,inst))
                          `(,(car spec) (,(cadr spec) ,inst))))
                    accessor-specs)
         ,@body))))

;;; ---- 25. PRINT-OBJECT integration --------------------------------------

;;; Override PRIN1 to call PRINT-OBJECT for CLOS instances.
;;; Uses %is-obj (kernel-based check) to avoid circularity.
;;; Guards against re-entry to avoid infinite recursion.
(defparameter *%kernel-prin1* (symbol-function 'prin1))
(defparameter *%in-print-object* nil)

(set-symbol-function
 (lambda (object &optional (stream *standard-output*))
   (if (and (%is-obj object) (not *%in-print-object*))
       (let ((*%in-print-object* t))
         (print-object object stream))
       (funcall *%kernel-prin1* object stream)))
 'prin1)

;;; ---- 26. Bootstrap the core class hierarchy ----------------------------

;;; Register STANDARD-OBJECT — the root of all user-defined CLOS classes.
(let ((so (make-%cls :name 'standard-object
                     :dsupers nil :dslots nil :eslots nil
                     :cpl nil :dsubs nil)))
  (setf (%cls-cpl so) (list so))
  (sethash 'standard-object *%classes* so))

;;; Define INITIALIZE-INSTANCE generic function and default method.
(defgeneric initialize-instance (instance &rest initargs))
(ensure-method 'initialize-instance nil '(standard-object)
               (lambda (instance &rest initargs)
                 (apply #'shared-initialize instance t initargs)
                 instance))

;;; REINITIALIZE-INSTANCE (CLHS 7.3).  The system-supplied primary method calls
;;; SHARED-INITIALIZE with slot-names NIL — no slots are initialised from their
;;; initforms; only slots whose initargs were supplied are set — and returns the
;;; instance.  A generic function so user / library code (e.g. ASDF, which
;;; reinitialises a SYSTEM in RESET-SYSTEM-CLASS) can add :before/:after methods.
(defgeneric reinitialize-instance (instance &rest initargs))
(ensure-method 'reinitialize-instance nil '(standard-object)
               (lambda (instance &rest initargs)
                 (apply #'shared-initialize instance nil initargs)
                 instance))

;;; shared-initialize is kept as a plain function (not a GF) to avoid
;;; infinite recursion: the default initialize-instance method calls it,
;;; and a GF method would call itself recursively.

;;; PRINT-OBJECT — default prints #<ClassName>.
(defgeneric print-object (object stream))
(ensure-method 'print-object nil '(standard-object t)
               (lambda (object stream)
                 (format stream "#<~A>" (%cls-name (%obj-class object)))))

;;; PRINT-OBJECT for conditions.  Conditions are kernel objects (not %obj
;;; instances), so the C printer dispatches them here explicitly.  With
;;; *print-escape* nil (PRINC, ~A) a simple condition prints its formatted
;;; report; with *print-escape* true (PRIN1, ~S) it is wrapped #<CLASS report>.
(ensure-method 'print-object nil '(condition t)
               (lambda (c stream)
                 (if (typep c 'simple-condition)
                     (if *print-escape*
                         (format stream "#<~A ~?>" (type-of c)
                                 (simple-condition-format-control c)
                                 (simple-condition-format-arguments c))
                         (format stream "~?"
                                 (simple-condition-format-control c)
                                 (simple-condition-format-arguments c)))
                     ;; A non-simple kernel condition may still carry a format-control
                     ;; (e.g. the STORAGE-CONDITION signalled on stack overflow); show
                     ;; it when present, else just the type.
                     (let ((fc (ignore-errors (simple-condition-format-control c))))
                       (if (stringp fc)
                           (format stream "#<~A ~?>" (type-of c) fc
                                   (or (ignore-errors (simple-condition-format-arguments c)) '()))
                           (format stream "#<~A>" (type-of c)))))
                 c))

;;; PRINT-OBJECT for PACKAGE.  A package is a kernel object (not a %obj instance),
;;; so the C printer routes it here via the PRINT-OBJECT generic — letting user
;;; methods dispatch and giving the readable-looking #<PACKAGE "name"> instead of
;;; #<PACKAGE 0xADDR>.  A package has no read syntax, so under *PRINT-READABLY* it
;;; signals PRINT-NOT-READABLE (CLHS).
(ensure-method 'print-object nil '(package t)
               (lambda (p stream)
                 (if *print-readably*
                     (error 'print-not-readable :object p)
                     (format stream "#<~A ~S>" (type-of p) (package-name p)))
                 p))

;;; Default PRINT-OBJECT method (T T).  The C printer now routes EVERY non-CLOS
;;; kernel object (streams, hash-tables, readtables, random-states, classes, ...)
;;; through PRINT-OBJECT so a user method can dispatch on any object type.  Objects
;;; with no more specific method fall back here, which re-enters the kernel printer;
;;; its recursion guard is active while this method runs, so PRIN1/PRINC reach the
;;; raw #<CLASS id> form (with the object's identity) rather than re-dispatching.
(ensure-method 'print-object nil '(t t)
               (lambda (object stream)
                 (if *print-escape*
                     (prin1 object stream)
                     (princ object stream))
                 object))

;;; Register the standard CONDITION class hierarchy as CLOS classes, so that
;;; (DEFCLASS foo (ERROR) ...) and DEFINE-CONDITION can inherit them (FIND-CLASS
;;; and ENSURE-CLOS-CLASS resolve superclasses from *%classes*).  These are SEPARATE
;;; from the kernel condition classes that HANDLER-CASE and TYPEP use via the C
;;; find_class / *%kernel-typep* (kernel lookup takes priority), so kernel-condition
;;; signalling, catching and printing are unaffected.  Registered AFTER the kernel
;;; print-object method above so that method keeps its kernel-symbol specializer.
(dolist (%cspec '((condition standard-object)
                  (serious-condition condition)
                  (warning condition)
                  (simple-condition condition)
                  (style-warning warning)
                  (storage-condition serious-condition)
                  (error serious-condition)
                  (simple-error error simple-condition)
                  (simple-warning warning simple-condition)
                  (type-error error)
                  (simple-type-error type-error simple-condition)
                  (program-error error)
                  (control-error error)
                  (stream-error error)
                  (end-of-file stream-error)
                  (file-error error)
                  (cell-error error)
                  (unbound-variable cell-error)
                  (undefined-function cell-error)
                  (unbound-slot cell-error)
                  (arithmetic-error error)
                  (division-by-zero arithmetic-error)
                  (floating-point-overflow arithmetic-error)
                  (floating-point-underflow arithmetic-error)
                  (floating-point-inexact arithmetic-error)
                  (floating-point-invalid-operation arithmetic-error)
                  (parse-error error)
                  (reader-error parse-error stream-error)
                  (print-not-readable error)
                  (package-error error)))
  (ensure-clos-class (car %cspec) (cdr %cspec) nil))

;;; DESCRIBE-OBJECT — stub; used by some code
(defgeneric describe-object (object stream))
(ensure-method 'describe-object nil '(standard-object t)
               (lambda (object stream)
                 (format stream "~A is an instance of ~A~%"
                         object (%cls-name (%obj-class object)))))

;;;; ========================================================================
;;;; Slice 20 — Standard library completions: function utilities, type
;;;;   system (DEFTYPE), string utilities, condition utilities, restart
;;;;   invokers, numeric (BOOLE, LOGBITP, float introspection), I/O,
;;;;   and miscellaneous missing standard functions.
;;;; ========================================================================

;;; ---- Function utilities ---------------------------------------------------

(defun complement (fn)
  "Return a function that is the logical complement of FN."
  (lambda (&rest args) (not (apply fn args))))

(defun constantly (value)
  "Return a function that always returns VALUE regardless of its arguments."
  (let ((v value))
    (lambda (&rest args) args v)))   ; capture value; args ignored

;;; ---- Type system: DEFTYPE -------------------------------------------------

(defparameter *%deftype-expanders* (make-hash-table)
  "Hash table mapping type-name symbol -> expander function.")

(defmacro deftype (name lambda-list &body body)
  "Define a type abbreviation NAME.  The BODY form produces a type specifier.
  Lambda-list parameters default to * (the wildcard)."
  `(progn
     (sethash ',name *%deftype-expanders*
              (lambda ,lambda-list ,@body))
     ',name))

;;; Extend TYPEP to handle DEFTYPE abbreviations and compound type specifiers.
;;; We chain: DEFTYPE → expand → recurse; compound types → dispatch; kernel → kernel typep.
(let ((old-typep (symbol-function 'typep)))
  (set-symbol-function
   (lambda (object type &optional env)
     env
     ;; Local alias for recursive calls through the current typep (not old-typep)
     (labels ((tp (o ty) (funcall (symbol-function 'typep) o ty)))
       (cond
         ;; User-defined DEFTYPE abbreviation
         ((and (symbolp type) (gethash type *%deftype-expanders*))
          (tp object (funcall (gethash type *%deftype-expanders*))))
         ;; Compound type specifiers
         ((consp type)
          (let ((head (car type)) (args (cdr type)))
            (cond
              ((eq head 'or)     (some  (lambda (t2) (tp object t2)) args))
              ((eq head 'and)    (every (lambda (t2) (tp object t2)) args))
              ((eq head 'not)    (not (tp object (car args))))
              ((eq head 'member) (not (null (member object args))))
              ((eq head 'eql)    (eql object (car args)))
              ((eq head 'satisfies) (not (null (funcall (car args) object))))
              ((eq head 'integer)
               (and (integerp object)
                    (let ((lo (car args)) (hi (cadr args)))
                      (and (or (null lo) (eq lo '*) (>= object (if (consp lo) (1+ (car lo)) lo)))
                           (or (null hi) (eq hi '*) (<= object (if (consp hi) (1- (car hi)) hi)))))))
              ((eq head 'float)
               (and (floatp object)
                    (let ((lo (car args)) (hi (cadr args)))
                      (and (or (null lo) (eq lo '*) (>= object (if (consp lo) (car lo) lo)))
                           (or (null hi) (eq hi '*) (<= object (if (consp hi) (car hi) hi)))))))
              ((eq head 'real)
               (and (or (integerp object) (floatp object))
                    (let ((lo (car args)) (hi (cadr args))
                          (v (if (floatp object) object (float object))))
                      (and (or (null lo) (eq lo '*) (>= v (float (if (consp lo) (car lo) lo))))
                           (or (null hi) (eq hi '*) (<= v (float (if (consp hi) (car hi) hi))))))))
              ((eq head 'string)        (stringp object))
              ((eq head 'vector)        (vectorp object))
              ((eq head 'simple-vector) (vectorp object))
              ((eq head 'array)         (or (vectorp object) (stringp object)))
              ((eq head 'function)      (functionp object))
              ((eq head 'values)        t)
              ;; Unknown compound — delegate to kernel
              (t (funcall old-typep object type)))))
         ;; Simple type — delegate to old typep (handles CLOS + kernel)
         (t (funcall old-typep object type)))))
   'typep))

;;; UPGRADED-ARRAY-ELEMENT-TYPE and UPGRADED-COMPLEX-PART-TYPE — stubs.
(defun upgraded-array-element-type (typespec &optional env)
  env typespec t)   ; everything upgrades to T in bocl's 1-type system

(defun upgraded-complex-part-type (typespec &optional env)
  env typespec 'real)

;;; COPY-SEQ — shallow copy of any sequence.
(defun copy-seq (sequence)
  "Return a fresh copy of SEQUENCE."
  (cond ((listp sequence) (copy-list sequence))
        ;; A 1-D %md-array is an adjustable or displaced VECTOR (a string when its
        ;; element type is CHARACTER, a bit-vector when BIT): copy it into a fresh
        ;; SIMPLE sequence of the same type, reading through ELT (which follows
        ;; displacement).  Must precede the VECTORP branch — VECTORP is true for a
        ;; %md-array, but SVREF would misread its storage.
        ((and (%md-array-p sequence)
              (= (length (%md-array-dimensions sequence)) 1))
         (let ((n  (length sequence))
               (et (%md-array-element-type sequence)))
           (cond ((eq et 'character)
                  (let ((s (make-string n)))
                    (dotimes (i n s) (char-set s i (elt sequence i)))))
                 ((eq et 'bit)
                  (let ((b (make-bit-vector n 0)))
                    (dotimes (i n b) (setf (bit b i) (elt sequence i)))))
                 (t (let ((v (make-array n)))
                      (dotimes (i n v) (setf (svref v i) (elt sequence i))))))))
        ((stringp sequence)
         (let ((s (make-string (length sequence))))
           (dotimes (i (length sequence) s)
             (char-set s i (char sequence i)))))
        ;; bit-vector before vectorp (a bit-vector is a vector subtype); copy
        ;; keeps the BIT_VECTOR type and the active (fill-pointer) length.
        ((bit-vector-p sequence)
         (let ((b (make-bit-vector (length sequence) 0)))
           (dotimes (i (length sequence) b)
             (setf (bit b i) (bit sequence i)))))
        ((vectorp sequence)
         (let ((v (make-array (length sequence))))
           (dotimes (i (length sequence) v)
             (setf (svref v i) (svref sequence i)))))
        (t (error "COPY-SEQ: not a sequence: ~S" sequence))))

;;; ---- String utilities -----------------------------------------------------

(defun nstring-upcase (string &key (start 0) end)
  "Destructively convert STRING to upper case in place."
  (let ((e (or end (length string))))
    (dotimes (i (- e start) string)
      (setf (char string (+ start i))
            (char-upcase (char string (+ start i)))))))

(defun nstring-downcase (string &key (start 0) end)
  "Destructively convert STRING to lower case in place."
  (let ((e (or end (length string))))
    (dotimes (i (- e start) string)
      (setf (char string (+ start i))
            (char-downcase (char string (+ start i)))))))

(defun nstring-capitalize (string &key (start 0) end)
  "Destructively capitalize STRING in place."
  (let ((e (or end (length string)))
        (in-word nil))
    (dotimes (i (- e start) string)
      (let* ((pos (+ start i))
             (c (char string pos)))
        ;; A word is a maximal run of ALPHANUMERIC characters (CLHS), so digits
        ;; stay inside a word — "foo16c" is one word, not "foo" + "c"; only the
        ;; first character is upcased (a leading digit upcases to itself).
        (cond ((alphanumericp c)
               (setf (char string pos)
                     (if in-word (char-downcase c) (char-upcase c)))
               (setq in-word t))
              (t (setq in-word nil)))))))

(defun copy-symbol (symbol &optional copy-props)
  "Return a new uninterned symbol with the same name as SYMBOL.
  If COPY-PROPS is true, copy the value, function, and plist."
  (let ((new (make-symbol (symbol-name symbol))))
    (when copy-props
      (when (boundp symbol)
        (set new (symbol-value symbol)))
      (when (fboundp symbol)
        (set-symbol-function (symbol-function symbol) new))
      (set-symbol-plist (symbol-plist symbol) new))
    new))

;;; CHAR-TYPE: return the type of a character (for ETYPECASE etc.)
;;; In bocl, characters are just characters; no subtypes.
(defun char-type (char) char 'character)

;;; ---- Condition utilities --------------------------------------------------

(defun condition-p (object) (conditionp object))
(defun simple-condition-p (object)
  (and (conditionp object)
       (or (typep object 'simple-condition)
           (typep object 'simple-error)
           (typep object 'simple-warning)
           (typep object 'simple-type-error))))

;;; Condition slot accessors for standard condition types.
;;; In bocl, conditions are simple objects with format-control/format-arguments.
;;; More complex conditions (type-error, stream-error, etc.) are kernel objects.

(defun type-error-datum (condition)
  "Return the offending object from a TYPE-ERROR condition."
  ;; bocl's type-error format string is "... expected TYPE, got OBJ"
  ;; We don't have real slot access; return the second slot if available.
  ;; For now, return nil as a stub — full support needs DEFINE-CONDITION.
  condition nil)

(defun type-error-expected-type (condition)
  "Return the expected type from a TYPE-ERROR condition."
  condition nil)

(defun stream-error-stream (condition) condition nil)
(defun file-error-pathname (condition) condition nil)
(defun cell-error-name (condition) condition nil)
(defun arithmetic-error-operands (condition) condition nil)
(defun arithmetic-error-operation (condition) condition nil)
(defun unbound-slot-instance (condition) condition nil)

;;; ---- Restart invocation functions ----------------------------------------

(defun muffle-warning (&optional condition)
  "Invoke the MUFFLE-WARNING restart; signal CONTROL-ERROR if none is active."
  (let ((r (find-restart 'muffle-warning condition)))
    (if r (invoke-restart r)
        (error 'control-error))))

(defun continue (&optional condition)
  "Invoke the CONTINUE restart if one is available, else return NIL."
  (let ((r (find-restart 'continue condition)))
    (when r (invoke-restart r))))

(defun warn (datum &rest arguments)
  "Signal a warning with a MUFFLE-WARNING restart active (CLHS WARN).  A handler
   may (muffle-warning) to suppress it; otherwise it is printed to *error-output*.
   Overrides the kernel WARN, which did not establish the restart."
  ;; Supplying initargs together with an already-built condition is a type-error
  ;; (SBCL/CCL signal SIMPLE-TYPE-ERROR).
  (when (and (typep datum 'condition) arguments)
    (error 'simple-type-error
           :format-control "WARN: initargs supplied with a condition object"
           :format-arguments nil))
  (let ((condition (cond ((typep datum 'condition) datum)
                         ((symbolp datum) (apply #'make-condition datum arguments))
                         (t (make-condition 'simple-warning
                                            :format-control datum
                                            :format-arguments arguments)))))
    ;; CLHS WARN: if the condition is not of type WARNING, signal a TYPE-ERROR and
    ;; do NOT signal the warning.
    (unless (typep condition 'warning)
      (error 'simple-type-error
             :format-control "WARN: ~S is not of type WARNING"
             :format-arguments (list condition)))
    (restart-case
        (progn
          (signal condition)
          (format *error-output* ";; WARNING: ~A~%" condition))
      (muffle-warning () nil))
    nil))

(defun store-value (value &optional condition)
  "Invoke the STORE-VALUE restart with VALUE, or return NIL if none is active."
  (let ((r (find-restart 'store-value condition)))
    (when r (invoke-restart r value))))

(defun use-value (value &optional condition)
  "Invoke the USE-VALUE restart with VALUE, or return NIL if none is active."
  (let ((r (find-restart 'use-value condition)))
    (when r (invoke-restart r value))))

;; Condition/restart association (CLHS 9.1.4.2.4).  bocl conflates a restart object
;; with its name symbol, so association is tracked in a dynamic list of
;; (condition . restart-name-list) entries consulted by FIND-RESTART (conditions.c):
;; a restart whose name appears in some entry is found only when the condition being
;; searched for is one of the conditions it was associated with.
(defvar *%restart-associations* nil)

(defmacro with-condition-restarts (condition-form restarts-form &body body)
  "Associate the restarts named by RESTARTS-FORM with the condition (CLHS
   WITH-CONDITION-RESTARTS) for the dynamic extent of BODY, so FIND-RESTART with a
   different condition does not see them."
  `(let ((*%restart-associations*
          (cons (cons ,condition-form ,restarts-form) *%restart-associations*)))
     ,@body))

;;; NOTE: ABORT is already a kernel built-in (conditions.c).

;;; ---- Debugger / break ----------------------------------------------------

(defun invoke-debugger (condition)
  "Invoke the debugger with CONDITION.  In bocl: signal an unhandled error."
  (error condition))

(defun break (&optional (format-string "Break") &rest args)
  "Invoke the debugger with a continuable breakpoint.
  In bocl's batch mode this just signals an error."
  (apply #'error format-string args))

;;; ---- Interactive I/O -----------------------------------------------------

(defun y-or-n-p (&optional (format-string "") &rest args)
  "Ask a yes-or-no question and return T or NIL."
  (fresh-line)
  (apply #'format t format-string args)
  (princ " (y or n) ")
  (force-output)
  (loop
    (let ((line (read-line)))
      (cond ((or (string= line "y") (string= line "Y") (string-equal line "yes")) (return t))
            ((or (string= line "n") (string= line "N") (string-equal line "no"))  (return nil))
            (t (princ "Please answer y or n: ") (force-output))))))

(defun yes-or-no-p (&optional (format-string "") &rest args)
  "Ask a 'yes' or 'no' question; requires full word."
  (fresh-line)
  (apply #'format t format-string args)
  (princ " (yes or no) ")
  (force-output)
  (loop
    (let ((line (read-line)))
      (cond ((string-equal line "yes") (return t))
            ((string-equal line "no")  (return nil))
            (t (princ "Please type yes or no: ") (force-output))))))

;;; ---- I/O: FRESH-LINE and related ----------------------------------------

(defun fresh-line (&optional (stream *standard-output*))
  "Output a newline unless already at the start of a line; return T if a newline
  was output, NIL otherwise (CLHS FRESH-LINE).  %STREAM-LINE-START-P detects the
  start of a line reliably for in-memory string streams; for a device stream bocl
  does not column-track, so it conservatively outputs a newline."
  (if (%stream-line-start-p stream)
      nil
      (progn (terpri stream) t)))

(defun pprint-newline (kind &optional (stream *standard-output*))
  "Pretty-printer newline.  In bocl: stub that may output a newline."
  (case kind
    (:mandatory (terpri stream) t)
    (t nil)))

(defun pprint-indent (relative-to n &optional (stream *standard-output*))
  "Pretty-printer indentation.  Stub in bocl."
  stream relative-to n nil)

;;; ---- ROOM and TIME -------------------------------------------------------

(defun room (&optional (detail :default))
  "Print information about memory usage.  Minimal implementation."
  detail
  (format t "~&;; bocl: no memory stats available~%")
  (values))

(defmacro time (form)
  "Evaluate FORM and print elapsed time."
  (let ((start (gensym "START-")) (end (gensym "END-")) (result (gensym "RESULT-")))
    `(let* ((,start (if (fboundp (quote get-internal-real-time))
                        (get-internal-real-time) 0))
            (,result ,form)
            (,end   (if (fboundp (quote get-internal-real-time))
                        (get-internal-real-time) 0)))
       (if (fboundp (quote get-internal-real-time))
           (format t "~&;; Elapsed: ~,3F s~%"
                   (/ (- ,end ,start) (float internal-time-units-per-second)))
           (format t "~&;; TIME: (no clock available)~%"))
       ,result)))

;;; ---- BOOLE and bitwise constants -----------------------------------------

(defconstant boole-clr   0)
(defconstant boole-set   1)
(defconstant boole-1     2)
(defconstant boole-2     3)
(defconstant boole-c1    4)
(defconstant boole-c2    5)
(defconstant boole-and   6)
(defconstant boole-ior   7)
(defconstant boole-orc1  8)
(defconstant boole-orc2  9)
(defconstant boole-andc1 10)
(defconstant boole-andc2 11)
(defconstant boole-xor   12)
(defconstant boole-eqv   13)
(defconstant boole-nand  14)
(defconstant boole-nor   15)

(defun boole (op integer1 integer2)
  "Apply bitwise operation OP to INTEGER1 and INTEGER2."
  (cond ((= op boole-clr)   0)
        ((= op boole-set)   -1)
        ((= op boole-1)     integer1)
        ((= op boole-2)     integer2)
        ((= op boole-c1)    (lognot integer1))
        ((= op boole-c2)    (lognot integer2))
        ((= op boole-and)   (logand integer1 integer2))
        ((= op boole-ior)   (logior integer1 integer2))
        ((= op boole-orc1)  (logior (lognot integer1) integer2))
        ((= op boole-orc2)  (logior integer1 (lognot integer2)))
        ((= op boole-andc1) (logand (lognot integer1) integer2))
        ((= op boole-andc2) (logand integer1 (lognot integer2)))
        ((= op boole-xor)   (logxor integer1 integer2))
        ((= op boole-eqv)   (lognot (logxor integer1 integer2)))
        ((= op boole-nand)  (lognot (logand integer1 integer2)))
        ((= op boole-nor)   (lognot (logior integer1 integer2)))
        (t (error "BOOLE: invalid operation ~S" op))))

;;; ---- Numeric type aliases -------------------------------------------------

;;; LOGIOR was already registered in arithmetic.c via kernel_LOGOR.
;;; Derived bitwise operations:
(defun lognand (x y) (lognot (logand x y)))
(defun lognor  (x y) (lognot (logior x y)))
(defun logeqv  (x y) (lognot (logxor x y)))
(defun logorc1 (x y) (logior (lognot x) y))
(defun logorc2 (x y) (logior x (lognot y)))
(defun logandc1 (x y) (logand (lognot x) y))
(defun logandc2 (x y) (logand x (lognot y)))

;;; ---- Miscellaneous standard functions ------------------------------------

;;; MAKE-SYMBOL — already in kernel; just ensure it's accessible.
;;; It creates an uninterned symbol.

;;; STRING — already defined in prelude (string.lisp section, Slice 5).
;;; NCONC — already in prelude (Slice 12).
;;; PPRINT — already a stub.

;;;; ========================================================================
;;;; Slice 21 — DEFINE-CONDITION, I/O additions, exploration utilities,
;;;;   stream predicates, and miscellaneous standard functions.
;;;; ========================================================================

;;; ---- DEFINE-CONDITION -----------------------------------------------------
;;;
;;; Creates a CLOS class that integrates with bocl's C signal system.
;;; After (%clos-register ...) was called above, CLOS instances are:
;;;   - Recognized by C class_of() → returns the CLOS class
;;;   - Dispatchable by C signal_condition() via the updated subclassp()
;;;   - Catchable by HANDLER-CASE via the updated eval.c clause resolver
;;;   - Acceptable by (error ...) via the updated build_condition()
;;;
;;; DEFINE-CONDITION inherits from CONDITION (a CLOS alias for standard-object)
;;; by default, or from user-specified superclasses.

;;; Standard condition names must NOT be defined as new CLOS classes because
;;; they would shadow the kernel condition classes and break handler dispatch.
;;; The kernel already has: CONDITION, SIMPLE-CONDITION, ERROR, WARNING,
;;; SIMPLE-ERROR, SIMPLE-WARNING, SIMPLE-TYPE-ERROR etc.
;;;
;;; For DEFINE-CONDITION to work with standard superclasses (like ERROR),
;;; HANDLER-CASE now looks up kernel classes first, then CLOS classes.
;;; So (handler-case ... (error ...)) continues to catch kernel errors.
;;;
;;; We define ONLY purely user-facing condition classes that DON'T shadow
;;; existing kernel classes:
(defclass program-error    (error) () )
(defclass control-error    (error) () )
(defclass cell-error       (error) ((name :initarg :name :accessor cell-error-name)))
(defclass unbound-variable (cell-error)      () )
(defclass undefined-function (cell-error)    () )
(defclass arithmetic-error (error)
  ((operation :initarg :operation :accessor arithmetic-error-operation)
   (operands  :initarg :operands  :accessor arithmetic-error-operands)))
(defclass division-by-zero         (arithmetic-error) ())
(defclass floating-point-overflow  (arithmetic-error) ())
(defclass floating-point-underflow (arithmetic-error) ())
(defclass floating-point-inexact   (arithmetic-error) ())
(defclass stream-error             (error)
  ((stream :initarg :stream :accessor stream-error-stream)))
(defclass end-of-file              (stream-error) ())
(defclass file-error               (error)
  ((pathname :initarg :pathname :accessor file-error-pathname)))
(defclass package-error            (error) ((package :initarg :package)))
(defclass parse-error              (error) ())
(defclass reader-error             (parse-error stream-error) ())
(defclass print-not-readable       (error)
  ((object :initarg :object :accessor print-not-readable-object)))
(defclass storage-condition        (serious-condition) ())
(defclass style-warning            (warning) ())

(defmacro define-condition (name superclasses slots &rest options)
  "Define a condition class NAME inheriting from SUPERCLASSES.
  Works like DEFCLASS but defaults the superclass to ERROR if none given.
  Supports :REPORT option (stored as a report function for DESCRIBE).
  The resulting instances integrate with HANDLER-CASE and SIGNAL."
  (let* ((actual-supers
           (if (null superclasses)
               (list 'error)
               superclasses))
         (slot-specs (mapcar (lambda (s)
                               ;; Normalize :condition-accessor -> :accessor for CLOS
                               s)
                             slots))
         ;; Extract :report option
         (report-option (assq :report options))
         (report-fn     (cadr report-option))
         (defclass-options (remove-if (lambda (o) (eq (car o) :report)) options)))
    `(progn
       (defclass ,name ,actual-supers ,slot-specs ,@defclass-options)
       ,@(when report-fn
           `((defmethod print-object ((condition ,name) stream)
               (cond ((functionp ,report-fn) (funcall ,report-fn condition stream))
                     ((stringp ,report-fn)   (write-string ,report-fn stream))
                     (t (call-next-method))))))
       ',name)))

;;; ---- Condition utilities --------------------------------------------------

;;; CONDITIONP already exists as kernel built-in (returns T for kernel conds).
;;; Extend it for CLOS conditions:
(let ((old-conditionp (symbol-function 'conditionp)))
  (set-symbol-function
   (lambda (x)
     (or (funcall old-conditionp x)
         (and (%is-obj x)
              (typep x 'condition))))
   'conditionp))

;;; Re-export condition-p and simple-condition-p to use the extended conditionp:
(set-symbol-function (symbol-function 'conditionp) 'condition-p)

(defun simple-condition-p (c)
  (and (conditionp c) (typep c 'simple-condition)))

;;; MAKE-CONDITION — user DEFINE-CONDITION classes (with their own slots and
;;; :REPORT methods) are built with MAKE-INSTANCE, but the built-in SIMPLE
;;; conditions carry FORMAT-CONTROL / FORMAT-ARGUMENTS that only the kernel
;;; constructor stores (their CLOS classes are registered slotless), so they must
;;; route to the kernel MAKE-CONDITION to produce a REPORTING condition.  Without
;;; this they print as a bare "#<SIMPLE-ERROR>" — see issue
;;; make-condition-clos-initargs.
(defparameter *%kernel-condition-types*
  '(simple-condition simple-error simple-warning simple-type-error))
(let ((old-make-condition (symbol-function 'make-condition)))
  (set-symbol-function
   (lambda (type &rest initargs)
     (let* ((cls  (find-class type nil))
            (name (cond ((symbolp type) type)
                        ((and cls (%is-cls cls)) (%cls-name cls)))))
       (if (and cls (%is-cls cls)
                (not (member name *%kernel-condition-types*)))
           (apply #'make-instance cls initargs)
           (apply old-make-condition type initargs))))
   'make-condition))

;;; CONDITION-REPORT — get the report string for a condition.
(defun condition-report-string (condition)
  "Return a human-readable report string for CONDITION."
  (with-output-to-string (s)
    (print-object condition s)))

;;; ---- Write-char is now a kernel built-in (see bocl.c). -------------------

;;; ---- Stream predicates ----------------------------------------------------

(defun input-stream-p (stream)
  "Return T if STREAM can be read from."
  (handler-case
      (progn (peek-char nil stream nil nil) t)
    (error () nil)))

(defun output-stream-p (stream)
  "Return T if STREAM can be written to."
  ;; In bocl we check via stream type
  (and (streamp stream) t))   ; approximate: all streams support output for now

(defun open-stream-p (stream)
  "Return T if STREAM is open."
  (and (streamp stream) t))  ; bocl doesn't track closed state robustly yet

(defun interactive-stream-p (stream)
  "Return T if STREAM is connected to a terminal."
  ;; Approximate: stdin is interactive if it's *terminal-io* or *standard-input*
  (eq stream *terminal-io*))

(defun stream-element-type (stream)
  "Return the element type of STREAM."
  stream 'character)  ; bocl streams are character streams

;;; ---- Exploration utilities ------------------------------------------------

(defun apropos-list (string &optional package)
  "Return a list of distinct symbols whose names contain STRING (as a
   case-insensitive substring), sorted by name.  With PACKAGE, only symbols
   accessible in that package are examined; otherwise every registered package
   is searched (CLHS APROPOS-LIST)."
  (let ((target (string string))
        ;; A symbol present in several packages must appear once (dedup O(1)).
        (seen   (make-hash-table :test 'eq))
        (result '()))
    (flet ((scan (syms)
             (dolist (sym syms)
               (unless (gethash sym seen)
                 (setf (gethash sym seen) t)
                 (when (%string-contains-ci target (symbol-name sym))
                   (push sym result))))))
      (if package
          (scan (%pkg-accessible-symbols (%designated-package package)))
          (dolist (p (list-all-packages))
            (scan (package-symbols p)))))
    ;; Order by name using the fast kernel comparison primitive directly — the
    ;; generic STRING< is ~5 ms/call and would dominate for a large result set.
    (sort result (lambda (a b)
                   (< (%string-compare (symbol-name a) (symbol-name b) nil) 0)))))

(defun apropos (string &optional package)
  "Print, to *STANDARD-OUTPUT*, every symbol whose name contains STRING (see
   APROPOS-LIST), one per line, annotated when bound as a function/variable."
  (dolist (sym (apropos-list string package))
    (format t "~&~S" sym)
    (when (fboundp sym) (format t " [function]"))
    (when (boundp sym)  (format t " [variable]")))
  (values))

(defun describe (object &optional (stream *standard-output*))
  "Print a description of OBJECT."
  (if (%is-obj object)
      (progn
        (format stream "~&~S is an instance of ~A~%"
                object (%cls-name (%obj-class object)))
        (dolist (eslot (%cls-eslots (%obj-class object)))
          (let ((name (getf eslot :name)))
            (format stream "  ~A: " name)
            (if (slot-boundp object name)
                (format stream "~S~%" (slot-value object name))
                (format stream "[unbound]~%")))))
      (format stream "~&~S~%" object))
  (values))

;;; ---- STRING conversion utilities -----------------------------------------

(defun number-to-string (number &optional (base 10))
  "Convert NUMBER to its string representation in BASE."
  (if (= base 10)
      (princ-to-string number)
      (if (integerp number)
          (with-output-to-string (s)
            (when (< number 0) (write-char #\- s) (setq number (- number)))
            (labels ((emit (n)
                       (when (>= n base) (emit (floor n base)))
                       (write-char (digit-char (mod n base) base) s)))
              (emit number)))
          (princ-to-string number))))

(defun string-to-number (string &key (base 10) (start 0) end junk-allowed)
  "Parse a number from STRING.  Returns the number or NIL if unparseable."
  end  ; ignored — uses whole string
  (handler-case
      (parse-integer string :start start :radix base :junk-allowed junk-allowed)
    (error () nil)))

;;; ---- WRITE-CHAR wrapper ---------------------------------------------------

;;; write-char is now registered as a kernel built-in in bocl.c.
;;; Add a Lisp-level wrapper that handles *standard-output* default:

;;; ---- ENSURE-DIRECTORIES-EXIST --------------------------------------------

(defun ensure-directories-exist (pathname &key verbose)
  "Create any missing directories in PATHNAME's directory component, walking the
   chain from the root.  Returns (values pathname created-p)."
  (let* ((ns (namestring pathname))
         (n  (length ns))
         (created nil)
         (last-slash -1))
    (dotimes (i n) (when (char= (char ns i) #\/) (setq last-slash i)))
    ;; Create each directory prefix ending at a slash up to and including the
    ;; last slash (the directory part); skip a leading-slash root (i starts at 1).
    (let ((i 1))
      (loop while (<= i last-slash) do
        (when (char= (char ns i) #\/)
          (let ((dir (subseq ns 0 i)))
            (when (and (plusp (length dir))
                       (not (common-lisp::%directory-p dir)))
              (when verbose (format *error-output* ";; Creating directory: ~A~%" dir))
              (common-lisp::%mkdir dir)
              (setq created t))))
        (incf i)))
    (values pathname created)))

;;; ---- DIRECTORY -----------------------------------------------------------

(defun %wild-match (pat str)
  "Glob-match STR against PAT: #\\* matches any (possibly empty) run, #\\? one char."
  (let ((pp 0) (sp 0) (pl (length pat)) (sl (length str)) (star -1) (m 0))
    (loop
      (cond
        ((< sp sl)
         (cond
           ((and (< pp pl) (or (char= (char pat pp) #\?)
                               (char= (char pat pp) (char str sp))))
            (incf pp) (incf sp))
           ((and (< pp pl) (char= (char pat pp) #\*))
            (setq star pp m sp) (incf pp))
           ((>= star 0) (setq pp (1+ star)) (incf m) (setq sp m))
           (t (return nil))))
        (t (loop while (and (< pp pl) (char= (char pat pp) #\*)) do (incf pp))
           (return (= pp pl)))))))

(defun %split-path (s)
  "Split namestring S on #\\/ into components, dropping empty ones."
  (let ((parts '()) (start 0) (n (length s)))
    (dotimes (i n)
      (when (char= (char s i) #\/)
        (when (> i start) (push (subseq s start i) parts))
        (setq start (1+ i))))
    (when (> n start) (push (subseq s start n) parts))
    (nreverse parts)))

(defun %glob (dir comps)
  "DIR is a directory namestring ending in '/'.  COMPS is a list of glob component
   strings.  Returns the matching path strings; '**' matches zero or more levels."
  (if (null comps)
      nil
      (let ((comp (first comps)) (rest (rest comps)) (acc '()))
        (cond
          ((string= comp "**")
           (setq acc (%glob dir rest))                  ; zero levels at DIR
           (dolist (e (common-lisp::%list-directory dir))
             (let ((sub (concatenate 'string dir e "/")))
               (when (common-lisp::%directory-p sub)
                 (setq acc (append acc (%glob sub comps))))))
           acc)
          ((or (find #\* comp) (find #\? comp))
           (dolist (e (common-lisp::%list-directory dir) (nreverse acc))
             (when (%wild-match comp e)
               (let ((full (concatenate 'string dir e)))
                 (if rest
                     (when (common-lisp::%directory-p (concatenate 'string full "/"))
                       (setq acc (nreconc (%glob (concatenate 'string full "/") rest) acc)))
                     (push full acc))))))
          (t
           (let ((full (concatenate 'string dir comp)))
             (if rest
                 (when (common-lisp::%directory-p (concatenate 'string full "/"))
                   (%glob (concatenate 'string full "/") rest))
                 (when (probe-file full) (list full)))))))))

(defun directory (pathspec &key)
  "List of pathnames matching PATHSPEC.  Supports #\\* / #\\? glob components and
   '**' for recursive (wild-inferiors) directory matching.  The recursive walk
   runs in the %DIRECTORY-GLOB kernel primitive (a Lisp-level walk was ~100x
   slower — a CHAR-loop match, a stat() per entry and interpreter funcalls all
   the way down; see issues/open/speed-directory.issue)."
  ;; %DIRECTORY-GLOB does ALL the parsing in C — splitting on '/', the absolute-
  ;; vs-CWD-relative base, and the trailing-slash "directories only" flag (a
  ;; pattern ending in '/' matches directories, e.g. "X/*/", per CL) — so the only
  ;; per-call Lisp cost is this MAPCAR over the matches.  We build them with the
  ;; kernel pathname constructor directly: results are absolute physical paths, and
  ;; the CL-level PATHNAME would first run logical-pathname host/device probes
  ;; (%LP-PARSE-HOST/DEVICE, ~80x the cost) that can never match a filesystem path
  ;; before calling this same constructor.  (See speed-directory.issue and
  ;; asdf-source-registry-inherit-scan-slow.issue.)
  (let ((ns (if (pathnamep pathspec) (namestring pathspec) (string pathspec))))
    (mapcar *%kernel-pathname-fn* (%directory-glob ns))))

;;; ---- Missing numeric utilities -------------------------------------------

;;; Float limits — bocl uses IEEE 754 double (DBL_MAX ~= 1.8e308).
;;; These are approximate and map single/short to double since bocl has one float type.
(defconstant most-positive-double-float   1.7976931348623157e+308)
(defconstant most-negative-double-float  -1.7976931348623157e+308)
(defconstant least-positive-double-float  2.2250738585072014e-308)  ; smallest normalised
(defconstant least-negative-double-float -2.2250738585072014e-308)
(defconstant least-positive-normalized-double-float 2.2250738585072014e-308)
(defconstant least-negative-normalized-double-float -2.2250738585072014e-308)
(defconstant most-positive-single-float   most-positive-double-float)
(defconstant most-negative-single-float   most-negative-double-float)
(defconstant least-positive-single-float  least-positive-double-float)
(defconstant least-negative-single-float  least-negative-double-float)
(defconstant most-positive-short-float    most-positive-double-float)
(defconstant most-negative-short-float    most-negative-double-float)
(defconstant least-positive-short-float   least-positive-double-float)
(defconstant least-negative-short-float   least-negative-double-float)
(defconstant least-positive-normalized-single-float least-positive-double-float)
(defconstant least-negative-normalized-single-float least-negative-double-float)
;;; long-float — bocl has one float type, so long-float maps to double-float.
(defconstant most-positive-long-float     most-positive-double-float)
(defconstant most-negative-long-float     most-negative-double-float)
(defconstant least-positive-long-float    least-positive-double-float)
(defconstant least-negative-long-float    least-negative-double-float)
(defconstant least-positive-normalized-long-float  least-positive-normalized-double-float)
(defconstant least-negative-normalized-long-float  least-negative-normalized-double-float)
;;; short-float normalized (non-normalized short-float limits are defined above).
(defconstant least-positive-normalized-short-float least-positive-normalized-double-float)
(defconstant least-negative-normalized-short-float least-negative-normalized-double-float)
;;; Float epsilons — DBL_EPSILON; with one float type all four sizes coincide.
(defconstant double-float-epsilon          2.220446049250313e-16)
(defconstant double-float-negative-epsilon 1.1102230246251565e-16)
(defconstant single-float-epsilon          double-float-epsilon)
(defconstant single-float-negative-epsilon double-float-negative-epsilon)
(defconstant short-float-epsilon           double-float-epsilon)
(defconstant short-float-negative-epsilon  double-float-negative-epsilon)
(defconstant long-float-epsilon            double-float-epsilon)
(defconstant long-float-negative-epsilon   double-float-negative-epsilon)

;;; ---- Standard array/call limits ------------------------------------------

(defconstant array-rank-limit       64)
(defconstant array-dimension-limit  (expt 2 24))
(defconstant array-total-size-limit (expt 2 24))
(defconstant call-arguments-limit   4096)

;;; ---- DOCUMENTATION -------------------------------------------------------

(defvar *documentation-table* (make-hash-table))

(defun documentation (object doc-type)
  "Return the documentation string for OBJECT of type DOC-TYPE, or NIL."
  (cdr (assoc doc-type (gethash object *documentation-table*))))

(defun (setf documentation) (new object doc-type)
  "Store the documentation string NEW for OBJECT of type DOC-TYPE."
  (let ((entry (assoc doc-type (gethash object *documentation-table*))))
    (if entry
        (setf (cdr entry) new)
        (push (cons doc-type new) (gethash object *documentation-table*))))
  new)

;; Called from C (DEFUN / BOCL:DEFINE-FUNCTION) to record an extracted defun
;; docstring; a plain function so the kernel can reach it via SYMBOL-FUNCTION
;; without the (SETF DOCUMENTATION) setf-function machinery.  NIL doc is a no-op.
(defun %set-function-documentation (name doc)
  (when doc (setf (documentation name 'function) doc))
  name)

;;;; ========================================================================
;;;; Slice 22 — I/O built-ins, list set operations, hash utilities,
;;;;   TYPE-OF, array operations, bit-vector ops, LOOP-FINISH, misc.
;;;; ========================================================================

;;; ---- List set operations -------------------------------------------------

(defun copy-alist (alist)
  "Return a fresh copy of ALIST — cons cells are new, keys and values shared."
  (mapcar (lambda (pair)
            (if (consp pair) (cons (car pair) (cdr pair)) pair))
          alist))

(defun set-exclusive-or (list1 list2 &key (test #'eql) test-not (key #'identity))
  "Return the symmetric set difference of LIST1 and LIST2."
  (setq test (%xtest test test-not))
  (append (set-difference list1 list2 :test test :key key)
          (set-difference list2 list1 :test test :key key)))

(defun nset-exclusive-or (list1 list2 &key (test #'eql) test-not (key #'identity))
  "Destructive version of SET-EXCLUSIVE-OR."
  (set-exclusive-or list1 list2 :test (%xtest test test-not) :key key))

(defun nintersection (list1 list2 &key (test #'eql) test-not (key #'identity))
  "Destructive INTERSECTION — modifies LIST1."
  (intersection list1 list2 :test (%xtest test test-not) :key key))

(defun nset-difference (list1 list2 &key (test #'eql) test-not (key #'identity))
  "Destructive SET-DIFFERENCE — modifies LIST1."
  (set-difference list1 list2 :test (%xtest test test-not) :key key))

(defun nunion (list1 list2 &key (test #'eql) test-not (key #'identity))
  "Destructive UNION — may modify LIST1 or LIST2."
  (union list1 list2 :test (%xtest test test-not) :key key))

;;; ---- Hash-table utilities -----------------------------------------------

(defun hash-table-keys (table)
  "Return a list of all keys in HASH-TABLE."
  (let ((keys nil))
    (maphash (lambda (k v) v (push k keys)) table)
    keys))

(defun hash-table-values (table)
  "Return a list of all values in HASH-TABLE."
  (let ((vals nil))
    (maphash (lambda (k v) k (push v vals)) table)
    vals))

(defun hash-table-alist (table)
  "Return an association list of (key . value) pairs from TABLE."
  (let ((result nil))
    (maphash (lambda (k v) (push (cons k v) result)) table)
    result))

;;; ---- Stream utilities ---------------------------------------------------

(defun read-sequence (sequence stream &key (start 0) end)
  "Read elements from STREAM into SEQUENCE[START..END].
  Returns the index of the first element not updated."
  (let* ((len (length sequence))
         (e   (or end len)))
    (dotimes (i (- e start) e)
      (let ((elem (read-byte stream nil nil)))
        (when (null elem) (return (+ start i)))
        (setf (elt sequence (+ start i))
              (if (characterp (elt sequence 0))
                  (code-char elem) elem))))
    e))

(defun write-sequence (sequence stream &key (start 0) end)
  "Write SEQUENCE[START..END] to STREAM.  Returns SEQUENCE."
  (let ((e (or end (length sequence))))
    (dotimes (i (- e start) sequence)
      (let ((elem (elt sequence (+ start i))))
        (if (characterp elem)
            (write-char elem stream)
            (write-byte elem stream))))))

;;; ---- Broadcast / compound stream constructors -------------------------
;;; MAKE-BROADCAST-STREAM is a real output stream built on the kernel hook
;;; BOCL::%MAKE-OUTPUT-CALLBACK-STREAM: its write-function fans every emitted
;;; chunk out to each component stream.  CL-level metadata (kind + components)
;;; lives in a side registry keyed by the stream object, so the predicate and
;;; accessor below can recover it, and FORCE-OUTPUT / FINISH-OUTPUT can recurse
;;; into the components (the callback stream itself is unbuffered).
;;; The input-side compound streams (concatenated, and the read half of
;;; two-way / echo) still need a kernel read-callback and remain stubs.

(defvar bocl::*%compound-stream-registry* (make-hash-table :test 'eq)
  "Maps a compound output stream to (KIND . COMPONENTS) where KIND is one of
   :BROADCAST :TWO-WAY :ECHO :SYNONYM and COMPONENTS is its component list.")

(defun bocl::%register-compound-stream (stream kind components)
  (setf (gethash stream bocl::*%compound-stream-registry*) (cons kind components))
  stream)

(defun bocl::%compound-stream-info (stream)
  "(KIND . COMPONENTS) for a compound STREAM, or NIL if STREAM is not one."
  (and (streamp stream) (gethash stream bocl::*%compound-stream-registry*)))

(defun make-broadcast-stream (&rest streams)
  "Return an output stream that writes everything written to it to each of
   STREAMS, in order (CLHS MAKE-BROADCAST-STREAM)."
  (let ((targets streams))
    (bocl::%register-compound-stream
     (bocl::%make-output-callback-stream
      (lambda (chunk) (dolist (s targets) (write-string chunk s))))
     :broadcast targets)))

(defun broadcast-stream-streams (broadcast-stream)
  "The ordered list of component streams of BROADCAST-STREAM."
  (let ((info (bocl::%compound-stream-info broadcast-stream)))
    (if (eq (car info) :broadcast)
        (cdr info)
        (error 'type-error :datum broadcast-stream
                           :expected-type 'broadcast-stream))))

;;; FORCE-OUTPUT / FINISH-OUTPUT must recurse into a compound stream's
;;; components (the kernel primitive only flushes the stream's own FILE*).
;;; Capture the kernel primitives, then redefine.
(setf (symbol-function 'bocl::%kernel-force-output)  (symbol-function 'force-output)
      (symbol-function 'bocl::%kernel-finish-output) (symbol-function 'finish-output))

(defun force-output (&optional (stream *standard-output*))
  (let ((info (bocl::%compound-stream-info stream)))
    (if info
        (progn (dolist (s (cdr info)) (force-output s)) nil)
        (bocl::%kernel-force-output stream))))

(defun finish-output (&optional (stream *standard-output*))
  (let ((info (bocl::%compound-stream-info stream)))
    (if info
        (progn (dolist (s (cdr info)) (finish-output s)) nil)
        (bocl::%kernel-finish-output stream))))

(defun make-concatenated-stream (&rest streams)
  "Return a stream that reads from STREAMS in sequence.
  Stub: returns the first stream or an empty string stream."
  (if streams (car streams) (make-string-input-stream "")))

(defun make-echo-stream (input-stream output-stream)
  "Return a stream that reads from INPUT-STREAM and echoes to OUTPUT-STREAM.
  Stub: returns INPUT-STREAM."
  output-stream input-stream)

(defun make-synonym-stream (symbol)
  "Return a stream that is a synonym for the stream in SYMBOL's value.
  Stub: returns the current value of SYMBOL."
  (symbol-value symbol))

(defun make-two-way-stream (input-stream output-stream)
  "Return a two-way stream combining INPUT-STREAM and OUTPUT-STREAM.
  Stub: returns INPUT-STREAM."
  output-stream input-stream)

;;; ---- Array utilities ----------------------------------------------------

(defun array-total-size (array)
  "Return the total number of elements in ARRAY."
  (reduce #'* (array-dimensions array) :initial-value 1))

(defun array-rank (array)
  "Return the number of dimensions of ARRAY."
  (length (array-dimensions array)))

(defun array-in-bounds-p (array &rest subscripts)
  "Return T if SUBSCRIPTS are valid indices into ARRAY."
  (let ((dims (array-dimensions array))
        (result t))
    (let ((ss subscripts) (ds dims))
      (loop
        (when (or (null ss) (null ds)) (return))
        (unless (and (>= (car ss) 0) (< (car ss) (car ds)))
          (setq result nil) (return))
        (setq ss (cdr ss) ds (cdr ds))))
    result))

(defun array-has-fill-pointer-p (array)
  "Return T if ARRAY has a fill pointer."
  (if (%fill-pointer-of array) t nil))

(defun fill-pointer (vector)
  "Return the fill pointer of VECTOR (an error if it has none)."
  (or (%fill-pointer-of vector)
      (error "FILL-POINTER: ~S has no fill pointer" vector)))

(defun %set-fill-pointer-checked (vector new)
  "Backing for (SETF FILL-POINTER): set VECTOR's fill pointer to NEW."
  (unless (%fill-pointer-of vector)
    (error "(SETF FILL-POINTER): ~S has no fill pointer" vector))
  (unless (and (integerp new) (<= 0 new) (<= new (array-total-size vector)))
    (error "(SETF FILL-POINTER): ~S out of range for ~S" new vector))
  (%set-fill-pointer vector new)
  new)

(defun adjustable-array-p (array)
  "Return T if ARRAY is adjustable."
  (and (%md-array-p array) (%md-array-adjustable array) t))

(defun simple-vector-p (object)
  "Return T if OBJECT is a SIMPLE GENERAL vector — element-type T, not adjustable,
no fill pointer, not displaced.  Strings and bit-vectors are vectors but NOT
simple-vectors (CLHS)."
  (and (vectorp object)
       (not (stringp object))
       (not (bit-vector-p object))
       (not (%md-array-p object))
       (not (%fill-pointer-of object))))

(defun simple-bit-vector-p (object)
  "Return T if OBJECT is a simple bit-vector (a bit-vector with no fill pointer;
bocl has no adjustable/displaced bit-vectors)."
  (and (bit-vector-p object) (not (%fill-pointer-of object))))

(defun vector-fill (vector item &key (start 0) end)
  "Fill elements of VECTOR with ITEM."
  (fill vector item :start start :end end))

(defun vector-push (new-element vector)
  "Store NEW-ELEMENT at VECTOR's fill pointer and increment it.  Returns the
index where NEW-ELEMENT was stored, or NIL if VECTOR is already full.  AREF
and (SETF AREF) operate on the underlying storage, so this works for both
character (string-backed) and general vectors."
  (let ((fp (%fill-pointer-of vector)))
    (unless fp (error "VECTOR-PUSH: ~S has no fill pointer" vector))
    (when (< fp (array-total-size vector))
      (setf (aref vector fp) new-element)
      (%set-fill-pointer vector (1+ fp))
      fp)))

(defun vector-pop (vector)
  "Decrement VECTOR's fill pointer and return the element now just past it."
  (let ((fp (%fill-pointer-of vector)))
    (unless fp (error "VECTOR-POP: ~S has no fill pointer" vector))
    (when (zerop fp) (error "VECTOR-POP: ~S is empty" vector))
    (let ((new (1- fp)))
      (%set-fill-pointer vector new)
      (aref vector new))))

(defun vector-push-extend (new-element vector &optional extension)
  "Like VECTOR-PUSH but grows the storage when full.  bocl cannot grow an
array object in place (no spare indirection), so when VECTOR is full this
signals an error rather than silently corrupting; while capacity remains it
behaves exactly like VECTOR-PUSH and always succeeds."
  (let ((fp (%fill-pointer-of vector)))
    (unless fp (error "VECTOR-PUSH-EXTEND: ~S has no fill pointer" vector))
    (when (>= fp (array-total-size vector))
      ;; Full: grow an adjustable array's storage in place (a %md-array keeps its
      ;; elements in a separate vector we can replace); a non-adjustable vector
      ;; cannot grow.
      (if (and (%md-array-p vector) (%md-array-adjustable vector))
          (let* ((cap    (array-total-size vector))
                 (newcap (max (or extension (max 8 (* 2 cap))) (1+ fp)))
                 (old    (%md-array-storage vector))
                 (new    (%make-typed-storage (%md-array-element-type vector)
                                              newcap nil nil 0)))
            (dotimes (i cap) (%storage-set new i (%storage-ref old i)))
            (setf (%md-array-storage vector)    new
                  (%md-array-dimensions vector) (list newcap)))
          (error "VECTOR-PUSH-EXTEND: ~S is full and not adjustable" vector)))
    (setf (aref vector fp) new-element)
    (%set-fill-pointer vector (1+ fp))
    fp))

;;; ---- Bit-vector operations: implemented below (search "BIT-VECTOR ops"). ----

;;; ---- LOOP-FINISH --------------------------------------------------------

;;; LOOP-FINISH is a macro that terminates the LOOP iteration at the named loop
;;; end tag.  In bocl's simple-loop form: wrap the tag in a block and use
;;; return.  For keyword-LOOP, it's already supported via named/RETURN.
;;;
;;; The simplest implementation: LOOP-FINISH is a macro that calls RETURN from
;;; the enclosing LOOP block (which is wrapped in (block nil ...)).

(defmacro loop-finish ()
  "Terminate the immediately enclosing LOOP form, returning the accumulated
  result (or NIL for simple loops)."
  '(return nil))

;;; ---- Pathname utilities --------------------------------------------------

(defun wild-pathname-p (pathname &optional field)
  "Return T if PATHNAME contains wildcard characters.
  Stub in bocl — wildcards not supported; always returns NIL."
  pathname field nil)

(defun pathname-match-p (pathname wildcard)
  "Return T if PATHNAME matches the WILDCARD pattern.  Stub: always NIL."
  pathname wildcard nil)

(defun translate-pathname (source from-wildcard to-wildcard)
  "Translate PATHNAME from FROM-WILDCARD to TO-WILDCARD.  Stub: returns SOURCE."
  from-wildcard to-wildcard source)

;;; ---- Type utilities -------------------------------------------------------

;;; Extend TYPE-OF for CLOS instances (complement the C implementation).
;;; The C kernel_TYPE_OF handles most cases; for CLOS instances it returns
;;; the class name symbol.  This Lisp wrapper is not needed but documents intent.

;;; TYPECASE / ETYPECASE already use TYPEP so they work with CLOS classes.

;;; ---- Character utilities -------------------------------------------------

(defun char-name (char)
  "Return the name of CHARACTER as a string, or NIL."
  ;; bocl's newline is NEL (code 133, the internal line separator); ASCII LF
  ;; (code 10) is #\Linefeed.  Keep this consistent with NAME-CHAR and the reader
  ;; so (name-char (char-name #\Newline)) round-trips.
  (case (char-code char)
    (0   "Null")     (7  "Bell")    (8  "Backspace")
    (9   "Tab")      (10 "Linefeed")(11 "Page")
    (12  "Page")     (13 "Return")  (27 "Escape")
    (32  "Space")    (127 "Rubout") (133 "Newline") (t nil)))

(defun name-char (name)
  "Return the character with the given NAME, or NIL."
  (let ((n (string-upcase (string name))))
    (cond ((string= n "NULL")      (code-char 0))
          ((string= n "BELL")      (code-char 7))
          ((string= n "BACKSPACE") (code-char 8))
          ((string= n "TAB")       (code-char 9))
          ((string= n "LINEFEED")  (code-char 10))
          ((string= n "NEWLINE")   (code-char 133)) ; NEL — bocl's newline
          ((string= n "LINE_SEPARATOR") (code-char 133))
          ((string= n "RETURN")    (code-char 13))
          ((string= n "ESCAPE")    (code-char 27))
          ((string= n "SPACE")     (code-char 32))
          ((string= n "RUBOUT")    (code-char 127))
          ((string= n "DELETE")    (code-char 127))
          (t nil))))

;;; ---- Miscellaneous -------------------------------------------------------

(defun make-load-form (object &optional environment)
  "Return a form that reconstructs OBJECT.  Stub — signals an error."
  environment
  (error "MAKE-LOAD-FORM: not supported for ~S" object))

(defun make-load-form-saving-slots (object &key slot-names environment)
  "Return forms to reconstruct OBJECT via slot-value.  Stub."
  slot-names environment
  (error "MAKE-LOAD-FORM-SAVING-SLOTS: not supported for ~S" object))

(defun upgraded-array-element-type (type &optional env)
  "The element type an array of element-type TYPE actually holds.  bocl stores
BIT and CHARACTER arrays specially and everything else in a general (T) array, so
report BIT / the character type / T accordingly (each is its own upgrade, so
SUBTYPEP round-trips)."
  (declare (ignore env))
  (let ((head (if (consp type) (car type) type)))
    (cond ((eq head 'bit) 'bit)
          ((member head '(character base-char standard-char extended-char)) head)
          (t t))))

(defun row-major-aref (array index)
  "Access element at ROW-MAJOR linear INDEX in ARRAY."
  (svref array index))

(defun array-row-major-index (array &rest subscripts)
  "Compute the row-major linear index from SUBSCRIPTS."
  (let ((dims (array-dimensions array))
        (idx 0))
    (dolist (sub subscripts idx)
      (setq idx (+ (* idx (car dims)) sub))
      (setq dims (cdr dims)))))

;;;; ========================================================================
;;;; Slice 23 — Timing, universal-time, string I/O, array predicates,
;;;;   ISQRT, NTHCDR alias, and other standard completions.
;;;; ========================================================================

;;; ---- Universal time: DECODE / ENCODE ------------------------------------
;;;
;;; CL universal time is seconds since 1900-01-01 00:00:00 UTC.

(defun decode-universal-time (universal-time &optional (time-zone 0))
  "Return 9 values: second minute hour date month year day-of-week
  daylight-saving-time-p time-zone."
  ;; Convert from 1900 epoch to 1970 epoch.
  (let* ((unix-time (- universal-time 2208988800))
         ;; Apply time zone (in hours, positive = west of UTC)
         (adjusted  (- unix-time (* time-zone 3600)))
         ;; Basic calendar decomposition
         (seconds-per-day 86400)
         (days-since-epoch (floor adjusted seconds-per-day))
         (seconds-of-day   (mod adjusted seconds-per-day))
         (hour   (floor seconds-of-day 3600))
         (minute (floor (mod seconds-of-day 3600) 60))
         (second (mod seconds-of-day 60))
         ;; Day-of-week: 1970-01-01 was a Thursday = day 3 (0=Monday per CL)
         (dow    (mod (+ days-since-epoch 3) 7))
         ;; Compute year/month/day from days-since-1970
         ;; Simplified: use 365/366 day years
         (year   (let ((y 1970) (d days-since-epoch))
                   (loop
                     (let ((dy (if (= 0 (mod y 4)) 366 365)))
                       (when (< d dy) (return y))
                       (decf d dy) (incf y)))))
         (day-of-year (let ((d days-since-epoch) (y 1970))
                        (loop
                          (let ((dy (if (= 0 (mod y 4)) 366 365)))
                            (when (< d dy) (return d))
                            (decf d dy) (incf y)))))
         (month-days #(31 28 31 30 31 30 31 31 30 31 30 31))
         (month      (let ((d day-of-year) (m 0))
                       (dotimes (i 12 (1+ m))
                         (let ((md (if (and (= i 1) (= 0 (mod year 4))) 29
                                       (svref month-days i))))
                           (when (< d md) (return (1+ i)))
                           (decf d md)
                           (setq m i)))))
         (day        (1+ (let ((d day-of-year))
                           (dotimes (i (1- month) d)
                             (decf d (if (and (= i 1) (= 0 (mod year 4)))
                                         29 (svref month-days i))))))))
    (values second minute hour day month year dow nil time-zone)))

(defun encode-universal-time (second minute hour date month year
                               &optional (time-zone 0))
  "Return universal time (seconds since 1900-01-01) for the given components."
  ;; Convert year/month/day to days since 1970-01-01
  (let* ((y (if (< year 100) (+ year 1900) year))
         (days 0))
    ;; Count days from 1970 to year
    (dotimes (yr (- y 1970))
      (incf days (if (= 0 (mod (+ 1970 yr) 4)) 366 365)))
    ;; Add months
    (let ((month-days #(31 28 31 30 31 30 31 31 30 31 30 31)))
      (dotimes (m (1- month))
        (incf days (if (and (= m 1) (= 0 (mod y 4))) 29
                       (svref month-days m)))))
    ;; Add days (1-based)
    (incf days (1- date))
    ;; Convert to seconds, add time of day, adjust timezone
    (+ 2208988800                               ; 1900→1970 offset
       (* days 86400)
       (* hour 3600)
       (* minute 60)
       second
       (* time-zone 3600))))                    ; timezone adjustment

;;; ---- COPY-SEQ as a Lisp function ----------------------------------------
;;; Already defined in Slice 20; no change needed.

;;; ---- NTHCDR alias --------------------------------------------------------
(defun nthcdr (n list)
  "Return the Nth CDR of LIST (standard alias for repeated CDR)."
  (dotimes (i n list)
    (setq list (cdr list))))

;;; ---- LAST-CONS -----------------------------------------------------------
;;; LAST is already in kernel; alias for CL compatibility.

;;; ---- DEFINE-MODIFY-MACRO ------------------------------------------------
(defmacro define-modify-macro (name lambda-list function &optional documentation)
  "Define a modify macro that applies FUNCTION to a place and stores the result."
  documentation  ; ignored
  (let ((place (gensym "PLACE-"))
        (env   (gensym "ENV-")))
    `(defmacro ,name (,place ,@lambda-list &environment ,env)
       ,env  ; ignored
       (let ((g (gensym "DM-")))
         `(let ((,g ,(list ',function ,place ,@lambda-list)))
            (setf ,,`(quote ,place) ,g)
            ,g)))))

;;; ---- INCF / DECF for generic delta --------------------------------------
;;; Already defined; no change.

;;; ---- REDUCE improvements ------------------------------------------------
;;; REDUCE with :key was added in Slice 18.  No change needed.

;;; ---- INTERNAL-TIME-UNITS-PER-SECOND -------------------------------------
;;; Already defined in prelude as (defconstant internal-time-units-per-second 1000000).

;;;; ========================================================================
;;;; Slice 25 — environment queries, module system, number conversions,
;;;;   compiler stubs, stream predicates, DEFSETF, MACRO-FUNCTION,
;;;;   WITH-HASH-TABLE-ITERATOR, and standard variable completions.
;;;; ========================================================================

;;; ---- RATIONAL / RATIONALIZE -----------------------------------------------
;;; bocl has no distinct rational/ratio type.  Integers stay as integers;
;;; floats are returned as-is (no exact rational approximation).

(defun rational (x)
  "Convert X to a rational.  In bocl: identity for integers; floats returned
  as-is since bocl has no ratio type."
  (if (integerp x) x
      (if (floatp x) x
          (error "RATIONAL: ~S is not a real number" x))))

(defun rationalize (x)
  "Like RATIONAL but may return a simpler rational.  In bocl: same as RATIONAL."
  (rational x))

;;; ---- *MODULES* / PROVIDE / REQUIRE ----------------------------------------

(defvar *modules* nil
  "List of (string) names of modules that have been loaded via PROVIDE.")

(defun provide (module-name)
  "Record MODULE-NAME as a loaded module.  Returns MODULE-NAME."
  (let ((name (string module-name)))
    (pushnew name *modules* :test #'string=)
    module-name))

(defun require (module-name &optional pathnames)
  "Load MODULE-NAME unless already provided.  PATHNAMES may be a path or a list of paths."
  (let ((name (string module-name)))
    (unless (member name *modules* :test #'string=)
      (cond
        (pathnames
         (dolist (p (if (listp pathnames) pathnames (list pathnames)))
           (load p)))
        (t (error "REQUIRE: module ~S not loaded and no pathname given" module-name)))))
  module-name)

;;; ---- Load-time dynamic variables -----------------------------------------

(defvar *load-pathname* nil
  "Pathname of the file currently being loaded, or NIL.")
(defvar *load-truename* nil
  "Truename of the file currently being loaded, or NIL.")
(defvar *compile-file-pathname* nil
  "Pathname of the file currently being compiled, or NIL.")
(defvar *compile-file-truename* nil
  "Truename of the file currently being compiled, or NIL.")

;;; ---- Site name -----------------------------------------------------------
;;; SHORT-SITE-NAME and LONG-SITE-NAME are kernel built-ins (sources/bocl.c):
;;; bocl maps the site name to the host identity — SHORT-SITE-NAME is the short
;;; host name, LONG-SITE-NAME the fully-qualified domain name. They are no longer
;;; defined here so the kernel definitions are not shadowed by NIL stubs.

;;; ---- Stream predicates ---------------------------------------------------
;;; STREAMP, INPUT-STREAM-P, OUTPUT-STREAM-P already in kernel.
;;; bocl has STRING-STREAM and FILE-STREAM kernel classes; use TYPEP.

(defun string-stream-p (stream)
  "Return T if STREAM is a string (in-memory) stream."
  (typep stream 'string-stream))

(defun file-stream-p (stream)
  "Return T if STREAM is a file (disk) stream.
  In bocl, string-stream is a subclass of file-stream; we exclude string streams."
  (and (typep stream 'file-stream)
       (not (typep stream 'string-stream))))

(defun synonym-stream-p (stream)
  "Return T if STREAM is a synonym stream.  In bocl: always NIL."
  (declare (ignore stream)) nil)

(defun broadcast-stream-p (stream)
  "Return T if STREAM is a broadcast stream."
  (eq :broadcast (car (bocl::%compound-stream-info stream))))

(defun concatenated-stream-p (stream)
  "Return T if STREAM is a concatenated stream.  In bocl: always NIL."
  (declare (ignore stream)) nil)

(defun two-way-stream-p (stream)
  "Return T if STREAM is a two-way stream.  In bocl: always NIL."
  (declare (ignore stream)) nil)

(defun echo-stream-p (stream)
  "Return T if STREAM is an echo stream.  In bocl: always NIL."
  (declare (ignore stream)) nil)

;;; ---- MACRO-FUNCTION / (SETF MACRO-FUNCTION) ------------------------------
;;; In bocl, DEFMACRO stores the expansion function on the symbol's plist
;;; under the key $MACRO (the symbol named "$MACRO" in the kernel).

(defun macro-function (symbol &optional environment)
  "Return the macro-expansion function for SYMBOL, or NIL if not a macro.  A
   lexically-local macro (MACROLET / WITH-HASH-TABLE-ITERATOR) visible in
   ENVIRONMENT shadows the global definition."
  (or (and environment (%find-local-macro symbol environment))
      (get symbol '$macro)))

;;; (setf macro-function) — bocl's defun doesn't support (setf name) form yet.
;;; Use set-symbol-macro-function directly via the plist.
(defun set-symbol-macro-function (symbol fn)
  "Set SYMBOL's macro expansion function to FN."
  (setf (get symbol '$macro) fn)
  fn)

;;; CLHS defines LAMBDA as a macro: (lambda lambda-list . body) expands to
;;; (function (lambda lambda-list . body)).  The evaluator handles (lambda ...)
;;; as a special form directly (eval.c), so this registration does not change
;;; evaluation — it just makes MACRO-FUNCTION / MACROEXPAND see LAMBDA as the
;;; macro it is (a bocl macro function receives the form's arguments).
(set-symbol-macro-function 'lambda
  (lambda (&rest lambda-list-and-body)
    (list 'function (cons 'lambda lambda-list-and-body))))

;;; SPECIAL-OPERATOR-P — kernel special forms.
(defun special-operator-p (symbol)
  "Return T if SYMBOL names a special operator."
  (if (member symbol '(block catch eval-when flet function go if labels
                       let let* load-time-value locally macrolet multiple-value-call
                       multiple-value-prog1 progn progv quote return-from setq
                       symbol-macrolet tagbody the throw unwind-protect))
      t nil))

;;; COMPILER-MACRO-FUNCTION — bocl has no compiler macros.
(defun compiler-macro-function (name &optional environment)
  "Return the compiler-macro function for NAME, or NIL.  bocl: always NIL."
  (declare (ignore name environment)) nil)

(defmacro define-compiler-macro (name lambda-list &body body)
  "Define a compiler macro.  In bocl: silently ignored (no compiler)."
  (declare (ignore lambda-list body))
  `',name)

;;; ---- DEFSETF ---------------------------------------------------------------
;;; Short form: (defsetf accessor update-fn)
;;; Long form:  (defsetf accessor lambda-list (store-var) &body body)

(defmacro defsetf (access-fn &rest rest)
  "Define a setf expansion for ACCESS-FN.
  Short form: (defsetf acc update-fn &optional doc)
  Long form:  (defsetf acc (params) (store-var) body...)"
  (cond
    ;; Short form: second arg is a symbol (the update function name)
    ((symbolp (car rest))
     (let ((update-fn (car rest)))
       `(define-setf-expander ,access-fn (&rest %args%)
          (let ((%store% (gensym "STORE-")))
            (values nil %args% (list %store%)
                    (list* ',update-fn (append %args% (list %store%)))
                    (cons ',access-fn %args%))))))
    ;; Long form: second arg is a lambda-list
    (t
     (let* ((lambda-list (car rest))
            (store-vars  (cadr rest))
            (body        (cddr rest)))
       `(define-setf-expander ,access-fn ,lambda-list
          (let ,(mapcar (lambda (v) `(,v (gensym ,(string v)))) store-vars)
            (values nil
                    (list ,@(remove-if (lambda (s) (member s lambda-list-keywords)) lambda-list))
                    (list ,@store-vars)
                    (progn ,@body)
                    (list ',access-fn
                          ,@(remove-if (lambda (s) (member s lambda-list-keywords)) lambda-list)))))))))

;;; ---- WITH-HASH-TABLE-ITERATOR -----------------------------------------------

(defmacro with-hash-table-iterator (name-and-table &body body)
  "Establish an iterator NAME over HASH-TABLE.
  Call syntax: (with-hash-table-iterator (name table) body...).
  NAME is a local function of no arguments; each call returns
  (values more-p key value).  When the table is exhausted, returns (values NIL)."
  ;; CLHS: NAME is established as a local MACRO (not a function), so it must be
  ;; built with MACROLET — that is what MACRO-FUNCTION/&ENVIRONMENT report.  The
  ;; macro takes no arguments and expands to the next-entry step form referencing
  ;; the gensym KEYS-VAR / TABLE-VAR bound by the surrounding LET*.  (bocl has no
  ;; nested backquote, so the expansion is built with explicit LIST forms.)
  (let ((name       (car  name-and-table))
        (hash-table (cadr name-and-table))
        (keys-var   (gensym "KEYS-"))
        (table-var  (gensym "HT-"))
        (kv         (gensym "K-"))
        (vv         (gensym "V-")))
    `(let* ((,table-var ,hash-table)
            (,keys-var  (hash-table-keys ,table-var)))
       (macrolet ((,name ()
                    (list 'if (list 'null ',keys-var)
                          '(values nil nil nil)
                          (list 'let* (list (list ',kv (list 'car ',keys-var))
                                            (list ',vv (list 'gethash ',kv ',table-var)))
                                (list 'setq ',keys-var (list 'cdr ',keys-var))
                                (list 'values t ',kv ',vv)))))
         ,@body))))

;;; ---- GET-SETF-EXPANSION ---------------------------------------------------

(defun get-setf-expansion (place &optional environment)
  "Return the five-value setf expansion for PLACE: (temps values stores
store-form access-form).  Derived from the very *SETF-EXPANDERS* table the SETF
macro uses, so it covers every place SETF does.  Each place subform is bound to
a fresh temp (evaluated once), and both the store-form and access-form refer to
those temps — the property SHIFTF/ROTATEF/INCF rely on for correctness when
places share structure."
  (declare (ignore environment))
  (cond
    ((symbolp place)
     (let ((store (gensym "STORE-")))
       (values nil nil (list store) `(setq ,place ,store) place)))
    ((and (consp place) (eq (car place) 'values))
     ;; (values p1 p2 ...): concatenate the sub-expansions.  STORES is the union
     ;; of every place's store variables (so a caller MULTIPLE-VALUE-BINDs them to
     ;; the supplied values); the store-form runs each place's store-form; the
     ;; access-form reads them all back as multiple values.
     (let ((vars nil) (vals nil) (stores nil) (sforms nil) (accesses nil))
       (dolist (p (cdr place))
         (multiple-value-bind (v vl s sf af) (get-setf-expansion p)
           (setq vars     (append vars v))
           (setq vals     (append vals vl))
           (setq stores   (append stores s))
           (setq sforms   (append sforms (list sf)))
           (setq accesses (append accesses (list af)))))
       (values vars vals stores
               `(progn ,@sforms (values ,@stores))
               `(values ,@accesses))))
    ((consp place)
     (let ((expander (gethash (car place) *setf-expanders*)))
       (if expander
           (let* ((args  (cdr place))
                  (temps (mapcar (lambda (a) a (gensym "G-")) args))
                  (store (gensym "STORE-")))
             (values temps
                     args
                     (list store)
                     (apply expander (append temps (list store)))
                     (cons (car place) temps)))
           (error "GET-SETF-EXPANSION: no setf expander for ~S" (car place)))))
    (t (error "GET-SETF-EXPANSION: invalid place ~S" place))))

;;; ---- DEFINE-SYMBOL-MACRO --------------------------------------------------
;;; Installs a global symbol macro so every reference to SYMBOL
;;; expands to EXPANSION.

(defmacro define-symbol-macro (symbol expansion)
  "Define SYMBOL as a global symbol macro expanding to EXPANSION."
  `(progn
     ;; Store the expansion in the symbol's plist under %symbol-macro.
     (setf (get ',symbol '%symbol-macro) ',expansion)
     ',symbol))

;;; ---- LOAD-LOGICAL-PATHNAME-TRANSLATIONS ----------------------------------

(defun load-logical-pathname-translations (host)
  "Load logical pathname translations for HOST.  Stub: always returns NIL."
  (declare (ignore host)) nil)

;;; ---- LOGICAL-PATHNAME stubs -----------------------------------------------

(defun logical-pathname (pathspec)
  "Convert PATHSPEC to a logical pathname.  In bocl: signal error (no logical pathnames)."
  (error "LOGICAL-PATHNAME: bocl does not support logical pathnames (got ~S)" pathspec))

(defun translate-logical-pathname (pathname &key)
  "Translate a logical pathname to a physical pathname.  Stub."
  pathname)

;;; ---- COMPILE-FILE-PATHNAME -----------------------------------------------

(defun compile-file-pathname (input-file &key output-file &allow-other-keys)
  "Return the pathname COMPILE-FILE would write to: the source pathname with
   its type changed to \"fasl\".  (bocl has no real compiler — see COMPILE-FILE.)
   A distinct type is required: programs such as the ANSI test suite assert that
   the compiled-file type differs from the source type."
  (or output-file
      (let* ((in  (pathname input-file))
             (dir (directory-namestring in))
             (nm  (or (pathname-name in) "")))
        (pathname (concatenate 'string dir nm ".fasl")))))

(defun compile-file (input-file &key output-file &allow-other-keys)
  "bocl is an interpreter with no real compiler.  COMPILE-FILE does not produce
   a fasl; it just reports the conventional output pathname.  LOAD of that
   (absent) pathname falls back to the source, so the COMPILE-AND-LOAD idiom in
   the ANSI suite still loads everything — without writing any files.
   Returns (values output-truename warnings-p failure-p).
   NOTE: the real definition (which also fires EVAL-WHEN :COMPILE-TOPLEVEL) is the
   later one near the COMPILE-FILE / lisp-file-types section; this early stub is
   superseded at load time."
  (let* ((in  (merge-pathnames input-file *default-pathname-defaults*))
         (out (compile-file-pathname in :output-file output-file)))
    (values (pathname out) nil nil)))

;;; ---- *PRINT-PPRINT-DISPATCH* ---------------------------------------------

;; bocl has no pretty printer, but CLHS requires *PRINT-PPRINT-DISPATCH* to hold a
;; pprint dispatch table object (not NIL).  This opaque empty-table struct is that
;; non-NIL placeholder; the dispatch stubs below ignore its contents.
(defstruct (%pprint-dispatch-table (:copier nil)))

(defvar *print-pprint-dispatch* (make-%pprint-dispatch-table)
  "The current pprint dispatch table.  In bocl: an opaque empty table (no pretty
   printer), but non-NIL as CLHS requires.")

(defun set-pprint-dispatch (type-specifier function &optional priority table)
  "Install FUNCTION as the pprint dispatch for TYPE-SPECIFIER.  Stub."
  (declare (ignore type-specifier function priority table)) nil)

(defun copy-pprint-dispatch (&optional table)
  "Return a copy of the pprint dispatch table.  Stub: a fresh empty table."
  (declare (ignore table)) (make-%pprint-dispatch-table))

;;; ---- MAKE-BROADCAST-STREAM etc. stubs ------------------------------------
;;; Broadcast, concatenated, synonym, echo, two-way streams are already
;;; stubbed in the stream-constructors section above; no additional stubs needed.

;;; ---- SET / FDEFINITION ---------------------------------------------------
;;; GENSYM and *GENSYM-COUNTER* are kernel-side (sources/sequences.c): GENSYM now
;;; accepts a string prefix or an integer suffix designator and uses the standard
;;; *GENSYM-COUNTER* special variable (defined via define_variable).

(defun set (symbol value)
  "Assign VALUE to the dynamic (special-variable) value cell of SYMBOL; return VALUE."
  (setf (symbol-value symbol) value))

(defun fdefinition (name)
  "Return the function definition named by the function-name NAME.  For a symbol
   this is its SYMBOL-FUNCTION.  bocl has no (SETF f) functions, so only symbol
   function-names are accepted."
  (if (symbolp name)
      (symbol-function name)
      (error "FDEFINITION: ~S is not a function name." name)))

;;;; =========================================================================
;;;; Slice 26 — string operations with :START/:END, char predicates,
;;;;   REMOVE-DUPLICATES for all sequences, FORMAT ~[~;~] conditional,
;;;;   FORMAT ~? indirect, EVAL/MACROEXPAND, princ keyword fix.
;;;; =========================================================================

;;; ---- STRING-UPCASE / STRING-DOWNCASE / STRING-CAPITALIZE with :start/:end

(defun string-upcase (string &key (start 0) end)
  "Return a copy of STRING with characters in [START,END) upcased."
  (let* ((s   (string string))
         (len (string-length s))
         (stop (min (or end len) len))
         (out (make-string len :initial-element #\Space)))
    (dotimes (i len out)
      (char-set out i (if (and (>= i start) (< i stop))
                          (char-upcase (char s i))
                          (char s i))))))

(defun string-downcase (string &key (start 0) end)
  "Return a copy of STRING with characters in [START,END) downcased."
  (let* ((s   (string string))
         (len (string-length s))
         (stop (min (or end len) len))
         (out (make-string len :initial-element #\Space)))
    (dotimes (i len out)
      (char-set out i (if (and (>= i start) (< i stop))
                          (char-downcase (char s i))
                          (char s i))))))

(defun string-capitalize (string &key (start 0) end)
  "Return a copy of STRING with words capitalised in [START,END)."
  (let* ((s      (string string))
         (len    (string-length s))
         (stop   (min (or end len) len))
         (out    (make-string len :initial-element #\Space))
         (in-word nil))
    (dotimes (i len out)
      (let ((c (char s i)))
        (cond ((and (>= i start) (< i stop))
               (cond ((alphanumericp c)
                      (char-set out i (if in-word (char-downcase c) (char-upcase c)))
                      (setq in-word t))
                     (t
                      (char-set out i c)
                      (setq in-word nil))))
              (t
               (char-set out i c)))))))

;;; Destructive variants
(defun nstring-upcase (string &key (start 0) end)
  "Destructively upcase characters in STRING[START,END)."
  (let* ((len  (string-length string))
         (stop (min (or end len) len)))
    (dotimes (i (- stop start) string)
      (char-set string (+ start i) (char-upcase (char string (+ start i)))))))

(defun nstring-downcase (string &key (start 0) end)
  "Destructively downcase characters in STRING[START,END)."
  (let* ((len  (string-length string))
         (stop (min (or end len) len)))
    (dotimes (i (- stop start) string)
      (char-set string (+ start i) (char-downcase (char string (+ start i)))))))

;;; ---- Character predicates -------------------------------------------------

(defun char-whitespace-p (char)
  "Return T if CHAR is a whitespace character.
  Note: in bocl, #\\Newline has char-code 133 (0x85 / Unicode NEL)."
  (let ((c (char-code char)))
    (or (= c 32)    ; Space
        (= c 9)     ; Tab
        (= c 133)   ; #\Newline in bocl (0x85)
        (= c 10)    ; LF (standard newline in ASCII)
        (= c 13)    ; Return
        (= c 12)    ; Form Feed
        (= c 11)    ; Vertical Tab
        )))

(defun char-upper-case-p (char) (upper-case-p char))
(defun char-lower-case-p (char) (lower-case-p char))
(defun char-alphabetic-p (char) (alpha-char-p char))
(defun char-numeric-p    (char) (if (digit-char-p char) t nil))
(defun char-int          (char) (char-code char))

;;; ---- REMOVE-DUPLICATES for all sequence types ----------------------------

(defun remove-duplicates (sequence &key (test #'eql) test-not (key #'identity)
                                        from-end (start 0) end)
  "Return SEQUENCE with duplicate elements in [START,END) removed.  Without
FROM-END the LAST of each set of duplicates is kept; with FROM-END, the FIRST."
  ;; O(n) fast path for a whole list: walk the conses (not ELT) and track seen
  ;; keys in a hash when TEST is standard (else MEMBER).  The general index-based
  ;; path below is O(n^2)/O(n^3-on-lists), which made large dedups (the ANSI
  ;; search-bitvector *pattern-subbitvectors*) crawl.
  (when (and (listp sequence) (eql start 0) (null end) (null test-not))
    (let* ((ht-test (%std-hash-test test))
           (seen-ht (and ht-test (make-hash-table :test ht-test)))
           (seen    nil)
           (acc     nil))
      (flet ((seen-p (k) (if seen-ht (gethash k seen-ht) (member k seen :test test)))
             (mark   (k) (if seen-ht (setf (gethash k seen-ht) t) (push k seen))))
        (dolist (e (if from-end sequence (reverse sequence)))
          (let ((k (funcall key e)))
            (unless (seen-p k) (mark k) (push e acc))))
        (return-from remove-duplicates (if from-end (nreverse acc) acc)))))
  (let* ((test  (if test-not (lambda (a b) (not (funcall test-not a b))) test))
         (len   (length sequence))
         (stop  (min (or end len) len))
         (seen  nil)
         (acc   nil))
    (if from-end
        ;; keep FIRST occurrence: scan forward, keep the first-seen of each key
        (do ((i start (1+ i))) ((>= i stop))
          (let* ((e (elt sequence i)) (k (funcall key e)))
            (unless (member k seen :test test) (push k seen) (push e acc))))
        ;; keep LAST occurrence: scan backward, keep the first-seen from the right
        (do ((i (1- stop) (1- i))) ((< i start))
          (let* ((e (elt sequence i)) (k (funcall key e)))
            (unless (member k seen :test test) (push k seen) (push e acc)))))
    (let ((middle (if from-end (nreverse acc) acc))
          (prefix (subseq sequence 0 start))
          (suffix (subseq sequence stop len)))
      (if (listp sequence)
          (append (coerce prefix 'list) middle (coerce suffix 'list))
          (concatenate (cond ((stringp sequence) 'string)
                             ((bit-vector-p sequence) 'bit-vector)
                             (t 'vector))
                       prefix middle suffix)))))

(defun delete-duplicates (sequence &rest keys)
  "Destructive version of REMOVE-DUPLICATES (non-destructive in bocl)."
  (apply #'remove-duplicates sequence keys))

;;; ---- VALUES-LIST ----------------------------------------------------------
;;; Already implemented as a C kernel function (kernel_VALUES_LIST).
;;; Expose a Lisp alias in case the kernel symbol was registered differently.

;;; ---- CHAR comparisons (variadic) -----------------------------------------

(defun char= (&rest chars)
  (or (null chars) (null (cdr chars))
      (and (eql (char-code (car chars)) (char-code (cadr chars)))
           (apply #'char= (cdr chars)))))

(defun char/= (&rest chars)
  ;; CHAR/= is true iff the characters are PAIRWISE distinct (CLHS) — every
  ;; character must differ from EVERY other, not merely from its neighbour, so
  ;; (char/= #\a #\b #\a) is false.  (CHAR-NOT-EQUAL defers here on downcased
  ;; arguments, so this also gives it the correct all-pairs semantics.)
  (or (null chars)
      (and (every (lambda (c) (not (eql (char-code (car chars)) (char-code c))))
                  (cdr chars))
           (apply #'char/= (cdr chars)))))

(defun char< (&rest chars)
  (or (null chars) (null (cdr chars))
      (and (< (char-code (car chars)) (char-code (cadr chars)))
           (apply #'char< (cdr chars)))))

(defun char> (&rest chars)
  (or (null chars) (null (cdr chars))
      (and (> (char-code (car chars)) (char-code (cadr chars)))
           (apply #'char> (cdr chars)))))

(defun char<= (&rest chars)
  (or (null chars) (null (cdr chars))
      (and (<= (char-code (car chars)) (char-code (cadr chars)))
           (apply #'char<= (cdr chars)))))

(defun char>= (&rest chars)
  (or (null chars) (null (cdr chars))
      (and (>= (char-code (car chars)) (char-code (cadr chars)))
           (apply #'char>= (cdr chars)))))

;;; Case-insensitive character comparisons, N-ary (the earlier 2-ary defs are
;;; superseded here): downcase every argument, then defer to the case-sensitive
;;; comparison of the same shape.
(defun char-equal        (&rest chars) (apply #'char=  (mapcar #'char-downcase chars)))
(defun char-not-equal    (&rest chars) (apply #'char/= (mapcar #'char-downcase chars)))
(defun char-lessp        (&rest chars) (apply #'char<  (mapcar #'char-downcase chars)))
(defun char-greaterp     (&rest chars) (apply #'char>  (mapcar #'char-downcase chars)))
(defun char-not-greaterp (&rest chars) (apply #'char<= (mapcar #'char-downcase chars)))
(defun char-not-lessp    (&rest chars) (apply #'char>= (mapcar #'char-downcase chars)))

;;; (CHARACTER designator) — the character a character-designator denotes:
;;; a character itself, a 1-char string, or a symbol with a 1-char name.
(defun character (designator)
  (cond ((characterp designator) designator)
        ((and (stringp designator) (= (length designator) 1)) (char designator 0))
        ((and (symbolp designator) (not (null designator))
              (= (length (symbol-name designator)) 1))
         (char (symbol-name designator) 0))
        (t (error 'type-error :datum designator
                  :expected-type '(or character (string 1) (symbol 1))))))

;;; ---- STANDARD-CHAR-P / GRAPHIC-CHAR-P ------------------------------------

(defun standard-char-p (char)
  "Return T if CHAR is a standard character (printable ASCII or newline).
  Accepts both bocl's #\\Newline (code 133) and ASCII LF (code 10)."
  (let ((c (char-code char)))
    (or (= c 133)                     ; bocl #\Newline (0x85)
        (= c 10)                      ; ASCII LF
        (and (>= c 32) (<= c 126))))) ; space..~

(defun graphic-char-p (char)
  "Return T if CHAR is a graphic character.  #\\Space is graphic (CLHS); the
   non-graphic ASCII characters are the controls (< 32), DEL (127), and the
   #\\Newline bocl stores at code 133."
  (let ((c (char-code char)))
    (and (>= c 32) (<= c 126))))

(defun print-char-p (char)
  "Synonym for GRAPHIC-CHAR-P — visible on a printed page."
  (graphic-char-p char))

;;;; =========================================================================
;;;; Slice 27 — sequence ops with :start/:end/:from-end/:count,
;;;;   string= etc. with bounds, WRITE-TO-STRING :base, number utilities.
;;;; =========================================================================

;;; ---- FIND-IF / FIND / POSITION-IF / POSITION  with full CL kwargs -------

(defun find-if (predicate sequence
                &key (start 0) end from-end (key #'identity))
  "Return first element in SEQUENCE[START,END) satisfying PREDICATE."
  ;; Forward list scan walks the conses directly — ELT on a list is O(n), so
  ;; the index loop below would be O(n^2) (and made e.g. SET-DIFFERENCE hang).
  (when (and (listp sequence) (not from-end))
    (return-from find-if
      (do ((tail (nthcdr start sequence) (cdr tail))
           (i start (1+ i)))
          ((or (null tail) (and end (>= i end))) nil)
        (when (funcall predicate (funcall key (car tail)))
          (return-from find-if (car tail))))))
  (let* ((len  (length sequence))
         (stop (min (or end len) len)))
    (if from-end
        (do ((i (1- stop) (1- i)))
            ((< i start) nil)
          (let ((x (elt sequence i)))
            (when (funcall predicate (funcall key x))
              (return-from find-if x))))
        (do ((i start (1+ i)))
            ((>= i stop) nil)
          (let ((x (elt sequence i)))
            (when (funcall predicate (funcall key x))
              (return-from find-if x)))))))

(defun find (item sequence
             &key (test #'eql) test-not (start 0) end from-end (key #'identity))
  "Return first occurrence of ITEM in SEQUENCE[START,END)."
  (find-if (if test-not
               (lambda (x) (not (funcall test-not item x)))
               (lambda (x) (funcall test item x)))
           sequence
           :start start :end end :from-end from-end :key key))

(defun find-if-not (predicate sequence
                    &key (start 0) end from-end (key #'identity))
  (find-if (lambda (x) (not (funcall predicate x))) sequence
           :start start :end end :from-end from-end :key key))

(defun position-if (predicate sequence
                    &key (start 0) end from-end (key #'identity))
  "Return index of first element in SEQUENCE[START,END) satisfying PREDICATE."
  (when (and (listp sequence) (not from-end))   ; O(n) cdr-walk (see FIND-IF)
    (return-from position-if
      (do ((tail (nthcdr start sequence) (cdr tail))
           (i start (1+ i)))
          ((or (null tail) (and end (>= i end))) nil)
        (when (funcall predicate (funcall key (car tail)))
          (return-from position-if i)))))
  (let* ((len  (length sequence))
         (stop (min (or end len) len)))
    (if from-end
        (do ((i (1- stop) (1- i)))
            ((< i start) nil)
          (when (funcall predicate (funcall key (elt sequence i)))
            (return-from position-if i)))
        (do ((i start (1+ i)))
            ((>= i stop) nil)
          (when (funcall predicate (funcall key (elt sequence i)))
            (return-from position-if i))))))

(defun position (item sequence
                 &key (test #'eql) test-not (start 0) end from-end (key #'identity))
  "Return index of first occurrence of ITEM in SEQUENCE[START,END)."
  (position-if (if test-not
                   (lambda (x) (not (funcall test-not item x)))
                   (lambda (x) (funcall test item x)))
               sequence
               :start start :end end :from-end from-end :key key))

(defun position-if-not (predicate sequence
                        &key (start 0) end from-end (key #'identity))
  (position-if (lambda (x) (not (funcall predicate x))) sequence
               :start start :end end :from-end from-end :key key))

;;; ---- REMOVE-IF / REMOVE with :count/:start/:end/:from-end ---------------

(defun remove-if (predicate sequence
                  &key (start 0) end from-end (count nil) (key #'identity))
  "Return SEQUENCE with elements (in [START,END)) satisfying PREDICATE removed.
:COUNT limits how many; :FROM-END chooses which when COUNT is given."
  (let* ((len  (length sequence))
         (stop (min (or end len) len))
         (mark (make-array len :initial-element nil))
         (n    0))
    ;; Mark the positions to delete, scanning in the :FROM-END direction so
    ;; :COUNT removes the rightmost (resp. leftmost) matches.
    (flet ((maybe (i)
             (when (and (or (null count) (< n count))
                        (funcall predicate (funcall key (elt sequence i))))
               (setf (aref mark i) t)
               (setq n (1+ n)))))
      (if from-end
          (do ((i (1- stop) (1- i))) ((< i start)) (maybe i))
          (do ((i start (1+ i)))     ((>= i stop)) (maybe i))))
    ;; Build the result (same sequence kind) from the unmarked elements.
    (let ((kept nil))
      (dotimes (i len) (unless (aref mark i) (push (elt sequence i) kept)))
      (setq kept (nreverse kept))
      (let ((klen (length kept)))
        (cond ((listp sequence) kept)
              ((stringp sequence)
               (let ((s (make-string klen)))
                 (dotimes (i klen s) (char-set s i (nth i kept)))))
              ((bit-vector-p sequence)
               (let ((v (make-bit-vector klen 0)))
                 (dotimes (i klen v) (bit-set v i (nth i kept)))))
              (t
               (let ((v (make-array klen)))
                 (dotimes (i klen v) (setf (aref v i) (nth i kept))))))))))

(defun remove (item sequence
               &key (test #'eql) test-not (start 0) end from-end count (key #'identity))
  (remove-if (if test-not
                 (lambda (x) (not (funcall test-not item x)))
                 (lambda (x) (funcall test item x)))
             sequence
             :start start :end end :from-end from-end :count count :key key))

(defun remove-if-not (predicate sequence
                      &key (start 0) end from-end count (key #'identity))
  (remove-if (lambda (x) (not (funcall predicate x))) sequence
             :start start :end end :from-end from-end :count count :key key))

;; DELETE family: CL permits DELETE/DELETE-IF/DELETE-IF-NOT to be non-destructive,
;; so reuse the generic REMOVE family (handles lists, strings, vectors, and — as
;; general vectors — bit-vectors).  This also gives them the full keyword set the
;; kernel DELETE (object+list only) and the early list-only DELETE-IF lacked.
(defun %delete-into-list (orig kept)
  "Destructively reshape ORIG's conses to hold KEPT's elements (KEPT is the
REMOVE result, so it is ORIG minus the deleted items, in order), and return it.
This makes DELETE mutate its list argument, as the destructive variant should."
  (cond ((null kept) nil)            ; everything deleted -> empty result
        ((null orig) kept)
        (t (do ((o orig (cdr o)) (k kept (cdr k)) (prev nil o))
               ((null k) (setf (cdr prev) nil) orig)
             (setf (car o) (car k))))))

(defun delete (item sequence &rest keys)
  (if (listp sequence)
      (%delete-into-list sequence (apply #'remove item sequence keys))
      (apply #'remove item sequence keys)))
(defun delete-if (predicate sequence &rest keys)
  (if (listp sequence)
      (%delete-into-list sequence (apply #'remove-if predicate sequence keys))
      (apply #'remove-if predicate sequence keys)))
(defun delete-if-not (predicate sequence &rest keys)
  (if (listp sequence)
      (%delete-into-list sequence (apply #'remove-if-not predicate sequence keys))
      (apply #'remove-if-not predicate sequence keys)))

(defun merge (result-type sequence1 sequence2 predicate &key (key #'identity))
  "Merge two sorted sequences into one sorted sequence of RESULT-TYPE (stable:
ties take from SEQUENCE1 first)."
  (let ((l1 (length sequence1)) (l2 (length sequence2))
        (acc nil) (i 0) (j 0))
    (do () ((and (>= i l1) (>= j l2)))
      (cond ((>= i l1) (push (elt sequence2 j) acc) (setq j (1+ j)))
            ((>= j l2) (push (elt sequence1 i) acc) (setq i (1+ i)))
            ((funcall predicate (funcall key (elt sequence2 j))
                                (funcall key (elt sequence1 i)))
             (push (elt sequence2 j) acc) (setq j (1+ j)))
            (t (push (elt sequence1 i) acc) (setq i (1+ i)))))
    (coerce (nreverse acc) result-type)))

;;; ---- STRING= / STRING< etc. with bounds ----------------------------------

(defun %string-compare-bounded (a b case-fold start1 end1 start2 end2)
  "Compare string subranges; returns <0, 0, or >0 (sign only).  Delegates to the
%STRING-CMP2 kernel primitive so STRING=/STRING-EQUAL etc. avoid the CHAR loop."
  (nth-value 1 (%string-cmp2 (string a) (string b) case-fold start1 end1 start2 end2)))

;;; Compare the substrings A[start1,end1) and B[start2,end2) (A and B are string
;;; designators; CI = case-insensitive).  Returns two values: the absolute index
;;; in A of the first mismatch (or the end of A's compared region when A is a
;;; prefix of B), and a relation -1 / 0 / 1.  CLHS string</string<=/... return
;;; that index (not just a boolean) when the relation holds, else NIL.
(defun %string-cmp (a b ci start1 end1 start2 end2)
  ;; The scan is done in C by %STRING-CMP2 (byte-wise; ASCII case fold matches
  ;; CHAR-UPCASE/DOWNCASE), returning exactly (values mismatch-index relation).
  (%string-cmp2 (string a) (string b) ci start1 end1 start2 end2))

(defun string= (a b &key (start1 0) end1 (start2 0) end2)
  (zerop (%string-compare-bounded a b nil start1 end1 start2 end2)))

(defun string/= (a b &key (start1 0) end1 (start2 0) end2)
  (multiple-value-bind (i rel) (%string-cmp a b nil start1 end1 start2 end2)
    (if (zerop rel) nil i)))

(defun string< (a b &key (start1 0) end1 (start2 0) end2)
  (multiple-value-bind (i rel) (%string-cmp a b nil start1 end1 start2 end2)
    (if (< rel 0) i nil)))

(defun string> (a b &key (start1 0) end1 (start2 0) end2)
  (multiple-value-bind (i rel) (%string-cmp a b nil start1 end1 start2 end2)
    (if (> rel 0) i nil)))

(defun string<= (a b &key (start1 0) end1 (start2 0) end2)
  (multiple-value-bind (i rel) (%string-cmp a b nil start1 end1 start2 end2)
    (if (<= rel 0) i nil)))

(defun string>= (a b &key (start1 0) end1 (start2 0) end2)
  (multiple-value-bind (i rel) (%string-cmp a b nil start1 end1 start2 end2)
    (if (>= rel 0) i nil)))

(defun string-equal (a b &key (start1 0) end1 (start2 0) end2)
  (zerop (%string-compare-bounded a b t start1 end1 start2 end2)))

(defun string-not-equal (a b &key (start1 0) end1 (start2 0) end2)
  (multiple-value-bind (i rel) (%string-cmp a b t start1 end1 start2 end2)
    (if (zerop rel) nil i)))

(defun string-lessp (a b &key (start1 0) end1 (start2 0) end2)
  (multiple-value-bind (i rel) (%string-cmp a b t start1 end1 start2 end2)
    (if (< rel 0) i nil)))

(defun string-greaterp (a b &key (start1 0) end1 (start2 0) end2)
  (multiple-value-bind (i rel) (%string-cmp a b t start1 end1 start2 end2)
    (if (> rel 0) i nil)))

(defun string-not-lessp (a b &key (start1 0) end1 (start2 0) end2)
  (multiple-value-bind (i rel) (%string-cmp a b t start1 end1 start2 end2)
    (if (>= rel 0) i nil)))

(defun string-not-greaterp (a b &key (start1 0) end1 (start2 0) end2)
  (multiple-value-bind (i rel) (%string-cmp a b t start1 end1 start2 end2)
    (if (<= rel 0) i nil)))

(defun simple-string-p (object)
  "A simple string: a string with no fill pointer (bocl has no adjustable
or displaced strings, so absence of a fill pointer is the only distinction)."
  (and (stringp object) (not (%fill-pointer-of object))))

;;; ---- WRITE-TO-STRING with :base ------------------------------------------

(defun write-to-string (object &rest keys &key &allow-other-keys)
  "Produce a string representation of OBJECT, honouring the print keywords
(:BASE/:RADIX/:CIRCLE/:LENGTH/:LEVEL/:ESCAPE/… via WRITE).  All numeric radix
formatting (including the #b/#o/#x/#NNr prefixes and the base-10 trailing dot for
*PRINT-RADIX*) is done by the kernel printer, so everything routes through WRITE,
which binds *PRINT-BASE*/*PRINT-RADIX* from the keywords around the print."
  (with-output-to-string (s)
    (apply #'write object :stream s keys)))

(defun princ-to-string (obj)
  "Produce a PRINC string representation of OBJ."
  (with-output-to-string (s) (princ obj s)))

;;; ---- SUBSTITUTE with bounds/count ----------------------------------------

(defun substitute (new old sequence
                   &key (test #'eql) test-not (start 0) end count from-end (key #'identity))
  "Return copy of SEQUENCE with occurrences of OLD replaced by NEW."
  (substitute-if new
                 (if test-not
                     (lambda (x) (not (funcall test-not old x)))
                     (lambda (x) (funcall test old x)))
                 sequence
                 :start start :end end :count count :from-end from-end :key key))

(defun substitute-if (new predicate sequence
                      &key (start 0) end count from-end (key #'identity))
  "Return copy of SEQUENCE with elements matching PREDICATE replaced by NEW."
  (let* ((len  (length sequence))
         (stop (min (or end len) len))
         (n    0))
    (if (listp sequence)
        (let ((result (copy-seq sequence)))
          (if from-end
              (do ((i (1- stop) (1- i)))
                  ((or (< i start) (and count (>= n count))))
                (when (funcall predicate (funcall key (nth i result)))
                  (setf (nth i result) new)
                  (setq n (1+ n))))
              (do ((i start (1+ i)))
                  ((or (>= i stop) (and count (>= n count))))
                (when (funcall predicate (funcall key (nth i result)))
                  (setf (nth i result) new)
                  (setq n (1+ n)))))
          result)
        ;; string/vector path
        (let ((result (copy-seq sequence)))
          (if from-end
              (do ((i (1- stop) (1- i)))
                  ((or (< i start) (and count (>= n count))))
                (when (funcall predicate (funcall key (elt result i)))
                  (setf (elt result i) new)
                  (setq n (1+ n))))
              (do ((i start (1+ i)))
                  ((or (>= i stop) (and count (>= n count))))
                (when (funcall predicate (funcall key (elt result i)))
                  (setf (elt result i) new)
                  (setq n (1+ n)))))
          result))))

(defun substitute-if-not (new predicate sequence &rest keys)
  (apply #'substitute-if new (lambda (x) (not (funcall predicate x))) sequence keys))

(defun nsubstitute-if (new predicate sequence
                       &key (start 0) end count from-end (key #'identity))
  "Destructive SUBSTITUTE-IF: replace matching elements of SEQUENCE in place."
  (let* ((len  (length sequence))
         (stop (min (or end len) len))
         (n    0))
    (if from-end
        (do ((i (1- stop) (1- i)))
            ((or (< i start) (and count (>= n count))))
          (when (funcall predicate (funcall key (elt sequence i)))
            (setf (elt sequence i) new) (setq n (1+ n))))
        (do ((i start (1+ i)))
            ((or (>= i stop) (and count (>= n count))))
          (when (funcall predicate (funcall key (elt sequence i)))
            (setf (elt sequence i) new) (setq n (1+ n)))))
    sequence))

(defun nsubstitute (new old sequence
                    &key (test #'eql) test-not (start 0) end count from-end (key #'identity))
  "Destructive SUBSTITUTE."
  (nsubstitute-if new
                  (if test-not
                      (lambda (x) (not (funcall test-not old x)))
                      (lambda (x) (funcall test old x)))
                  sequence
                  :start start :end end :count count :from-end from-end :key key))

(defun nsubstitute-if-not (new predicate sequence &rest keys)
  (apply #'nsubstitute-if new (lambda (x) (not (funcall predicate x))) sequence keys))

;;; ---- MEMBER with :key ----------------------------------------------------

(defun member-if (predicate list &key (key #'identity))
  "Return the tail of LIST starting with first element satisfying PREDICATE."
  (do ((l list (cdr l)))
      ((null l) nil)
    (when (funcall predicate (funcall key (car l)))
      (return l))))

(defun member-if-not (predicate list &key (key #'identity))
  (member-if (lambda (x) (not (funcall predicate x))) list :key key))

;;; ---- MISMATCH with full kwargs -------------------------------------------

(defun mismatch (sequence1 sequence2
                 &key (test #'eql) test-not (key #'identity)
                      (start1 0) end1
                      (start2 0) end2
                      from-end)
  "Return the index in SEQUENCE1 of the first (or, FROM-END, one past the last)
position at which the two subranges differ, or NIL if they match."
  ;; Fast path: two strings compared front-to-back with the default character
  ;; test and no KEY — the whole scan runs in C via %STRING-CMP2, which returns
  ;; exactly (values mismatch-index relation) for these subranges.
  (when (and (not from-end) (null test-not) (eq key #'identity)
             (stringp sequence1) (stringp sequence2)
             (or (eq test #'eql) (eq test #'char=) (eq test #'char-equal)))
    (return-from mismatch
      (multiple-value-bind (idx rel)
          (%string-cmp2 sequence1 sequence2 (eq test #'char-equal)
                        start1 end1 start2 end2)
        (if (zerop rel) nil idx))))
  (let* ((test  (if test-not (lambda (a b) (not (funcall test-not a b))) test))
         (len1  (length sequence1))
         (len2  (length sequence2))
         (stop1 (min (or end1 len1) len1))
         (stop2 (min (or end2 len2) len2))
         ;; Clamp subrange lengths to >= 0: an empty/over-bounded range (start
         ;; >= stop) must not yield a negative N and out-of-bounds ELT access.
         (sub1  (max 0 (- stop1 start1)))
         (sub2  (max 0 (- stop2 start2)))
         (n     (min sub1 sub2)))
    (if from-end
        (do ((i 1 (1+ i)))
            ((> i n)
             ;; matched the common tail; if lengths differ the mismatch is at
             ;; the boundary — one past SEQUENCE1's extra prefix.
             (cond ((= sub1 sub2) nil)
                   ((> sub1 sub2) (+ start1 (- sub1 sub2)))
                   (t             start1)))
          (unless (funcall test
                           (funcall key (elt sequence1 (- stop1 i)))
                           (funcall key (elt sequence2 (- stop2 i))))
            (return (1+ (- stop1 i)))))
        (do ((i 0 (1+ i)))
            ((= i n)
             (if (= sub1 sub2) nil
                 (+ start1 i)))
          (unless (funcall test
                           (funcall key (elt sequence1 (+ start1 i)))
                           (funcall key (elt sequence2 (+ start2 i))))
            (return (+ start1 i)))))))

;;; ---- SEARCH with full kwargs (update for CL compliance) -----------------

(defun search (pattern target
               &key (test #'eql) test-not (key #'identity)
                    (start1 0) end1
                    (start2 0) end2
                    from-end)
  "Return the start index in TARGET of a subsequence matching PATTERN, or NIL.
FROM-END returns the rightmost such index."
  ;; Fast path: search a string for a whole string pattern with the default
  ;; character test and no KEY — the scan runs in C via %STRING-SEARCH.
  (when (and (not from-end) (null test-not) (eq key #'identity)
             (stringp pattern) (stringp target)
             (eql start1 0) (or (null end1) (eql end1 (length pattern)))
             (or (eq test #'eql) (eq test #'char=) (eq test #'char-equal)))
    (return-from search
      (%string-search pattern target (eq test #'char-equal) start2 end2)))
  (let* ((plen  (length pattern))
         (tlen  (length target))
         (pe    (min (or end1 plen) plen))
         (te    (min (or end2 tlen) tlen))
         (pn    (- pe start1))
         (tn    (- te start2))
         (limit (- tn pn)))
    (when (< limit 0) (return-from search nil))
    (flet ((match-at (i)
             (null (mismatch pattern target
                             :start1 start1 :end1 pe
                             :start2 i :end2 (+ i pn)
                             :test test :test-not test-not :key key))))
      (if from-end
          (do ((i (+ start2 limit) (1- i)))
              ((< i start2) nil)
            (when (match-at i) (return i)))
          (do ((i start2 (1+ i)))
              ((> i (+ start2 limit)) nil)
            (when (match-at i) (return i)))))))

;;;; =========================================================================
;;;; Slice 28 — multi-sequence predicates, hash iteration, string utilities,
;;;;   list utilities, WHEN-LET/IF-LET, IOTA, FLATTEN, TAKE/DROP.
;;;; =========================================================================

;;; ---- EVERY/SOME/NOTEVERY/NOTANY for multiple sequences ------------------

(defun every (predicate &rest sequences)
  "Return T if PREDICATE returns true for every set of corresponding elements.
  With one sequence: standard single-sequence behaviour.
  With multiple sequences: stops at the shortest."
  (if (null (cdr sequences))
      ;; Single-sequence fast path
      (let ((seq (car sequences)))
        (if (listp seq)
            (dolist (x seq t)
              (unless (funcall predicate x) (return nil)))
            (dotimes (i (length seq) t)
              (unless (funcall predicate (elt seq i)) (return-from every nil)))))
      ;; Multi-sequence path
      (let ((len (apply #'min (mapcar #'length sequences))))
        (dotimes (i len t)
          (unless (apply predicate (mapcar (lambda (s) (elt s i)) sequences))
            (return-from every nil))))))

(defun some (predicate &rest sequences)
  "Return the first true result of PREDICATE over sequence elements, or NIL."
  (if (null (cdr sequences))
      (let ((seq (car sequences)))
        (if (listp seq)
            (dolist (x seq nil)
              (let ((v (funcall predicate x))) (when v (return v))))
            (dotimes (i (length seq) nil)
              (let ((v (funcall predicate (elt seq i))))
                (when v (return-from some v))))))
      (let ((len (apply #'min (mapcar #'length sequences))))
        (dotimes (i len nil)
          (let ((v (apply predicate (mapcar (lambda (s) (elt s i)) sequences))))
            (when v (return-from some v)))))))

(defun notevery (predicate &rest sequences)
  "Return T if PREDICATE is false for at least one element."
  (not (apply #'every predicate sequences)))

(defun notany (predicate &rest sequences)
  "Return T if PREDICATE is false for all elements."
  (not (apply #'some predicate sequences)))

;;; ---- DO-HASH-TABLE -------------------------------------------------------

(defmacro do-hash-table (var-spec &body body)
  "Iterate over a hash table, binding key and value for each entry.
  Syntax: (do-hash-table (key value table &optional result) body...)
  RESULT is evaluated and returned after iteration."
  (let ((key    (first  var-spec))
        (value  (second var-spec))
        (table  (third  var-spec))
        (result (fourth var-spec)))
    `(progn
       (maphash (lambda (,key ,value) ,@body) ,table)
       ,result)))

;;; ---- IOTA ----------------------------------------------------------------

(defun iota (count &optional (start 0) (step 1))
  "Return a list of COUNT numbers starting at START with increment STEP.
  Example: (iota 5) → (0 1 2 3 4), (iota 3 1) → (1 2 3)."
  (let ((acc nil))
    (dotimes (i count (nreverse acc))
      (push (+ start (* i step)) acc))))

;;; ---- FLATTEN -------------------------------------------------------------

(defun flatten (tree)
  "Return a flat list of all atoms in TREE (ignoring NIL from list structure)."
  (cond ((null tree)   nil)
        ((atom tree)   (list tree))
        (t (nconc (flatten (car tree)) (flatten (cdr tree))))))

;;; ---- ENSURE-LIST ---------------------------------------------------------

(defun ensure-list (x)
  "If X is a list, return it; otherwise return (LIST X)."
  (if (listp x) x (list x)))

;;; ---- TAKE / DROP ---------------------------------------------------------

(defun take (n sequence)
  "Return the first N elements of SEQUENCE."
  (subseq sequence 0 (min n (length sequence))))

(defun drop (n sequence)
  "Return SEQUENCE with the first N elements removed."
  (subseq sequence (min n (length sequence))))

;;; ---- WHEN-LET / IF-LET ---------------------------------------------------

(defmacro when-let (bindings &body body)
  "Like WHEN but establishes variable bindings. Evaluates BODY only if all
  values are non-NIL.
  Example: (when-let ((x (find-it))) (use x))"
  (let* ((pairs (if (and (consp bindings) (symbolp (car bindings)))
                    (list bindings)         ; single (var form) without extra parens
                    bindings))
         (vars  (mapcar #'car  pairs))
         (forms (mapcar #'cadr pairs))
         (tmp   (mapcar (lambda (v) (gensym (symbol-name v))) vars)))
    `(let ,(mapcar #'list tmp forms)
       (when (and ,@tmp)
         (let ,(mapcar #'list vars tmp)
           ,@body)))))

(defmacro if-let (bindings then &optional else)
  "Like IF but with variable binding.  Binds VAR to VALUE; if non-NIL,
  evaluates THEN with VAR bound; otherwise evaluates ELSE."
  (let* ((pairs (if (and (consp bindings) (symbolp (car bindings)))
                    (list bindings)
                    bindings))
         (vars  (mapcar #'car  pairs))
         (forms (mapcar #'cadr pairs))
         (tmp   (mapcar (lambda (v) (gensym (symbol-name v))) vars)))
    `(let ,(mapcar #'list tmp forms)
       (if (and ,@tmp)
           (let ,(mapcar #'list vars tmp) ,then)
           ,else))))

;;; ---- STRING-JOIN / SPLIT-SEQUENCE ----------------------------------------

(defun string-join (sequences &optional (separator ""))
  "Concatenate strings in SEQUENCES with SEPARATOR between them."
  (if (null sequences)
      ""
      (let ((sep (string separator))
            (strs (mapcar #'string sequences)))
        (reduce (lambda (a b)
                  (concatenate 'string a sep b))
                strs))))

(defun split-sequence (delimiter sequence &key (test #'eql) (remove-empty-subseqs nil) count)
  "Split SEQUENCE on elements matching DELIMITER; return list of subsequences."
  (let ((result nil)
        (start  0)
        (len    (length sequence))
        (n      0))
    (dotimes (i len)
      (when (funcall test delimiter (elt sequence i))
        (let ((sub (subseq sequence start i)))
          (when (or (not remove-empty-subseqs) (> (length sub) 0))
            (push sub result)
            (setq n (1+ n))))
        (setq start (1+ i))
        (when (and count (>= n (1- count))) (return))))
    ;; Add the remaining tail
    (let ((tail (subseq sequence start)))
      (when (or (not remove-empty-subseqs) (> (length tail) 0))
        (push tail result)))
    (nreverse result)))

;;; ---- ALEXANDRIA-style extras ---------------------------------------------

(defun last-elt (sequence)
  "Return the last element of SEQUENCE."
  (elt sequence (1- (length sequence))))

(defun emptyp (sequence)
  "Return T if SEQUENCE has zero elements."
  (= (length sequence) 0))

(defun starts-with (prefix sequence &key (test #'eql))
  "Return T if SEQUENCE starts with PREFIX."
  (let ((plen (length prefix))
        (slen (length sequence)))
    (and (<= plen slen)
         (null (mismatch prefix sequence :end1 plen :end2 plen :test test)))))

(defun ends-with (suffix sequence &key (test #'eql))
  "Return T if SEQUENCE ends with SUFFIX."
  (let ((suflen (length suffix))
        (slen   (length sequence)))
    (and (<= suflen slen)
         (null (mismatch suffix sequence
                         :start2 (- slen suflen) :test test)))))

;;;; =========================================================================
;;;; Slice 29 — &AUX in lambda lists (C), PRINT-UNREADABLE-OBJECT,
;;;;   COUNT/REDUCE with :start/:end, LOOP BEING hash iteration,
;;;;   LOGTEST, and miscellaneous improvements.
;;;; =========================================================================

;;; ---- PRINT-UNREADABLE-OBJECT ---------------------------------------------

(defmacro print-unreadable-object (args &body body)
  "Print an unreadable object representation to STREAM.
  Syntax: (print-unreadable-object (object stream &key type identity) body...)
  Emits #<TYPE BODY [address]> format."
  (let* ((object   (first  args))
         (stream   (second args))
         (keys     (cddr   args))
         (typep-v  (getf keys :type nil))
         (identp-v (getf keys :identity nil))
         (res      (gensym "OBJ-")))
    `(let ((,res ,object))
       (write-char #\# ,stream)
       (write-char #\< ,stream)
       ,@(when typep-v
           `((write-string (symbol-name (class-name (class-of ,res))) ,stream)
             (write-char #\Space ,stream)))
       ,@body
       ,@(when identp-v
           `((write-char #\Space ,stream)
             (write-string (number-to-string (sxhash ,res)) ,stream)))
       (write-char #\> ,stream)
       nil)))

;;; ---- SXHASH stub ---------------------------------------------------------
;;; Returns a hash code for the object. Used by PRINT-UNREADABLE-OBJECT.

(defun sxhash (object)
  "Return a non-negative fixnum hash code for OBJECT.
  This implementation is not stable across sessions."
  (if (integerp object) (logand (abs object) most-positive-fixnum)
      0))

;;; ---- COUNT-IF / COUNT / COUNT-IF-NOT with :start/:end -------------------

(defun count-if (predicate sequence
                 &key (start 0) end from-end (key #'identity))
  "Count elements in SEQUENCE[START,END) satisfying PREDICATE.  FROM-END does not
change the count, but PREDICATE is called in that order (it may have effects)."
  (let* ((len  (length sequence))
         (stop (min (or end len) len))
         (n    0))
    (if from-end
        (do ((i (1- stop) (1- i))) ((< i start) n)
          (when (funcall predicate (funcall key (elt sequence i))) (setq n (1+ n))))
        (do ((i start (1+ i))) ((>= i stop) n)
          (when (funcall predicate (funcall key (elt sequence i))) (setq n (1+ n)))))))

(defun count (item sequence
              &key (test #'eql) test-not (start 0) end from-end (key #'identity))
  "Count occurrences of ITEM in SEQUENCE[START,END)."
  (count-if (if test-not
                (lambda (x) (not (funcall test-not item x)))
                (lambda (x) (funcall test item x)))
            sequence
            :start start :end end :from-end from-end :key key))

(defun count-if-not (predicate sequence
                     &key (start 0) end from-end (key #'identity))
  (count-if (lambda (x) (not (funcall predicate x))) sequence
            :start start :end end :from-end from-end :key key))

;;; ---- REDUCE with :start/:end --------------------------------------------

(defun reduce (function sequence
               &key (start 0) end from-end (initial-value nil iv-supplied)
                    (key #'identity))
  "Reduce SEQUENCE[START,END) with FUNCTION."
  (let* ((len  (length sequence))
         (stop (min (or end len) len))
         (elts (let ((acc nil))
                 (do ((i start (1+ i)))
                     ((>= i stop) (nreverse acc))
                   (push (funcall key (elt sequence i)) acc)))))
    (when from-end (setq elts (reverse elts)))
    (cond
      ((null elts)
       (if iv-supplied initial-value (funcall function)))
      (iv-supplied
       (let ((acc initial-value))
         (dolist (x elts acc)
           (setq acc (if from-end (funcall function x acc)
                                  (funcall function acc x))))))
      (t
       (let ((acc (car elts)))
         (dolist (x (cdr elts) acc)
           (setq acc (if from-end (funcall function x acc)
                                  (funcall function acc x)))))))))

;;; ---- LOGTEST (bitwise) ---------------------------------------------------
;;; kernel_LOGTEST is registered in arithmetic.c; just add a Lisp
;;; alias comment here for documentation.

;;; ---- LOOP BEING THE HASH-KEYS/HASH-VALUES --------------------------------
;;; Extend the LOOP macro's FOR clause with BEING THE HASH-KEYS/HASH-VALUES.
;;; We do this by replacing the `(t (error ...))` fallback in the FOR clause
;;; with a BEING handler.  Since the full LOOP macro is above, we patch it
;;; by re-defining the helper that LOOP uses for the FOR sub-clause.
;;;
;;; Simpler approach: the LOOP macro can't be easily patched at this level.
;;; Instead, provide a FOR-EACH-HASH-KEY pattern as a stand-alone macro.

(defmacro loop-for-hash (var-spec &body body)
  "Helper: iterate BODY over a hash table, binding key and value.
  Syntax: (loop-for-hash (key-var value-var hash-table) body...)
  Use as an escape hatch when LOOP BEING is not available."
  (let ((key   (first  var-spec))
        (value (second var-spec))
        (table (third  var-spec)))
    `(maphash (lambda (,key ,value) ,@body) ,table)))

;;; ---- MAKE-HASH-TABLE improvements ----------------------------------------

(defun hash-table-count (ht)
  "Return the number of entries in HT."
  (let ((n 0))
    (maphash (lambda (k v) (declare (ignore k v)) (setq n (1+ n))) ht)
    n))

(defun clrhash (ht)
  "Remove all entries from HT. Returns HT."
  ;; Collect keys first to avoid modifying table while iterating.
  (let ((keys nil))
    (maphash (lambda (k v) (declare (ignore v)) (push k keys)) ht)
    (dolist (k keys ht)
      (remhash k ht))))

;;; ---- STRING-CONCAT -------------------------------------------------------

(defun string-concat (&rest strings)
  "Concatenate STRING arguments."
  (apply #'concatenate 'string strings))

;;; ---- CHAR-CODE-LIMIT -----------------------------------------------------

(defconstant char-code-limit 256
  "Upper exclusive bound on character codes in bocl.")

;;; ---- TYPE-OF for more types ----------------------------------------------

;;; bocl's kernel TYPE-OF handles most types; add coverage for a few more.

(defun subtypep (type1 type2 &optional environment)
  "Return T if TYPE1 is definitely a subtype of TYPE2.
  Simplified: handles common cases."
  (declare (ignore environment))
  (cond
    ((eql type1 type2)           (values t t))
    ((eql type2 t)               (values t t))
    ((eql type1 nil)             (values t t))
    ;; Number hierarchy
    ((and (eql type1 'integer)   (eql type2 'rational))  (values t t))
    ((and (eql type1 'rational)  (eql type2 'real))      (values t t))
    ((and (eql type1 'real)      (eql type2 'number))    (values t t))
    ((and (eql type1 'float)     (eql type2 'real))      (values t t))
    ((and (eql type1 'float)     (eql type2 'number))    (values t t))
    ((and (eql type1 'integer)   (eql type2 'number))    (values t t))
    ;; FIXNUM / BIGNUM are subtypes of INTEGER (and so of the whole tower).
    ((and (member type1 '(fixnum bignum)) (eql type2 'integer))  (values t t))
    ((and (member type1 '(fixnum bignum)) (eql type2 'rational)) (values t t))
    ((and (member type1 '(fixnum bignum)) (eql type2 'real))     (values t t))
    ((and (member type1 '(fixnum bignum)) (eql type2 'number))   (values t t))
    ;; Sequence hierarchy
    ((and (eql type1 'string)    (eql type2 'vector))    (values t t))
    ((and (eql type1 'string)    (eql type2 'sequence))  (values t t))
    ((and (eql type1 'vector)    (eql type2 'sequence))  (values t t))
    ((and (eql type1 'list)      (eql type2 'sequence))  (values t t))
    ((and (eql type1 'cons)      (eql type2 'list))      (values t t))
    ((and (eql type1 'null)      (eql type2 'list))      (values t t))
    ((and (eql type1 'null)      (eql type2 'symbol))    (values t t))
    ;; CLOS class subtyping: works for class OBJECTS and class-name symbols.
    ;; TYPE2 is a supertype of TYPE1 when its %cls is in TYPE1's class precedence
    ;; list (covers conditions defined via DEFCLASS / DEFINE-CONDITION, and UIOP
    ;; COERCE-CLASS, which calls (subtypep <class-object> <class-object>)).
    ((%as-clos-class type1)
     (let ((c1 (%as-clos-class type1))
           (c2 (%as-clos-class type2)))
       (cond (c2 (values (not (null (member c2 (%cls-cpl c1)))) t))
             ;; Every CLOS class is a subtype of these metaclass / root types.
             ((member type2 '(standard-object t class standard-class)) (values t t))
             ((null type2) (values nil t))
             (t (values nil nil)))))
    (t (values nil nil))))

;;;; =========================================================================
;;;; Slice 30 — TYPEP compound types, FORMAT improvements, misc utilities.
;;;; =========================================================================

;;; ---- TYPEP with compound type specifiers ---------------------------------
;;; Overrides the Slice 19 definition with full compound-type support.

(defun %num-lower-ok (object bound)
  "Check OBJECT against the LOW bound of a numeric type-spec (CLHS 4.2.3).
   BOUND is NIL or * (unbounded), a number N (inclusive: object >= N), or a
   one-element list (N) (exclusive: object > N)."
  (cond ((or (null bound) (eq bound '*)) t)
        ((consp bound) (> object (car bound)))
        (t (>= object bound))))

(defun %num-upper-ok (object bound)
  "Check OBJECT against the HIGH bound of a numeric type-spec: NIL/* unbounded,
   N inclusive (object <= N), (N) exclusive (object < N)."
  (cond ((or (null bound) (eq bound '*)) t)
        ((consp bound) (< object (car bound)))
        (t (<= object bound))))

(defun typep (object type &optional environment)
  "Extended TYPEP with compound type specifiers."
  (declare (ignore environment))
  (cond
    ((eq type t) t)
    ((eq type nil) nil)
    ;; A CLOS class object (from DEFCLASS / FIND-CLASS) is itself an instance of
    ;; STANDARD-CLASS / CLASS / STANDARD-OBJECT / METAOBJECT (the metaclass
    ;; hierarchy) — needed by UIOP COERCE-CLASS's ETYPECASE over class objects.
    ((and (%is-cls object)
          (member type '(standard-class class standard-object metaobject specializer)))
     t)
    ;; FIXNUM / BIGNUM — both are kernel INTEGERs; a bignum is the boxed kind.
    ;; Guarded on %bignump so this is correct even when bignum support is absent
    ;; (then every integer is a fixnum and there are no bignums).
    ((eq type 'fixnum) (and (typep object 'integer)
                            (not (and (fboundp '%bignump) (%bignump object)))))
    ((eq type 'bignum) (and (fboundp '%bignump) (%bignump object)))
    ;; Atomic type names the kernel TYPEP does not cover.  Each is the standard
    ;; CL type (CLHS 4.3, 12.1.1) expressed via the matching predicate.
    ((eq type 'function)      (functionp object))
    ((eq type 'boolean)       (or (eq object t) (eq object nil)))
    ((eq type 'keyword)       (keywordp object))
    ((eq type 'atom)          (not (consp object)))
    ((eq type 'bit)           (or (eql object 0) (eql object 1)))
    ((eq type 'signed-byte)   (integerp object))
    ((eq type 'unsigned-byte) (and (integerp object) (>= object 0)))
    ((eq type 'simple-string) (simple-string-p object))
    ((eq type 'simple-vector) (simple-vector-p object))
    ((eq type 'simple-array)  (or (vectorp object) (and (fboundp '%md-array-p)
                                                        (%md-array-p object))))
    ((eq type 'base-char)     (characterp object))
    ((eq type 'standard-char) (and (characterp object) (standard-char-p object)))
    ;; Compound type specifiers
    ((consp type)
     (let ((head (car type))
           (args (cdr type)))
       (cond
         ;; (or t1 t2 ...) — true if any ti matches
         ((eq head 'or)
          (some (lambda (ti) (typep object ti)) args))
         ;; (and t1 t2 ...) — true if all ti match
         ((eq head 'and)
          (every (lambda (ti) (typep object ti)) args))
         ;; (not ti) — true if ti does NOT match
         ((eq head 'not)
          (not (typep object (car args))))
         ;; (member x1 x2 ...) — true if object is EQL to any xi
         ((eq head 'member)
          (not (null (member object args :test #'eql))))
         ;; (eql x) — true if object is EQL to x
         ((eq head 'eql)
          (eql object (car args)))
         ;; (satisfies predicate) — call predicate on object
         ;; Resolve symbol to its function binding if needed.
         ((eq head 'satisfies)
          (let ((pred (car args)))
            (not (null (funcall (if (symbolp pred) (symbol-function pred) pred)
                                object)))))
         ;; (integer low high) — integer in range.  LOW/HIGH may be NIL or *
         ;; (unbounded), N (inclusive), or (N) (exclusive); see CLHS 4.2.3.
         ((eq head 'integer)
          (and (integerp object)
               (%num-lower-ok object (car args))
               (%num-upper-ok object (cadr args))))
         ;; (rational low high) — rational in range
         ((eq head 'rational)
          (and (rationalp object)
               (%num-lower-ok object (car args))
               (%num-upper-ok object (cadr args))))
         ;; (float low high) — float in range
         ((eq head 'float)
          (and (floatp object)
               (%num-lower-ok object (car args))
               (%num-upper-ok object (cadr args))))
         ;; (real low high) — any real in range
         ((eq head 'real)
          (and (or (integerp object) (floatp object))
               (%num-lower-ok object (car args))
               (%num-upper-ok object (cadr args))))
         ;; (unsigned-byte &optional s) — non-negative integer; with size S an
         ;; integer in [0, 2^S - 1].  (mod n) — integer in [0, n-1].
         ((eq head 'unsigned-byte)
          (and (integerp object) (>= object 0)
               (let ((s (car args)))
                 (or (null s) (eq s '*) (< object (ash 1 s))))))
         ((eq head 'signed-byte)
          (and (integerp object)
               (let ((s (car args)))
                 (if (or (null s) (eq s '*))
                     t
                     (let ((lim (ash 1 (1- s))))
                       (and (>= object (- lim)) (< object lim)))))))
         ((eq head 'mod)
          (and (integerp object) (>= object 0) (< object (car args))))
         ;; (string length) — string of given length (* / NIL = any length)
         ((eq head 'string)
          (and (stringp object)
               (or (null (car args)) (eq (car args) '*)
                   (= (string-length object) (car args)))))
         ;; (vector elt-type length) — vector (* / NIL length = any)
         ((eq head 'vector)
          (and (vectorp object)
               (or (null (cadr args)) (eq (cadr args) '*)
                   (= (vector-length object) (cadr args)))))
         ;; (array ...) — array
         ((eq head 'array)
          (or (stringp object) (vectorp object)))
         ;; (values ...) — ignore for now
         ((eq head 'values) t)
         ;; Unknown compound — try symbol lookup of head
         (t (typep object head)))))
    ;; Direct %cls object (CLOS class)
    ((%is-cls type)
     (and (%is-obj object)
          (not (null (member type (%cls-cpl (%obj-class object)))))))
    ;; Symbol — a CLOS class (for %obj instances) and/or a kernel class (for
    ;; kernel objects); some names (the CONDITION hierarchy) are BOTH, so check
    ;; the %cls CPL for CLOS instances and fall back to the kernel hierarchy.
    ((symbolp type)
     (let ((cls (gethash type *%classes*)))
       (if (%is-cls cls)
           (or (and (%is-obj object)
                    (not (null (member cls (%cls-cpl (%obj-class object))))))
               (funcall *%kernel-typep* object type))
           (funcall *%kernel-typep* object type))))
    ;; Non-symbol non-CLOS type (e.g. a structure-class or kernel class object)
    ;; — delegate directly to the kernel typep which handles the class hierarchy.
    (t (funcall *%kernel-typep* object type))))

;;; ---- NUMERIC-TYPE-SPEC helpers -------------------------------------------

(defun integer-type-p (type)
  "Return T if TYPE names or describes an integer."
  (or (eq type 'integer) (eq type 'fixnum) (eq type 'bignum)
      (and (consp type) (eq (car type) 'integer))))

;;; ---- BIT-VECTOR ops ------------------------------------------------------
;;; The 11 CL bit-array logical operations.  Implemented over BIT / BIT-SET
;;; (which work on real BIT_VECTOR objects), element by element.
;;;
;;; Each op takes an optional RESULT-BIT-ARRAY: NIL (the default) makes a fresh
;;; bit-vector; T means update the first argument in place; an explicit
;;; bit-vector stores the result there.  Reads are always taken before the
;;; same-index write, so destructive use (result EQ an input) is safe.

;; The bit-* ops accept bit-vectors AND multi-dimensional bit arrays.  Both are
;; addressed through their flat row-major storage via ROW-MAJOR-AREF (which knows
;; how to reach the bits of either), %BIT-SIZE gives the element count, and a NIL
;; result-arg makes a fresh bit array of the SAME shape as the first operand.
(defun %bit-size (a)
  (if (%md-array-p a) (array-total-size a) (length a)))

(defun %bit-result-vector (v1 result-bit-array)
  (cond ((null result-bit-array)
         (if (%md-array-p v1)
             (make-array (array-dimensions v1) :element-type 'bit)
             (make-bit-vector (length v1) 0)))
        ((eq result-bit-array t) v1)
        (t result-bit-array)))

(defun %bit-binop (fn v1 v2 result-bit-array)
  "Apply binary bit function FN element-wise to V1 and V2 into the resolved
result array; return that array."
  (let ((out (%bit-result-vector v1 result-bit-array))
        (n   (%bit-size v1)))
    (dotimes (i n out)
      (setf (row-major-aref out i)
            (logand 1 (funcall fn (row-major-aref v1 i) (row-major-aref v2 i)))))))

(defun bit-and   (v1 v2 &optional r) (%bit-binop #'logand v1 v2 r))
(defun bit-ior   (v1 v2 &optional r) (%bit-binop #'logior v1 v2 r))
(defun bit-or    (v1 v2 &optional r) (bit-ior v1 v2 r))   ; non-standard alias for BIT-IOR
(defun bit-xor   (v1 v2 &optional r) (%bit-binop #'logxor v1 v2 r))
(defun bit-eqv   (v1 v2 &optional r) (%bit-binop (lambda (a b) (logxor (logxor a b) 1)) v1 v2 r))
(defun bit-nand  (v1 v2 &optional r) (%bit-binop (lambda (a b) (logxor (logand a b) 1)) v1 v2 r))
(defun bit-nor   (v1 v2 &optional r) (%bit-binop (lambda (a b) (logxor (logior a b) 1)) v1 v2 r))
(defun bit-andc1 (v1 v2 &optional r) (%bit-binop (lambda (a b) (logand (logxor a 1) b)) v1 v2 r))
(defun bit-andc2 (v1 v2 &optional r) (%bit-binop (lambda (a b) (logand a (logxor b 1))) v1 v2 r))
(defun bit-orc1  (v1 v2 &optional r) (%bit-binop (lambda (a b) (logior (logxor a 1) b)) v1 v2 r))
(defun bit-orc2  (v1 v2 &optional r) (%bit-binop (lambda (a b) (logior a (logxor b 1))) v1 v2 r))

(defun bit-not (v1 &optional result-bit-array)
  "Return a bit array with all bits of V1 flipped (see RESULT-BIT-ARRAY above)."
  (let ((out (%bit-result-vector v1 result-bit-array))
        (n   (%bit-size v1)))
    (dotimes (i n out)
      (setf (row-major-aref out i) (logxor (row-major-aref v1 i) 1)))))

;;; ---- REVERSE for strings and vectors (kernel handles lists) ---------------

(defun reverse-seq (sequence)
  "Return a reversed copy of SEQUENCE (for non-list types)."
  (if (listp sequence)
      (reverse sequence)   ; kernel REVERSE handles lists
      (let* ((len (length sequence))
             (out (copy-seq sequence)))
        (dotimes (i (floor len 2) out)
          (let ((j  (- len 1 i))
                (tmp (elt out i)))
            (setf (elt out i) (elt out j))
            (setf (elt out j) tmp))))))

;;; ---- NUMBER TYPE predicates -----------------------------------------------

(defun fixnump  (n) (and (integerp n)
                         (<= most-negative-fixnum n most-positive-fixnum)))
(defun bignump  (n) (and (integerp n) (not (fixnump n))))
(defun bit-p    (n) (or (eql n 0) (eql n 1)))

;;; make-bit-vector is provided by the kernel (registered in sequences.c).

;;;; =========================================================================
;;;; Slice 31 — APPLY/FUNCALL symbol fix (C), COERCE 'character (C),
;;;;   MAKE-SEQUENCE, LOOP BEING hash, string utilities, number coercions.
;;;; =========================================================================

;;; ---- MAKE-SEQUENCE -------------------------------------------------------

(defun make-sequence (result-type length &key (initial-element nil))
  "Return a sequence of type RESULT-TYPE and LENGTH.
  :INITIAL-ELEMENT fills every position."
  (case (%seq-type-kind result-type)
    (:list       (make-list length :initial-element initial-element))
    (:string     (make-string length :initial-element (or initial-element #\Space)))
    (:bit-vector (make-bit-vector length (if (integerp initial-element) initial-element 0)))
    (:vector     (make-array length :initial-element initial-element))
    (t           (error 'type-error :datum result-type :expected-type 'sequence))))

;;; ---- COERCE to sequence types --------------------------------------------
;;; Kernel handles 'character, 'float, 'integer; prelude handles sequences.
;;; This extension handles (coerce list 'string) and similar.

;;; The Lisp-level COERCE already handles these; just add 'character:
(defun %coerce-to-character (object)
  "Convert OBJECT to a character."
  (cond ((characterp object) object)
        ((integerp object)   (code-char object))
        ((stringp object)    (if (= (string-length object) 1)
                                 (char object 0)
                                 (error "COERCE: string ~S is not length 1" object)))
        (t (error "COERCE: cannot convert ~S to CHARACTER" object))))

;;; ---- REVERSE for strings/vectors -----------------------------------------
;;; The kernel handles lists; we add string and vector.

(defun reverse (sequence)
  "Return a reversed copy of SEQUENCE."
  (if (listp sequence)
      (let ((result nil))
        (dolist (x sequence result)
          (push x result)))
      (reverse-seq sequence)))   ; Slice 30 helper for non-lists

;;; ---- CONCATENATE for 'string result from char lists ----------------------
;;; Allow (concatenate 'string '(#\a #\b)) — list of chars to string.

;;; ---- LOOP BEING THE HASH-KEYS/HASH-VALUES OF hash ----------------------
;;; Full LOOP BEING rewrite deferred; use loop-for-hash as the escape hatch.
;;; (loop-for-hash (k v ht) body...) iterates over hash table ht.

;;; ---- NUMBER conversion utilities ----------------------------------------

(defun integer-to-char (n)
  "Return the character with char-code N."
  (code-char n))

(defun char-to-integer (c)
  "Return the char-code of C."
  (char-code c))

;;; ---- STRING-CONTAINS / STRING-PREFIX-P / STRING-SUFFIX-P -----------------

(defun string-contains (needle haystack &key (test #'char=))
  "Return the start index where NEEDLE appears in HAYSTACK, or NIL."
  (search needle haystack :test test))

;;; ---- MAP improvements for 'bit-vector result ----------------------------

(defun map (result-type function &rest sequences)
  "Apply FUNCTION element-wise to SEQUENCES; collect result as RESULT-TYPE."
  (let* ((len  (reduce #'min (mapcar #'length sequences)))
         (vals (let ((acc nil))
                 (dotimes (i len (nreverse acc))
                   (push (apply function
                                (mapcar (lambda (s) (elt s i)) sequences))
                         acc)))))
    (if (null result-type)
        nil
        (case (%seq-type-kind result-type)
          (:list vals)
          (:string
           (let ((s (make-string len)))
             (dotimes (i len s) (setf (char s i) (nth i vals)))))
          (:bit-vector
           (let ((v (make-bit-vector len 0)))
             (dotimes (i len v) (bit-set v i (logand 1 (nth i vals))))))
          (:vector
           (let ((v (make-array len)))
             (dotimes (i len v) (setf (aref v i) (nth i vals)))))
          (t (error 'type-error :datum result-type :expected-type 'sequence))))))

;;;; =========================================================================
;;;; Slice 32 additions (bundled with TODO-driven fixes)
;;;; =========================================================================

;;; ---- CHANGE-CLASS --------------------------------------------------------

(defgeneric update-instance-for-different-class (previous current &rest initargs))

(defmethod update-instance-for-different-class (previous current &rest initargs)
  "Default: call shared-initialize with all slot names."
  (declare (ignore previous))
  (apply #'shared-initialize current t initargs))

(defun change-class (instance new-class &rest initargs)
  "Change the class of INSTANCE to NEW-CLASS (in-place)."
  (unless (%is-obj instance)
    (error "CHANGE-CLASS: ~S is not a CLOS instance" instance))
  (let* ((old-class (%obj-class instance))
         (new-cls   (if (%is-cls new-class) new-class
                        (or (gethash new-class *%classes*)
                            (error "CHANGE-CLASS: unknown class ~S" new-class))))
         (old-slots (%cls-eslots old-class))
         (new-slots (%cls-eslots new-cls))
         ;; Save old slot values for slots that carry over
         (saved (let ((acc nil))
                  (dolist (s old-slots acc)
                    (let* ((nm  (getf s :name))
                           (loc (getf s :location))
                           (val (svref (%obj-slots instance) loc)))
                      (push (list nm val) acc)))))
         ;; New slot vector
         (new-vec (make-array (length new-slots) :initial-element *%unbound*)))
    ;; Re-tag the instance
    (setf (%obj-class instance) new-cls)
    (setf (%obj-slots instance) new-vec)
    ;; Carry over shared slot values
    (dolist (s new-slots)
      (let* ((nm  (getf s :name))
             (loc (getf s :location))
             (old (assoc nm saved)))
        (when old
          (setf (svref new-vec loc) (second old)))))
    ;; Call protocol
    (apply #'update-instance-for-different-class
           instance instance initargs)
    instance))

;;;; =========================================================================
;;;; Slice 33 — Logical Pathnames (conforming implementation)
;;;;
;;;; Parsing strategy
;;;; ----------------
;;;;  "C:/foo/bar"          physical, device="C", dir=(:abs "foo"), name="bar"
;;;;  "C:\\foo\\bar"    same (Windows backslash normalised to /)
;;;;  "LOGHOST:;dir;name.type" logical, host="LOGHOST"
;;;;  "/usr/local/bin/cc"   physical, no device
;;;;
;;;; Single ASCII letter before ":" → Windows device letter.
;;;; 2+ chars before ":" with a registered logical host → logical pathname.
;;;; =========================================================================

;;; ---- DEFSTRUCT for logical pathnames ------------------------------------

(defstruct (logical-pathname
            (:constructor %make-lp)
            (:predicate   logical-pathname-p))
  host       ; uppercase string
  directory  ; (:absolute/:relative component…) all upcased
  name       ; uppercase string, :wild, or nil
  type       ; uppercase string, :wild, or nil
  version    ; integer, :newest, :wild, or nil
)

;;; ---- Logical-pathname translation table ---------------------------------

(defvar *logical-pathname-translations*
  (make-hash-table :test 'equal)
  "Maps upcased logical host strings to translation lists.")

(defun logical-pathname-translations (host)
  (gethash (string-upcase (string host)) *logical-pathname-translations*))

(defun set-logical-pathname-translations (host translations)
  (setf (gethash (string-upcase (string host)) *logical-pathname-translations*)
        translations)
  translations)

(defsetf logical-pathname-translations set-logical-pathname-translations)

;;; ---- Parsing helpers -----------------------------------------------------

(defun %lp-valid-word-char-p (c)
  (or (char<= #\A c #\Z) (char<= #\a c #\z)
      (char<= #\0 c #\9) (eql c #\-)))

(defun %lp-parse-device (s)
  "If S starts with a single ASCII letter then ':', return that letter string."
  (when (>= (string-length s) 2)
    (let ((c0 (char s 0)) (c1 (char s 1)))
      (when (and (eql c1 #\:)
                 (or (char<= #\A c0 #\Z) (char<= #\a c0 #\Z)))
        (string-upcase (string c0))))))

(defun %lp-parse-host (s)
  "If S starts with 2+ logical-word chars then ':', return (values HOST REST-STRING)."
  (let ((len (string-length s)) (i 0))
    (loop while (< i len)
          while (%lp-valid-word-char-p (char s i))
          do (incf i))
    (when (and (>= i 2) (< i len) (eql (char s i) #\:))
      (values (string-upcase (subseq s 0 i))
              (subseq s (1+ i))))))

(defun %lp-upcase-or-wild (x)
  (cond ((string= x "**") :wild-inferiors)
        ((string= x "*")  :wild)
        ((string= x "")   nil)
        (t (string-upcase x))))

(defun %lp-parse-body (rest)
  "Parse the directory/name/type/version from the part after HOST: in a logical namestring."
  ;; Normalise: accept both ; and > as separator
  (let* ((s (map 'string (lambda (c) (if (eql c #\>) #\; c)) rest))
         (abs-p (and (> (string-length s) 0) (eql (char s 0) #\;)))
         (s     (if abs-p (subseq s 1) s))
         (parts (split-sequence #\; s)))
    ;; Last part is the file component; the rest are directory components
    (let* ((file  (if parts (car (last parts)) ""))
           (dirs  (if parts (butlast parts) '()))
           ;; Parse file: NAME.TYPE or NAME.TYPE.VERSION
           (fparts (split-sequence #\. file))
           (fname  (car fparts))
           (ftype  (cadr fparts))
           (fver   (caddr fparts))
           ;; Build directory list
           (dir-list (cons (if abs-p :absolute :relative)
                           (mapcar #'%lp-upcase-or-wild dirs)))
           ;; Parse version
           (ver  (cond ((null fver)                 nil)
                       ((string-equal fver "NEWEST") :newest)
                       ((string= fver "*")           :wild)
                       ((every #'digit-char-p fver) (parse-integer fver))
                       (t                            nil))))
      (values (if (equal dir-list '(:relative)) nil dir-list)
              (%lp-upcase-or-wild fname)
              (%lp-upcase-or-wild ftype)
              ver))))

;;; ---- LOGICAL-PATHNAME constructor ----------------------------------------

(defun logical-pathname (pathspec)
  "Coerce PATHSPEC to a logical-pathname struct."
  (cond
    ((logical-pathname-p pathspec) pathspec)
    ((stringp pathspec)
     (multiple-value-bind (host rest)
         (%lp-parse-host pathspec)
       (unless host
         (error "LOGICAL-PATHNAME: no logical host in ~S" pathspec))
       (multiple-value-bind (dir name type ver)
           (%lp-parse-body rest)
         (%make-lp :host host :directory dir
                   :name name :type type :version ver))))
    (t (error "LOGICAL-PATHNAME: not a string or logical-pathname: ~S" pathspec))))

;;; ---- NAMESTRING for logical pathnames ------------------------------------

(defun logical-pathname-namestring (lp)
  "Produce the canonical namestring for a logical-pathname."
  (flet ((comp-str (c)
           (cond ((null c) "")
                 ((eq c :wild) "*")
                 ((eq c :wild-inferiors) "**")
                 (t (string c)))))
    (let* ((host  (logical-pathname-host lp))
           (dir   (logical-pathname-directory lp))
           (name  (logical-pathname-name lp))
           (type  (logical-pathname-type lp))
           (ver   (logical-pathname-version lp))
           (abs-p (and (listp dir) (eq (car dir) :absolute)))
           (dirs  (if (listp dir) (cdr dir) nil))
           (dir-s (apply #'concatenate 'string
                          (mapcar (lambda (d) (concatenate 'string (comp-str d) ";")) dirs)))
           (name-s (comp-str name))
           (type-s (if type (concatenate 'string "." (comp-str type)) ""))
           (ver-s  (cond ((null ver) "")
                         ((eq ver :newest) ".NEWEST")
                         ((eq ver :wild)   ".*")
                         ((integerp ver)   (concatenate 'string "." (number-to-string ver)))
                         (t ""))))
      (concatenate 'string
                   host ":"
                   (if abs-p ";" "")
                   dir-s name-s type-s ver-s))))

;;; ---- Updated NAMESTRING ---------------------------------------------------

(defvar *%kernel-namestring-fn*
  (symbol-function 'namestring))

(defun namestring (p)
  "Return the namestring of pathname designator P."
  (cond
    ((logical-pathname-p p)     (logical-pathname-namestring p))
    ((stringp p)                p)
    (t                          (funcall *%kernel-namestring-fn* p))))

;;; ---- Updated PATHNAME -----------------------------------------------------

(defvar *%kernel-pathname-fn*
  (symbol-function 'pathname))

(defun pathname (designator)
  "Coerce DESIGNATOR to a pathname (logical or physical)."
  (cond
    ((logical-pathname-p designator) designator)
    ((or (physical-pathname-p designator)
         (typep designator 'file-stream))
     (funcall *%kernel-pathname-fn* designator))
    ((stringp designator)
     ;; A single-letter device prefix always means physical (Windows)
     (let ((dev (%lp-parse-device designator)))
       (if dev
           (funcall *%kernel-pathname-fn* designator)
           (multiple-value-bind (host _rest)
               (%lp-parse-host designator)
             (declare (ignore _rest))
             (if (and host
                      (gethash host *logical-pathname-translations*))
                 (logical-pathname designator)
                 (funcall *%kernel-pathname-fn* designator))))))
    (t (funcall *%kernel-pathname-fn* designator))))

(defun parse-namestring (thing &optional host default-pathname
                                         &key (start 0) end junk-allowed)
  "Parse THING into a pathname (CLHS 19.2.2); return (values pathname position).
   bocl pathnames are namestring-backed, so a string THING is converted by
   PATHNAME over its [START, END) substring and POSITION is END; a pathname /
   stream THING is returned unchanged with POSITION = START.  HOST, when given,
   must name a defined logical host (else an error, per the standard); DEFAULT-
   PATHNAME is accepted and ignored.  JUNK-ALLOWED is accepted; this parser
   consumes the whole substring, so it never reports junk."
  (declare (ignore default-pathname junk-allowed))
  (when (and host (not (gethash (string host) *logical-pathname-translations*)))
    (error "PARSE-NAMESTRING: ~S does not name a defined logical host." host))
  (cond
    ((logical-pathname-p thing) (values thing start))
    ((or (physical-pathname-p thing) (typep thing 'file-stream))
     (values (pathname thing) start))
    ((stringp thing)
     (let* ((stop (or end (length thing)))
            (sub  (if (and (eql start 0) (eql stop (length thing)))
                      thing
                      (subseq thing start stop))))
       (values (pathname sub) stop)))
    (t (values (pathname thing) start))))

;;; ---- Updated pathname component accessors --------------------------------

(defun pathname-host (p)
  (if (logical-pathname-p p)
      (logical-pathname-host p)
      ;; For physical pathnames check if the namestring starts with a
      ;; registered logical host prefix.
      (let ((s (if (stringp p) p (namestring p))))
        (multiple-value-bind (h _) (%lp-parse-host s)
          (declare (ignore _))
          (when (and h (gethash h *logical-pathname-translations*)) h)))))

(defun pathname-device (p)
  (if (logical-pathname-p p)
      nil
      (%lp-parse-device (if (stringp p) p (namestring p)))))

(defun pathname-version (p)
  (if (logical-pathname-p p)
      (logical-pathname-version p)
      nil))

;;; Extend pathname-directory / pathname-name / pathname-type for
;;; logical-pathnames (physical versions already exist above).

(let ((old-dir  (symbol-function 'pathname-directory))
      (old-name (symbol-function 'pathname-name))
      (old-type (symbol-function 'pathname-type)))
  (set-symbol-function
   (lambda (p)
     (if (logical-pathname-p p)
         (logical-pathname-directory p)
         (funcall old-dir p)))
   'pathname-directory)
  (set-symbol-function
   (lambda (p)
     (if (logical-pathname-p p)
         (logical-pathname-name p)
         (funcall old-name p)))
   'pathname-name)
  (set-symbol-function
   (lambda (p)
     (if (logical-pathname-p p)
         (logical-pathname-type p)
         (funcall old-type p)))
   'pathname-type))

;;; ---- MAKE-PATHNAME (logical-aware) ---------------------------------------

(defun %pn-comp-string (c)
  "Coerce a pathname component (name / type / directory part) to the string
   that represents it in a namestring.
     NIL              -> \"\"   (component absent)
     :WILD            -> \"*\"
     :WILD-INFERIORS  -> \"**\"
     :UNSPECIFIC      -> \"\"
     string           -> itself
     other symbol     -> its (downcased) name."
  (cond ((null c)               "")
        ((eq c :wild)           "*")
        ((eq c :wild-inferiors) "**")
        ((or (eq c :up) (eq c :back)) "..")
        ((eq c :unspecific)     "")
        ((stringp c)            c)
        ((symbolp c)            (string-downcase (symbol-name c)))
        (t                      (princ-to-string c))))

(defun make-pathname (&key host device (directory nil directory-p)
                           (name nil name-p) (type nil type-p) version
                           (defaults nil defaults-p) case)
  "Construct a pathname from components."
  (declare (ignore case))
  (let* ((uhost (when host (string-upcase (string host))))
         (logicalp (and uhost (gethash uhost *logical-pathname-translations*))))
    (if logicalp
        (%make-lp :host      uhost
                  :directory directory
                  :name      name
                  :type      type
                  :version   version)
        ;; Physical pathname — build namestring
        (let* ((def-s (if defaults-p
                          (namestring (if (stringp defaults) defaults
                                          (or defaults "")))
                          ""))
               (dev-pref (or (when device (concatenate 'string
                                                        (string-upcase device) ":"))
                             (when (%lp-parse-device def-s)
                               (concatenate 'string (%lp-parse-device def-s) ":"))
                             ""))
               (dir-s
                (cond
                  ;; DIRECTORY not supplied: inherit from DEFAULTS (CLHS make-pathname).
                  ((and (not directory-p) defaults-p)
                   (let ((dd (funcall (symbol-function 'pathname-directory) def-s)))
                     (if (listp dd)
                         (apply #'concatenate 'string
                                (let ((abs (eq (car dd) :absolute)))
                                  (cons (if abs "/" "")
                                        (mapcar (lambda (d) (concatenate 'string (%pn-comp-string d) "/")) (cdr dd)))))
                         (or dd ""))))
                  ;; DIRECTORY explicitly NIL (or no defaults): no directory — this
                  ;; keeps an explicit :directory nil RELATIVE, rather than wrongly
                  ;; inheriting an absolute directory from DEFAULTS (UIOP relies on
                  ;; (make-pathname :directory nil :defaults <abs>) staying relative).
                  ((null directory) "")
                  ((stringp directory) directory)
                  ((listp directory)
                   (let* ((abs  (eq (car directory) :absolute))
                          (parts (cdr directory)))
                     (apply #'concatenate 'string
                            (cons (if abs "/" "")
                                  (mapcar (lambda (d) (concatenate 'string (%pn-comp-string d) "/")) parts)))))
                  (t "")))
               ;; NAME / TYPE: when not supplied, inherit from DEFAULTS (CLHS
               ;; make-pathname); an EXPLICIT NIL is honoured as "no component"
               ;; (this is how the directory-pathname idiom
               ;; (make-pathname :name nil :type nil :defaults p) strips the file
               ;; part).  ASDF relies on (make-pathname :type "fasl" :defaults
               ;; "foo.lisp") -> "foo.fasl" (name inherited from DEFAULTS).
               (name-s (%pn-comp-string
                        (cond (name-p     name)
                              (defaults-p (funcall (symbol-function 'pathname-name) def-s))
                              (t          nil))))
               (type-s (let ((ts (%pn-comp-string
                                  (cond (type-p     type)
                                        (defaults-p (funcall (symbol-function 'pathname-type) def-s))
                                        (t          nil)))))
                         (if (string= ts "") "" (concatenate 'string "." ts)))))
          (funcall *%kernel-pathname-fn*
                   (concatenate 'string dev-pref dir-s name-s type-s))))))

;;; ---- TRANSLATE-LOGICAL-PATHNAME ------------------------------------------

(defun %lp-wild-match (pattern value)
  "Return non-nil if PATTERN matches VALUE (both uppercase strings or :wild etc.)."
  (cond
    ((or (eq pattern :wild) (eq pattern :wild-inferiors)) t)
    ((null pattern) (null value))
    ((null value)   nil)
    ((stringp pattern)
     ;; Simple: exact or star wildcard
     (if (string= pattern "*")
         t
         (let ((star (search "*" pattern)))
           (if star
               (and (<= star (string-length value))
                    (string= (subseq pattern 0 star)
                             (subseq value 0 (min star (string-length value))))
                    (let ((suffix-len (- (string-length pattern) star 1)))
                      (and (>= (string-length value) (+ star suffix-len))
                           (string= (subseq pattern (+ star 1))
                                    (subseq value (- (string-length value) suffix-len))))))
               (string= pattern value)))))
    (t nil)))

(defun %lp-dir-match (wpat adir)
  "Return non-nil if wildcard directory WPAT matches actual ADIR."
  (cond
    ((null wpat) (null adir))
    ((null adir) nil)
    ((not (eq (car wpat) (car adir))) nil)  ; :absolute/:relative mismatch
    (t
     (labels ((match (pp ap)
                (cond ((and (null pp) (null ap)) t)
                      ((null pp) nil)
                      ((eq (car pp) :wild-inferiors)
                       ;; ** matches zero or more components.
                       ;; Try skipping ** (zero match) or consuming one ap component.
                       (or (match (cdr pp) ap)          ; ** matches 0 remaining
                           (and (not (null ap))          ; ** consumes 1 component
                                (match pp (cdr ap)))))
                      ((null ap) nil)
                      ((%lp-wild-match (car pp) (car ap))
                       (match (cdr pp) (cdr ap)))
                      (t nil))))
       (match (cdr wpat) (cdr adir))))))

(defun %lp-customary-case (s)
  "Apply the customary case for logical->physical translation.
  CLHS 19.3.1.1.3: components transferred from a logical pathname (which are
  customarily uppercase) are downcased in the resulting physical pathname."
  (if (stringp s) (string-downcase s) s))

(defun %lp-translate-wild (from-pat to-pat actual)
  "Given FROM-PAT wildcard, TO-PAT pattern with *, and ACTUAL value, produce output.
  The portion substituted from the (upcased) logical ACTUAL is downcased to the
  customary physical case."
  (cond
    ((or (null to-pat) (null actual)) (%lp-customary-case actual))
    ((eq to-pat :wild) (%lp-customary-case actual))
    ((stringp to-pat)
     (let ((star (search "*" to-pat)))
       (if star
           ;; Extract the part matched by * in from-pat, then downcase it.
           (let* ((matched
                   (if (or (null from-pat) (eq from-pat :wild))
                       (or actual "")
                       (if (stringp from-pat)
                           (let ((fs (search "*" from-pat)))
                             (if fs
                                 (subseq (or actual "")
                                         (min fs (string-length (or actual "")))
                                         (max 0 (- (string-length (or actual ""))
                                                   (- (string-length from-pat) fs 1))))
                                 (or actual "")))
                           (or actual "")))))
             (concatenate 'string
                          (subseq to-pat 0 star)
                          (%lp-customary-case matched)
                          (subseq to-pat (+ star 1))))
           to-pat)))
    (t to-pat)))

(defun translate-logical-pathname (pathname &key)
  "Translate logical pathname PATHNAME to a physical pathname."
  (let* ((lp (cond ((logical-pathname-p pathname) pathname)
                   ((stringp pathname)
                    (multiple-value-bind (host rest)
                        (%lp-parse-host pathname)
                      (when host
                        (multiple-value-bind (dir name type ver)
                            (%lp-parse-body rest)
                          (%make-lp :host host :directory dir
                                    :name name :type type :version ver)))))
                   (t nil))))
    (unless lp
      ;; Not a logical pathname — return as-is
      (return-from translate-logical-pathname (pathname pathname)))
    (let* ((host  (logical-pathname-host lp))
           (rules (gethash host *logical-pathname-translations*)))
      (unless rules
        (error "TRANSLATE-LOGICAL-PATHNAME: no translations for host ~S" host))
      (dolist (rule rules
               (error "TRANSLATE-LOGICAL-PATHNAME: no matching rule for ~S"
                      (logical-pathname-namestring lp)))
        (destructuring-bind (from to) rule
          (let* ((fp (if (logical-pathname-p from) from
                         (ignore-errors (logical-pathname from))))
                 (tp (if (stringp to) to (namestring to))))
            (when fp
              (let ((fd  (logical-pathname-directory fp))
                    (fn  (logical-pathname-name fp))
                    (ft  (logical-pathname-type fp))
                    (ld  (logical-pathname-directory lp))
                    (ln  (logical-pathname-name lp))
                    (lt  (logical-pathname-type lp)))
                (when (and (or (null fd) (%lp-dir-match fd ld))
                           (%lp-wild-match fn ln)
                           (%lp-wild-match ft lt))
                  ;; Build physical pathname from TO template + translated parts
                  (let* ((tp-s    (if (stringp tp) tp (namestring tp)))
                         ;; Parse the TO string directly to avoid relying on
                         ;; pathname-directory's string return type.
                         (tp-s   (map 'string (lambda (c) (if (eql c #\\) #\/ c)) tp-s))
                         (to-abs (and (> (string-length tp-s) 0) (eql (char tp-s 0) #\/)))
                         (to-de  (%path-dir-end tp-s))
                         (to-dir-str (if (>= to-de 0) (subseq tp-s 0 (+ to-de 1)) ""))
                         (to-dir-parts (split-sequence #\/ to-dir-str :remove-empty-subseqs t))
                         (to-name-s  (file-namestring tp-s))
                         (to-dot     (%path-dot-pos to-name-s))
                         (to-name (if (< to-dot 0)
                                      (if (string= to-name-s "") nil to-name-s)
                                      (subseq to-name-s 0 to-dot)))
                         (to-type (if (< to-dot 0) nil (subseq to-name-s (+ to-dot 1))))
                         ;; Build directory list, normalising "**" to :wild-inferiors
                         (to-dir-norm
                          (cons (if to-abs :absolute :relative)
                                (mapcar (lambda (d) (if (equal d "**") :wild-inferiors d))
                                        to-dir-parts)))
                         ;; Translate directory: :wild-inferiors absorbs actual dir components
                         (new-dir
                          (if (and (listp to-dir-norm)
                                   (member :wild-inferiors (cdr to-dir-norm)))
                              (let* ((ti     (position :wild-inferiors (cdr to-dir-norm)))
                                     (before (subseq (cdr to-dir-norm) 0 ti))
                                     (after  (subseq (cdr to-dir-norm) (+ ti 1)))
                                     ;; Directory components absorbed from the (upcased)
                                     ;; logical pathname are downcased to customary case.
                                     (actual-parts (if (listp ld)
                                                       (mapcar #'%lp-customary-case (cdr ld))
                                                       '())))
                                (cons :absolute (append before actual-parts after)))
                              to-dir-norm))
                         (dir-s
                          (cond
                            ((null new-dir) "")
                            ((listp new-dir)
                             (apply #'concatenate 'string
                                    (cons (if (eq (car new-dir) :absolute) "/" "")
                                          (mapcar (lambda (d) (concatenate 'string d "/"))
                                                  (cdr new-dir)))))
                            (t "")))
                         (final-name (%lp-translate-wild fn to-name ln))
                         (final-type (%lp-translate-wild ft to-type lt))
                         (path-s
                          (concatenate 'string dir-s
                                       (or final-name "")
                                       (if final-type
                                           (concatenate 'string "." final-type)
                                           ""))))
                    (return-from translate-logical-pathname
                      (funcall *%kernel-pathname-fn* path-s))))))))))))

;;; ---- LOAD-LOGICAL-PATHNAME-TRANSLATIONS ----------------------------------

(defun load-logical-pathname-translations (host)
  "Load translations for HOST from a configuration file.
  Searches (in order):
    ~/.bocl/<host>.translations
    /etc/bocl/<host>.translations
    ./<host>.translations
  Returns T if a file was found and loaded."
  (let* ((uhost (string-upcase (string host)))
         (lhost (string-downcase uhost))
         (fname (concatenate 'string lhost ".translations")))
    (dolist (dir (list
                  (let ((h (ignore-errors
                             (let* ((home (namestring (funcall *%kernel-pathname-fn* "~")))
                                    ;; On Unix ~ is a literal filename not expanded
                                    ;; Try $HOME instead
                                    )
                               (concatenate 'string home "/.bocl/")))))
                    (or h ""))
                  "/etc/bocl/"
                  "./"))
      (let ((path (concatenate 'string dir fname)))
        (when (ignore-errors (probe-file path))
          (load path)
          (return-from load-logical-pathname-translations t))))
    nil))

;;; ---- MERGE-PATHNAMES (logical-aware) -------------------------------------

(defvar *default-pathname-defaults*
  (funcall *%kernel-pathname-fn* "")
  "Default pathname for MERGE-PATHNAMES.")

(let ((old-merge (symbol-function 'merge-pathnames)))
  (set-symbol-function
   (lambda (pathname &optional (defaults *default-pathname-defaults*)
                     default-version)
     (declare (ignore default-version))
     (let ((p (pathname pathname))
           (d (pathname defaults)))
       (cond
         ((logical-pathname-p p)
          ;; LP + anything: fill in missing fields from D if D is also LP
          (if (logical-pathname-p d)
              (%make-lp
               :host      (or (logical-pathname-host p)      (logical-pathname-host d))
               :directory (or (logical-pathname-directory p) (logical-pathname-directory d))
               :name      (or (logical-pathname-name p)      (logical-pathname-name d))
               :type      (or (logical-pathname-type p)      (logical-pathname-type d))
               :version   (or (logical-pathname-version p)   (logical-pathname-version d)))
              p))
         (t
          ;; Physical: delegate to old merge-pathnames
          (funcall old-merge pathname defaults)))))
   'merge-pathnames))

;;; ---- WILD-PATHNAME-P (logical-aware) -------------------------------------

(defun wild-pathname-p (pathname &optional field-key)
  (let ((p (pathname pathname)))
    (flet ((wld (x) (or (eq x :wild) (eq x :wild-inferiors)
                        (and (stringp x) (search "*" x)))))
      (let ((all-fields
             (list (pathname-name p) (pathname-type p) (pathname-version p)
                   (pathname-host p) (pathname-device p)
                   (let ((d (pathname-directory p)))
                     (if (listp d) (some #'wld (cdr d)) (wld d))))))
        (if field-key
            (case field-key
              (:name      (wld (pathname-name p)))
              (:type      (wld (pathname-type p)))
              (:version   (wld (pathname-version p)))
              (:host      (wld (pathname-host p)))
              (:device    (wld (pathname-device p)))
              (:directory (let ((d (pathname-directory p)))
                            (if (listp d) (some #'wld (cdr d)) (wld d)))))
            (some #'identity (mapcar #'wld (butlast all-fields))))))))

;;; ---- PATHNAME-MATCH-P ----------------------------------------------------

(defun pathname-match-p (pathname wildcard)
  "Return T if PATHNAME matches the wildcard WILDCARD."
  (let ((p (pathname pathname))
        (w (pathname wildcard)))
    (and (%lp-wild-match (pathname-name w) (pathname-name p))
         (%lp-wild-match (pathname-type w) (pathname-type p))
         (let ((wd (pathname-directory w))
               (pd (pathname-directory p)))
           (or (null wd) (%lp-dir-match wd pd))))))

;;; ---- Ensure PATHNAMEP recognises logical pathnames ----------------------

(let ((old-pathnamep (symbol-function 'pathnamep)))
  (set-symbol-function
   (lambda (x) (or (logical-pathname-p x) (funcall old-pathnamep x)))
   'pathnamep))

;;;; THE END OF SLICE 33

;;;; =========================================================================
;;;; Slice 34 — RATIO and COMPLEX (C kernel extension)
;;;; =========================================================================

;;; The C kernel now provides:
;;;   RATIOP, NUMERATOR, DENOMINATOR, RATIONALP
;;;   COMPLEXP, COMPLEX, REALPART, IMAGPART
;;;   Ratio/complex arithmetic via + - * /
;;;   Ratio printing (1/3) and reading
;;;   Complex printing (#C(1 2)) and reading

;;; ---- Predicates --------------------------------------------------------

(defun numberp (x)
  "Return T if X is any number."
  (or (integerp x) (floatp x) (ratiop x) (complexp x)))

;;; ---- RATIONAL / RATIONALIZE (ratio-aware) ------------------------------

(defun rational (x)
  "Convert X to a rational; floats are approximated."
  (cond ((integerp x) x)
        ((ratiop x) x)
        ((floatp x)
         ;; Approximate: multiply by a large power of 10 and reduce
         (let* ((neg (< x 0.0))
                (xabs (if neg (- x) x))
                (n (round (* xabs 1000000)))
                (result (/ n 1000000)))
           (if neg (- result) result)))
        (t (error "RATIONAL: ~S is not real" x))))

(defun rationalize (x)
  "Return a simple rational approximation to X."
  (rational x))

;;; ---- ABS for ratios ----------------------------------------------------

(defun abs (x)
  "Return the absolute value of X."
  (cond ((ratiop x)   (if (< (numerator x) 0) (/ (- (numerator x)) (denominator x)) x))
        ((integerp x) (if (< x 0) (- x) x))
        ((floatp x)   (if (< x 0.0) (- x) x))
        (t (error "ABS: ~S is not a real number" x))))

;;; ---- Numeric predicates update ----------------------------------------

(defun zerop (x)
  (cond ((ratiop x)   nil)
        ((integerp x) (= x 0))
        ((floatp x)   (= x 0.0))
        (t (error "ZEROP: ~S" x))))

(defun plusp (x)
  (cond ((ratiop x)   (> (numerator x) 0))
        ((integerp x) (> x 0))
        ((floatp x)   (> x 0.0))
        (t (error "PLUSP: ~S" x))))

(defun minusp (x)
  (cond ((ratiop x)   (< (numerator x) 0))
        ((integerp x) (< x 0))
        ((floatp x)   (< x 0.0))
        (t (error "MINUSP: ~S" x))))

;;; ---- Update TYPEP for new types ----------------------------------------
;;; Use *%kernel-typep* (the C kernel typep) directly to check new types.
;;; This avoids circular calls through the Lisp wrappers at lines 196-201.

(let ((old-typep (symbol-function 'typep))
      (ktp *%kernel-typep*))
  (set-symbol-function
   (lambda (object type &optional environment)
     (declare (ignore environment))
     (cond
       ;; User DEFTYPE abbreviation (a no-arg type name): expand and recurse
       ;; through the full TYPEP.  Later TYPEP redefinitions (this one and the
       ;; DEFUN above) had dropped the *%DEFTYPE-EXPANDERS* lookup installed
       ;; earlier, so (typep x 'my-deftype) silently stopped working.
       ((and (symbolp type) (gethash type *%deftype-expanders*))
        (funcall (symbol-function 'typep) object
                 (funcall (gethash type *%deftype-expanders*))))
       ((eq type 'ratio)    (funcall ktp object 'ratio))
       ((eq type 'rational) (or (funcall ktp object 'integer) (funcall ktp object 'ratio)))
       ((eq type 'complex)  (funcall ktp object 'complex))
       ((eq type 'real)     (or (funcall ktp object 'integer)
                                (funcall ktp object 'float)
                                (funcall ktp object 'ratio)))
       ((eq type 'number)   (or (funcall ktp object 'integer)
                                (funcall ktp object 'float)
                                (funcall ktp object 'ratio)
                                (funcall ktp object 'complex)))
       (t (funcall old-typep object type))))
   'typep))

;;;; THE END OF SLICE 34

;;;; ========================================================================
;;;; Slice 35 — LOAD merges its filespec against *DEFAULT-PATHNAME-DEFAULTS*
;;;; ========================================================================
;;;
;;; The bare kernel LOAD opens the namestring it is given verbatim, relative
;;; to the process working directory.  Conforming LOAD must instead merge a
;;; relative FILESPEC against *DEFAULT-PATHNAME-DEFAULTS* (CLHS 20 / LOAD).
;;; Programs such as the ANSI test suite rely on this: doit.lsp rebinds
;;; *DEFAULT-PATHNAME-DEFAULTS* to its own directory and then does
;;; (load "init.lsp"), expecting the file to be found there rather than in
;;; the caller's working directory.

(defvar *%kernel-load* (symbol-function 'load)
  "The bare kernel LOAD: opens an explicit namestring and binds
   *LOAD-PATHNAME* / *LOAD-TRUENAME* for the duration of the load.")

;;; FILE-WRITE-DATE: the kernel primitive signals a generic SIMPLE-ERROR when
;;; the file does not exist; CLHS requires FILE-ERROR (a missing file is a
;;; file-system operation that cannot be performed).  UIOP's SAFE-FILE-WRITE-DATE
;;; relies on this — it does (handler-case (file-write-date p) (file-error () nil))
;;; while planning, since generated outputs (e.g. fasls bocl never writes) do not
;;; yet exist.  Wrap the primitive so a missing file raises a proper FILE-ERROR.
(defvar *%kernel-file-write-date* (symbol-function 'file-write-date)
  "The bare kernel FILE-WRITE-DATE (stats the namestring; errors if absent).")

(defun file-write-date (filespec)
  "FILE-WRITE-DATE per CLHS: universal time of FILESPEC's last write, signalling
   FILE-ERROR if the file does not exist."
  (let ((pathname (pathname filespec)))
    (if (probe-file pathname)
        (funcall *%kernel-file-write-date* pathname)
        (error 'file-error :pathname pathname))))

(defun load (filespec &key (verbose *load-verbose*) (print *load-print*)
                           (if-does-not-exist t) (external-format :default))
  "Load FILESPEC.  A relative pathname is merged against
   *DEFAULT-PATHNAME-DEFAULTS* before opening (conforming behaviour).  A bocl fasl
   (written by COMPILE-FILE — a header comment plus macro-free forms) loads with
   the ordinary source loader; its version header is checked first (see
   %CHECK-FASL-VERSION).  Should a named compiled file (e.g. \"foo.fasl\") be
   ABSENT, LOAD transparently falls back to its source sibling \"foo.lsp\", so the
   COMPILE-AND-LOAD idiom keeps working even when no fasl was written."
  (declare (ignore verbose print external-format))
  ;; CLHS LOAD: bind *PACKAGE* and *READTABLE* to their pre-load values so that an
  ;; IN-PACKAGE (or readtable change) inside the loaded file is local to the load
  ;; and does not leak into the caller.  (*LOAD-PATHNAME* / *LOAD-TRUENAME* are
  ;; already saved and restored around the load by the kernel loader.)
  (let ((*package* *package*)
        (*readtable* *readtable*))
   (let* ((merged (merge-pathnames filespec *default-pathname-defaults*))
         (ns     (namestring merged))
         (type   (pathname-type merged)))
    (flet ((do-load (path)
             ;; A fast-load file (compiled type) may carry a bocl fasl header.
             ;; When it does, read it back under the SAME controlled syntax it was
             ;; written with (standard readtable, DOUBLE-FLOAT floats, base 10, and
             ;; *READ-EVAL* off — a bocl fasl is macro-free and never contains #.);
             ;; %CHECK-FASL-VERSION also warns on a newer major version.  A source
             ;; or foreign file (no header) loads with the caller's syntax.
             (if (and (%known-file-type-p (pathname-type (pathname path))
                                          *accepted-compiled-file-types*)
                      (%check-fasl-version path))
                 (let ((*readtable*                 (copy-readtable nil))
                       (*read-default-float-format* 'double-float)
                       (*read-base*                 10)
                       (*read-eval*                 nil))
                   (funcall *%kernel-load* path))
                 (funcall *%kernel-load* path)))
           (missing ()
             (if if-does-not-exist
                 (error "LOAD: file ~S does not exist" ns)
                 nil)))
      (cond
        ;; (1) An explicit file-type was given.
        ((%explicit-type-p merged)
         (cond
           ((probe-file ns) (do-load ns))
           ;; bocl writes no real fast-load file, so LOAD of a named-but-absent
           ;; compiled file falls back to its source sibling — this is what makes
           ;; the COMPILE-AND-LOAD idiom (load (compile-file f)) work.
           ((%known-file-type-p type *accepted-compiled-file-types*)
            (let ((source (%find-source-file merged)))
              (if source (do-load source) (missing))))
           (t (missing))))
        ;; (2) No type, but the file exists exactly as named.
        ((probe-file ns) (do-load ns))
        ;; (3) No type: search the accepted compiled/source candidates and load
        ;;     the more recent when both exist.
        (t
         (let ((compiled (%find-compiled-file merged))
               (source   (%find-source-file merged)))
           (cond
             ((and compiled source)
              (if (>= (file-write-date compiled) (file-write-date source))
                  (do-load compiled)
                  (do-load source)))
             (compiled (do-load compiled))
             (source   (do-load source))
             (t (missing))))))))))

;;; ---- PROCLAIM / DECLAIM --------------------------------------------------
;;; Global declarations are advisory in bocl (an interpreter that does no
;;; optimisation and treats every binding uniformly).  We accept and ignore
;;; them so that conforming programs — notably the ANSI test suite — load.
;;; The one declaration with run-time meaning, (SPECIAL var...), is honoured
;;; by marking the named variables special via DEFVAR.

(defun proclaim (declaration-specifier)
  (when (and (consp declaration-specifier)
             (eq (car declaration-specifier) 'special))
    (dolist (v (cdr declaration-specifier))
      (when (symbolp v)
        (eval (list 'defvar v)))))      ; DEFVAR marks the variable special
  nil)

(defmacro declaim (&rest declaration-specifiers)
  `(eval-when (:compile-toplevel :load-toplevel :execute)
     ,@(mapcar (lambda (spec) `(proclaim ',spec)) declaration-specifiers)
     nil))

;;; ---- DEFPACKAGE / IMPORT / SHADOW / UNINTERN / ... -----------------------
;;; True per-package operators, built on the kernel table primitives
;;; (%pkg-import, %pkg-unintern, %pkg-unexport, %pkg-shadow-add, %pkg-unuse,
;;; %pkg-rename, %pkg-delete) and the package accessors.

(defun %designated-package (package)
  (or (find-package package)
      (error "There is no package named ~S" package)))

(defun %as-symbol-list (symbols)
  "Accept a single symbol or a list of symbols; always return a list."
  (if (listp symbols) symbols (list symbols)))

(defun %pkg-err (control &rest args)
  (error 'package-error :format-control control :format-arguments args))

(defun %pkg-conflict (name pkg except)
  "If a symbol named NAME is accessible in PKG, is NOT EXCEPT, and is not one of
   PKG's shadowing symbols, return that conflicting symbol; else NIL."
  (multiple-value-bind (sym status) (find-symbol name pkg)
    (when (and status (not (eq sym except))
               (not (member sym (package-shadowing-symbols pkg))))
      sym)))

(defun import (symbols &optional (package *package*))
  ;; CLHS IMPORT: signals a (correctable) PACKAGE-ERROR if a distinct symbol of the
  ;; same name is already accessible in PACKAGE.
  (let ((pkg (%designated-package package)))
    (dolist (sym (%as-symbol-list symbols) t)
      ;; A name conflict is any DISTINCT symbol of the same name already accessible
      ;; (shadowing does not exempt IMPORT — unlike USE-PACKAGE).
      (multiple-value-bind (existing status) (find-symbol (symbol-name sym) pkg)
        (when (and status (not (eq existing sym)))
          (%pkg-err "IMPORT: importing ~S conflicts with ~S already accessible in ~S."
                    sym existing pkg)))
      (%pkg-import sym pkg))))

(defun shadow (symbols &optional (package *package*))
  ;; SYMBOLS are symbols or strings; make a present (home) symbol of each name in
  ;; PKG — shadowing any inherited one — and mark it shadowing.
  (let ((pkg (%designated-package package)))
    (dolist (name (%as-symbol-list symbols) t)
      (%pkg-shadow-add (%pkg-make-present (string name) pkg) pkg))))

(defun shadowing-import (symbols &optional (package *package*))
  ;; CLHS SHADOWING-IMPORT: like IMPORT but never errors on a name clash — any
  ;; DISTINCT symbol of the same name that is PRESENT in PKG is first uninterned, so
  ;; the imported symbol replaces it; the import is then marked shadowing.
  (let ((pkg (%designated-package package)))
    (dolist (sym (%as-symbol-list symbols) t)
      (multiple-value-bind (old status) (find-symbol (symbol-name sym) pkg)
        (when (and old (not (eq old sym)) (member status '(:internal :external)))
          (%pkg-unintern old pkg)))
      (%pkg-import sym pkg)
      (%pkg-shadow-add sym pkg))))

(defun unintern (symbol &optional (package *package*))
  ;; CLHS UNINTERN: if SYMBOL is a shadowing symbol of PACKAGE that resolves a name
  ;; conflict among two or more distinct inherited symbols of the same name,
  ;; uninterning it would expose the conflict — signal a PACKAGE-ERROR.
  (let ((pkg (%designated-package package)))
    (when (member symbol (package-shadowing-symbols pkg))
      (let ((name (symbol-name symbol)) (inherited '()))
        (dolist (u (package-use-list pkg))
          (multiple-value-bind (s status) (find-symbol name u)
            (when (and (eq status :external) (not (member s inherited)))
              (push s inherited))))
        (when (> (length inherited) 1)
          (%pkg-err "UNINTERN: uninterning the shadowing symbol ~S exposes a name conflict among ~S in ~S."
                    symbol inherited pkg))))
    (%pkg-unintern symbol pkg)))

(defun unexport (symbols &optional (package *package*))
  ;; CLHS UNEXPORT: signals a PACKAGE-ERROR if a symbol is not accessible in PACKAGE.
  (let ((pkg (%designated-package package)))
    (dolist (sym (%as-symbol-list symbols) t)
      (multiple-value-bind (existing status) (find-symbol (symbol-name sym) pkg)
        (unless (and status (eq existing sym))
          (%pkg-err "UNEXPORT: ~S is not accessible in ~S." sym pkg)))
      (%pkg-unexport sym pkg))))

(defun unuse-package (packages-to-unuse &optional (package *package*))
  (let ((pkg (%designated-package package)))
    (dolist (u (%as-symbol-list packages-to-unuse) t)
      (%pkg-unuse (%designated-package u) pkg))))

(defun rename-package (package new-name &optional new-nicknames)
  (%pkg-rename (%designated-package package)
               (string new-name)
               (mapcar #'string new-nicknames)))

(defun delete-package (package)
  ;; CLHS DELETE-PACKAGE: an already-deleted package object (its name is NIL), or a
  ;; name designator naming no package, returns NIL.  A package still used by others
  ;; signals a (correctable) PACKAGE-ERROR.
  (let ((pkg (find-package package)))
    (cond
      ((or (null pkg) (null (package-name pkg))) nil)
      ((package-used-by-list pkg)
       (%pkg-err "DELETE-PACKAGE: ~S is used by ~S." pkg (package-used-by-list pkg)))
      (t (%pkg-delete pkg) t))))

;;; Conformance wrappers over the kernel MAKE-PACKAGE / USE-PACKAGE / EXPORT, adding
;;; the CLHS name/use/export conflict checks (signalling PACKAGE-ERROR) the bare
;;; kernel mutators omit.
(let ((%k-make-package (symbol-function 'make-package)))
  (set-symbol-function
   (lambda (name &rest options)
     (let* ((sname     (string name))
            (nicknames (getf options :nicknames))
            (use-raw   (getf options :use '%unset))
            (use       (if (eq use-raw '%unset) '("COMMON-LISP") use-raw)))
       ;; Name / nickname must not already name a package.
       (dolist (n (cons sname (mapcar #'string nicknames)))
         (when (find-package n)
           (%pkg-err "MAKE-PACKAGE: ~S already names a package." n)))
       ;; Build with NO use list, then USE-PACKAGE each (which checks conflicts).
       (let ((pkg (funcall %k-make-package sname :nicknames nicknames :use nil)))
         (when use (use-package use pkg))
         pkg)))
   'make-package))

(let ((%k-use-package (symbol-function 'use-package)))
  (set-symbol-function
   (lambda (packages-to-use &optional (package *package*))
     (let ((pkg  (%designated-package package))
           (uses (%as-symbol-list packages-to-use)))
       ;; Apply each package one at a time so a conflict BETWEEN two newly-used
       ;; packages (e.g. CL and a package that also exports CAR) is detected.
       (dolist (u uses t)
         (let ((up (%designated-package u)))
           (dolist (s (package-exported-symbols up))
             (let ((c (%pkg-conflict (symbol-name s) pkg s)))
               (when c
                 (%pkg-err "USE-PACKAGE: ~S from ~S conflicts with ~S in ~S."
                           s up c pkg))))
           (funcall %k-use-package up pkg)))))
   'use-package))

(let ((%k-export (symbol-function 'export)))
  (set-symbol-function
   (lambda (symbols &optional (package *package*))
     (let ((pkg (%designated-package package)))
       (dolist (s (%as-symbol-list symbols))
         ;; The symbol must be accessible in PKG.
         (multiple-value-bind (existing status) (find-symbol (symbol-name s) pkg)
           (unless (and status (eq existing s))
             (%pkg-err "EXPORT: ~S is not accessible in ~S." s pkg)))
         ;; Exporting must not create a conflict in any package that uses PKG.
         (dolist (q (package-used-by-list pkg))
           (let ((c (%pkg-conflict (symbol-name s) q s)))
             (when c
               (%pkg-err "EXPORT: exporting ~S would conflict with ~S in ~S." s c q)))))
       (funcall %k-export symbols package)))
   'export))

(defun find-all-symbols (name)
  (let ((sname (string name)) (result '()))
    (dolist (pkg (list-all-packages) result)
      (multiple-value-bind (sym status) (find-symbol sname pkg)
        (when (and status (not (member sym result)))
          (push sym result))))))

(defun %dp-desig (d)
  ;; A package designator in DEFPACKAGE may already be a PACKAGE object (e.g. built
  ;; via EVAL/APPEND with a spliced (find-package ...)); keep it as-is, else coerce
  ;; the string designator (string / symbol / character) to its name string.
  (if (packagep d) d (string d)))

(defmacro defpackage (name &rest options)
  (let ((sname     (string name))
        (nicknames '()) (use '()) (exports '()) (interns '()) (shadows '())
        (import-froms '()) (shadow-import-froms '()) (use-given nil)
        (body '()))
    (dolist (opt options)
      (when (consp opt)
        (let ((k (car opt)) (a (cdr opt)))
          (cond
            ((eq k :nicknames) (setq nicknames (append nicknames (mapcar #'string a))))
            ((eq k :use)       (setq use (append use (mapcar #'%dp-desig a)) use-given t))
            ((eq k :export)    (setq exports (append exports (mapcar #'string a))))
            ((eq k :intern)    (setq interns (append interns (mapcar #'string a))))
            ((eq k :shadow)    (setq shadows (append shadows (mapcar #'string a))))
            ((eq k :import-from)
             (push (cons (%dp-desig (car a)) (mapcar #'string (cdr a))) import-froms))
            ((eq k :shadowing-import-from)
             (push (cons (%dp-desig (car a)) (mapcar #'string (cdr a))) shadow-import-froms))
            (t nil)))))     ; :documentation, :size, etc. — ignored
    ;; Build the body forms in the order DEFPACKAGE specifies:
    ;; shadows, shadowing-import-from, import-from, use, intern, export.  Package
    ;; designators are QUOTED so a spliced PACKAGE object survives to runtime.
    (dolist (n shadows) (push `(shadow ,n pkg) body))
    (dolist (spec shadow-import-froms)
      (let ((from (car spec)))
        (dolist (n (cdr spec)) (push `(shadowing-import (intern ,n ',from) pkg) body))))
    (dolist (spec import-froms)
      (let ((from (car spec)))
        (dolist (n (cdr spec)) (push `(import (intern ,n ',from) pkg) body))))
    (dolist (u use) (push `(use-package ',u pkg) body))
    (dolist (n interns) (push `(intern ,n pkg) body))
    (dolist (n exports) (push `(export (intern ,n pkg) pkg) body))
    `(eval-when (:compile-toplevel :load-toplevel :execute)
       (let ((pkg (or (find-package ,sname)
                      (make-package ,sname :nicknames ',nicknames
                                          :use ',(if use-given use '("COMMON-LISP"))))))
         ,@(nreverse body)
         pkg))))

;;; ---- DOLIST / DOTIMES: tolerate leading (DECLARE ...) --------------------
;;; The earlier DOLIST / DOTIMES splice their body straight into a TAGBODY, so
;;; a conforming body that begins with (declare ...) would try to *call* DECLARE.
;;; Redefine them to strip leading declarations first.  (Placed late so the
;;; redefinitions are what user code — e.g. the ANSI suite — expands with.)

(defun %strip-leading-declares (body)
  "BODY with any leading (DECLARE ...) forms removed."
  (if (and (consp body) (consp (car body)) (eq (car (car body)) 'declare))
      (%strip-leading-declares (cdr body))
      body))

(defmacro dolist (head &body body)
  (let ((var       (first head))
        (list-form (second head))
        (result    (third head))
        (real-body (%strip-leading-declares body)))
    `(let ((dolist-list ,list-form))
       (block nil
         (let ((,var nil))
           (tagbody
              dolist-loop
              (if (null dolist-list) (go dolist-done))
              (setq ,var (car dolist-list))
              (setq dolist-list (cdr dolist-list))
              ,@real-body
              (go dolist-loop)
              dolist-done)
           ;; CLHS: result-form is evaluated INSIDE var's scope, with var = NIL.
           (setq ,var nil)
           ,result)))))

(defmacro dotimes (head &body body)
  (let ((var        (first head))
        (count-form (second head))
        (result     (third head))
        (real-body  (%strip-leading-declares body)))
    `(let ((dotimes-count ,count-form))
       (block nil
         (let ((,var 0))
           (tagbody
              dotimes-loop
              (if (>= ,var dotimes-count) (go dotimes-done))
              ,@real-body
              (setq ,var (+ ,var 1))
              (go dotimes-loop)
              dotimes-done)
           ;; CLHS: result-form evaluated INSIDE var's scope, with var = count.
           ,result)))))

;;; ---- Per-form LOAD recovery (used under -on-error appease) ----------------
;;; LOAD normally aborts the whole file on the first error.  When the kernel
;;; enables recovery (only for user files, never the bootstrap, and only under
;;; -on-error appease), each top-level form is evaluated through this wrapper so
;;; that one bad form is reported and skipped instead of killing the load.  This
;;; lets large suites — e.g. the ANSI tests — load every file they can and still
;;; reach their final report, rather than dying on the first unsupported form.

(defvar *load-error-count* 0
  "Number of top-level forms skipped due to errors during recovering LOADs.")

(defun %load-eval-toplevel-form (form)
  (handler-case
      (eval form)
    (error (c)
      (setq *load-error-count* (+ *load-error-count* 1))
      (ignore-errors
        (format *error-output* "~&;; [load] skipped a form: ~A~%" c))
      nil)))

;;;; THE END OF SLICE 35

;;;; ====================================================================
;;;; Slice 44: file-type resolution variables + MACROEXPAND family
;;;; (issues: lisp-file-types, minimal-compilation)
;;;; ====================================================================

;;; --- Accepted file types for source / compiled files -----------------
;;; A file type is either a string (matched case-sensitively, but a
;;; case-insensitive file system will of course match case-insensitively),
;;; or (:case-insensitive "type") to force a case-insensitive search even on
;;; a case-sensitive file system, with "type" tried first.
(defparameter *accepted-source-file-types*
  '("lisp" "LISP" (:case-insensitive "lisp")
    (:case-insensitive "lsp") (:case-insensitive "cl") (:case-insensitive "l"))
  "File types tried (in order) to find a source file given a typeless pathname.")

(defparameter *accepted-compiled-file-types*
  '("fasl" "FASL" (:case-insensitive "fasl")
    (:case-insensitive "fas") (:case-insensitive "obj") (:case-insensitive "o"))
  "File types tried (in order) to find a compiled file given a typeless pathname.")

(defparameter *default-compiled-file-type* "fasl"
  "The file type COMPILE-FILE gives its output file when none is specified.")

(defun %ftype-string (ft) (if (consp ft) (second ft) ft))

(defun %find-typed-file (pathname types)
  "Return the namestring of the first existing file obtained by giving PATHNAME
   each of TYPES, or NIL.  (:case-insensitive t) entries fall back to upper- and
   lower-case variants — adequate on the common case-insensitive file systems
   and a best effort on case-sensitive ones.)"
  (let ((dir  (directory-namestring pathname))
        (name (or (pathname-name pathname) "")))
    (dolist (ft types nil)
      (let* ((ts (%ftype-string ft))
             (candidates (if (consp ft)
                             (list ts (string-downcase ts) (string-upcase ts))
                             (list ts))))
        (dolist (c candidates)
          (let ((ns (concatenate 'string dir name "." c)))
            (when (probe-file ns) (return-from %find-typed-file ns))))))))

(defun %find-source-file (pathname)
  "Namestring of the first existing source sibling of PATHNAME, or NIL."
  (%find-typed-file pathname *accepted-source-file-types*))

(defun %find-compiled-file (pathname)
  "Namestring of the first existing compiled sibling of PATHNAME, or NIL."
  (%find-typed-file pathname *accepted-compiled-file-types*))

(defun %known-file-type-p (type types)
  "True if the string TYPE matches one of the accepted file TYPES.  Plain
   (case-sensitive) entries are matched with STRING=, (:case-insensitive ..)
   entries with STRING-EQUAL."
  (and type (stringp type)
       (some (lambda (ft)
               (let ((s (%ftype-string ft)))
                 (if (consp ft) (string-equal type s) (string= type s))))
             types)))

(defun %explicit-type-p (pathname)
  "True if PATHNAME carries an explicit, non-empty file type."
  (let ((tp (pathname-type pathname)))
    (and tp (stringp tp) (plusp (length tp)))))

;;; --- MACROEXPAND-1 / MACROEXPAND / MACROEXPAND-ALL -------------------
;;; bocl stores a global macro's expander on the symbol plist ($MACRO); the
;;; expander takes the macro call's arguments (the cdr of the form).
(defun macroexpand-1 (form &optional environment)
  "Expand FORM once if it is a macro call; returns (values expansion expanded-p)."
  (declare (ignore environment))
  (if (and (consp form) (symbolp (car form)) (macro-function (car form)))
      (values (apply (macro-function (car form)) (rest form)) t)
      (values form nil)))

(defun macroexpand (form &optional environment)
  "Repeatedly MACROEXPAND-1 FORM until it is no longer a macro call."
  (multiple-value-bind (exp expanded) (macroexpand-1 form environment)
    (if expanded (values (macroexpand exp environment) t) (values form nil))))

(defun macroexpand-all (form &optional environment)
  "Recursively expand every macro call in FORM, returning a macro-free form.
   This is the core of bocl's minimal compilation (CLHS 3.2.2.2): the result
   contains only special operators and function calls, no macros.  Quoted data,
   binding-form variable lists and (THE type ...) type specifiers are left
   untouched; all evaluated subforms are walked."
  (declare (ignore environment))
  (cond
    ((atom form) form)
    ((eq (car form) 'quote) form)
    ((and (symbolp (car form)) (macro-function (car form)))
     (macroexpand-all (macroexpand-1 form)))
    ((eq (car form) 'function)
     (if (and (consp (cdr form)) (consp (cadr form)) (eq (car (cadr form)) 'lambda))
         (list 'function (cons 'lambda (cons (cadr (cadr form))
                                             (mapcar #'macroexpand-all (cddr (cadr form))))))
         form))
    ((eq (car form) 'lambda)
     (cons 'lambda (cons (cadr form) (mapcar #'macroexpand-all (cddr form)))))
    ((member (car form) '(let let*))
     (list* (car form)
            (mapcar (lambda (b)
                      (if (consp b) (list (car b) (macroexpand-all (cadr b))) b))
                    (cadr form))
            (mapcar #'macroexpand-all (cddr form))))
    ((eq (car form) 'setq)
     (cons 'setq
           (let ((r '()) (l (cdr form)))
             (loop (when (null l) (return (nreverse r)))
                   (push (car l) r)                       ; variable, untouched
                   (push (macroexpand-all (cadr l)) r)    ; value form, walked
                   (setq l (cddr l))))))
    ((eq (car form) 'the)
     (list 'the (cadr form) (macroexpand-all (caddr form))))
    ((member (car form) '(flet labels))
     ;; (flet/labels ((name lambda-list body...) ...) body...): keep each name
     ;; and lambda-list, walk the function bodies and the main body.
     (list* (car form)
            (mapcar (lambda (b)
                      (if (and (consp b) (consp (cdr b)))
                          (list* (car b) (cadr b) (mapcar #'macroexpand-all (cddr b)))
                          b))
                    (cadr form))
            (mapcar #'macroexpand-all (cddr form))))
    ((eq (car form) 'macrolet)
     ;; (macrolet ((name macro-lambda-list body...) ...) body...): keep each local
     ;; macro definition (walking its expander body, leaving the macro lambda list),
     ;; and walk the main body.  The special operator is preserved so the evaluator
     ;; performs the LEXICAL local-macro expansion (eval.c) — this understands the
     ;; header instead of mishandling it, and keeps shadowing correct without
     ;; tracking a local-macro environment in the walker itself.
     (list* 'macrolet
            (mapcar (lambda (d)
                      (if (and (consp d) (consp (cdr d)))
                          (list* (car d) (cadr d) (mapcar #'macroexpand-all (cddr d)))
                          d))
                    (cadr form))
            (mapcar #'macroexpand-all (cddr form))))
    ((eq (car form) 'symbol-macrolet)
     ;; (symbol-macrolet ((symbol expansion) ...) body...): keep the definitions,
     ;; walk the body; the evaluator expands the symbol-macros lexically (and knows
     ;; when an inner binding shadows one), which the walker would have to re-derive.
     (list* 'symbol-macrolet
            (cadr form)
            (mapcar #'macroexpand-all (cddr form))))
    (t                                                    ; special op or call
     (cons (car form) (mapcar #'macroexpand-all (cdr form))))))

;;; --- COMPILE (issue minimal-compilation) ------------------------------
;;; Minimal compilation (CLHS 3.2.2): COMPILE produces an interpreted function
;;; whose body is FULLY MACROEXPANDED — no macro call remains, so the function's
;;; behaviour is fixed at compile time and a later redefinition of a macro it
;;; used does not change it.  This replaces the kernel COMPILE primitive (which
;;; only EVAL'd the lambda, leaving macros to be displaced lazily on first call).
(defun %compile-definition (definition)
  "Coerce DEFINITION (a lambda expression, (function ...) form, or a function)
   to a function whose body has been MACROEXPAND-ALL'd."
  (cond
    ((functionp definition) definition)                  ; already a function
    ((and (consp definition) (eq (car definition) 'lambda))
     ;; Wrap in FUNCTION *before* walking: LAMBDA is itself a macro, so
     ;; MACROEXPAND-ALL of a bare (lambda ...) already yields (function
     ;; (lambda ...)).  Wrapping the walked result in another FUNCTION would
     ;; double-wrap into (function (function (lambda ...))), which the FUNCTION
     ;; special operator rejects as an invalid function name.
     (eval (macroexpand-all (list 'function definition))))
    ((and (consp definition) (eq (car definition) 'function))
     (eval (macroexpand-all definition)))
    (t (eval definition))))

(defun compile (name &optional (definition nil definition-supplied-p))
  "Minimal-compile NAME/DEFINITION, returning (values result warnings-p failure-p).
   (compile nil lambda-expr) -> the (macroexpanded) function;
   (compile nil function)    -> the function unchanged;
   (compile name lambda-expr) -> install the function under NAME, return NAME;
   (compile name)            -> NAME (bocl keeps no source, so the current
                                 definition is taken as already compiled)."
  (let ((result
          (cond
            ((null name)
             (unless definition-supplied-p
               (error "COMPILE: a definition is required when NAME is NIL"))
             (%compile-definition definition))
            ((and (symbolp name) (not definition-supplied-p))
             name)
            ((symbolp name)
             (let ((fn (%compile-definition definition)))
               (when (functionp fn) (setf (symbol-function name) fn))
               name))
            (t (error "COMPILE: NAME must be NIL or a symbol, got ~S" name)))))
    (values result nil nil)))

;;; --- Missing CL functions (issue missing-functions) -------------------
;;; Implemented from the fboundp audit over the 978 external COMMON-LISP
;;; symbols.  Pure functions built on existing kernel primitives.

;; Hyperbolic functions and their inverses (CLHS 12.2).
(defun sinh (x) (/ (- (exp x) (exp (- x))) 2))
(defun cosh (x) (/ (+ (exp x) (exp (- x))) 2))
(defun tanh (x) (let ((e (exp (* 2 x)))) (/ (- e 1) (+ e 1))))
(defun asinh (x) (log (+ x (sqrt (+ (* x x) 1)))))
(defun acosh (x) (log (+ x (* (sqrt (- x 1)) (sqrt (+ x 1))))))
(defun atanh (x) (/ (log (/ (+ 1 x) (- 1 x))) 2))

;; cis / conjugate / phase (CLHS 12.2).
(defun cis (radians)
  "Return e^(i*RADIANS) = (complex (cos radians) (sin radians))."
  (complex (cos radians) (sin radians)))
(defun conjugate (number)
  (if (complexp number)
      (complex (realpart number) (- (imagpart number)))
      number))
(defun phase (number)
  "The angle of NUMBER's polar representation (radians)."
  (if (complexp number)
      (atan (imagpart number) (realpart number))
      (if (minusp number) pi 0)))

;; Bit-test helpers (CLHS 12.2).
(defun logtest (integer1 integer2)
  (not (zerop (logand integer1 integer2))))
(defun ldb-test (bytespec integer)
  (not (zerop (ldb bytespec integer))))

;; Float division operators (CLHS 12.2) — like floor/ceiling/round/truncate but
;; the quotient is returned as a float.
(defun ffloor (number &optional (divisor 1))
  (multiple-value-bind (q r) (floor number divisor) (values (float q) r)))
(defun fceiling (number &optional (divisor 1))
  (multiple-value-bind (q r) (ceiling number divisor) (values (float q) r)))
(defun fround (number &optional (divisor 1))
  (multiple-value-bind (q r) (round number divisor) (values (float q) r)))
(defun ftruncate (number &optional (divisor 1))
  (multiple-value-bind (q r) (truncate number divisor) (values (float q) r)))

;; Misc predicates / accessors.
(defun constantp (form &optional environment)
  "True for self-evaluating objects, QUOTE forms, keywords, and T/NIL."
  (declare (ignore environment))
  (or (keywordp form) (numberp form) (stringp form) (characterp form)
      (eq form t) (eq form nil) (vectorp form)
      (and (consp form) (eq (car form) 'quote))))
(defun compiled-function-p (object)
  "bocl interprets; no function is a loaded compiled-function."
  (declare (ignore object))
  nil)
(defun function-lambda-expression (function)
  "Return (values lambda-expression closure-p name) for FUNCTION (CLHS 3.8).  bocl
   is an interpreter, so an ordinary FUNCTION object (or a closure over one) still
   holds its lambda list and body — already MACROEXPAND-ALL'd for a COMPILE'd
   function — from which (lambda <ll> . <body>) is reconstructed.  A built-in
   function has no lambda expression, so LAMBDA-EXPRESSION is NIL there."
  (multiple-value-bind (available lambda-list body name closurep)
      (%function-lambda-body function)
    (if available
        (values (list* 'lambda lambda-list body) closurep name)
        (values nil t name))))

(defun disassemble (fn)
  "bocl is an interpreter, not a native compiler.  DISASSEMBLE writes the
   function's lambda expression (its macroexpanded source, via FUNCTION-LAMBDA-
   EXPRESSION) to *STANDARD-OUTPUT* when one is available, and nothing when it is
   not (e.g. a kernel built-in).  FN is a function designator or a lambda
   expression (which is compiled first).  Returns NIL (CLHS 25.1)."
  (let* ((f    (cond ((and (consp fn) (eq (car fn) 'lambda)) (compile nil fn))
                     ((and (symbolp fn) (fboundp fn))        (symbol-function fn))
                     (t                                       fn)))
         (expr (function-lambda-expression f)))
    (when expr
      (fresh-line)
      (prin1 expr)
      (terpri))
    nil))

;;; --- DEFUN as a macro (issue def-forms-macros-and-lexical-blocks) ----------
;;; DEFUN is an ANSI macro, not a special operator.  bocl open-codes it in the
;;; evaluator (eval.c) for bootstrap + speed — the special-form dispatch runs
;;; before macro dispatch, so eval keeps using the C DEFUN — but installing this
;;; macro-function makes MACROEXPAND / MACROEXPAND-ALL / a code walker / the fasl
;;; treat it as the macro it is.  The expansion produces the SAME shape the C
;;; DEFUN builds (make_block_body): a name-preserving install of
;;; (lambda lambda-list <decls+doc> (block name . forms)), via the C helper
;;; BOCL:DEFINE-FUNCTION (lisp_name = name), so eval and macroexpand agree.
(defun %parse-body (body)
  "Split a function BODY into (values declarations docstring forms): DECLARATIONS
   is the leading (declare ...) forms, DOCSTRING an optional documentation string
   (a string counts only when at least one more form follows — a lone trailing
   string is a return value), FORMS the rest.  Declarations and a docstring may
   interleave at the head (CLHS 3.4.11).  Mirrors the C make_block_body split so
   the DEFUN macro and the interpreter's DEFUN agree; declarations stay OUTSIDE the
   lexical BLOCK (which would otherwise evaluate a declare as a call)."
  (let ((decls '()) (doc nil) (doc-seen nil) (rest body))
    (loop
      (cond ((and (consp rest) (consp (car rest)) (eq (caar rest) 'declare))
             (push (car rest) decls) (setf rest (cdr rest)))
            ((and (not doc-seen) (consp rest) (stringp (car rest)) (consp (cdr rest)))
             (setf doc (car rest) doc-seen t) (setf rest (cdr rest)))
            (t (return))))
    (values (nreverse decls) doc rest)))

(defmacro defun (name lambda-list &rest body)
  (multiple-value-bind (decls doc forms) (%parse-body body)
    (if (and (consp name) (eq (car name) 'setf)
             (consp (cdr name)) (symbolp (cadr name)) (null (cddr name)))
        ;; (defun (setf foo) (val arg...) ...) — register a SETF function for FOO;
        ;; the implicit block is named FOO (CLHS 3.1.3).  Handled at the Lisp level
        ;; because the kernel names functions with symbols only.  A docstring stays
        ;; in the body (setf functions carry no separate documentation).
        (let ((sym (cadr name)))
          `(%register-setf-function ',sym
             (function (lambda ,lambda-list ,@decls ,@(and doc (list doc))
                         (block ,sym ,@forms)))))
        ;; The docstring is EXTRACTED and passed to BOCL:DEFINE-FUNCTION, which
        ;; records it as (documentation name 'function) — matching the C DEFUN.
        `(bocl:define-function ',name ,doc
           (function (lambda ,lambda-list ,@decls (block ,name ,@forms)))))))

;; File / time helpers.
(defun truename (filespec)
  (or (probe-file filespec)
      (error "TRUENAME: file ~S does not exist" filespec)))
(defun file-author (pathspec)
  "bocl does not record file authors."
  (declare (ignore pathspec))
  nil)
(defun get-decoded-time ()
  "Decode the current time (CLHS 25.1.4.1)."
  (decode-universal-time (get-universal-time)))
(defun clear-output (&optional stream)
  "No buffered partial output to discard in bocl."
  (declare (ignore stream))
  nil)

;;; --- compile-file-pathname now honours *default-compiled-file-type* --
(defun compile-file-pathname (input-file &key output-file &allow-other-keys)
  "Pathname COMPILE-FILE writes to: the source pathname with its type changed to
   *DEFAULT-COMPILED-FILE-TYPE* (default \"fasl\"), unless OUTPUT-FILE is given."
  (or output-file
      (let* ((in  (pathname input-file))
             (dir (directory-namestring in))
             (nm  (or (pathname-name in) "")))
        (pathname (concatenate 'string dir nm "." *default-compiled-file-type*)))))

;;;; THE END OF SLICE 44

;;; --- COMPILE-FILE -----------------------------------------------------
;;; (issues: compile-file-output, minimal-compilation)
;;; bocl is an interpreter, but COMPILE-FILE writes a real fast-load file: a
;;; sequence of MACROEXPAND-ALL'd (macro-free) top-level forms, headed by a
;;; ";; BOCL <version> FASL" line that identifies it to file(1) and LOAD.  A fasl
;;; is ordinary Lisp — LOAD reads it with the normal source loader (the header is
;;; a comment) — so minimal compilation = macroexpansion (CLHS 3.2.2.2): loading
;;; it needs no macro expansion.  COMPILE-FILE also performs the compile-time
;;; processing CLHS 3.2.3.1 requires: (EVAL-WHEN (:COMPILE-TOPLEVEL) ...) bodies
;;; run at compile time, and the forms that shape the compilation environment
;;; (IN-PACKAGE / DEFPACKAGE / DEFMACRO / DEFINE-SYMBOL-MACRO) run now too so the
;;; remaining forms are read, printed and expanded in the right package with the
;;; file's own macros known.
(defvar *bocl-fasl-header-prefix* ";; BOCL "
  "First-line prefix of a bocl fast-load file, followed by the producing
   LISP-IMPLEMENTATION-VERSION and \" FASL\".  A Lisp line comment, so LOAD's
   source loader skips it; file(1) and LOAD use it to recognise a bocl fasl and
   check version compatibility (see %BOCL-FASL-VERSION).")

(defun %compile-file-toplevel (form)
  "Perform COMPILE-FILE's compile-time effects for one top-level FORM (CLHS
   3.2.3.1): PROGN splices into several top-level forms; an
   (EVAL-WHEN situations . body) whose SITUATIONS name :COMPILE-TOPLEVEL (or the
   deprecated COMPILE) has its body evaluated NOW; and IN-PACKAGE / DEFPACKAGE /
   DEFMACRO / DEFINE-SYMBOL-MACRO are evaluated NOW so later forms are read and
   MACROEXPAND-ALL'd in the correct package with the file's macros defined.
   Ordinary forms and EVAL-WHEN with only :LOAD-TOPLEVEL / :EXECUTE are left for
   LOAD, which evaluates the fasl form itself."
  (let ((form (macroexpand form)))              ; a top-level macro may yield PROGN/EVAL-WHEN
    (when (consp form)
      (cond
        ((eq (car form) 'progn)
         (dolist (f (cdr form)) (%compile-file-toplevel f)))
        ((eq (car form) 'eval-when)
         (let ((situations (cadr form)))
           (when (or (member :compile-toplevel situations) (member 'compile situations))
             (dolist (f (cddr form)) (eval f)))))
        ((member (car form) '(in-package defpackage defmacro define-symbol-macro))
         (eval form))
        (t nil)))))

(defun %write-fasl-form (form stream readtable)
  "Write one top-level FORM to a fasl STREAM as a macro-free form: MACROEXPAND-ALL
   it, then PRIN1 under a controlled I/O syntax so it reads back identically —
   WITH-STANDARD-IO-SYNTAX (canonical *PRINT-* values, incl. no *PRINT-LEVEL* /
   *PRINT-LENGTH* truncation) plus the standard READTABLE, *PRINT-CIRCLE* on (so a
   symbol shared across an expansion — e.g. a gensym — round-trips as one object)
   and *READ-DEFAULT-FLOAT-FORMAT* DOUBLE-FLOAT (bocl's only float type, so floats
   print without an exponent marker).  *PACKAGE* is held at the compile-time
   package so symbols qualify relative to the fasl's IN-PACKAGE sequence.  READTABLE
   is a pre-made standard readtable (shared across the file; PRIN1 never mutates
   it).  Returns T on success, NIL if the form could not be macroexpanded (then
   written verbatim)."
  (multiple-value-bind (expanded ok)
      (handler-case (values (macroexpand-all form) t)
        (error () (values form nil)))
    (let ((pkg *package*))
      (with-standard-io-syntax
        (let ((*package*                   pkg)
              (*readtable*                 readtable)
              (*print-circle*              t)
              (*read-default-float-format* 'double-float))
          (prin1 expanded stream)
          (terpri stream))))
    ok))

(defun compile-file (input-file &key output-file &allow-other-keys)
  "Minimal-compile INPUT-FILE to a fast-load file and return its truename.  Each
   top-level form is read; its compile-time effects fire (%COMPILE-FILE-TOPLEVEL —
   EVAL-WHEN :COMPILE-TOPLEVEL, IN-PACKAGE, DEFMACRO, ...), then it is
   MACROEXPAND-ALL'd and written to OUTPUT-FILE (default: source with type
   *DEFAULT-COMPILED-FILE-TYPE*).  The file begins with a ';; BOCL <version> FASL'
   header and an IN-PACKAGE pinning the load package; LOAD reads it with the
   ordinary source loader.  A compile-time IN-PACKAGE is contained by the
   *PACKAGE* rebinding.  Returns (values output-truename warnings-p failure-p):
   warnings-p is always NIL; failure-p is T if some form could not be
   macroexpanded (it was written verbatim)."
  (let* ((merged (merge-pathnames input-file *default-pathname-defaults*))
         ;; Select the actual source file (issue lisp-file-types pseudocode):
         ;; an explicit type or an existing file is used as is; otherwise search
         ;; the accepted source file types.
         (actual (cond
                   ((%explicit-type-p merged)      (namestring merged))
                   ((probe-file (namestring merged)) (namestring merged))
                   ((%find-source-file merged))
                   (t (error "COMPILE-FILE: no source file for ~S"
                             (namestring merged)))))
         (out     (compile-file-pathname actual :output-file output-file))
         (out-ns  (namestring out))
         (*package*   *package*)                ; contain a compile-time IN-PACKAGE
         (*readtable* *readtable*)              ; the SOURCE is read with this readtable
         (std-rt   (copy-readtable nil))        ; the FASL is written with the standard one
         (failure nil))
    (with-open-file (src actual :direction :input)
      (with-open-file (dst out-ns :direction :output
                                  :if-exists :supersede :if-does-not-exist :create)
        ;; Identifying header (a comment: the source loader skips it) + provenance.
        (format dst "~A~A FASL~%" *bocl-fasl-header-prefix* (lisp-implementation-version))
        (format dst ";;; minimal-compiled (macroexpanded) forms.  Source: ~A~%" actual)
        ;; Pin the load package to the compile-time package, in case the source
        ;; carries no IN-PACKAGE of its own.  (Written under standard syntax too.)
        (%write-fasl-form (list 'in-package (package-name *package*)) dst std-rt)
        (let ((eof '#:eof))
          (loop
            ;; READ the source under the caller's *READTABLE* (it may use custom
            ;; read syntax); WRITE the fasl under the standard readtable (std-rt).
            (let ((form (read src nil eof)))
              (when (eq form eof) (return))
              (%compile-file-toplevel form)               ; compile-time effects
              (unless (%write-fasl-form form dst std-rt)   ; macroexpand + emit
                (setq failure t)))))))
    (values (truename out) nil failure)))

(defun %bocl-fasl-version (path)
  "If the file at PATH begins with a bocl fasl header
   (\";; BOCL <version> FASL\"), return the <version> string; else NIL.  Lets LOAD
   (and external tools) recognise a bocl fast-load file and read its version."
  (ignore-errors
    (with-open-file (s path :direction :input :if-does-not-exist nil)
      (when s
        (let ((line   (read-line s nil nil))
              (prefix *bocl-fasl-header-prefix*))
          (when (and line
                     (>= (length line) (length prefix))
                     (string= prefix line :end2 (length prefix)))
            (let* ((start (length prefix))
                   (end   (search " FASL" line :start2 start)))
              (when (and end (> end start))
                (subseq line start end)))))))))

(defun %check-fasl-version (path)
  "Warn if PATH is a bocl fasl produced by a newer MAJOR version than this build
   (its macro-free forms may use constructs this runtime lacks).  Returns the
   fasl version string, or NIL when PATH carries no bocl fasl header."
  (let ((v (%bocl-fasl-version path)))
    (when v
      (flet ((major (s) (ignore-errors (parse-integer s :end (position #\. s)))))
        (let ((fasl-major (major v))
              (my-major   (major (lisp-implementation-version))))
          (when (and fasl-major my-major (> fasl-major my-major))
            (warn "LOAD: fast-load file ~A was produced by bocl ~A, newer than ~
                   this ~A — loading anyway."
                  path v (lisp-implementation-version))))))
    v))

;;; WITH-COMPILATION-UNIT (CLHS 3.2.2.3 / macro).  Its purpose is to batch
;;; deferred compiler warnings across an extent.  bocl interprets and defers no
;;; warnings, so this just evaluates BODY; the OPTIONS plist (e.g. :OVERRIDE) is
;;; accepted and ignored.  ASDF wraps its PERFORM plan in WITH-COMPILATION-UNIT.
(defmacro with-compilation-unit ((&rest options) &body body)
  (declare (ignore options))
  `(progn ,@body))

;;; ---- Multi-dimensional arrays (ranks 0..array-rank-limit-1 = 0..7) -------
;;; Rank-1 arrays stay kernel vectors (so VECTORP / SVREF and the existing code
;;; keep working).  Ranks 0 and >=2 are an %MD-ARRAY: a flat row-major storage
;;; vector plus the dimension list.  AREF / MAKE-ARRAY / ARRAY-DIMENSIONS etc.
;;; dispatch on the representation; the rank-1 path delegates to the kernel.

(defvar *%kernel-aref*             (symbol-function 'aref))
(defvar *%kernel-aref-set*         (symbol-function 'aref-set))
(defvar *%kernel-array-dimensions* (symbol-function 'array-dimensions))
(defvar *%kernel-arrayp*           (symbol-function 'arrayp))

;; %md-array is the representation for every "complex" array — multi-dimensional,
;; adjustable, or displaced — of any element type.  Simple 1-D vectors/strings/
;; bit-vectors stay kernel objects.
(defstruct (%md-array (:predicate %md-array-p) (:copier nil))
  dimensions            ; list of dimension sizes
  storage               ; flat row-major storage (simple-vector / string / bit-vector); NIL if displaced
  (element-type t)      ; canonical element type: T / CHARACTER / BIT
  (fill-pointer nil)    ; integer fill pointer, or NIL
  (displaced-to nil)    ; the array this one is displaced onto, or NIL
  (displaced-offset 0)  ; index offset into the target's row-major storage
  (adjustable nil))     ; T if adjustable

(defun %make-vector-filled (n initial-element)
  (let ((lst nil)) (dotimes (i n) (push initial-element lst)) (apply #'vector lst)))

;; Flat storage access dispatches on the storage kind: strings hold octets read
;; with CHAR, bit-vectors with BIT, everything else with SVREF.  (SVREF would
;; misread octet storage as object references.)
(defun %storage-ref (storage index)
  (cond ((stringp storage) (char storage index))
        ((bit-vector-p storage) (bit storage index))
        (t (svref storage index))))
(defun %storage-set (storage index value)
  (cond ((stringp storage) (char-set storage index value))
        ((bit-vector-p storage) (bit-set storage index value))
        (t (setf (svref storage index) value)))
  value)

;; Row-major element access on a %md-array, FOLLOWING displacement: a displaced
;; array reads/writes its target's storage at (displaced-offset + index).
(defun %md-rmref (array index)
  (let ((target (%md-array-displaced-to array)))
    (if target
        (row-major-aref target (+ (%md-array-displaced-offset array) index))
        (%storage-ref (%md-array-storage array) index))))
(defun %md-rmset (array index value)
  (let ((target (%md-array-displaced-to array)))
    (if target
        (setf (row-major-aref target (+ (%md-array-displaced-offset array) index)) value)
        (%storage-set (%md-array-storage array) index value)))
  value)

;; Canonical element type (T / CHARACTER / BIT) for storage selection.
(defun %canon-element-type (element-type)
  (cond ((member element-type '(character base-char standard-char extended-char)) 'character)
        ((eq element-type 'bit) 'bit)
        (t t)))

;; Make + fill flat storage of TOTAL elements of (canonical) ELEMENT-TYPE.
(defun %make-typed-storage (element-type total initial-element initial-contents rank)
  (let ((storage (cond ((eq element-type 'character)
                        (make-string total :initial-element
                                     (if (characterp initial-element) initial-element #\Space)))
                       ((eq element-type 'bit)
                        (make-bit-vector total (if (integerp initial-element) initial-element 0)))
                       (t (%make-vector-filled total initial-element)))))
    (when initial-contents
      (let ((flat (if (<= rank 1) (coerce initial-contents 'list) (%md-flatten initial-contents rank)))
            (i 0))
        (dolist (e flat) (when (< i total) (%storage-set storage i e) (incf i)))))
    storage))

;; Decode a row-major index back to its subscript list for an array of DIMS.
(defun %decode-subscripts (rm dims)
  (let ((subs nil) (r rm))
    (dolist (d (reverse dims) subs)
      (push (mod r d) subs)
      (setq r (floor r d)))))

;; Copy elements at coinciding SUBSCRIPTS (not row-major positions) from OLD
;; (its current storage) into NEW-STORAGE — the element-preserving rule of
;; ADJUST-ARRAY when dimensions change.
(defun %array-copy-overlap (old old-dims new-storage new-dims)
  (let* ((overlap (mapcar #'min old-dims new-dims))
         (otot    (reduce #'* overlap :initial-value 1)))
    (dotimes (k otot)
      (let ((subs (%decode-subscripts k overlap)))
        (%storage-set new-storage (%md-row-major-index new-dims subs)
                      (row-major-aref old (%md-row-major-index old-dims subs)))))))

(defun %md-row-major-index (dimensions subscripts)
  (let ((index 0) (ss subscripts))
    (dolist (d dimensions)
      (when (null ss) (error "AREF: too few subscripts"))
      (unless (and (integerp (car ss)) (<= 0 (car ss)) (< (car ss) d))
        (error "AREF: subscript ~S out of range for dimension ~S" (car ss) d))
      (setq index (+ (* index d) (car ss)) ss (cdr ss)))
    (when ss (error "AREF: too many subscripts"))
    index))

(defun %md-dims-from-contents (contents rank)
  (let ((dims nil) (c contents))
    (dotimes (i rank)
      (let ((l (coerce c 'list))) (push (length l) dims) (setq c (if l (car l) nil))))
    (nreverse dims)))

(defun %md-flatten (contents rank)
  (if (<= rank 1)
      (coerce contents 'list)
      (let ((acc nil))
        (dolist (sub (coerce contents 'list)) (setq acc (append acc (%md-flatten sub (1- rank)))))
        acc)))

(defun %fill-pointer-value (fill-pointer size)
  "Resolve the :FILL-POINTER make-array argument to an integer fill pointer
for a vector of SIZE elements, or NIL when no fill pointer was requested.
T means \"the whole vector is active\"; an integer is taken verbatim."
  (cond ((null fill-pointer) nil)
        ((eq fill-pointer t)  size)
        ((integerp fill-pointer)
         (unless (<= 0 fill-pointer size)
           (error "MAKE-ARRAY: fill pointer ~S out of range for size ~S"
                  fill-pointer size))
         fill-pointer)
        (t size)))

(defun make-array (dimensions &key initial-element initial-contents
                                   element-type adjustable fill-pointer
                                   displaced-to displaced-index-offset)
  (let* ((dims (if (listp dimensions) dimensions (list dimensions)))
         (rank (length dims))
         (total (reduce #'* dims :initial-value 1))
         (et   (%canon-element-type element-type))
         (fp   (and (= rank 1) (%fill-pointer-value fill-pointer (car dims)))))
    (cond
      ;; Displaced array: shares the target's storage; element access redirects
      ;; to the target at the given offset.  Any rank, any element type.
      (displaced-to
       (make-%md-array :dimensions dims :storage nil :element-type et :fill-pointer fp
                       :displaced-to displaced-to
                       :displaced-offset (or displaced-index-offset 0)
                       :adjustable (and adjustable t)))
      ;; Adjustable array: a %md-array wrapper (any rank) with its own typed
      ;; storage, so ADJUST-ARRAY can resize it in place, preserving identity.
      (adjustable
       (make-%md-array :dimensions dims
                       :storage (%make-typed-storage et total initial-element initial-contents rank)
                       :element-type et :fill-pointer fp :adjustable t))
      ;; A 1-D array of characters is a STRING.  With a fill pointer it is a
      ;; non-simple string: the storage holds CAR DIMS chars and the fill
      ;; pointer (registered below) bounds the active portion.
      ((and (= rank 1)
            (member element-type '(character base-char standard-char extended-char)))
       (let ((s (make-string (car dims)
                             :initial-element (if (characterp initial-element)
                                                  initial-element #\Space))))
         (when initial-contents
           (let ((i 0))
             (dolist (e (coerce initial-contents 'list))
               (when (< i (car dims)) (char-set s i e) (incf i)))))
         (when fp (%set-fill-pointer s fp))
         s))
      ;; A 1-D array of bits is a BIT-VECTOR (a real BIT_VECTOR object, so
      ;; BIT-VECTOR-P / TYPE-OF / the #* printer all see it correctly).
      ((and (= rank 1) (eq element-type 'bit))
       (let ((b (make-bit-vector (car dims)
                                 (if (integerp initial-element) initial-element 0))))
         (when initial-contents
           (let ((i 0))
             (dolist (e (coerce initial-contents 'list))
               (when (< i (car dims)) (setf (bit b i) e) (incf i)))))
         (when fp (%set-fill-pointer b fp))
         b))
      ((= rank 1)
       (let ((v (%make-vector-filled (car dims) initial-element)))
         (when initial-contents
           (let ((i 0)) (dolist (e (coerce initial-contents 'list)) (setf (svref v i) e) (incf i))))
         (when fp (%set-fill-pointer v fp))
         v))
      (t
       ;; A multi-dimensional :ELEMENT-TYPE BIT array stores its bits in a
       ;; bit-vector (so AREF/ROW-MAJOR-AREF read/write real bits, and the bit-*
       ;; ops can operate on the flat storage); other element types use a
       ;; simple-vector.
       (let ((storage (if (eq element-type 'bit)
                          (make-bit-vector total (if (integerp initial-element) initial-element 0))
                          (%make-vector-filled total initial-element))))
         (when initial-contents
           (let ((flat (if (= rank 0) (list initial-contents) (%md-flatten initial-contents rank)))
                 (i 0))
             (dolist (e flat) (%storage-set storage i e) (incf i))))
         (make-%md-array :dimensions dims :storage storage :element-type et))))))

(defun arrayp (x) (or (%md-array-p x) (funcall *%kernel-arrayp* x)))

(defun array-dimensions (array)
  (if (%md-array-p array) (copy-list (%md-array-dimensions array))
      (funcall *%kernel-array-dimensions* array)))

(defun array-dimension (array axis-number)
  "The length of dimension AXIS-NUMBER of ARRAY."
  (nth axis-number (array-dimensions array)))

(defun aref (array &rest subscripts)
  (if (%md-array-p array)
      (%md-rmref array (%md-row-major-index (%md-array-dimensions array) subscripts))
      (apply *%kernel-aref* array subscripts)))

(defun aref-set (value array &rest subscripts)
  (if (%md-array-p array)
      (%md-rmset array (%md-row-major-index (%md-array-dimensions array) subscripts) value)
      (apply *%kernel-aref-set* value array subscripts)))

;; (SETF (AREF array sub...) value) — variadic; the value is the last arg the
;; setf machinery appends after the place subscripts.
(sethash 'aref *setf-expanders*
  (lambda (array &rest subs-and-val)
    (let* ((rev (reverse subs-and-val)) (val (car rev)) (subs (reverse (cdr rev))))
      `(aref-set ,val ,array ,@subs))))

;; (SETF (FILL-POINTER vector) value) — moves the active boundary of VECTOR.
(sethash 'fill-pointer *setf-expanders*
  (lambda (vector val) `(%set-fill-pointer-checked ,vector ,val)))

(defun row-major-aref (array index)
  ;; Strings and bit-vectors are vector subtypes whose storage is octets, so
  ;; SVREF would misread them — use CHAR / BIT.  %md-arrays follow displacement.
  (cond ((%md-array-p array)   (%md-rmref array index))
        ((stringp array)       (char array index))
        ((bit-vector-p array)  (bit array index))
        (t                     (svref array index))))

(define-setf-expander row-major-aref (array index) (val)
  `(row-major-aref-set ,val ,array ,index))
(defun row-major-aref-set (value array index)
  (cond ((%md-array-p array)   (%md-rmset array index value))
        ((stringp array)       (char-set array index value) value)
        ((bit-vector-p array)  (bit-set array index value) value)
        (t                     (setf (svref array index) value) value)))

;; ---- Aggregate #nA(...) printing for multidimensional / adjustable / displaced
;;      arrays (a %md-array is opaque to the kernel printer, which otherwise emits
;;      #<%MD-ARRAY …>).  The kernel printer (printer.c) calls this via
;;      %MD-ARRAY-PRINT-STRING and prints the returned string; a NIL return means
;;      "use the #<…> fallback" — i.e. *PRINT-ARRAY* is NIL or A is not a
;;      %md-array.  ESCAPE selects PRIN1 (T) vs PRINC (NIL) for each element.
(defun %md-array-print-string (a escape base)
  ;; BASE is the *PRINT-LEVEL* depth the kernel printer held when it reached A
  ;; (A's own nesting level).  When that already meets the *PRINT-LEVEL* limit the
  ;; whole array abbreviates to "#" (like any too-deep composite); otherwise emit
  ;; the aggregate form, then restore the kernel depth counter to BASE so callers
  ;; that keep printing around A (e.g. a list element after A) are unaffected.
  (when (and (%md-array-p a) *print-array*)
    ;; *PRINT-READABLY* forces *PRINT-LEVEL* to be ignored (CLHS 22.1.3.11).
    (let ((pl (and (not *print-readably*) *print-level*)))
      (if (and pl (>= base pl))
          "#"
          (prog1 (with-output-to-string (s)
                   ;; *PRINT-CIRCLE*: the array itself may be shared (e.g. a
                   ;; self-referential array).  A positive action is a back-ref
                   ;; (#n#); a negative one prefixes #n= before the aggregate.
                   (let ((act (and (%circle-active-p) (%circle-action a))))
                     (cond
                       ((and act (plusp act)) (format s "#~D#" act))
                       (t (when (and act (minusp act)) (format s "#~D=" (- act)))
                          (%md-emit a escape s (list a) base)))))
            (%set-print-level-depth base))))))

(defun %md-emit (a escape s seen base)
  ;; Emit the aggregate printed form of %md-array A to stream S.  Each dimension
  ;; is one *PRINT-LEVEL* level beyond BASE: A's first-dimension slices print at
  ;; level BASE+1 and the innermost scalar elements at level BASE+RANK.  A
  ;; slice/composite-element at level >= *PRINT-LEVEL* abbreviates to "#";
  ;; *PRINT-LENGTH* truncates each dimension after that many items with " ...".
  ;; SEEN is the stack of arrays currently being printed (a lexical cycle guard —
  ;; bocl has no #n=/#n# array-circle printing yet).  Nested %md-array elements
  ;; recurse here; every other element delegates to the kernel PRIN1 (escape) /
  ;; PRINC, after syncing the kernel depth counter to the element's level so a
  ;; list/vector element abbreviates at the correct depth.
  (let ((dims (%md-array-dimensions a))
        (et   (%md-array-element-type a)))
    (cond
      ;; A CHARACTER array (adjustable/displaced/fill-pointer string) prints as a
      ;; string of its active (fill-pointer-bounded) elements.
      ((eq et 'character)
       (let* ((fp  (%md-array-fill-pointer a))
              (n   (or fp (reduce #'* dims :initial-value 1)))
              (str (make-string n)))
         (dotimes (i n) (setf (char str i) (row-major-aref a i)))
         (if escape (prin1 str s) (princ str s))))
      (t
       ;; *PRINT-READABLY* forces *PRINT-LEVEL*/*PRINT-LENGTH* off (CLHS 22.1.3.11).
       (let ((idx 0) (rank (length dims))
             (plevel (and (not *print-readably*) *print-level*))
             (plen   (and (not *print-readably*) *print-length*)))
         (labels ((stride (ds) (if (null ds) 1 (reduce #'* ds :initial-value 1)))
                  (emit-scalar (level)
                    (let ((e (row-major-aref a idx)))
                      (incf idx)
                      (cond
                        ((%md-array-p e)
                         (cond
                           ;; too deep for *PRINT-LEVEL* (nil/ignored under readably)
                           ((and plevel (>= level plevel)) (write-char #\# s))
                           (t
                            ;; *PRINT-CIRCLE*: a shared / self-referential sub-array
                            ;; prints as #n# (back-ref) or #n=<array> (first visit).
                            ;; With circle off, fall back to the lexical SEEN guard.
                            (let ((act (and (%circle-active-p) (%circle-action e))))
                              (cond
                                ((and act (plusp act)) (format s "#~D#" act))
                                (t (when (and act (minusp act)) (format s "#~D=" (- act)))
                                   (if (and (not (%circle-active-p))
                                            (member e seen :test #'eq))
                                       (write-string "#<circular-array>" s)
                                       (%md-emit e escape s (cons e seen) level))))))))
                        (t
                         (%set-print-level-depth level)
                         (if escape (prin1 e s) (princ e s))))))
                  (rec (ds level)
                    ;; emit one bracketed group; its direct children are at LEVEL.
                    (write-char #\( s)
                    (let* ((n    (car ds))
                           (rest (cdr ds))
                           (st   (stride rest)))
                      (dotimes (k n)
                        (cond
                          ((and plen (>= k plen))
                           (write-string (if (plusp k) " ..." "...") s)
                           (incf idx (* (- n k) st))   ; skip rest of this dimension
                           (return))
                          (t
                           (when (plusp k) (write-char #\Space s))
                           (if (null rest)
                               (emit-scalar level)
                               (if (and plevel (>= level plevel))
                                   (progn (write-char #\# s) (incf idx st))
                                   (rec rest (1+ level))))))))
                    (write-char #\) s)))
           (cond
             ((= rank 0)                                   ; #0A scalar (no dims)
              (write-string "#0A" s)
              (emit-scalar (1+ base)))
             ((= rank 1)                                   ; #(...) vector syntax
              (write-string "#" s)
              (rec dims (1+ base)))
             (t (format s "#~DA" rank)                     ; #nA(...) for rank >= 2
                (rec dims (1+ base))))))))))

(defun array-row-major-index (array &rest subscripts)
  (%md-row-major-index (array-dimensions array) subscripts))

;; BIT / SBIT accept any number of subscripts (a bit array may be multi-
;; dimensional).  AREF / AREF-SET already route a 1-D bit-vector to the kernel
;; and a multi-dimensional bit %md-array through row-major indexing, so delegate
;; to them — keeping the kernel fast path for a plain 1-D bit-vector.
(defparameter *%kernel-bit*  (symbol-function 'bit))
(defparameter *%kernel-sbit* (symbol-function 'sbit))
(defun bit (bit-array &rest subscripts)
  (if (or (cdr subscripts) (%md-array-p bit-array))
      (apply #'aref bit-array subscripts)
      (funcall *%kernel-bit* bit-array (car subscripts))))
(defun sbit (bit-array &rest subscripts)
  (if (or (cdr subscripts) (%md-array-p bit-array))
      (apply #'aref bit-array subscripts)
      (funcall *%kernel-sbit* bit-array (car subscripts))))
(sethash 'bit *setf-expanders*
  (lambda (array &rest subs-and-val)
    (let* ((rev (reverse subs-and-val)) (val (car rev)) (subs (reverse (cdr rev))))
      `(aref-set ,val ,array ,@subs))))
(sethash 'sbit *setf-expanders*
  (lambda (array &rest subs-and-val)
    (let* ((rev (reverse subs-and-val)) (val (car rev)) (subs (reverse (cdr rev))))
      `(aref-set ,val ,array ,@subs))))

;;; ---- Displaced and adjustable arrays -------------------------------------

(defun array-displacement (array)
  "Two values: the array ARRAY is displaced to (or NIL), and the displacement
offset (0 when not displaced)."
  (if (%md-array-p array)
      (values (%md-array-displaced-to array) (%md-array-displaced-offset array))
      (values nil 0)))

(defun adjust-array (array new-dimensions &key (initial-element nil)
                                               initial-contents fill-pointer
                                               element-type
                                               displaced-to displaced-index-offset)
  "Change the dimensions of ARRAY.  When ARRAY is a %md-array (i.e. adjustable),
it is modified in place and the SAME object is returned; otherwise a fresh
adjustable %md-array is returned.  Without :DISPLACED-TO or :INITIAL-CONTENTS,
elements at subscripts present in both the old and new array are preserved and
the rest take :INITIAL-ELEMENT."
  (let* ((new-dims (if (listp new-dimensions) new-dimensions (list new-dimensions)))
         (rank     (length new-dims))
         (new-total (reduce #'* new-dims :initial-value 1))
         (et       (if element-type
                       (%canon-element-type element-type)
                       (if (%md-array-p array) (%md-array-element-type array) t)))
         (old-dims (array-dimensions array))
         (new-fp   (and fill-pointer (= rank 1) (%fill-pointer-value fill-pointer (car new-dims))))
         ;; Storage / displacement for the result.
         (target   displaced-to)
         (offset   (or displaced-index-offset 0))
         (storage  (cond (displaced-to nil)
                         (initial-contents
                          (%make-typed-storage et new-total initial-element initial-contents rank))
                         (t
                          (let ((s (%make-typed-storage et new-total initial-element nil 0)))
                            (%array-copy-overlap array old-dims s new-dims)
                            s)))))
    (cond
      ((%md-array-p array)
       (setf (%md-array-dimensions array)       new-dims
             (%md-array-storage array)          storage
             (%md-array-element-type array)     et
             (%md-array-displaced-to array)     target
             (%md-array-displaced-offset array) offset)
       (when fill-pointer (setf (%md-array-fill-pointer array) new-fp))
       array)
      (t
       (make-%md-array :dimensions new-dims :storage storage :element-type et
                       :fill-pointer new-fp :adjustable t
                       :displaced-to target :displaced-offset offset)))))

;;; ---- EQUALP on vectors and arrays ----------------------------------------
;;; The kernel EQUALP handles numbers/characters/strings/conses but falls back
;;; to EQUAL (identity) for vectors and arrays.  CLHS requires EQUALP to compare
;;; arrays of the same rank/dimensions element-wise (each pair EQUALP).  Wrap it.
(defvar *%old-equalp* (symbol-function 'equalp))

(defun %equalp-seq (a b n)
  "True if ROW-MAJOR elements [0,N) of arrays A and B are pairwise EQUALP."
  (dotimes (i n t)
    (unless (equalp (row-major-aref a i) (row-major-aref b i))
      (return nil))))

(defun equalp (a b)
  (cond
    ((eq a b) t)
    ;; Conses: recurse with THIS equalp so nested arrays compare by content too.
    ((consp a) (and (consp b) (equalp (car a) (car b)) (equalp (cdr a) (cdr b))))
    ;; Multi-dimensional arrays: same dimensions + element-wise EQUALP.
    ((%md-array-p a)
     (and (%md-array-p b)
          (equal (array-dimensions a) (array-dimensions b))
          (%equalp-seq a b (array-total-size a))))
    ;; Vectors (incl. bit-vectors), but NOT strings — the kernel does those,
    ;; case-insensitively.  A vector is EQUALP only to another non-string vector
    ;; of the same length with element-wise-EQUALP contents.
    ((and (vectorp a) (not (stringp a)))
     (and (vectorp b) (not (stringp b))
          (= (length a) (length b))
          (%equalp-seq a b (length a))))
    ;; Numbers, characters, strings, structures, hash-tables: the kernel EQUALP.
    (t (funcall *%old-equalp* a b))))

;;; ---- #n= / #n# label fixup (shared and circular structure) --------------
;;; A #n# read before its #n= finishes yields a placeholder %READ-REF; once the
;;; #n= object is fully read, %PATCH-READ-LABEL walks it and replaces every
;;; occurrence of that placeholder with the object itself — across conses,
;;; vectors, multi-dimensional arrays, and structures — which is what makes
;;; circular structure work.

(defstruct (%read-ref (:predicate %read-ref-p) (:copier nil)) label)

(defun %patch-read-label (object placeholder replacement)
  "Destructively replace PLACEHOLDER by REPLACEMENT throughout OBJECT, following
   conses, vectors, md-arrays and structure slots; a visited set bounds the walk
   over the (now possibly circular) result."
  (let ((visited (make-hash-table :test 'eq)))
    (labels ((sub (x) (if (eq x placeholder) replacement (progn (walk x) x)))
             (walk (x)
               (unless (or (%read-ref-p x) (gethash x visited))
                 (cond
                   ((consp x)
                    (setf (gethash x visited) t)
                    (setf (car x) (sub (car x)))
                    (setf (cdr x) (sub (cdr x))))
                   ((%md-array-p x)
                    (setf (gethash x visited) t)
                    (let ((s (%md-array-storage x)))
                      (dotimes (i (length s)) (setf (svref s i) (sub (svref s i))))))
                   ((and (vectorp x) (not (stringp x)))
                    (setf (gethash x visited) t)
                    (dotimes (i (length x)) (setf (svref x i) (sub (svref x i)))))
                   ((typep x 'structure-object)
                    (setf (gethash x visited) t)
                    (let ((n (length (gethash (type-of x) *defstruct-slot-specs*))))
                      (dotimes (i n) (structure-set (sub (structure-get x i)) x i))))))))
      (walk object))
    object))

;;; ---- Standard # dispatch reader macros -----------------------------------
;;; Radix (#B #O #X #R), array (#A), structure (#S), labels (#= ##), and the
;;; unreadable marker (#<).  Registered on the standard readtable so they apply
;;; to every subsequent read.  (The character macros and #' #( #\ #* #+ #- #. #:
;;; #P #C #| are handled by the kernel reader.)

(defvar *read-labels* nil
  "Alist of (label . object) for #n= / #n# shared-structure references.")

;; #B / #O / #X — fixed-radix rationals; #R — radix given by the infix argument.
(set-dispatch-macro-character #\# #\B
  (lambda (stream sub arg) (declare (ignore sub arg))
    (let ((*read-base* 2)) (read stream t nil t))))
(set-dispatch-macro-character #\# #\O
  (lambda (stream sub arg) (declare (ignore sub arg))
    (let ((*read-base* 8)) (read stream t nil t))))
(set-dispatch-macro-character #\# #\X
  (lambda (stream sub arg) (declare (ignore sub arg))
    (let ((*read-base* 16)) (read stream t nil t))))
(set-dispatch-macro-character #\# #\R
  (lambda (stream sub arg) (declare (ignore sub))
    (unless (and (integerp arg) (<= 2 arg 36))
      (error "#R requires a radix between 2 and 36, got ~S" arg))
    (let ((*read-base* arg)) (read stream t nil t))))

;; #nA — array of rank N from nested sequences (ranks 0..array-rank-limit-1).
(set-dispatch-macro-character #\# #\A
  (lambda (stream sub arg) (declare (ignore sub))
    (let ((contents (read stream t nil t))
          (rank (or arg 1)))
      (unless (< rank array-rank-limit)
        (error "#~DA: rank exceeds ARRAY-RANK-LIMIT (~D)" rank array-rank-limit))
      (cond ((= rank 0) (make-array nil :initial-contents contents))
            ((= rank 1) (make-array (length contents) :initial-contents contents))
            (t (make-array (%md-dims-from-contents contents rank)
                           :initial-contents contents))))))

;; #S(type slot value ...) — make a structure instance via its constructor
;; MAKE-<type> with keyword initargs from the slot names.
(set-dispatch-macro-character #\# #\S
  (lambda (stream sub arg) (declare (ignore sub arg))
    (let* ((spec (read stream t nil t))
           (type (car spec))
           (ctor (find-symbol (concatenate 'string "MAKE-" (string type))
                              (or (symbol-package type) *package*)))
           (initargs '()))
      (do ((p (cdr spec) (cddr p)))
          ((null p))
        (push (intern (string (car p)) "KEYWORD") initargs)
        (push (cadr p) initargs))
      (unless (and ctor (fboundp ctor))
        (error "#S: ~S is not a structure type" type))
      (apply (symbol-function ctor) (nreverse initargs)))))

;; #n= label / #n# reference — supports shared AND circular structure.
;; #n= registers a placeholder, reads its object (during which a nested #n#
;; yields that placeholder), then patches the placeholder to the object.
(set-dispatch-macro-character #\# #\=
  (lambda (stream sub arg) (declare (ignore sub))
    (unless arg (error "#= requires a label"))
    (let* ((ref  (make-%read-ref :label arg))
           (cell (cons arg ref)))
      (push cell *read-labels*)               ; placeholder visible to nested #n#
      (let ((object (read stream t nil t)))
        (setf (cdr cell) object)              ; label now resolves to the object
        (%patch-read-label object ref object) ; close any circular references
        object))))
(set-dispatch-macro-character #\# #\#
  (lambda (stream sub arg) (declare (ignore sub))
    (let ((cell (assoc arg *read-labels*)))
      (if cell (cdr cell)
          (error 'reader-error :stream stream)))))   ; undefined #n# label

;; #< — objects printed this way cannot be read back (CLHS: a reader-error).
(set-dispatch-macro-character #\# #\<
  (lambda (stream sub arg) (declare (ignore sub arg))
    (error 'reader-error :stream stream)))

;;; ---- BOCL extension hooks (init / abort / quit) + QUIT -------------------
;;; bocl extension (see issues/closed/extension-hooks): three lists of function
;;; designators run as best-effort hooks, and BOCL:QUIT, a non-local exit that
;;; runs pending UNWIND-PROTECT cleanups, then *QUIT-HOOK*, then exits.
(in-package "BOCL")

(defvar *init-hook* '()
  "List of (lambda ()) function designators, each called inside IGNORE-ERRORS
   after the init/rc file has been loaded (or --no-init processed), before the
   main task (-l/-x/-i and the REPL).")
(defvar *abort-hook* '()
  "List of (lambda (signum)) function designators, each called inside
   IGNORE-ERRORS when the process is crashing on a signal.  The GC is disabled
   and the system may be corrupt: minimal work, best effort, no guarantees.")
(defvar *quit-hook* '()
  "List of (lambda (status)) function designators, each called inside
   IGNORE-ERRORS by BOCL:QUIT before the process exits.")
(defvar *stop-hook* '()
  "List of (lambda ()) function designators, each called inside IGNORE-ERRORS
   when the process is about to be suspended by SIGTSTP (Ctrl-Z) — e.g. to reset
   the terminal out of raw/ncurses mode or release a resource that cannot be
   serviced while stopped.  Best effort, run from the signal handler (the system
   is live but the hooks should keep to async-safe work); the process then
   actually suspends.  Restored on resume by [[*cont-hook*]].")
(defvar *cont-hook* '()
  "List of (lambda ()) function designators, each called inside IGNORE-ERRORS
   when the process resumes after a stop (SIGCONT) — e.g. to re-enter ncurses raw
   mode or re-open a connection released in [[*stop-hook*]].  Best effort.")
(defvar *prompt-function* nil
  "NIL, or a function designator of one argument (a stream).  When non-NIL the
   REPL calls (funcall *prompt-function* stream) to write each prompt instead of
   the default implementation-defined prompt.")

(defun quit (&optional (status 0))
  "Quit bocl.  Performs a non-local exit so pending UNWIND-PROTECT clean-ups run
   (WITH-OPEN-FILE etc. close their resources); then *QUIT-HOOK* is run and the
   process exits with STATUS.  Does not return."
  (throw '%quit-catch-tag status))

;; Internal hook runners, called from the C launcher / crash handler.
(defun %run-init-hooks ()
  (dolist (f *init-hook*) (ignore-errors (funcall f))))
(defun %run-quit-hooks (status)
  (dolist (f *quit-hook*) (ignore-errors (funcall f status))))
(defun %run-abort-hooks (signum)
  (dolist (f *abort-hook*) (ignore-errors (funcall f signum))))
(defun %run-stop-hooks ()
  (dolist (f *stop-hook*) (ignore-errors (funcall f))))
(defun %run-cont-hooks ()
  (dolist (f *cont-hook*) (ignore-errors (funcall f))))

;;; ---- Debugger support (issue bocl-or-bash-debugger) ------------------

(defvar *on-error* :debug
  "What the top-level REPL does with an unhandled SERIOUS-CONDITION: :DEBUG enters
   the debugger, :APPEASE / :ABORT report it and return to the prompt, :EXIT exits
   the process.  Set from the --on-error command-line option by the launcher.")

;; Printer specials bound by INVOKE-DEBUGGER around the debugger's own output and
;; its evaluation REPL (issue table).  The user may customise these.
(defvar *debugger-print-array*        t)
(defvar *debugger-print-base*         10)
(defvar *debugger-print-case*         :upcase)
(defvar *debugger-print-circle*       t)
(defvar *debugger-print-escape*       t)
(defvar *debugger-print-gensym*       t)
(defvar *debugger-print-length*       8)
(defvar *debugger-print-level*        3)
(defvar *debugger-print-lines*        2)
(defvar *debugger-print-pretty*       nil)
(defvar *debugger-print-radix*        nil)
(defvar *debugger-print-readably*     nil)
(defvar *debugger-print-right-margin* 72)

;; Backtrace API.  bocl has no function-call stack yet, so a "frame" is currently
;; an opaque descriptor of an active dynamic CONTROL frame (the kernel %BACKTRACE).
(defun backtrace ()
  "List of the current frames, innermost (top of stack) first."
  (common-lisp::%backtrace))

(defun frame-kind (frame)
  "Keyword denoting the kind of FRAME (:CALL :BLOCK :CATCH :RESTART :HANDLER-CASE
   :HANDLER-BIND :UNWIND-PROTECT :DYNAMIC-BINDING :TAGBODY), or NIL."
  (and (consp frame) (car frame)))

(defun frame-name (frame)
  "The name associated with FRAME: the function name of a :CALL frame, the block /
   catch name, or the list of restart names; NIL for frames that carry no name."
  (cond ((not (consp frame)) nil)
        ((eq (car frame) :restart) (cdr frame))
        ((consp (cdr frame)) (cadr frame))
        (t nil)))

(defun frame-bindings (frame)
  "For a :CALL frame, the alist ((parameter . value) ...) of the function's
   parameters and their current values; NIL for any other frame."
  (if (and (consp frame) (eq (car frame) :call))
      (cddr frame)
      nil))

(defun print-frame (frame &optional (stream *standard-output*) &key (verbosity 0))
  "Print FRAME on STREAM.  VERBOSITY 0 prints the kind and name only; for a :CALL
   frame, 1 also prints the parameter names and 2 the parameter names and their
   values (subject to the *PRINT-* variables INVOKE-DEBUGGER binds from the
   BOCL:*DEBUGGER-PRINT-* variables)."
  (let ((kind (frame-kind frame)) (name (frame-name frame)))
    (case kind
      (:call
       (format stream "(~S" name)
       (when (and (integerp verbosity) (>= verbosity 1))
         (dolist (b (frame-bindings frame))
           (if (>= verbosity 2)
               (format stream " ~S=~S" (car b) (cdr b))
               (format stream " ~S" (car b)))))
       (format stream ")"))
      ((:block :catch) (format stream "~(~A~) ~S" kind name))
      (:restart        (format stream "restart~{ ~S~}" name))
      ((nil)           (format stream "<frame>"))
      (otherwise       (format stream "~(~A~)" kind))))
  frame)

(export '(*init-hook* *abort-hook* *quit-hook* *stop-hook* *cont-hook*
          *prompt-function* quit
          *on-error* backtrace frame-kind frame-name frame-bindings print-frame
          *debugger-print-array* *debugger-print-base* *debugger-print-case*
          *debugger-print-circle* *debugger-print-escape* *debugger-print-gensym*
          *debugger-print-length* *debugger-print-level* *debugger-print-lines*
          *debugger-print-pretty* *debugger-print-radix* *debugger-print-readably*
          *debugger-print-right-margin*))

(in-package "COMMON-LISP")

;;; ---- The minimal debugger (issue bocl-or-bash-debugger) --------------
;;; INVOKE-DEBUGGER prints the condition, the backtrace and the restart menu,
;;; then runs a debugger REPL that reads commands from *DEBUG-IO*: keyword
;;; commands (:BACKTRACE :RESTARTS :RESTART :CONTINUE :ABORT :QUIT :HELP) and a
;;; bare restart number invoke restarts / leave the debugger; anything else is
;;; evaluated as a Lisp form in the error's dynamic context.

(defvar *debugger-hook* nil
  "NIL or a function of (condition hook) called by INVOKE-DEBUGGER before the
   built-in debugger, with *DEBUGGER-HOOK* bound to NIL during the call.")

(defun %debug-output ()
  "An output stream for the debugger (*DEBUG-IO* if usable, else *STANDARD-OUTPUT*)."
  (if (and (boundp '*debug-io*) (streamp *debug-io*) (output-stream-p *debug-io*))
      *debug-io*
      *standard-output*))

(defun %debug-input ()
  "An input stream for the debugger (*DEBUG-IO* if usable, else *STANDARD-INPUT*)."
  (if (and (boundp '*debug-io*) (streamp *debug-io*) (input-stream-p *debug-io*))
      *debug-io*
      *standard-input*))

(defun %debugger-frames ()
  "BOCL:BACKTRACE with the debugger's own machinery elided: drop every frame up to
   and including the INVOKE-DEBUGGER call, leaving the signalling handler and the
   error's own call chain (cosmetic)."
  (let* ((frames (bocl:backtrace))
         (tail (member 'invoke-debugger frames
                       :key (lambda (f) (and (eq (bocl:frame-kind f) :call)
                                             (bocl:frame-name f))))))
    (if tail (cdr tail) frames)))

(defun %print-backtrace (stream &key (verbosity 1) (type :all) (start 0) count)
  "Print the backtrace on STREAM.  VERBOSITY 0/1/2 (see BOCL:PRINT-FRAME); TYPE :ALL
   or :CALL (only function-call frames); START skips that many frames; COUNT limits
   the number printed."
  (let ((frames (%debugger-frames)))
    (when (eq type :call)
      (setq frames (remove-if-not (lambda (f) (eq (bocl:frame-kind f) :call)) frames)))
    (format stream "~&Backtrace (innermost frame first):~%")
    (let ((i 0) (shown 0))
      (dolist (f frames)
        (when (and (>= i start) (or (null count) (< shown count)))
          (format stream "  ~2D: " i)
          (bocl:print-frame f stream :verbosity verbosity)
          (terpri stream)
          (incf shown))
        (incf i))))
  (values))

(defun %parse-backtrace-options (args)
  "Map the :BACKTRACE command's keyword arguments (e.g. :TYPE call :VERBOSE 1
   :COUNT 10 :START 20) to the &KEY arguments of %PRINT-BACKTRACE."
  (let ((opts '()))
    (loop while (and args (keywordp (car args)))
          do (let ((k (pop args)) (v (when args (pop args))))
               (case k
                 ((:verbose :verbosity) (setq opts (list* :verbosity v opts)))
                 (:type (setq opts (list* :type
                                          (if (keywordp v) v
                                              (intern (string-upcase (string v)) :keyword))
                                          opts)))
                 (:count (setq opts (list* :count v opts)))
                 (:start (setq opts (list* :start v opts)))
                 (t nil))))
    opts))

(defun %print-restart-menu (stream restarts)
  (if restarts
      (progn
        (format stream "~&Available restarts:~%")
        (let ((i 0))
          (dolist (r restarts)
            (format stream "  ~2D: [~A]~%" i (restart-name r))
            (incf i))))
      (format stream "~&No available restarts.~%"))
  (values))

(defun %invoke-selected-restart (stream restarts which condition)
  (let ((r (cond ((integerp which) (nth which restarts))
                 ((symbolp which)  (find-restart which condition))
                 (t nil))))
    (if r
        (invoke-restart r)               ; transfers control; does not return
        (format stream "~&No such restart: ~S~%" which))))

(defun %debugger-help (stream)
  (format stream "~&Debugger commands:~%~
                  ~&  :backtrace        print the backtrace~
                  ~&  :restarts         print the restart menu~
                  ~&  :restart N|name   invoke a restart (or just type its number)~
                  ~&  :continue         return NIL from the debugger~
                  ~&  :abort            return to the top-level REPL~
                  ~&  :quit             exit bocl~
                  ~&  <form>            evaluate <form> and print its values~%")
  (values))

(defun %debugger-read-line-forms (in)
  "Read one debugger command line from IN and return the list of Lisp forms on it
   (so `:restart try-again` or `:backtrace :verbose 1` parse as several tokens), or
   :EOF at end of input."
  (let ((line (read-line in nil :eof)))
    (if (eq line :eof)
        :eof
        (with-input-from-string (s line)
          (let ((forms '()))
            (loop (let ((f (read s nil '%eof)))
                    (when (eq f '%eof) (return))
                    (push f forms)))
            (nreverse forms))))))

(defun %run-debugger (condition)
  (let ((out (%debug-output)) (in (%debug-input)))
    (fresh-line out)
    (format out "~&Debugger invoked on condition of type ~A:~%  ~A~%"
            (type-of condition) condition)
    (%print-backtrace out)
    (let ((restarts (compute-restarts condition)))
      (%print-restart-menu out restarts)
      (loop
        (format out "~&debug> ")
        (force-output out)
        (handler-case
            (let ((forms (%debugger-read-line-forms in)))
              (cond
                ((eq forms :eof) (return nil))
                ((null forms)    nil)          ; blank line — re-prompt
                (t
                 (let ((cmd (car forms)) (args (cdr forms)))
                   (cond
                     ((eq cmd :continue) (return nil))
                     ((eq cmd :abort)
                      (let ((r (find-restart 'abort condition)))
                        (if r (invoke-restart r) (return nil))))
                     ((eq cmd :quit)     (bocl:quit))
                     ((eq cmd :help)     (%debugger-help out))
                     ((eq cmd :restarts)
                      (setq restarts (compute-restarts condition))
                      (%print-restart-menu out restarts))
                     ((eq cmd :restart)
                      (%invoke-selected-restart out restarts (car args) condition))
                     ((eq cmd :backtrace)
                      (apply #'%print-backtrace out (%parse-backtrace-options args)))
                     ((integerp cmd)
                      (%invoke-selected-restart out restarts cmd condition))
                     (t
                      ;; Not a debugger command — evaluate each form on the line as
                      ;; Lisp in the error's dynamic context.
                      (dolist (form forms)
                        (handler-case
                            (let ((vals (multiple-value-list (eval form))))
                              (if vals
                                  (dolist (v vals) (format out "~&~S~%" v))
                                  (format out "~&; No values~%")))
                          (serious-condition (e)
                            (format out "~&; Error during debugger eval: ~A~%" e))))))))))
          ;; A READ error (e.g. an unknown package in a typed form) or an error
          ;; while handling a debugger command must NOT escape %RUN-DEBUGGER — that
          ;; would unwind past the REPL and abort the whole process (issue
          ;; error-in-debugger).  Debug it in a NESTED debugger level instead
          ;; (CLHS 9.1 recursive debugging); then fall through and re-prompt.
          (serious-condition (e)
            (invoke-debugger e)))))))

(defun invoke-debugger (condition)
  "Enter the debugger on CONDITION (CLHS 9.1.4.2).  Honours *DEBUGGER-HOOK*, binds
   the CL printer specials from the BOCL:*DEBUGGER-PRINT-* variables, then runs the
   built-in debugger.  Returns NIL if the debugger is 'continued'."
  (when *debugger-hook*
    (let ((hook *debugger-hook*))
      (let ((*debugger-hook* nil))
        (funcall hook condition hook))))
  (let ((*print-array*        bocl:*debugger-print-array*)
        (*print-base*         bocl:*debugger-print-base*)
        (*print-case*         bocl:*debugger-print-case*)
        (*print-circle*       bocl:*debugger-print-circle*)
        (*print-escape*       bocl:*debugger-print-escape*)
        (*print-gensym*       bocl:*debugger-print-gensym*)
        (*print-length*       bocl:*debugger-print-length*)
        (*print-level*        bocl:*debugger-print-level*)
        (*print-lines*        bocl:*debugger-print-lines*)
        (*print-pretty*       bocl:*debugger-print-pretty*)
        (*print-radix*        bocl:*debugger-print-radix*)
        (*print-readably*     bocl:*debugger-print-readably*)
        (*print-right-margin* bocl:*debugger-print-right-margin*))
    (%run-debugger condition)))

(defun break (&optional (format-control "Break") &rest format-arguments)
  "Enter the debugger with a CONTINUE restart (CLHS 9.1).  Returns NIL when the
   debugger continues."
  (restart-case
      (let ((*debugger-hook* nil))
        (invoke-debugger
         (make-condition 'simple-condition
                         :format-control format-control
                         :format-arguments format-arguments)))
    (continue () :report "Return from BREAK." nil))
  nil)

(defun %report-repl-condition (condition)
  (let ((err (if (boundp '*error-output*) *error-output* *standard-output*)))
    (format err "~&;; Unhandled ~A:~%;;   ~A~%" (type-of condition) condition)
    (force-output err))
  (values))

(defun %repl-eval (form)
  "Evaluate FORM for the top-level REPL, returning its values.  An unhandled
   SERIOUS-CONDITION enters the debugger (when BOCL:*ON-ERROR* is :DEBUG) or is
   reported (:APPEASE/:ABORT) or exits (:EXIT); the process is never killed and
   control returns to the REPL."
  (block done
    (restart-case
        (handler-bind
            ((serious-condition
               (lambda (c)
                 ;; Record the condition on ";; C: " lines in the dribble file
                 ;; (issue dribble); no-ops when not dribbling.
                 (bocl::%dribble-note-condition c)
                 (cond
                   ;; The control stack is exhausted: do NOT run the debugger here —
                   ;; it would execute on the near-exhausted stack.  Report and ABORT
                   ;; (unwind to the REPL, freeing the stack and re-arming the guard).
                   ((typep c 'storage-condition)
                    (%report-repl-condition c) (invoke-restart 'abort))
                   (t
                    (case bocl:*on-error*
                      (:appease (%report-repl-condition c) (invoke-restart 'abort))
                      (:abort   (%report-repl-condition c) (invoke-restart 'abort))
                      (:exit    (%report-repl-condition c) (bocl:quit 1))
                      (otherwise
                       (invoke-debugger c)        ; :debug — may transfer via a restart
                       (invoke-restart 'abort)))))))) ; debugger continued -> back to REPL
          (return-from done (eval form)))
      (abort () :report "Return to the top-level REPL." (return-from done (values))))))

;;; ---- OS portability primitives (UIOP/ASDF) — BOCL extensions -------------
;;; Thin wrappers over the kernel %GETENV / %GETCWD / %CHDIR / %COMMAND-LINE-
;;; ARGUMENTS, in the BOCL package, that the UIOP port layer (#+bocl clauses)
;;; calls for getenv / getcwd / chdir / quit / raw-command-line-arguments.

(defun bocl::getenv (name)
  "Value of environment variable NAME (a string designator), or NIL if unset."
  (common-lisp::%getenv (string name)))

(defun bocl::setenv (name value)
  "Set environment variable NAME to VALUE (both string designators)."
  (common-lisp::%setenv (string name) (string value)))

(defun (setf bocl::getenv) (value name)
  (common-lisp::%setenv (string name) (string value))
  value)

(defun bocl::unsetenv (name)
  "Remove environment variable NAME."
  (common-lisp::%unsetenv (string name)))

(defun bocl::getcwd ()
  "The current working directory, as a pathname (POSIX getcwd(3))."
  (let ((s (common-lisp::%getcwd)))
    (and s (pathname s))))

(defun bocl::chdir (path)
  "Change the current directory to PATH (a string or pathname); T on success."
  (common-lisp::%chdir (if (pathnamep path) (namestring path) (string path))))

(defun bocl::command-line-arguments ()
  "The process command-line arguments (a list of strings; argv[0] first)."
  (common-lisp::%command-line-arguments))

(defvar bocl::*command-line-arguments* (common-lisp::%command-line-arguments)
  "The process command-line arguments captured at start-up.")

(export '(bocl::getenv bocl::setenv bocl::unsetenv bocl::getcwd bocl::chdir
          bocl::command-line-arguments bocl::*command-line-arguments*)
        "BOCL")

;;; ---- DRIBBLE (issue dribble) ---------------------------------------------
;;; CL:DRIBBLE records the REPL interaction to a file: each output line is
;;; prefixed ";; O: " (standard output) or ";; E: " (error output); conditions
;;; ";; C: "; the user's input verbatim with no prefix; a header ";; H: bocl
;;; <version>".  Built on real compound streams: *STANDARD-OUTPUT* / *ERROR-
;;; OUTPUT* are rebound to broadcasts of {the real stream, a line-prefix stream
;;; over the dribble file}.  The two prefix streams share one state object so an
;;; interrupting source terminates the current line (CLHS-ish: "a new-line is
;;; deemed separating them").

;; Dribble state is a 3-slot simple-vector #(file mid-line last-source) rather
;; than a DEFSTRUCT: bocl's DEFSTRUCT interns its accessors in *PACKAGE* (CL at
;; load time) regardless of a package-qualified struct name, which would not
;; match BOCL::%DS-* references.  (See issue defstruct-conc-name-package.issue.)
;;   slot 0 = file        the open dribble FILE-STREAM (appended to)
;;   slot 1 = mid-line    T when the last char written had no trailing newline
;;   slot 2 = last-source :O or :E — the source currently mid-line
(defun bocl::%make-dribble-state (file) (vector file nil nil))
(defun bocl::%ds-file        (st) (svref st 0))
(defun bocl::%ds-mid-line    (st) (svref st 1))
(defun bocl::%ds-last-source (st) (svref st 2))

(defun bocl::%dribble-prefix (source)
  (if (eq source :e) ";; E: " ";; O: "))

(defun bocl::%make-line-prefix-stream (source state)
  "An output stream that writes every line it receives to STATE's file,
   prefixed per SOURCE (:O / :E), coordinating with the partner stream via STATE
   so that a source interrupting the other mid-line first terminates that line."
  (bocl::%make-output-callback-stream
   (lambda (chunk)
     (let ((file (bocl::%ds-file state)))
       (loop for ch across chunk do
         (when (and (bocl::%ds-mid-line state)
                    (not (eq (bocl::%ds-last-source state) source)))
           (write-char #\linefeed file)
           (setf (svref state 1) nil))
         (unless (bocl::%ds-mid-line state)
           (write-string (bocl::%dribble-prefix source) file)
           (setf (svref state 1) t
                 (svref state 2) source))
         (write-char ch file)
         (when (char= ch #\linefeed)
           (setf (svref state 1) nil)
           (force-output file)))))))

;;; --- DRIBBLE-FILE-PATHNAME ------------------------------------------------
;;; Zero-padding done by hand (FORMAT ~mincol,'padchar D is the subject of a
;;; separate open issue and not relied on here).

(defun bocl::%zp (n width)
  "N as a decimal string, left-padded with #\\0 to at least WIDTH digits."
  (let ((s (princ-to-string n)))
    (if (< (length s) width)
        (concatenate 'string
                     (make-string (- width (length s)) :initial-element #\0) s)
        s)))

(defun bocl::%dribble-tz-suffix (time-zone)
  ;; CL time zone = hours west of UTC; the ISO-8601 suffix is east-positive,
  ;; i.e. offset = -TIME-ZONE.  (TIME-ZONE 0 -> \"+0000\".)
  (let* ((off  (- (round (* time-zone 60))))   ; minutes east of UTC
         (sign (if (minusp off) "-" "+"))
         (a    (abs off)))
    (concatenate 'string sign (bocl::%zp (floor a 60) 2) (bocl::%zp (mod a 60) 2))))

(defun bocl::%dribble-directory-namestring ()
  "Absolute namestring of the dribbles directory:
   ${XDG_STATE_HOME:-~/.local/state}/bocl/dribbles/ (with trailing slash)."
  (let ((xdg (bocl::getenv "XDG_STATE_HOME")))
    (concatenate 'string
      (if (and xdg (plusp (length xdg)))
          (if (char= (char xdg (1- (length xdg))) #\/)
              xdg (concatenate 'string xdg "/"))
          (concatenate 'string (namestring (user-homedir-pathname)) ".local/state/"))
      "bocl/dribbles/")))

(defun bocl::dribble-file-pathname (&key (type "log") (time-zone nil))
  "An absolute physical pathname for the next dribble file, named after the
   current time in ISO-8601 basic format YYYYMMDDThhmmss with a TYPE file type.
   TIME-ZONE NIL = local (bocl has no local-TZ primitive, so UTC digits with no
   suffix); :UTC = UTC with a trailing Z; a rational = that zone with a ±hhmm
   suffix."
  (multiple-value-bind (sec min hour day month year)
      (decode-universal-time (get-universal-time)
                             (if (eq time-zone :utc) 0 (or time-zone 0)))
    (let ((stamp (concatenate 'string
                   (bocl::%zp year 4) (bocl::%zp month 2) (bocl::%zp day 2) "T"
                   (bocl::%zp hour 2) (bocl::%zp min 2) (bocl::%zp sec 2)
                   (cond ((eq time-zone :utc) "Z")
                         ((null time-zone) "")
                         (t (bocl::%dribble-tz-suffix time-zone))))))
      (pathname (concatenate 'string
                  (bocl::%dribble-directory-namestring) stamp "." type)))))

;;; --- The DRIBBLE function -------------------------------------------------

(defvar bocl::*dribble-stream*       nil "Open dribble FILE-STREAM, or NIL.")
(defvar bocl::*dribble-state*        nil)
(defvar bocl::*dribble-pathname*     nil "Absolute pathname of the dribble file.")
(defvar bocl::*dribble-saved-output* nil)
(defvar bocl::*dribble-saved-error*  nil)

(defun bocl::dribbling-p () (and bocl::*dribble-stream* t))

(defun bocl::%dribble-note-input (text)
  "Record a verbatim input line TEXT (no prefix) to the dribble file."
  (when bocl::*dribble-stream*
    (let ((st bocl::*dribble-state*))
      ;; close any line the previous output left open before the bare input
      (when (bocl::%ds-mid-line st)
        (write-char #\linefeed bocl::*dribble-stream*)
        (setf (svref st 1) nil)))
    (write-string text bocl::*dribble-stream*)
    (unless (and (plusp (length text))
                 (char= (char text (1- (length text))) #\linefeed))
      (write-char #\linefeed bocl::*dribble-stream*))
    (force-output bocl::*dribble-stream*)))

(defun bocl::%dribble-note-condition (condition)
  "Record CONDITION's report on ';; C: '-prefixed lines in the dribble file."
  (when bocl::*dribble-stream*
    (let ((st bocl::*dribble-state*))
      (when (bocl::%ds-mid-line st)
        (write-char #\linefeed bocl::*dribble-stream*)
        (setf (svref st 1) nil)))
    (dolist (line (bocl::%split-lines
                   (handler-case (princ-to-string condition)
                     (error () "<error printing condition>"))))
      (write-string ";; C: " bocl::*dribble-stream*)
      (write-string line bocl::*dribble-stream*)
      (write-char #\linefeed bocl::*dribble-stream*))
    (force-output bocl::*dribble-stream*)))

(defun bocl::%split-lines (string)
  (let ((lines '()) (start 0))
    (dotimes (i (length string))
      (when (char= (char string i) #\linefeed)
        (push (subseq string start i) lines)
        (setf start (1+ i))))
    (push (subseq string start) lines)
    (nreverse lines)))

(defun dribble (&optional pathname)
  "Without PATHNAME, stop dribbling, close the dribble file and return NIL.
   With PATHNAME, (re)start recording the REPL interaction to that file
   (appending if it exists) and return its absolute pathname."
  (cond
    (pathname
     (when bocl::*dribble-stream* (dribble))      ; close any current dribble
     (let ((path (merge-pathnames (pathname pathname) (bocl::getcwd))))
       (ensure-directories-exist path)
       (let* ((file  (open path :direction :output
                                :if-exists :append :if-does-not-exist :create))
              (state (bocl::%make-dribble-state file)))
         (format file ";; H: bocl ~A~%" (lisp-implementation-version))
         (force-output file)
         (setf bocl::*dribble-saved-output* *standard-output*
               bocl::*dribble-saved-error*  *error-output*
               *standard-output*
                 (make-broadcast-stream bocl::*dribble-saved-output*
                                        (bocl::%make-line-prefix-stream :o state))
               *error-output*
                 (make-broadcast-stream bocl::*dribble-saved-error*
                                        (bocl::%make-line-prefix-stream :e state))
               bocl::*dribble-stream*   file
               bocl::*dribble-state*    state
               bocl::*dribble-pathname* (truename path))
         bocl::*dribble-pathname*)))
    (bocl::*dribble-stream*
     (force-output *standard-output*)
     (force-output *error-output*)
     (setf *standard-output* bocl::*dribble-saved-output*
           *error-output*    bocl::*dribble-saved-error*)
     (close bocl::*dribble-stream*)
     (setf bocl::*dribble-stream*       nil
           bocl::*dribble-state*        nil
           bocl::*dribble-pathname*     nil
           bocl::*dribble-saved-output* nil
           bocl::*dribble-saved-error*  nil)
     nil)
    (t nil)))

(export '(bocl::dribble-file-pathname) "BOCL")

;;; ---- Module system (issue asdf) ------------------------------------------
;;; Installed "system" modules live under <libexec>/bocl/modules/<dir>/, each with a
;;; module.manifest plist  (:name "NAME" :files ("rel.lisp" ...)).  At boot we scan
;;; those manifests, merge each :FILES entry with its module directory to an absolute
;;; namestring, and push the resulting plist on BOCL:*MODULE-LIST*.  REQUIRE then
;;; loads a module by name.  The DIRECTORY name is not significant — the :NAME in the
;;; manifest selects the module (so modules/asdf3/ can provide module "ASDF").

(defvar bocl::*module-list* nil
  "Module plists (:name string :files (absolute-namestrings...)) discovered at boot
   under <libexec>/bocl/modules/.")

(defvar bocl::*modules-directory* nil
  "The directory scanned at boot for module manifests, as resolved by the kernel
   %DEFAULT-MODULES-DIRECTORY ($BOCL_MODULES_DIR, else the install-configured
   location, else a default).  Recorded so a failed REQUIRE can report where it
   looked.")

(define-condition bocl::invalid-module-plist (error)
  ((object :initarg :object :reader bocl::invalid-module-plist-object)
   (reason :initarg :reason :reader bocl::invalid-module-plist-reason)))

(define-condition bocl::undefined-module-error (error)
  ((module-name :initarg :module-name :reader bocl::undefined-module-name))
  (:report
   (lambda (condition stream)
     (let* ((name  (bocl::undefined-module-name condition))
            (avail (ignore-errors (mapcar #'bocl::module-name bocl::*module-list*))))
       (format stream "REQUIRE: module ~S is not defined.~%" name)
       (format stream "  Modules directory searched: ~A~%"
               (or bocl::*modules-directory*
                   "(none - in-tree run, or bocl was not installed with a modules/ dir)"))
       (format stream "  Modules found there / already provided: ~A~%"
               (if avail (format nil "~{~A~^, ~}" avail) "none"))
       (when (string-equal (string name) "ASDF")
         (format stream "  The ASDF module is built from a git submodule; to install it:~%")
         (format stream "      git submodule update --init modules/asdf-src~%")
         (format stream "      make modules && make install~%")
         (format stream "  (see modules/README.org).~%"))))))

(defun bocl::module-name (module-plist)
  "The :NAME of a module plist (a string), or signal INVALID-MODULE-PLIST."
  (or (getf module-plist :name)
      (error 'bocl::invalid-module-plist :object module-plist
             :reason "Missing :NAME entry")))

(defun bocl::module-files (module-plist)
  "The :FILES of a module plist (absolute namestrings), or INVALID-MODULE-PLIST."
  (or (getf module-plist :files)
      (error 'bocl::invalid-module-plist :object module-plist
             :reason "Missing :FILES entry")))

(defun bocl::%read-manifest (manifest-path module-dir)
  "Read the plist in MANIFEST-PATH; return (:name STRING :files (ABS...)) with each
   :FILES entry merged against MODULE-DIR, or NIL on any error / malformed manifest."
  (ignore-errors
    (with-open-file (s manifest-path :direction :input)
      (let* ((plist (read s nil nil))
             (name  (getf plist :name))
             (files (getf plist :files)))
        (when (and name files)
          (list :name (string name)
                :files (mapcar (lambda (f)
                                 (concatenate 'string module-dir (string f)))
                               (if (listp files) files (list files)))))))))

(defun bocl::%scan-modules (modules-dir)
  "Return the module plists found under MODULES-DIR/<dir>/module.manifest."
  (let ((result '()))
    (dolist (entry (ignore-errors (common-lisp::%list-directory modules-dir)) result)
      (let* ((mdir     (concatenate 'string modules-dir entry "/"))
             (manifest (concatenate 'string mdir "module.manifest")))
        (when (probe-file manifest)
          (let ((m (bocl::%read-manifest manifest mdir)))
            (when m (push m result))))))))

;;; Populate the module list at boot (best effort: NIL when the resolved modules
;;; directory does not exist, e.g. an uninstalled in-tree run).
;;; The modules directory is resolved by the kernel (%DEFAULT-MODULES-DIRECTORY):
;;; $BOCL_MODULES_DIR, else the install-time-configured location (derived from the
;;; compiled-in prelude path, so it carries the install PREFIX), else a default.
;;; It is deliberately NOT derived from where THIS prelude file was found — that
;;; can be a stray CWD-relative match when bocl is run from a source checkout.
(let ((modules-dir (ignore-errors (common-lisp::%default-modules-directory))))
  (when modules-dir
    (setq bocl::*modules-directory* modules-dir)
    ;; %scan-modules safely returns NIL when the directory is absent.
    (setq bocl::*module-list* (bocl::%scan-modules modules-dir))))

;;; Enhanced REQUIRE (CLHS 24): MODULE-NAME is a string designator.  With no
;;; PATHNAME-LIST, look the module up by name in BOCL:*MODULE-LIST* and load its
;;; files; otherwise load the given pathnames.  A loaded file conventionally calls
;;; PROVIDE; we also record the name so a second REQUIRE is a no-op.
(defun require (module-name &optional pathname-list)
  (let ((name (string module-name)))
    (unless (member name *modules* :test #'string=)
      (let ((paths
              (cond
                (pathname-list
                 (if (listp pathname-list) pathname-list (list pathname-list)))
                (t (let ((module (find name bocl::*module-list*
                                       :key #'bocl::module-name
                                       :test #'string-equal)))
                     (if module
                         (bocl::module-files module)
                         (error 'bocl::undefined-module-error :module-name name)))))))
        (dolist (p paths) (load p))
        (pushnew name *modules* :test #'string=)))
    module-name))

;;; ---- BIGNUM support -------------------------------------------------------
;;; When the kernel was built with bignum support (the %BIGNUMP primitive is
;;; bound), load the arbitrary-precision integer engine (bootstrap/bignum.lisp)
;;; and the kernel-integration glue (bootstrap/bignum-integration.lisp), siblings
;;; of this prelude.  Loaded last, under *PACKAGE* = COMMON-LISP, so the engine
;;; and glue symbols (u%/i%/%bignum-*) live in the package the kernel's S(...)
;;; macro interns into.  *LOAD-TRUENAME* is bound to this file by the loader.
(when (fboundp '%bignump)
  (let ((here (or *load-truename* *load-pathname*)))
    (when here
      (let* ((ns  (if (stringp here) here (namestring here)))
             (pos (search "prelude.lisp" ns :from-end t))
             (dir (if pos (subseq ns 0 pos) "")))
        (load (concatenate 'string dir "bignum.lisp"))
        (load (concatenate 'string dir "bignum-integration.lisp"))))))

;;; ---- SOCKET support -------------------------------------------------------
;;; When the kernel was built with BSD-socket support (the %SOCKET primitive is
;;; bound), load the usocket-compatible BOCL-SOCKET package (bootstrap/socket.lisp),
;;; a sibling of this prelude.  *LOAD-TRUENAME* is bound to this file by the loader.
(when (fboundp 'bocl::%socket)
  (let ((here (or *load-truename* *load-pathname*)))
    (when here
      (let* ((ns  (if (stringp here) here (namestring here)))
             (pos (search "prelude.lisp" ns :from-end t))
             (dir (if pos (subseq ns 0 pos) "")))
        (load (concatenate 'string dir "socket.lisp"))))))
