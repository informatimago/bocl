;;;; -*- mode:lisp; coding:utf-8 -*-
;;;; bootstrap/vm-compiler.lisp
;;;;
;;;; plan-2 byte-code compiler for bocl — vm-3: the compiler FRONT-END.
;;;;
;;;; This file holds the Lisp-side analysis pass: it walks a (LAMBDA ...)
;;;; expression, expands every macro (global + local MACROLET / SYMBOL-MACROLET),
;;;; and resolves every lexical variable / local-function reference to a
;;;; (DEPTH INDEX) address in the FLAT-LOCALS model (see the header of
;;;; sources/bytecode.h and issues/open/vm-COMPILER-PLAN.md §3.1 and §5).  Its
;;;; output is an intermediate representation (IR) — a tree of keyword-tagged
;;;; lists — consumed by the codegen passes (vm-4/vm-5) that emit byte-code.
;;;;
;;;; The whole file is written to be self-compilable later (vm-9): it uses only
;;;; ordinary Common Lisp and the kernel primitive %GLOBALLY-SPECIAL-P.
;;;;
;;;; Loaded at the END of bootstrap/prelude.lisp (so the full CL runtime — and
;;;; MACROEXPAND / DESTRUCTURING-BIND — is already present), under
;;;; *PACKAGE* = COMMON-LISP, so the %VM-... helpers intern into CL as internal
;;;; symbols alongside the rest of the implementation.

;;; ===========================================================================
;;; Flat-locals model
;;; ---------------------------------------------------------------------------
;;; ALL lexical variables of a function — its parameters AND every LET/LET*/&aux
;;; binding, at any nesting depth WITHIN the function — get distinct slot indices
;;; in ONE frame (the function's locals vector).  A nested LET does NOT open a new
;;; VM frame; it consumes more slots.  DEPTH>0 crosses an enclosing *lambda*
;;; boundary only (a closure capture — recorded here, converted in vm-6).
;;;
;;; A CENV (compilation environment) is a list of FLEVELs, innermost first.
;;; An FLEVEL tracks one function's frame:  (NEXT-SLOT . SCOPES)
;;;   NEXT-SLOT — fixnum, the next free slot; grows monotonically (never reused,
;;;               correctness over density — see the plan §6).  Its final value is
;;;               the function's N-LOCALS.
;;;   SCOPES    — a stack of lexical scopes (innermost first); each scope is a
;;;               list (alist) of BINDINGs.  Scopes give correct shadowing and
;;;               control when a name leaves lexical view; slots are never reclaimed.
;;; A BINDING is (NAME NAMESPACE INDEX SPECIAL-P CAPTURED-P), NAMESPACE :VAR|:FUN.
;;; ===========================================================================

(defun %vm-new-flevel () (cons 0 '()))
(defun %vm-flevel-alloc (fl)            ; allocate one slot, return its index
  (let ((i (car fl))) (setf (car fl) (1+ i)) i))
(defun %vm-flevel-nlocals (fl) (car fl))
(defun %vm-flevel-push-scope (fl) (setf (cdr fl) (cons '() (cdr fl))))
(defun %vm-flevel-pop-scope  (fl) (setf (cdr fl) (cddr fl)))
(defun %vm-flevel-scopes (fl) (cdr fl))
(defun %vm-flevel-add (fl binding)      ; prepend BINDING to the innermost scope
  (setf (cadr fl) (cons binding (cadr fl))))

(defun %vm-binding (name ns index special-p) (list name ns index special-p nil))
(defun %vm-binding-name       (b) (first  b))
(defun %vm-binding-ns         (b) (second b))
(defun %vm-binding-index      (b) (third  b))
(defun %vm-binding-special-p  (b) (fourth b))
(defun %vm-binding-captured-p (b) (fifth  b))
(defun %vm-binding-mark-captured (b) (setf (fifth b) t))

(defun %vm-resolve (name ns cenv)
  "Resolve NAME in namespace NS (:VAR or :FUN) against CENV.
   Return (VALUES binding depth); (VALUES NIL NIL) if free/global.  DEPTH counts
   the enclosing FUNCTION levels crossed (0 = the current function).  A hit at
   depth>0 marks the binding CAPTURED (an inner lambda closes over it — vm-6)."
  (let ((depth 0))
    (dolist (fl cenv (values nil nil))
      (dolist (scope (%vm-flevel-scopes fl))
        (dolist (b scope)
          (when (and (eq (%vm-binding-name b) name)
                     (eq (%vm-binding-ns b) ns))
            (when (> depth 0) (%vm-binding-mark-captured b))
            (return-from %vm-resolve (values b depth)))))
      (setq depth (1+ depth)))))

;;; ---------------------------------------------------------------------------
;;; Lexical environment for local macros / symbol-macros / block & tag names.
;;; Threaded functionally (extended by rebinding), unlike the mutable CENV.
;;; A LEXENV is (MACROS SYMBOL-MACROS BLOCKS TAGS):
;;;   MACROS        — alist NAME -> expander-function (of the whole call form)
;;;   SYMBOL-MACROS — alist SYMBOL -> (SYMBOL EXPANSION-FORM)
;;;   BLOCKS/TAGS   — lists of visible block names / go tags (for validation)
;;; ---------------------------------------------------------------------------

(defun %vm-lexenv-empty () (list '() '() '() '()))
(defun %vm-lexenv-macros        (le) (first  le))
(defun %vm-lexenv-symbol-macros (le) (second le))
(defun %vm-lexenv-add-macros (le pairs)
  (list (append pairs (first le)) (second le) (third le) (fourth le)))
(defun %vm-lexenv-add-symbol-macros (le pairs)
  (list (first le) (append pairs (second le)) (third le) (fourth le)))
(defun %vm-lexenv-add-block (le name)
  (list (first le) (second le) (cons name (third le)) (fourth le)))
(defun %vm-local-macro  (le name) (cdr    (assoc name (%vm-lexenv-macros le))))
(defun %vm-symbol-macro (le name) (assoc name (%vm-lexenv-symbol-macros le)))

(defun %vm-globally-special-p (name)
  "T iff NAME is a symbol globally proclaimed SPECIAL (DEFVAR/DEFPARAMETER/
   proclaim); keywords and non-symbols are not variables."
  (and (symbolp name) (not (keywordp name)) (%globally-special-p name)))

(defun %vm-make-local-macro-expander (lambda-list body)
  "Build a MACROLET expander: a function of the whole call form that
   destructures its arguments against LAMBDA-LIST and runs BODY."
  (eval (list 'lambda '(%vm-form)
              (list 'destructuring-bind lambda-list '(cdr %vm-form)
                    (cons 'progn body)))))

;;; ===========================================================================
;;; Body / lambda-list parsing
;;; ===========================================================================

(defun %vm-parse-body (body)
  "Return (VALUES forms special-vars): strip leading DECLARE forms (collecting
   SPECIAL declarations) and a leading docstring (a string with more forms after)."
  (let ((specials '()) (forms body) (doc-ok (consp (cdr body))))
    (loop
      (cond ((and (consp forms) (consp (car forms)) (eq (caar forms) 'declare))
             (dolist (spec (cdar forms))
               (when (and (consp spec) (eq (car spec) 'special))
                 (dolist (v (cdr spec)) (push v specials))))
             (setq forms (cdr forms)))
            ((and doc-ok (consp forms) (stringp (car forms)) (consp (cdr forms)))
             (setq forms (cdr forms) doc-ok nil))
            (t (return))))
    (values forms (nreverse specials))))

(defun %vm-parse-let-binding (bspec)
  "A LET/&aux binding spec -> (VALUES name init-form)."
  (cond ((symbolp bspec) (values bspec nil))
        ((null (cdr bspec)) (values (car bspec) nil))
        (t (values (car bspec) (cadr bspec)))))

(defun %vm-parse-opt (item)
  "An &optional spec -> (VALUES var default-form supplied-p-var)."
  (if (symbolp item) (values item nil nil)
      (values (car item) (cadr item) (caddr item))))

(defun %vm-parse-key (item)
  "A &key spec -> (VALUES keyword var default-form supplied-p-var)."
  (if (symbolp item)
      (values (intern (symbol-name item) :keyword) item nil nil)
      (let ((head (car item)))
        (if (consp head)                      ; ((:key var) ...)
            (values (car head) (cadr head) (cadr item) (caddr item))
            (values (intern (symbol-name head) :keyword) head
                    (cadr item) (caddr item))))))

(defun %vm-bind-param (name fl)
  "Allocate a lexical slot for parameter NAME on FL and bind it in the current
   scope; return the slot index.  (A parameter that is globally SPECIAL still
   takes a slot here; wrapping it in a dynamic binding is a vm-5 refinement.)"
  (let ((idx (%vm-flevel-alloc fl)))
    (%vm-flevel-add fl (%vm-binding name :var idx nil))
    idx))

(defun %vm-analyze-lambda-list (ll fl cenv lexenv)
  "Parse ordinary lambda list LL: allocate a slot for every parameter (and every
   supplied-p var) on FL, bind them (LET*-visible to later defaults), analyze the
   default/init forms in CENV.  Return (VALUES params-descriptor arity).
   PARAMS is a plist (:required (idx...) :optional ((idx supplied-idx default-ir)...)
   :rest idx-or-nil :key ((kw idx supplied-idx default-ir)...) :allow-other-keys b
   :aux ((idx init-ir)...)); ARITY is (:arity nreq nopt rest-p key-p aok-p)."
  (let ((state :required)
        (required '()) (optional '()) (rest nil)
        (keys '()) (allow-other-keys nil) (aux '())
        (nreq 0) (nopt 0))
    (dolist (item ll)
      (cond
        ((eq item '&optional) (setq state :optional))
        ((eq item '&rest)     (setq state :rest))
        ((eq item '&body)     (setq state :rest))
        ((eq item '&key)      (setq state :key))
        ((eq item '&allow-other-keys) (setq allow-other-keys t))
        ((eq item '&aux)      (setq state :aux))
        (t (case state
             (:required
              (push (%vm-bind-param item fl) required)
              (setq nreq (1+ nreq)))
             (:optional
              (multiple-value-bind (var default supp) (%vm-parse-opt item)
                ;; default sees prior params but not VAR itself: analyze first.
                (let* ((didx (and default (%vm-analyze-form default cenv lexenv)))
                       (idx  (%vm-bind-param var fl))
                       (sidx (and supp (%vm-bind-param supp fl))))
                  (push (list idx sidx didx) optional)
                  (setq nopt (1+ nopt)))))
             (:rest
              (setq rest (%vm-bind-param item fl)))
             (:key
              (multiple-value-bind (kw var default supp) (%vm-parse-key item)
                (let* ((didx (and default (%vm-analyze-form default cenv lexenv)))
                       (idx  (%vm-bind-param var fl))
                       (sidx (and supp (%vm-bind-param supp fl))))
                  (push (list kw idx sidx didx) keys))))
             (:aux
              (multiple-value-bind (var init) (%vm-parse-let-binding item)
                (let* ((iidx (and init (%vm-analyze-form init cenv lexenv)))
                       (idx  (%vm-bind-param var fl)))
                  (push (list idx iidx) aux))))))))
    (values (list :required (nreverse required)
                  :optional (nreverse optional)
                  :rest rest
                  :key (nreverse keys)
                  :allow-other-keys allow-other-keys
                  :aux (nreverse aux))
            (list :arity nreq nopt (and rest t) (and keys t) allow-other-keys))))

;;; ===========================================================================
;;; The analyzer proper
;;; ===========================================================================

(defun %vm-analyze-form (form cenv lexenv)
  "Analyze one FORM to IR under compilation environment CENV / lexenv LEXENV."
  (cond
    ((symbolp form) (%vm-analyze-symbol form cenv lexenv))
    ((atom form)    (list :const form))                  ; number, char, string...
    ((eq (car form) 'lambda) (%vm-analyze-lambda-value form cenv lexenv))
    (t (let ((op (car form)))
         (cond
           ((not (symbolp op))                           ; ((lambda ...) . args)
            (%vm-analyze-call op (cdr form) cenv lexenv))
           ((%vm-local-macro lexenv op)
            (%vm-analyze-form (funcall (%vm-local-macro lexenv op) form) cenv lexenv))
           ((special-operator-p op)
            (%vm-analyze-special op form cenv lexenv))
           ((%vm-resolve op :fun cenv)                   ; a local FLET/LABELS fn
            (%vm-analyze-call op (cdr form) cenv lexenv))
           ((macro-function op)
            (%vm-analyze-form (macroexpand-1 form) cenv lexenv))
           (t (%vm-analyze-call op (cdr form) cenv lexenv)))))))

(defun %vm-analyze-symbol (sym cenv lexenv)
  (cond
    ((keywordp sym) (list :const sym))
    ((eq sym 't)    (list :const t))
    ((eq sym 'nil)  (list :const nil))
    ((%vm-symbol-macro lexenv sym)
     (%vm-analyze-form (cadr (%vm-symbol-macro lexenv sym)) cenv lexenv))
    (t (multiple-value-bind (b depth) (%vm-resolve sym :var cenv)
         (if (and b (not (%vm-binding-special-p b)))
             (list :lref depth (%vm-binding-index b))
             (list :gref sym))))))              ; special/free/global -> dynamic

(defun %vm-analyze-call (op args cenv lexenv)
  (let ((arg-irs (mapcar (lambda (a) (%vm-analyze-form a cenv lexenv)) args)))
    (if (consp op)                              ; ((lambda ...) . args)
        (list* :call (%vm-analyze-form op cenv lexenv) arg-irs)
        (multiple-value-bind (b depth) (%vm-resolve op :fun cenv)
          (if b
              (list* :call (list :lref depth (%vm-binding-index b)) arg-irs)
              (list* :call-named op arg-irs))))))

(defun %vm-analyze-progn (forms cenv lexenv)
  (cond ((null forms) (list :const nil))
        ((null (cdr forms)) (%vm-analyze-form (car forms) cenv lexenv))
        (t (cons :progn (mapcar (lambda (f) (%vm-analyze-form f cenv lexenv)) forms)))))

(defun %vm-analyze-lambda-value (form cenv lexenv)
  "A LAMBDA in value position -> a closure over the current environment."
  (list :make-closure (%vm-analyze-lambda form cenv lexenv)))

(defun %vm-analyze-lambda (form cenv lexenv)
  "Analyze (LAMBDA lambda-list . body) into a :LAMBDA IR template, opening a NEW
   function level so references to outer variables cross a closure boundary."
  (let* ((lambda-list (cadr form))
         (body        (cddr form))
         (fl          (%vm-new-flevel))
         (cenv*       (cons fl cenv)))
    (%vm-flevel-push-scope fl)
    (multiple-value-bind (params arity) (%vm-analyze-lambda-list lambda-list fl cenv* lexenv)
      (multiple-value-bind (forms body-specials) (%vm-parse-body body)
        (declare (ignore body-specials))
        (let ((body-ir (%vm-analyze-progn forms cenv* lexenv)))
          (%vm-flevel-pop-scope fl)
          (list :lambda
                :arity    arity
                :n-locals (%vm-flevel-nlocals fl)
                :params   params
                :body     body-ir
                :source   form))))))

(defun %vm-analyze-special (op form cenv lexenv)
  (case op
    ((quote) (list :const (cadr form)))
    ((if)
     (list :if (%vm-analyze-form (cadr form) cenv lexenv)
               (%vm-analyze-form (caddr form) cenv lexenv)
               (if (cdddr form)
                   (%vm-analyze-form (cadddr form) cenv lexenv)
                   (list :const nil))))
    ((progn) (%vm-analyze-progn (cdr form) cenv lexenv))
    ((setq)  (%vm-analyze-setq (cdr form) cenv lexenv))
    ((let)   (%vm-analyze-let (cadr form) (cddr form) cenv lexenv nil))
    ((let*)  (%vm-analyze-let (cadr form) (cddr form) cenv lexenv t))
    ((function) (%vm-analyze-function (cadr form) cenv lexenv))
    ((the)   (%vm-analyze-form (caddr form) cenv lexenv))    ; type ignored
    ((flet)   (%vm-analyze-flet (cadr form) (cddr form) cenv lexenv nil))
    ((labels) (%vm-analyze-flet (cadr form) (cddr form) cenv lexenv t))
    ((locally)
     (multiple-value-bind (forms specials) (%vm-parse-body (cdr form))
       (declare (ignore specials))
       (%vm-analyze-progn forms cenv lexenv)))
    ((eval-when) (%vm-analyze-eval-when (cadr form) (cddr form) cenv lexenv))
    ((macrolet)
     (%vm-analyze-progn
      (cddr form) cenv
      (%vm-lexenv-add-macros
       lexenv (mapcar (lambda (d) (cons (car d)
                                        (%vm-make-local-macro-expander (cadr d) (cddr d))))
                      (cadr form)))))
    ((symbol-macrolet)
     (%vm-analyze-progn (cddr form) cenv
                        (%vm-lexenv-add-symbol-macros lexenv (cadr form))))
    ((block)
     (list :block (cadr form)
           (%vm-analyze-progn (cddr form) cenv
                              (%vm-lexenv-add-block lexenv (cadr form)))))
    ((return-from)
     (list :return-from (cadr form)
           (if (cddr form) (%vm-analyze-form (caddr form) cenv lexenv) (list :const nil))))
    ((tagbody) (%vm-analyze-tagbody (cdr form) cenv lexenv))
    ((go) (list :go (cadr form)))
    ((catch)
     (list :catch (%vm-analyze-form (cadr form) cenv lexenv)
                  (%vm-analyze-progn (cddr form) cenv lexenv)))
    ((throw)
     (list :throw (%vm-analyze-form (cadr form) cenv lexenv)
                  (%vm-analyze-form (caddr form) cenv lexenv)))
    ((unwind-protect)
     (list :unwind-protect (%vm-analyze-form (cadr form) cenv lexenv)
                           (%vm-analyze-progn (cddr form) cenv lexenv)))
    ((multiple-value-call)
     (list* :mv-call (%vm-analyze-form (cadr form) cenv lexenv)
            (mapcar (lambda (a) (%vm-analyze-form a cenv lexenv)) (cddr form))))
    ((multiple-value-prog1)
     (list* :mv-prog1 (%vm-analyze-form (cadr form) cenv lexenv)
            (mapcar (lambda (a) (%vm-analyze-form a cenv lexenv)) (cddr form))))
    ((progv)
     (list :progv (%vm-analyze-form (cadr form) cenv lexenv)
                  (%vm-analyze-form (caddr form) cenv lexenv)
                  (%vm-analyze-progn (cdddr form) cenv lexenv)))
    ((load-time-value)
     (list :load-time-value
           (%vm-analyze-form (cadr form) (list (%vm-new-flevel)) (%vm-lexenv-empty))
           (and (cddr form) (caddr form))))
    (t (error "VM compiler: unhandled special operator ~S" op))))

(defun %vm-analyze-setq (pairs cenv lexenv)
  (cond
    ((null pairs) (list :const nil))
    ((null (cddr pairs)) (%vm-analyze-setq-1 (car pairs) (cadr pairs) cenv lexenv))
    (t (let ((assignments '()) (p pairs))
         (loop (when (null p) (return))
               (push (%vm-analyze-setq-1 (car p) (cadr p) cenv lexenv) assignments)
               (setq p (cddr p)))
         (cons :progn (nreverse assignments))))))

(defun %vm-analyze-setq-1 (var val cenv lexenv)
  (let ((sm (%vm-symbol-macro lexenv var)))
    (if sm                                    ; (setq symbol-macro v) => (setf expansion v)
        (%vm-analyze-form (list 'setf (cadr sm) val) cenv lexenv)
        (let ((val-ir (%vm-analyze-form val cenv lexenv)))
          (multiple-value-bind (b depth) (%vm-resolve var :var cenv)
            (if (and b (not (%vm-binding-special-p b)))
                (list :lset depth (%vm-binding-index b) val-ir)
                (list :gset var val-ir)))))))

(defun %vm-analyze-let (bindings body cenv lexenv sequential-p)
  "Analyze LET (SEQUENTIAL-P nil) / LET* (t).  Every binding gets a FRESH slot on
   the current function level, so parallel vs sequential differs only in what the
   init forms can see; both lower to a :LET / :LET* node whose entries are
   (:LEX idx init-ir) for lexical vars and (:SPECIAL name init-ir) for dynamic
   binds (a globally-special var or one declared SPECIAL in the body)."
  (let ((fl (car cenv)))
    (multiple-value-bind (forms body-specials) (%vm-parse-body body)
      (%vm-flevel-push-scope fl)
      (let ((entries '()) (pending '()))
        (dolist (bspec bindings)
          (multiple-value-bind (name init) (%vm-parse-let-binding bspec)
            (let ((special-p (or (member name body-specials)
                                 (%vm-globally-special-p name)))
                  (init-ir (%vm-analyze-form init cenv lexenv)))
              (if special-p
                  (let ((binding (%vm-binding name :var nil t)))
                    (push (list :special name init-ir) entries)
                    (if sequential-p (%vm-flevel-add fl binding) (push binding pending)))
                  (let* ((idx (%vm-flevel-alloc fl))
                         (binding (%vm-binding name :var idx nil)))
                    (push (list :lex idx init-ir) entries)
                    (if sequential-p (%vm-flevel-add fl binding) (push binding pending)))))))
        (unless sequential-p               ; LET: bindings become visible only now
          (dolist (b pending) (%vm-flevel-add fl b)))
        (let ((body-ir (%vm-analyze-progn forms cenv lexenv)))
          (%vm-flevel-pop-scope fl)
          (list (if sequential-p :let* :let) (nreverse entries) body-ir))))))

(defun %vm-analyze-function (spec cenv lexenv)
  (cond
    ((and (consp spec) (eq (car spec) 'lambda))
     (%vm-analyze-lambda-value spec cenv lexenv))
    ((symbolp spec)
     (multiple-value-bind (b depth) (%vm-resolve spec :fun cenv)
       (if b (list :lref depth (%vm-binding-index b)) (list :function spec))))
    ((and (consp spec) (eq (car spec) 'setf))     ; (function (setf foo))
     (list :function spec))
    (t (error "VM compiler: bad FUNCTION spec ~S" spec))))

(defun %vm-analyze-flet (defs body cenv lexenv labels-p)
  "Analyze FLET (LABELS-P nil) / LABELS (t).  Each local function gets a slot on
   the enclosing function level holding its closure.  FLET bodies do NOT see the
   local functions; LABELS bodies (and the functions themselves) do."
  (let* ((fl (car cenv))
         ;; (name idx lambda-list fbody), slots allocated on the enclosing frame.
         (specs (mapcar (lambda (def)
                          (list (car def) (%vm-flevel-alloc fl) (cadr def) (cddr def)))
                        defs)))
    (%vm-flevel-push-scope fl)
    (when labels-p
      (dolist (s specs)
        (%vm-flevel-add fl (%vm-binding (first s) :fun (second s) nil))))
    (let ((fn-irs (mapcar (lambda (s)
                            (cons (second s)
                                  (%vm-analyze-lambda
                                   (list* 'lambda (third s) (fourth s)) cenv lexenv)))
                          specs)))
      (unless labels-p
        (dolist (s specs)
          (%vm-flevel-add fl (%vm-binding (first s) :fun (second s) nil))))
      (multiple-value-bind (forms body-specials) (%vm-parse-body body)
        (declare (ignore body-specials))
        (let ((body-ir (%vm-analyze-progn forms cenv lexenv)))
          (%vm-flevel-pop-scope fl)
          (list (if labels-p :labels :flet) fn-irs body-ir))))))

(defun %vm-analyze-eval-when (situations body cenv lexenv)
  (if (or (member :execute situations) (member 'eval situations)
          (member :load-toplevel situations) (member 'load situations))
      (%vm-analyze-progn body cenv lexenv)
      (list :const nil)))

(defun %vm-analyze-tagbody (items cenv lexenv)
  (let ((result '()))
    (dolist (item items)
      (if (or (symbolp item) (integerp item))
          (push (list :tag item) result)
          (push (list :stmt (%vm-analyze-form item cenv lexenv)) result)))
    (list :tagbody (nreverse result))))

;;; ===========================================================================
;;; Entry point + a small IR pretty-printer (eyeballing addresses)
;;; ===========================================================================

(defun %vm-analyze (lambda-expr)
  "Analyze a top-level (LAMBDA lambda-list . body) into a :LAMBDA IR template.
   This is the deliverable of vm-3; codegen (vm-4+) consumes the returned tree."
  (unless (and (consp lambda-expr) (eq (car lambda-expr) 'lambda))
    (error "VM compiler: %VM-ANALYZE expects a (LAMBDA ...) expression, got ~S"
           lambda-expr))
  (%vm-analyze-lambda (%vm-macroexpand-toplevel lambda-expr) '() (%vm-lexenv-empty)))

(defun %vm-macroexpand-toplevel (form)
  "Expand FORM if it is itself a macro call yielding a (LAMBDA ...) — lets callers
   pass e.g. a DEFUN-style body already wrapped, or a macro that produces a lambda."
  (if (and (consp form) (eq (car form) 'lambda))
      form
      (multiple-value-bind (exp expanded) (macroexpand form)
        (if expanded (%vm-macroexpand-toplevel exp) form))))

(defun %vm-ir-op (ir) (and (consp ir) (car ir)))

(defun %vm-pp-ir (ir &optional (stream *standard-output*) (indent 0))
  "Print IR indented, one node per line — a debugging aid for vm-3 acceptance."
  (dotimes (i indent) (write-char #\Space stream))
  (cond
    ((and (consp ir) (eq (car ir) :lambda))
     (format stream "(:lambda arity=~S n-locals=~S~%"
             (getf (cdr ir) :arity) (getf (cdr ir) :n-locals))
     (%vm-pp-ir (getf (cdr ir) :body) stream (+ indent 2))
     (write-char #\) stream) (terpri stream))
    ((member (%vm-ir-op ir) '(:progn :if :call :call-named :make-closure
                              :block :return-from :catch :throw :unwind-protect
                              :mv-call :mv-prog1 :progv :let :let*))
     (format stream "(~S~%" (car ir))
     (dolist (sub (cdr ir))
       (if (and (consp sub) (keywordp (car sub)))
           (%vm-pp-ir sub stream (+ indent 2))
           (progn (dotimes (i (+ indent 2)) (write-char #\Space stream))
                  (format stream "~S~%" sub))))
     (dotimes (i indent) (write-char #\Space stream))
     (write-char #\) stream) (terpri stream))
    (t (format stream "~S~%" ir)))
  (values))
