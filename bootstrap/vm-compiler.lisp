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

(defun %vm-special-operator-p (op)
  "Operators the analyzer routes to %VM-ANALYZE-SPECIAL: the standard special operators
   PLUS the kernel special operators that — a bocl quirk — are NOT in SPECIAL-OPERATOR-P's
   list (eval.c handles them inline).  HANDLER-BIND/HANDLER-CASE and RESTART-BIND/RESTART-CASE
   are handled; the top-level def-forms are recognized only so the analyzer ERRORS on them
   (compile time) instead of miscompiling them into calls — so a caller with a safe fallback
   (COMPILE) reverts to the tree-walker for such a function."
  (or (special-operator-p op)
      (member op '(handler-bind handler-case
                   restart-bind restart-case
                   defun defmacro defvar defparameter defconstant in-package))))

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
           ((%vm-special-operator-p op)
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
    ;; A LEXICAL binding shadows a symbol-macro of the same name (CLHS 3.1.2.1.1),
    ;; so resolve the lexical/special binding FIRST, then fall back to a symbol-macro.
    (t (multiple-value-bind (b depth) (%vm-resolve sym :var cenv)
         (cond
           (b (if (%vm-binding-special-p b)
                  (list :gref sym)                       ; special LET binding -> dynamic
                  (list :lref depth (%vm-binding-index b))))
           ((%vm-symbol-macro lexenv sym)                ; not lexically bound -> symbol-macro
            (%vm-analyze-form (cadr (%vm-symbol-macro lexenv sym)) cenv lexenv))
           (t (list :gref sym)))))))                     ; free / global

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
     ;; keep the ORIGINAL form: LOAD-TIME-VALUE evaluates at load time, which for
     ;; %VM-COMPILE (compile == load) is compile time — codegen evaluates it then.
     (list :load-time-value (cadr form) (and (cddr form) (caddr form))))
    ((handler-bind) (%vm-analyze-handler-bind (cadr form) (cddr form) cenv lexenv))
    ((handler-case)
     ;; HANDLER-CASE = HANDLER-BIND + BLOCK/RETURN-FROM + CATCH/THROW (all supported):
     ;; expand it HERE (analyzer-local, so the tree-walker's own special op is untouched)
     ;; and re-analyze.
     (%vm-analyze-form (%vm-expand-handler-case form) cenv lexenv))
    ((restart-bind) (%vm-analyze-restart-bind (cadr form) (cddr form) cenv lexenv))
    ((restart-case)
     ;; RESTART-CASE = RESTART-BIND (throwing functions) + BLOCK/RETURN-FROM + CATCH/THROW,
     ;; exactly parallel to HANDLER-CASE.  Expand analyzer-locally and re-analyze.
     (%vm-analyze-form (%vm-expand-restart-case form) cenv lexenv))
    (t (error "VM compiler: unhandled special operator ~S" op))))

(defun %vm-analyze-handler-bind (bindings body cenv lexenv)
  "HANDLER-BIND: analyze each (type handler-fn) clause and the body.  The type is a
   type SPECIFIER (not evaluated); a class-name symbol resolves via FIND-CLASS, any
   other spec is passed through as a constant (SIGNAL's SUBCLASSP consults it)."
  (list :handler-bind
        (mapcar (lambda (cl)
                  (let ((type (car cl)) (fn (cadr cl)))
                    (cons (if (and (symbolp type) (not (keywordp type)))
                              (list :call-named 'find-class (list :const type))
                              (list :const type))
                          (%vm-analyze-form fn cenv lexenv))))
                bindings)
        (%vm-analyze-progn body cenv lexenv)))

(defun %vm-expand-handler-case (form)
  "Rewrite (HANDLER-CASE body clause...) into BLOCK/RETURN-FROM + CATCH/THROW +
   HANDLER-BIND (with capturing handler closures).  A matching handler stashes the
   condition and THROWs a clause index to a fresh-cons tag; the CATCH returns it and a
   CASE runs the clause with the condition bound.  Normal completion RETURN-FROMs the
   body value out (through the catch/handler-bind frames, which unwind).  An optional
   (:NO-ERROR (vars) . forms) clause wraps the body via MULTIPLE-VALUE-CALL."
  (let* ((body (cadr form))
         (clauses (cddr form))
         (no-error (assoc :no-error clauses))
         (real (remove no-error clauses))
         (tag (gensym "HC-TAG-")) (cvar (gensym "HC-COND-"))
         (which (gensym "HC-WHICH-")) (evar (gensym "HC-E-"))
         (bname (gensym "HC-BLOCK-"))
         (handlers '()) (cases '()) (i 0))
    (dolist (cl real)
      (let ((type (car cl)) (varlist (cadr cl)) (cbody (cddr cl)) (idx i))
        (push (list type (list 'lambda (list evar)
                               (list 'setq cvar evar) (list 'throw tag idx)))
              handlers)
        (push (list idx (if varlist
                            (list* 'let (list (list (car varlist) cvar)) cbody)
                            (cons 'progn cbody)))
              cases)
        (setq i (1+ i))))
    (let ((normal (if no-error
                      (list* 'multiple-value-call
                             (list* 'lambda (cadr no-error) (cddr no-error))
                             (list body))
                      body)))
      (list 'let (list (list tag (list 'cons nil nil)) (list cvar nil))
            (list 'block bname
                  (list 'let (list (list which
                                         (list 'catch tag
                                               (list 'return-from bname
                                                     (list* 'handler-bind (nreverse handlers)
                                                            (list normal))))))
                        (list* 'case which (nreverse cases))))))))

;;; --- RESTART system (vm-conditions RESTART) --------------------------------------
;;; RESTART-BIND is structurally HANDLER-BIND: a no-cleanup dynamic frame_restart that
;;; FIND-RESTART / INVOKE-RESTART (conditions.c, shared with the tree-walker) walk.  A
;;; restart-bind restart's spec is (name fn interactive-fn test-fn) — INVOKE-RESTART tells
;;; it from a RESTART-CASE restart by the 2nd element being CALLABLE, and applies it in
;;; place.  RESTART-CASE reuses this: each clause becomes a restart-bind restart whose fn
;;; THROWs, so invoking it unwinds to a CATCH that runs the clause (parallel to HANDLER-CASE).

(defun %vm-restart-option (opts key)
  "The value form for KEY in a RESTART-BIND option plist OPTS (:interactive-function /
   :test-function / :report-function ...), or NIL."
  (loop for r on opts by #'cddr
        when (and (consp (cdr r)) (eq (car r) key)) do (return (cadr r))))

(defun %vm-analyze-restart-bind (bindings body cenv lexenv)
  "RESTART-BIND: analyze each (name fn . options) into an IR list
   (name-const fn-ir interactive-fn-ir test-fn-ir); the codegen builds the runtime spec."
  (list :restart-bind
        (mapcar (lambda (b)
                  (let ((name (car b)) (fn (cadr b)) (opts (cddr b)))
                    (let ((ifn (%vm-restart-option opts :interactive-function))
                          (tfn (%vm-restart-option opts :test-function)))
                      (list (list :const name)
                            (%vm-analyze-form fn cenv lexenv)
                            (%vm-analyze-form (if ifn ifn nil) cenv lexenv)
                            (%vm-analyze-form (if tfn tfn nil) cenv lexenv)))))
                bindings)
        (%vm-analyze-progn body cenv lexenv)))

(defun %vm-expand-restart-case (form)
  "Rewrite (RESTART-CASE form clause...) into RESTART-BIND (throwing functions) +
   BLOCK/RETURN-FROM + CATCH/THROW.  Invoking restart R (INVOKE-RESTART applies its
   restart-bind function IN PLACE) THROWs the clause index + args to a fresh-cons tag; the
   CATCH returns the index and a CASE runs that clause's body with its lambda-list bound to
   the invocation arguments (full ordinary lambda list via APPLY)."
  (let* ((rform (cadr form))
         (clauses (cddr form))
         (tag (gensym "RC-TAG-")) (which (gensym "RC-WHICH-")) (avar (gensym "RC-ARGS-"))
         (bname (gensym "RC-BLOCK-"))
         (restarts '()) (cases '()) (i 0))
    (dolist (cl clauses)
      (let* ((name (car cl)) (rest (cdr cl))
             (varlist (car rest)) (rest2 (cdr rest)))
        ;; skip leading :report / :interactive / :test option pairs (a body may itself
        ;; begin with a keyword literal, so only skip these three known options)
        (loop while (and (consp rest2) (consp (cdr rest2))
                         (member (car rest2) '(:report :interactive :test)))
              do (setq rest2 (cddr rest2)))
        (let ((idx i) (rargs (gensym "RC-A-")))
          (push (list name (list 'lambda (list '&rest rargs)
                                 (list 'setq avar rargs) (list 'throw tag idx)))
                restarts)
          (push (list idx (list* 'apply (list* 'lambda varlist rest2) (list avar)))
                cases)
          (setq i (1+ i)))))
    (list 'let (list (list tag (list 'cons nil nil)) (list which nil) (list avar nil))
          (list 'block bname
                (list 'let (list (list which
                                       (list 'catch tag
                                             (list 'return-from bname
                                                   (list* 'restart-bind (nreverse restarts)
                                                          (list rform))))))
                      (list* 'case which (nreverse cases)))))))

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

;;; ===========================================================================
;;; vm-4 — CODE GENERATION (core forms)
;;; ---------------------------------------------------------------------------
;;; Lower the analyzer's IR to byte-code for the vm-2 ISA and box it in a
;;; CompiledFunction (%MAKE-COMPILED-FUNCTION).  The everyday, non-control path:
;;; constants, variable/global refs, calls, IF, PROGN, LET/LET* (lexical), SETQ,
;;; FUNCTION.  Closures / local functions are vm-6; control operators and dynamic
;;; (special) LET binds are vm-5 — codegen errors on them for now.
;;;
;;; %VM-GEN(node cs) emits code that leaves NODE's single value on the operand
;;; stack (VALUE context); an EFFECT context is VALUE + POP.  No tail-call yet
;;; (CALL then RETURN — genuine TCO needs the vm-6 trampoline).
;;; ===========================================================================

;;; --- opcodes: one source of truth = the C enum via %BYTECODE-OPCODES ---
(defvar *vm-opcodes* (%bytecode-opcodes))
(defun %vm-op (name)
  (let ((c (assoc name *vm-opcodes*)))
    (if c (cdr c) (error "VM codegen: no such opcode ~S" name))))

;;; --- LEB128 encoders ---
(defun %vm-uvar-bytes (n)
  "Minimal LEB128 unsigned encoding of N (a non-negative fixnum) -> octet list."
  (let ((out '()))
    (loop
      (let ((b (logand n #x7f)))
        (setq n (ash n -7))
        (if (zerop n) (progn (push b out) (return))
            (push (logior b #x80) out))))
    (nreverse out)))

(defun %vm-zigzag (n) (if (minusp n) (1- (* -2 n)) (* 2 n)))

(defun %vm-svar5-bytes (offset)
  "Fixed-width (5-octet) LEB128 of the zig-zag of OFFSET.  A fixed width lets the
   assembler resolve jumps in a single position pass — no fixpoint — since every
   jump instruction is exactly 6 bytes.  5 groups carry 35 bits, ample for any
   in-function displacement.  (vm_read_uvar accepts the non-minimal encoding.)"
  (let ((z (%vm-zigzag offset)) (out '()))
    (dotimes (i 5)
      (let ((b (logand z #x7f)))
        (setq z (ash z -7))
        (push (if (< i 4) (logior b #x80) b) out)))
    (nreverse out)))

;;; --- compiler state (CS): a mutable vector ---
;;;   0 code-rev  (asm items, newest first)   4 max-sp    (high-water)
;;;   1 consts    (pool, in order)            5 label-ctr
;;;   2 nconsts                               6 control-scopes (block/tagbody, innermost first)
;;;   3 sp        (current stack depth)       7 nlocals   (grows as scratch slots are taken)
;;;   8 max-depth (the deepest upvalue depth this fn reaches, DIRECTLY or via a nested
;;;             closure it must relay; >=1 means it captures — see %VM-GEN-MAKE-CLOSURE)
(defun %vm-cs-new (nlocals) (vector '() '() 0 0 0 0 '() nlocals 0))
(defun %vm-cs-max-depth (cs) (svref cs 8))
(defun %vm-cs-bump-depth (cs d) (when (> d (svref cs 8)) (setf (svref cs 8) d)))
(defun %vm-cs-emit (cs item) (setf (svref cs 0) (cons item (svref cs 0))))
(defun %vm-cs-sp+ (cs d)
  (let ((s (+ (svref cs 3) d)))
    (setf (svref cs 3) s)
    (when (> s (svref cs 4)) (setf (svref cs 4) s))))
(defun %vm-cs-sp (cs) (svref cs 3))
(defun %vm-cs-set-sp (cs s) (setf (svref cs 3) s))
(defun %vm-cs-const (cs value)
  "Intern VALUE in the constant pool (EQL dedup); return its index."
  (let ((p (position value (svref cs 1) :test #'eql)))
    (or p
        (prog1 (svref cs 2)
          (setf (svref cs 1) (append (svref cs 1) (list value)))
          (setf (svref cs 2) (1+ (svref cs 2)))))))
(defun %vm-cs-label (cs) (prog1 (svref cs 5) (setf (svref cs 5) (1+ (svref cs 5)))))
(defun %vm-cs-scratch (cs)
  "Allocate a fresh local slot beyond the analyzer's lexicals (e.g. a BLOCK result
   cell); grows the function's N-LOCALS."
  (prog1 (svref cs 7) (setf (svref cs 7) (1+ (svref cs 7)))))
(defun %vm-cs-nlocals (cs) (svref cs 7))
;;; control scopes (block / tagbody) — innermost first, for lexical RETURN-FROM / GO:
;;;   (:block name base-sp result-slot end-label)
;;;   (:tagbody base-sp ((tag . label) ...))
(defun %vm-cs-push-control (cs scope) (setf (svref cs 6) (cons scope (svref cs 6))))
(defun %vm-cs-pop-control  (cs)       (setf (svref cs 6) (cdr (svref cs 6))))
(defun %vm-cs-find-block (cs name)
  (dolist (s (svref cs 6) nil)
    (when (and (eq (car s) :block) (eql (cadr s) name)) (return s))))
(defun %vm-cs-find-tag (cs tag)
  (dolist (s (svref cs 6) nil)
    (when (eq (car s) :tagbody)
      (let ((c (assoc tag (caddr s) :test #'eql))) (when c (return s))))))
(defun %vm-cs-dframe-active-p (cs)
  "T if any dynamic frame (:dframe — CATCH/UNWIND-PROTECT/special-LET/PROGV/HANDLER-BIND)
   is open in codegen: a call there is NOT a true tail call (the frame must unwind first)."
  (dolist (s (svref cs 6) nil) (when (eq (car s) :dframe) (return t))))

;;; --- asm item emitters (item forms: (:op b) (:u8 n) (:uvar n) (:label l) (:jump op l)) ---
(defun %vm-emit-op    (cs op)      (%vm-cs-emit cs (list :op op)))
(defun %vm-emit-u8    (cs n)       (%vm-cs-emit cs (list :u8 n)))
(defun %vm-emit-uvar  (cs n)       (%vm-cs-emit cs (list :uvar n)))
(defun %vm-emit-label (cs lbl)     (%vm-cs-emit cs (list :label lbl)))
(defun %vm-emit-jump  (cs op lbl)  (%vm-cs-emit cs (list :jump op lbl)))

;;; --- assembler: asm items -> an octet string, jumps resolved ---
(defun %vm-item-length (it)
  (case (car it)
    (:op 1) (:u8 1)
    (:uvar (length (%vm-uvar-bytes (cadr it))))
    (:label 0)
    (:jump 6)))
(defun %vm-item-bytes (it labels p)
  (case (car it)
    (:op   (list (logand (cadr it) #xff)))
    (:u8   (list (logand (cadr it) #xff)))
    (:uvar (%vm-uvar-bytes (cadr it)))
    (:label '())
    (:jump (cons (logand (cadr it) #xff)
                 (%vm-svar5-bytes (- (cdr (assoc (caddr it) labels)) (+ p 6)))))))
(defun %vm-assemble (code-rev)
  (let ((items (reverse code-rev)) (labels '()) (pos 0))
    (dolist (it items)                             ; pass 1: label positions
      (when (eq (car it) :label) (push (cons (cadr it) pos) labels))
      (setq pos (+ pos (%vm-item-length it))))
    (let ((bytes '()) (p 0))                        ; pass 2: emit
      (dolist (it items)
        (let ((ib (%vm-item-bytes it labels p)))
          (dolist (b ib) (push b bytes))
          (setq p (+ p (length ib)))))
      (map 'string #'code-char (nreverse bytes)))))

;;; --- the codegen tree walk ---
(defun %vm-gen (node cs)
  "Emit code leaving NODE's value on the operand stack."
  (case (car node)
    (:const      (%vm-gen-const (cadr node) cs))
    (:lref       (%vm-gen-lref (cadr node) (caddr node) cs))
    (:gref       (%vm-gen-gref (cadr node) cs))
    (:lset       (%vm-gen-lset (cadr node) (caddr node) (cadddr node) cs))
    (:gset       (%vm-gen-gset (cadr node) (caddr node) cs))
    (:if         (%vm-gen-if node cs))
    (:progn      (%vm-gen-progn (cdr node) cs))
    (:call       (%vm-gen-call (cadr node) (cddr node) cs))
    (:call-named (%vm-gen-call-named (cadr node) (cddr node) cs))
    (:function   (%vm-gen-function (cadr node) cs))
    (:let        (%vm-gen-let (cadr node) (caddr node) cs nil))
    (:let*       (%vm-gen-let (cadr node) (caddr node) cs t))
    (:block      (%vm-gen-block (cadr node) (caddr node) cs))
    (:return-from(%vm-gen-return-from (cadr node) (caddr node) cs))
    (:tagbody    (%vm-gen-tagbody (cadr node) cs))
    (:go         (%vm-gen-go (cadr node) cs))
    (:mv-call    (%vm-gen-mv-call (cadr node) (cddr node) cs))
    (:mv-prog1   (%vm-gen-mv-prog1 (cadr node) (cddr node) cs))
    (:catch      (%vm-gen-catch (cadr node) (caddr node) cs))
    (:throw      (%vm-gen-throw (cadr node) (caddr node) cs))
    (:handler-bind (%vm-gen-handler-bind (cadr node) (caddr node) cs))
    (:restart-bind (%vm-gen-restart-bind (cadr node) (caddr node) cs))
    (:unwind-protect (%vm-gen-unwind-protect (cadr node) (caddr node) cs))
    (:progv      (%vm-gen-progv (cadr node) (caddr node) (cadddr node) cs))
    (:load-time-value (%vm-gen-load-time-value (cadr node) (caddr node) cs))
    (:make-closure (%vm-gen-make-closure (cadr node) cs))
    ((:flet :labels) (%vm-gen-flet (cadr node) (caddr node) cs))
    (t (error "VM codegen: unknown IR node ~S" node))))

(defun %vm-gen-const (v cs)
  (cond ((null v) (%vm-emit-op cs (%vm-op :push-nil)) (%vm-cs-sp+ cs 1))
        ((eq v t) (%vm-emit-op cs (%vm-op :push-t))   (%vm-cs-sp+ cs 1))
        (t (%vm-emit-op cs (%vm-op :push-const))
           (%vm-emit-uvar cs (%vm-cs-const cs v))
           (%vm-cs-sp+ cs 1))))

(defun %vm-gen-lref (depth index cs)
  (%vm-cs-bump-depth cs depth)                          ; track the deepest upvalue this fn reaches
  (%vm-emit-op cs (%vm-op :load-local)) (%vm-emit-uvar cs depth) (%vm-emit-uvar cs index)
  (%vm-cs-sp+ cs 1))

(defun %vm-gen-gref (sym cs)
  (%vm-emit-op cs (%vm-op :load-global)) (%vm-emit-uvar cs (%vm-cs-const cs sym))
  (%vm-cs-sp+ cs 1))

(defun %vm-gen-lset (depth index val-ir cs)
  (%vm-cs-bump-depth cs depth)                          ; track the deepest upvalue this fn reaches
  (%vm-gen val-ir cs)                                  ; value on stack (+1)
  (%vm-emit-op cs (%vm-op :store-local)) (%vm-emit-uvar cs depth) (%vm-emit-uvar cs index))
                                                        ; STORE-LOCAL leaves the value (net 0)

(defun %vm-gen-gset (sym val-ir cs)
  (%vm-gen val-ir cs)
  (%vm-emit-op cs (%vm-op :store-global)) (%vm-emit-uvar cs (%vm-cs-const cs sym)))

(defun %vm-gen-progn (forms cs)
  (cond ((null forms) (%vm-gen-const nil cs))
        (t (loop for rest on forms do
             (%vm-gen (car rest) cs)
             (when (cdr rest)                           ; drop non-final values
               (%vm-emit-op cs (%vm-op :pop)) (%vm-cs-sp+ cs -1))))))

(defun %vm-gen-if (node cs)
  (let ((test (cadr node)) (then (caddr node)) (else (cadddr node))
        (l-else (%vm-cs-label cs)) (l-end (%vm-cs-label cs)))
    (%vm-gen test cs)
    (%vm-emit-jump cs (%vm-op :jump-if-nil) l-else) (%vm-cs-sp+ cs -1)  ; pops test
    (let ((base (%vm-cs-sp cs)))
      (%vm-gen then cs)                                  ; base -> base+1
      (%vm-emit-jump cs (%vm-op :jump) l-end)
      (%vm-emit-label cs l-else)
      (%vm-cs-set-sp cs base)                            ; else runs with the test popped
      (%vm-gen else cs)                                  ; base -> base+1
      (%vm-emit-label cs l-end))))

;;; --- inline primitives (vm-inline-primitives) ------------------------------------
;;; A CALL to one of these UNSHADOWED standard CL functions, at the listed arg count,
;;; compiles to a dedicated stack op (no arg-list cons, no kernel_apply) instead of
;;; CALL_NAMED.  The op computes exactly what the builtin would (same C core), and
;;; redefining a CL standard function is undefined (CLHS 11.1.2.1.2), so this needs no
;;; runtime guard.  The analyzer only produces :call-named for the GLOBAL function, so a
;;; lexical FLET/LABELS shadowing `+` is never inlined.  1+/1- expand to ADD2/SUB2 with a
;;; literal 1 (no dedicated opcode needed).
(defparameter *vm-inline-primitives*
  '((+ 2 :add2) (- 2 :sub2) (* 2 :mul2)
    (= 2 :num-eq2) (< 2 :lt2) (> 2 :gt2) (<= 2 :le2) (>= 2 :ge2)
    (cons 2 :cons) (eq 2 :eq) (eql 2 :eql)
    (car 1 :car) (cdr 1 :cdr) (null 1 :null) (not 1 :null)))

(defun %vm-inline-op (sym n)
  "The inline opcode keyword for a call (SYM a1..aN), or NIL if SYM/N is not inlinable."
  (dolist (e *vm-inline-primitives* nil)
    (when (and (eq sym (car e)) (= n (cadr e))) (return (caddr e)))))

(defun %vm-inline-call-p (sym n)
  "True when (SYM . N args) compiles to inline byte-code (a table op, or 1+/1-)."
  (or (%vm-inline-op sym n)
      (and (member sym '(1+ 1-)) (= n 1))))

(defun %vm-gen-call-named (sym arg-irs cs)
  (let* ((n (length arg-irs)) (op (%vm-inline-op sym n)))
    (cond
      (op                                               ; a table primitive (2-arg / 1-arg)
       (dolist (a arg-irs) (%vm-gen a cs))              ; args left-to-right (+n)
       (%vm-emit-op cs (%vm-op op))
       (%vm-cs-sp+ cs (- 1 n)))                         ; pops n, pushes 1
      ((and (eq sym '1+) (= n 1))                       ; (1+ x) -> x 1 ADD2
       (%vm-gen (car arg-irs) cs) (%vm-gen-const 1 cs)
       (%vm-emit-op cs (%vm-op :add2)) (%vm-cs-sp+ cs -1))
      ((and (eq sym '1-) (= n 1))                       ; (1- x) -> x 1 SUB2
       (%vm-gen (car arg-irs) cs) (%vm-gen-const 1 cs)
       (%vm-emit-op cs (%vm-op :sub2)) (%vm-cs-sp+ cs -1))
      (t
       (dolist (a arg-irs) (%vm-gen a cs))              ; args left-to-right (+n)
       (%vm-emit-op cs (%vm-op :call-named)) (%vm-emit-uvar cs (%vm-cs-const cs sym))
       (%vm-emit-u8 cs n)
       (%vm-cs-sp+ cs (- 1 n))))))                      ; pops n, pushes 1

(defun %vm-gen-call (fn-ir arg-irs cs)
  (let ((n (length arg-irs)))
    (%vm-gen fn-ir cs)                                   ; callee (+1)
    (dolist (a arg-irs) (%vm-gen a cs))                 ; args (+n)
    (%vm-emit-op cs (%vm-op :call)) (%vm-emit-u8 cs n)
    (%vm-cs-sp+ cs (- n))))                              ; pops callee+n, pushes 1 => -n

(defun %vm-gen-function (spec cs)
  (unless (symbolp spec)
    (error "VM codegen: (FUNCTION ~S) — non-symbol function specs are vm-6" spec))
  (%vm-emit-op cs (%vm-op :load-function)) (%vm-emit-uvar cs (%vm-cs-const cs spec))
  (%vm-cs-sp+ cs 1))

(defun %vm-gen-lex-bind (entry cs)
  "Bind one lexical LET entry: init -> fresh slot (net 0 stack effect)."
  (%vm-gen (caddr entry) cs)                            ; init (+1)
  (%vm-emit-op cs (%vm-op :store-local))
  (%vm-emit-uvar cs 0) (%vm-emit-uvar cs (cadr entry))  ; STORE-LOCAL 0 idx (leaves value)
  (%vm-emit-op cs (%vm-op :pop)) (%vm-cs-sp+ cs -1))    ; drop -> net 0

(defun %vm-gen-let (entries body cs sequential-p &optional (body-gen #'%vm-gen))
  "Generate a LET (SEQUENTIAL-P nil) / LET* (t).  Lexical entries bind fresh slots;
   (:special name init) entries dynamically bind a special var (save/restore via a
   DYNAMIC_BIND frame that unwinds correctly on a non-local exit).  For LET the inits
   are all evaluated before any special is bound (parallel); for LET* each binding
   takes effect before the next init.  BODY-GEN lets an mv-context caller use
   %VM-GEN-FOR-VALUES for the body."
  (let ((nspecial 0))
    (if sequential-p
        (dolist (entry entries)                         ; LET*: bind each in order
          (ecase (car entry)
            (:lex (%vm-gen-lex-bind entry cs))
            (:special
             (%vm-gen (caddr entry) cs)                 ; value on top (+1)
             (%vm-emit-op cs (%vm-op :dynamic-bind))
             (%vm-emit-uvar cs (%vm-cs-const cs (cadr entry))) (%vm-cs-sp+ cs -1)
             (incf nspecial))))
        (let ((special-syms '()))                       ; LET: all inits first, then bind
          (dolist (entry entries)
            (ecase (car entry)
              (:lex (%vm-gen-lex-bind entry cs))
              (:special (%vm-gen (caddr entry) cs)       ; leave value on the stack (+1)
                        (push (cadr entry) special-syms))))  ; reversed = last entry first
          (dolist (sym special-syms)                    ; matches stack top-down
            (%vm-emit-op cs (%vm-op :dynamic-bind))
            (%vm-emit-uvar cs (%vm-cs-const cs sym)) (%vm-cs-sp+ cs -1)
            (incf nspecial))))
    ;; Each active special binding is a dynamic frame a lexical exit must unwind.
    (dotimes (i nspecial) (%vm-cs-push-control cs (list :dframe :dbind)))
    (funcall body-gen body cs)                          ; body value on top
    (dotimes (i nspecial) (%vm-cs-pop-control cs))
    (dotimes (i nspecial)                               ; unbind (LIFO); no stack effect
      (%vm-emit-op cs (%vm-op :dynamic-unbind)))))

;;; --- multiple values: MULTIPLE-VALUE-CALL / -PROG1 (hence -BIND / -LIST / NTH-VALUE) ---
;;; A "producer" leaves its PRIMARY on the stack and its FULL value set in the MV
;;; buffer.  Calls set the buffer themselves; %VM-GEN-FOR-VALUES adds a VALUES 1 after
;;; single-valued leaves so BC-MV-LIST (which reads the buffer) captures all values.
(defun %vm-gen-for-values (node cs)
  (case (car node)
    ((:call :call-named :mv-call :mv-prog1 :catch :unwind-protect :progv
      :handler-bind :restart-bind :block)
     ;; A real CALL sets the MV buffer; but an INLINED :call-named (an arithmetic/cons/… op)
     ;; leaves only a single value on the stack and does NOT touch the buffer, so it is a
     ;; single-valued leaf — sync the buffer with VALUES 1 (as for a constant / lref).
     (if (and (eq (car node) :call-named)
              (%vm-inline-call-p (cadr node) (length (cddr node))))
         (progn (%vm-gen node cs)
                (%vm-emit-op cs (%vm-op :values)) (%vm-emit-u8 cs 1))
         (%vm-gen node cs)))                                     ; sets the MV buffer
    (:if
     (let ((l-else (%vm-cs-label cs)) (l-end (%vm-cs-label cs)))
       (%vm-gen (cadr node) cs)
       (%vm-emit-jump cs (%vm-op :jump-if-nil) l-else) (%vm-cs-sp+ cs -1)
       (let ((base (%vm-cs-sp cs)))
         (%vm-gen-for-values (caddr node) cs)
         (%vm-emit-jump cs (%vm-op :jump) l-end)
         (%vm-emit-label cs l-else) (%vm-cs-set-sp cs base)
         (%vm-gen-for-values (cadddr node) cs)
         (%vm-emit-label cs l-end))))
    (:progn
     (let ((forms (cdr node)))
       (if (null forms)
           (%vm-gen-for-values (list :const nil) cs)
           (loop for r on forms do
             (if (cdr r)
                 (progn (%vm-gen (car r) cs) (%vm-emit-op cs (%vm-op :pop)) (%vm-cs-sp+ cs -1))
                 (%vm-gen-for-values (car r) cs))))))
    (:let  (%vm-gen-let (cadr node) (caddr node) cs nil #'%vm-gen-for-values))
    (:let* (%vm-gen-let (cadr node) (caddr node) cs t   #'%vm-gen-for-values))
    (t   ; single-valued leaf (const/lref/gref/lset/gset/function/block/tagbody/...)
     (%vm-gen node cs)
     (%vm-emit-op cs (%vm-op :values)) (%vm-emit-u8 cs 1))))     ; sync MV buffer = {top}

(defun %vm-gen-mv-call (fn-ir producer-irs cs)
  (%vm-gen fn-ir cs)                                   ; callee (+1)
  (let ((k (length producer-irs)))
    (dolist (p producer-irs)
      (%vm-gen-for-values p cs)                        ; primary + MV buffer
      (%vm-emit-op cs (%vm-op :mv-list)))              ; -> that producer's value list (net 0)
    (%vm-emit-op cs (%vm-op :call-named))              ; (append l1 .. lk)
    (%vm-emit-uvar cs (%vm-cs-const cs 'append)) (%vm-emit-u8 cs k)
    (%vm-cs-sp+ cs (- 1 k))
    (%vm-emit-op cs (%vm-op :call-named))              ; (apply callee appended)
    (%vm-emit-uvar cs (%vm-cs-const cs 'apply)) (%vm-emit-u8 cs 2)
    (%vm-cs-sp+ cs (- 1 2))))

;;; --- dynamic NLX: CATCH / THROW (vm-5 part 2b) ---
;;; CATCH is dynamically scoped — a runtime frame on g_frame_stack whose setjmp is the
;;; THROW landing point.  CATCH_OPEN takes a jump-style operand (offset to the handler).
(defun %vm-gen-catch (tag-ir body-ir cs)
  (let ((l-handler (%vm-cs-label cs)) (l-end (%vm-cs-label cs)))
    (%vm-gen tag-ir cs)                                   ; push tag (+1)
    (%vm-emit-jump cs (%vm-op :catch-open) l-handler)     ; CATCH_OPEN pops the tag
    (%vm-cs-sp+ cs -1)
    (let ((base (%vm-cs-sp cs)))
      (%vm-cs-push-control cs (list :dframe :catch))      ; a lexical exit must pop this frame
      (%vm-gen-for-values body-ir cs)                     ; body value + MV buffer (sp = base+1)
      (%vm-cs-pop-control cs)
      (%vm-emit-op cs (%vm-op :catch-close))              ; drop the frame; value stays
      (%vm-emit-jump cs (%vm-op :jump) l-end)
      (%vm-emit-label cs l-handler)                       ; THROW resume: VM pushed the primary
      (%vm-cs-set-sp cs (1+ base))
      (%vm-emit-label cs l-end))))

(defun %vm-gen-throw (tag-ir value-ir cs)
  (let ((s (%vm-cs-sp cs)))
    (%vm-gen tag-ir cs)                                   ; push tag
    (%vm-gen-for-values value-ir cs)                      ; push primary + set MV buffer
    (%vm-emit-op cs (%vm-op :throw))                      ; never returns
    (%vm-cs-set-sp cs (1+ s))))                            ; phantom value (dead continuation)

;;; HANDLER-BIND: a no-cleanup dynamic frame the SIGNAL machinery walks and CALLS into.
;;; Build the ((type-class . handler-fn) ...) list, push the frame, run body, pop.
(defun %vm-gen-handler-bind (clauses body-ir cs)
  (let ((list-ir (list* :call-named 'list
                        (mapcar (lambda (cl) (list :call-named 'cons (car cl) (cdr cl)))
                                clauses))))
    (%vm-gen list-ir cs)                                  ; the bindings list on stack (+1)
    (%vm-emit-op cs (%vm-op :handler-bind-open)) (%vm-cs-sp+ cs -1)  ; pop list, push frame
    (%vm-cs-push-control cs (list :dframe :handler))      ; a lexical exit must pop this frame
    (%vm-gen-for-values body-ir cs)                       ; body value + MV
    (%vm-cs-pop-control cs)
    (%vm-emit-op cs (%vm-op :handler-bind-close))))       ; drop the frame (no stack effect)

;;; RESTART-BIND: mirror HANDLER-BIND.  Each spec is (name-ir fn-ir ifn-ir tfn-ir); build
;;; (list (list name fn ifn tfn) ...) at runtime, push a frame_restart, run body, pop.
(defun %vm-gen-restart-bind (specs body-ir cs)
  (let ((list-ir (list* :call-named 'list
                        (mapcar (lambda (s) (list* :call-named 'list s)) specs))))
    (%vm-gen list-ir cs)                                  ; the restarts list on stack (+1)
    (%vm-emit-op cs (%vm-op :restart-bind-open)) (%vm-cs-sp+ cs -1)  ; pop list, push frame
    (%vm-cs-push-control cs (list :dframe :restart))      ; a lexical exit must pop this frame
    (%vm-gen-for-values body-ir cs)                       ; body value + MV
    (%vm-cs-pop-control cs)
    (%vm-emit-op cs (%vm-op :restart-bind-close))))       ; drop the frame (no stack effect)

;;; UNWIND-PROTECT: the cleanup runs on BOTH normal completion and any non-local exit
;;; through the protected form.  A frame_protect on g_frame_stack catches the unwind;
;;; UNWIND_NORMAL / UNWIND_END drive the normal path (save protected values, run
;;; cleanup for effect, restore) and the unwind path (run cleanup, continue cascade).
(defun %vm-gen-unwind-protect (protected-ir cleanup-ir cs)
  (let ((l-cleanup (%vm-cs-label cs)) (l-after (%vm-cs-label cs)))
    (%vm-emit-jump cs (%vm-op :unwind-protect) l-cleanup) ; open frame; handler_sp = base
    (let ((base (%vm-cs-sp cs)))
      (%vm-cs-push-control cs (list :dframe :uprotect cleanup-ir)) ; lexical exit runs cleanup+pops
      (%vm-gen-for-values protected-ir cs)               ; protected value + MV (sp = base+1)
      (%vm-cs-pop-control cs)
      (%vm-emit-op cs (%vm-op :unwind-normal))           ; save, drop frame, sp -> base
      (%vm-cs-set-sp cs base)
      (%vm-emit-label cs l-cleanup)                      ; unwind path resumes here too
      (%vm-gen cleanup-ir cs)                            ; cleanup for effect (+1)
      (%vm-emit-op cs (%vm-op :pop)) (%vm-cs-sp+ cs -1)  ; discard cleanup value
      (%vm-emit-op cs (%vm-op :unwind-end))              ; normal: restore value; unwind: cascade
      (%vm-cs-set-sp cs (1+ base))                       ; protected value back on top
      (%vm-emit-label cs l-after))))

;;; PROGV — bind a RUNTIME list of special symbols to a runtime list of values.
(defun %vm-gen-progv (symbols-ir values-ir body-ir cs)
  (%vm-gen symbols-ir cs)                              ; push the symbols list (+1)
  (%vm-gen values-ir cs)                               ; push the values list (+1)
  (%vm-emit-op cs (%vm-op :progv-bind)) (%vm-cs-sp+ cs -2)  ; bind all; frame -> g_frame_stack
  (%vm-cs-push-control cs (list :dframe :progv))       ; a lexical exit must restore+pop
  (%vm-gen-for-values body-ir cs)                      ; body value + MV (+1)
  (%vm-cs-pop-control cs)
  (%vm-emit-op cs (%vm-op :progv-unbind)))             ; restore all + pop (no stack effect)

;;; LOAD-TIME-VALUE — evaluate the form now (compile == load time for %VM-COMPILE)
;;; and embed the result as a constant.
(defun %vm-gen-load-time-value (form read-only-p cs)
  (declare (ignore read-only-p))
  (%vm-gen-const (funcall (%vm-compile (list 'lambda '() form))) cs))

(defun %vm-gen-flet (fn-irs body-ir cs)
  "FLET / LABELS: build each local function (a closure over the current frame, or a
   plain template if it captures nothing) and store it in its enclosing-frame slot;
   then run the body.  Visibility (FLET bodies don't see the locals, LABELS do, and a
   LABELS fn sees its siblings via the shared frame) was resolved by the analyzer."
  (dolist (f fn-irs)
    (%vm-gen-make-closure (cdr f) cs)                    ; closure/template on stack (+1)
    (%vm-emit-op cs (%vm-op :store-local)) (%vm-emit-uvar cs 0) (%vm-emit-uvar cs (car f))
    (%vm-emit-op cs (%vm-op :pop)) (%vm-cs-sp+ cs -1))   ; store into slot, drop (net 0)
  (%vm-gen body-ir cs))

(defun %vm-gen-make-closure (lambda-ir cs)
  "A LAMBDA in value position.  Compile it to a template; if it captures outer
   variables (a depth>0 reference), emit MAKE-CLOSURE to pair the template with a
   display over the current activation's frame — otherwise push the template as a
   plain constant (no capture needed)."
  (multiple-value-bind (tmpl ndepth) (%vm-compile-lambda lambda-ir)
    ;; If the nested fn reaches depth N, the ENCLOSING fn must itself capture the
    ;; frames beyond its own (depth N-1) so its display can relay them — propagate.
    (when (>= ndepth 2) (%vm-cs-bump-depth cs (1- ndepth)))
    (if (>= ndepth 1)                                   ; captures at least the current frame
        (progn (%vm-emit-op cs (%vm-op :make-closure)) (%vm-emit-uvar cs (%vm-cs-const cs tmpl)))
        (progn (%vm-emit-op cs (%vm-op :push-const))   (%vm-emit-uvar cs (%vm-cs-const cs tmpl))))
    (%vm-cs-sp+ cs 1)))

(defun %vm-gen-mv-prog1 (first-ir rest-irs cs)
  (%vm-gen-for-values first-ir cs)                     ; primary + MV buffer
  (%vm-emit-op cs (%vm-op :mv-list))                   ; L = first's value list (net 0)
  (dolist (r rest-irs)                                 ; the rest, for effect
    (%vm-gen r cs) (%vm-emit-op cs (%vm-op :pop)) (%vm-cs-sp+ cs -1))
  (%vm-emit-op cs (%vm-op :call-named))                ; (values-list L) -> restore the values
  (%vm-emit-uvar cs (%vm-cs-const cs 'values-list)) (%vm-emit-u8 cs 1)
  (%vm-cs-sp+ cs (- 1 1)))

;;; --- lexical control: BLOCK / RETURN-FROM / TAGBODY / GO as byte-code jumps ---
;;; Within a single compiled function (no compiled closures until vm-6) every block
;;; and tag is NON-ESCAPING, so an exit is a forward/backward JUMP.  Because the VM
;;; is a stack machine, an exit taken from inside a sub-expression must first CUT the
;;; operand stack back to the block/tagbody base; the exit value travels through a
;;; scratch local slot so both the normal and the RETURN-FROM path converge.

(defun %vm-emit-drop (cs n)
  "Pop N values off the operand stack (DROP's count is u8; chunk if > 255)."
  (when (> n 0)
    (let ((remaining n))
      (loop while (> remaining 0) do
        (let ((k (min remaining 255)))
          (%vm-emit-op cs (%vm-op :drop)) (%vm-emit-u8 cs k)
          (setq remaining (- remaining k))))
      (%vm-cs-sp+ cs (- n)))))

(defun %vm-gen-block (name body-ir cs)
  (let* ((base  (%vm-cs-sp cs))
         (slot  (%vm-cs-scratch cs))
         (l-end (%vm-cs-label cs)))
    (%vm-cs-push-control cs (list :block name base slot l-end))
    (%vm-gen-for-values body-ir cs)                       ; value on top + MV buffer set
    (%vm-cs-pop-control cs)
    ;; STORE/POP/LOAD carry only the primary through the slot, but do NOT touch the MV
    ;; buffer — so the full multiple values survive to the block's RETURN.
    (%vm-emit-op cs (%vm-op :store-local)) (%vm-emit-uvar cs 0) (%vm-emit-uvar cs slot)
    (%vm-emit-op cs (%vm-op :pop)) (%vm-cs-sp+ cs -1)      ; body value -> slot, sp = base
    (%vm-emit-label cs l-end)
    (%vm-emit-op cs (%vm-op :load-local)) (%vm-emit-uvar cs 0) (%vm-emit-uvar cs slot)
    (%vm-cs-sp+ cs 1)))                                    ; block value on top (sp = base+1)

(defun %vm-emit-unwind-to (cs target-scope)
  "Emit the dynamic-frame unwinds for every dynamic frame between here and
   TARGET-SCOPE (exclusive), innermost first — so a lexical RETURN-FROM/GO that
   crosses CATCH / UNWIND-PROTECT / special bindings runs their cleanups and pops
   their frames, exactly as a runtime non-local exit would.  (These pops/cleanups
   are net-0 on the operand stack.)"
  (dolist (s (svref cs 6))
    (when (eq s target-scope) (return))
    (when (eq (car s) :dframe)
      (case (cadr s)
        (:dbind    (%vm-emit-op cs (%vm-op :dynamic-unbind)))
        (:progv    (%vm-emit-op cs (%vm-op :progv-unbind)))
        (:handler  (%vm-emit-op cs (%vm-op :handler-bind-close)))
        (:restart  (%vm-emit-op cs (%vm-op :restart-bind-close)))
        (:catch    (%vm-emit-op cs (%vm-op :catch-close)))
        (:uprotect (%vm-gen (caddr s) cs)                  ; run the cleanup for effect
                   (%vm-emit-op cs (%vm-op :pop)) (%vm-cs-sp+ cs -1)
                   (%vm-emit-op cs (%vm-op :catch-close)))))))  ; then pop the protect frame

(defun %vm-gen-return-from (name value-ir cs)
  (let ((scope (%vm-cs-find-block cs name)))
    (unless scope
      (error "VM codegen: RETURN-FROM ~S has no lexically visible block (escaping is vm-6)" name))
    (let ((base (caddr scope)) (slot (cadddr scope)) (l-end (nth 4 scope))
          (site (%vm-cs-sp cs)))
      (%vm-gen-for-values value-ir cs)                     ; value + MV buffer (sp = site+1)
      (%vm-emit-op cs (%vm-op :store-local)) (%vm-emit-uvar cs 0) (%vm-emit-uvar cs slot)
      (%vm-emit-op cs (%vm-op :pop)) (%vm-cs-sp+ cs -1)    ; value -> slot, sp = site
      (%vm-emit-unwind-to cs scope)                       ; unwind crossed dynamic frames
      (%vm-emit-drop cs (- site base))                    ; cut intervening -> sp = base
      (%vm-emit-jump cs (%vm-op :jump) l-end)
      (%vm-cs-set-sp cs (1+ site)))))                      ; phantom value for dead continuation

(defun %vm-gen-tagbody (items cs)
  (let* ((base (%vm-cs-sp cs))
         (tag-labels (let ((acc '()))
                       (dolist (it items (nreverse acc))
                         (when (eq (car it) :tag)
                           (push (cons (cadr it) (%vm-cs-label cs)) acc))))))
    (%vm-cs-push-control cs (list :tagbody base tag-labels))
    (dolist (it items)
      (case (car it)
        (:tag  (%vm-emit-label cs (cdr (assoc (cadr it) tag-labels :test #'eql))))
        (:stmt (%vm-gen (cadr it) cs)                      ; statement value (+1)
               (%vm-emit-op cs (%vm-op :pop)) (%vm-cs-sp+ cs -1))))  ; ... dropped (effect)
    (%vm-cs-pop-control cs)
    (%vm-gen-const nil cs)))                               ; TAGBODY returns NIL

(defun %vm-gen-go (tag cs)
  (let ((scope (%vm-cs-find-tag cs tag)) (site (%vm-cs-sp cs)))
    (unless scope
      (error "VM codegen: GO ~S has no lexically visible tag (escaping is vm-6)" tag))
    (let ((base (cadr scope)) (label (cdr (assoc tag (caddr scope) :test #'eql))))
      (%vm-emit-unwind-to cs scope)                       ; unwind crossed dynamic frames
      (%vm-emit-drop cs (- site base))                    ; cut to tagbody base
      (%vm-emit-jump cs (%vm-op :jump) label)
      (%vm-cs-set-sp cs (1+ site)))))                      ; phantom value for dead continuation

;;; --- top-level: analyze + generate + box ---
(defun %vm-pack-arity (a)
  "(:arity nreq nopt rest-p key-p aok-p) -> the BC_ARITY_PACK fixnum."
  (logior (logand (cadr a) #xff)
          (ash (logand (caddr a) #xff) 8)
          (if (cadddr a)   (ash 1 16) 0)
          (if (nth 4 a)    (ash 1 17) 0)
          (if (nth 5 a)    (ash 1 18) 0)))

(defun %vm-complex-lambda-list-p (params)
  "T if the lambda list needs the RAW-entry + prologue path.  The fast VM entry does
   required / &optional-defaulting-to-NIL / &rest only; an &optional with a non-NIL
   default or a supplied-p var, &key, or &aux needs the prologue."
  (or (getf params :key)
      (getf params :aux)
      (some (lambda (o) (or (caddr o) (cadr o))) (getf params :optional))))

;;; --- lambda-list PROLOGUE (vm-6 part 2): destructure the raw arg list (slot AL,
;;; put there by the RAW VM entry) into the analyzer's parameter slots. ---
(defun %vm-gen-effect (ir cs)                          ; gen IR, drop its value
  (%vm-gen ir cs) (%vm-emit-op cs (%vm-op :pop)) (%vm-cs-sp+ cs -1))

(defun %vm-gen-prologue-optional (o al cs)
  (let ((idx (car o)) (sidx (cadr o)) (def (caddr o))
        (al-ref (list :lref 0 al))
        (l-def (%vm-cs-label cs)) (l-end (%vm-cs-label cs)))
    (%vm-gen al-ref cs)                                 ; push AL (+1)
    (%vm-emit-jump cs (%vm-op :jump-if-nil) l-def) (%vm-cs-sp+ cs -1)  ; no more args -> default
    (let ((base (%vm-cs-sp cs)))
      (%vm-gen-effect (list :lset 0 idx (list :call-named 'car al-ref)) cs)
      (when sidx (%vm-gen-effect (list :lset 0 sidx '(:const t)) cs))
      (%vm-gen-effect (list :lset 0 al (list :call-named 'cdr al-ref)) cs)
      (%vm-emit-jump cs (%vm-op :jump) l-end)
      (%vm-emit-label cs l-def) (%vm-cs-set-sp cs base)
      (%vm-gen-effect (list :lset 0 idx (or def '(:const nil))) cs)
      (when sidx (%vm-gen-effect (list :lset 0 sidx '(:const nil)) cs))
      (%vm-emit-label cs l-end))))

(defun %vm-gen-prologue-key (k al marker cs)
  (let ((kw (car k)) (idx (cadr k)) (sidx (caddr k)) (def (cadddr k))
        (al-ref (list :lref 0 al))
        (l-sup (%vm-cs-label cs)) (l-end (%vm-cs-label cs)))
    ;; idx = (getf al kw marker); if it is the marker, the key was not supplied.
    (%vm-gen-effect (list :lset 0 idx
                          (list :call-named 'getf al-ref (list :const kw) (list :const marker))) cs)
    (%vm-gen (list :call-named 'eq (list :lref 0 idx) (list :const marker)) cs)  ; eq? (+1)
    (%vm-emit-jump cs (%vm-op :jump-if-nil) l-sup) (%vm-cs-sp+ cs -1)   ; not-eq -> supplied
    (let ((base (%vm-cs-sp cs)))
      (%vm-gen-effect (list :lset 0 idx (or def '(:const nil))) cs)
      (when sidx (%vm-gen-effect (list :lset 0 sidx '(:const nil)) cs))
      (%vm-emit-jump cs (%vm-op :jump) l-end)
      (%vm-emit-label cs l-sup) (%vm-cs-set-sp cs base)
      (when sidx (%vm-gen-effect (list :lset 0 sidx '(:const t)) cs))
      (%vm-emit-label cs l-end))))

(defun %vm-gen-prologue (params al marker cs)
  (let ((al-ref (list :lref 0 al)))
    (dolist (idx (getf params :required))              ; slot = (car al); al = (cdr al)
      (%vm-gen-effect (list :lset 0 idx (list :call-named 'car al-ref)) cs)
      (%vm-gen-effect (list :lset 0 al  (list :call-named 'cdr al-ref)) cs))
    (dolist (o (getf params :optional)) (%vm-gen-prologue-optional o al cs))
    (let ((r (getf params :rest)))                     ; rest = whatever is left (incl. keys)
      (when r (%vm-gen-effect (list :lset 0 r al-ref) cs)))
    (dolist (k (getf params :key)) (%vm-gen-prologue-key k al marker cs))
    (dolist (a (getf params :aux))                     ; aux = sequential init
      (%vm-gen-effect (list :lset 0 (car a) (or (cadr a) '(:const nil))) cs))))

;;; --- TAIL calls (vm-6b): a call in tail position reuses the caller's VM frame via the
;;; trampoline in bytecode_apply (constant C-stack space for loops-as-recursion). ---
(defun %vm-gen-tail (node cs)
  "Generate NODE in TAIL position.  A :call/:call-named with no dynamic frame active
   becomes a TAIL_CALL (the callee returns to OUR caller); tail context propagates
   through :if branches, :progn's last form, and let/let* bodies.  Everything else is a
   normal %VM-GEN — the function's trailing RETURN returns the value."
  (if (%vm-cs-dframe-active-p cs)
      (%vm-gen node cs)
      (case (car node)
        (:call-named (if (%vm-inline-call-p (cadr node) (length (cddr node)))
                         ;; an inline primitive is not a CALL — emit it (value on the stack)
                         ;; and let the function's trailing RETURN return that value.
                         (%vm-gen node cs)
                         (%vm-gen-tail-call-named (cadr node) (cddr node) cs)))
        (:call       (%vm-gen-tail-call (cadr node) (cddr node) cs))
        (:if         (%vm-gen-tail-if node cs))
        (:progn      (%vm-gen-tail-progn (cdr node) cs))
        (:let        (%vm-gen-let (cadr node) (caddr node) cs nil #'%vm-gen-tail))
        (:let*       (%vm-gen-let (cadr node) (caddr node) cs t   #'%vm-gen-tail))
        (t           (%vm-gen node cs)))))

(defun %vm-gen-tail-call-named (sym arg-irs cs)
  (let ((base (%vm-cs-sp cs)) (n (length arg-irs)))
    (dolist (a arg-irs) (%vm-gen a cs))                 ; args left-to-right
    (%vm-emit-op cs (%vm-op :tail-call-named))
    (%vm-emit-uvar cs (%vm-cs-const cs sym)) (%vm-emit-u8 cs n)
    (%vm-cs-set-sp cs (1+ base))))                       ; phantom value (dead continuation)

(defun %vm-gen-tail-call (fn-ir arg-irs cs)
  (let ((base (%vm-cs-sp cs)) (n (length arg-irs)))
    (%vm-gen fn-ir cs)                                   ; callee
    (dolist (a arg-irs) (%vm-gen a cs))                 ; args
    (%vm-emit-op cs (%vm-op :tail-call)) (%vm-emit-u8 cs n)
    (%vm-cs-set-sp cs (1+ base))))

(defun %vm-gen-tail-if (node cs)
  (let ((l-else (%vm-cs-label cs)) (l-end (%vm-cs-label cs)))
    (%vm-gen (cadr node) cs)
    (%vm-emit-jump cs (%vm-op :jump-if-nil) l-else) (%vm-cs-sp+ cs -1)
    (let ((base (%vm-cs-sp cs)))
      (%vm-gen-tail (caddr node) cs)
      (%vm-emit-jump cs (%vm-op :jump) l-end)
      (%vm-emit-label cs l-else) (%vm-cs-set-sp cs base)
      (%vm-gen-tail (cadddr node) cs)
      (%vm-emit-label cs l-end))))

(defun %vm-gen-tail-progn (forms cs)
  (cond ((null forms) (%vm-gen-const nil cs))
        (t (loop for r on forms do
             (if (cdr r)
                 (progn (%vm-gen (car r) cs) (%vm-emit-op cs (%vm-op :pop)) (%vm-cs-sp+ cs -1))
                 (%vm-gen-tail (car r) cs))))))

(defun %vm-compile-lambda (lir)
  "Generate a runnable CompiledFunction from a (:LAMBDA ...) IR node."
  (let* ((plist   (cdr lir))
         (params  (getf plist :params))
         (body    (getf plist :body))
         (source  (getf plist :source))
         (cs      (%vm-cs-new (getf plist :n-locals)))
         (complex (%vm-complex-lambda-list-p params)))
    (when complex
      ;; RAW entry left the whole arg list in slot 0; move it to a scratch slot AL and
      ;; destructure into the analyzer's parameter slots before the body runs.
      (let ((al (%vm-cs-scratch cs)) (marker (list :vm-key-unsupplied)))
        (%vm-gen-effect (list :lset 0 al (list :lref 0 0)) cs)
        (%vm-gen-prologue params al marker cs)))
    (%vm-gen-tail body cs)                             ; body in TAIL position (emits TAIL_CALLs)
    (%vm-emit-op cs (%vm-op :return))                 ; catches every non-tail-call fall-through
    (values
     (%make-compiled-function
      nil                                             ; anonymous template
      (cadr source)                                   ; lambda list (introspection)
      (%vm-assemble (svref cs 0))
      (make-array (svref cs 2) :initial-contents (svref cs 1))
      (let ((a (%vm-pack-arity (getf plist :arity))))
        (if complex (logior a (ash 1 19)) a))          ; RAW-entry bit (BC_ARITY_RAW)
      (max 1 (svref cs 4))                            ; max-stack (>= 1)
      (%vm-cs-nlocals cs)                             ; incl. scratch slots
      ;; store the source with a MACROEXPAND-ALL'd BODY so FUNCTION-LAMBDA-EXPRESSION
      ;; reports a macro-free body (CLHS 3.2.2 — as an interpreted COMPILE'd function
      ;; does).  Expand only the body forms — MACROEXPAND-ALL of the whole (LAMBDA ...)
      ;; would wrap it in (FUNCTION ...) since LAMBDA is itself a macro.
      (if (and (consp source) (eq (car source) 'lambda))
          (list* 'lambda (cadr source) (mapcar #'macroexpand-all (cddr source)))
          source))
     (%vm-cs-max-depth cs))))                         ; deepest upvalue depth reached

(defun %vm-compile (lambda-expr)
  "Analyze + byte-compile LAMBDA-EXPR into a runnable CompiledFunction.  The vm-4
   core path (no closures / control); wiring COMPILE / COMPILE-FILE to emit
   byte-code is vm-8."
  (%vm-compile-lambda (%vm-analyze lambda-expr)))

;;; ===========================================================================
;;; vm-8 part 2 — wire COMPILE to emit byte-code (with a safe fallback)
;;; ---------------------------------------------------------------------------
;;; COMPILE / %COMPILE-DEFINITION (defined earlier in the prelude) are REDEFINED here
;;; — vm-compiler.lisp loads last — so (COMPILE nil '(lambda ...)) / (COMPILE 'name)
;;; byte-compile via %VM-COMPILE.  When the definition is not a plain lambda, or the VM
;;; analyzer/codegen signals it cannot handle a form (RESTART-*, an unsupported special
;;; operator, ...), we FALL BACK to the original interpreted minimal compilation, so no
;;; program regresses — only the compilable functions gain the VM.
;;; ===========================================================================

(defvar *vm-compile-enable* t
  "When true, COMPILE byte-compiles a lambda via %VM-COMPILE (with a fallback to the
   interpreted minimal compilation).  Bind to NIL to force the interpreter.")

(defun %vm-lambda-of (definition)
  "The (LAMBDA ...) expression underlying a COMPILE DEFINITION, or NIL when there is
   none to byte-compile (a function value, or a (SETF ...) name form)."
  (cond ((and (consp definition) (eq (car definition) 'lambda)) definition)
        ((and (consp definition) (eq (car definition) 'function)
              (consp (cadr definition)) (eq (car (cadr definition)) 'lambda))
         (cadr definition))
        (t nil)))

(defun %vm-compile-interpreted (definition)
  "The original minimal-compilation path: an interpreted function whose body is fully
   MACROEXPAND-ALL'd (CLHS 3.2.2)."
  (cond
    ((functionp definition) definition)
    ((and (consp definition) (eq (car definition) 'lambda))
     (eval (macroexpand-all (list 'function definition))))
    ((and (consp definition) (eq (car definition) 'function))
     (eval (macroexpand-all definition)))
    (t (eval definition))))

(defun %compile-definition (definition)
  "Coerce DEFINITION to a function — byte-compiled via %VM-COMPILE when enabled and the
   definition is a lambda the VM can handle; otherwise the interpreted minimal-compiled
   function.  Any error from the VM path falls back rather than propagating."
  (let ((lam (and *vm-compile-enable* (%vm-lambda-of definition))))
    (if lam
        ;; Fall back on ANY serious condition, not just ERROR: byte-compiling a deeply
        ;; nested form can exhaust the control stack (a STORAGE-CONDITION, which is NOT an
        ;; ERROR) — the interpreted path is always correct, so never let a compile failure
        ;; escape.  (vm-9: this keeps COMPILE-FILE of a whole prelude from aborting.)
        (handler-case (%vm-compile lam)
          (serious-condition () (%vm-compile-interpreted definition)))
        (%vm-compile-interpreted definition))))

;;; Now that COMPILE emits real byte-code, CL:COMPILED-FUNCTION-P must recognise it
;;; (the prelude's stub always returned NIL — see its "bocl interprets" comment).
(defun compiled-function-p (object)
  "T if OBJECT is a compiled function — now that COMPILE can produce byte-code."
  (and (%compiled-function-p object) t))

;;; --- COMPILE-FILE hook: byte-compile DEFUN bodies (the load-speed payoff) ---
;;; COMPILE-FILE (prelude) macroexpand-all's each top-level form; DEFUN expands to
;;; (BOCL:DEFINE-FUNCTION 'name doc (FUNCTION (LAMBDA ...))).  This turns that into an
;;; install of the PRE-COMPILED template — cfasl serializes the template (vm-8 part 1),
;;; and LOAD deserializes + installs it with NO re-compilation / macroexpansion.  Any
;;; form the VM can't compile is left unchanged (loads interpreted, as before).
(defun %vm-compile-file-form (form)
  (if (and (consp form)
           (eq (car form) (and (find-package "BOCL")
                               (find-symbol "DEFINE-FUNCTION" (find-package "BOCL"))))
           (= (length form) 4)
           (let ((fn (fourth form)))
             (and (consp fn) (eq (car fn) 'function)
                  (consp (cadr fn)) (eq (car (cadr fn)) 'lambda))))
      (let ((template (handler-case (%vm-compile (cadr (fourth form)))
                        ;; SERIOUS-CONDITION, not ERROR: a stack overflow while compiling
                        ;; one form must leave the rest of the file compilable (this form
                        ;; simply stays interpreted).  vm-9.
                        (serious-condition () nil))))
        (if template
            (list (car form) (second form) (third form) (list 'quote template))
            form))
      form))

;;; ===========================================================================
;;; vm-9 — byte-code DISASSEMBLE
;;; ---------------------------------------------------------------------------
;;; A disassembler for CompiledFunction / CompiledClosure templates: it decodes the
;;; octet code stream into a readable opcode listing (mnemonic + operands, with
;;; constants and jump targets resolved).  DISASSEMBLE (CLHS 25.1) prints it and
;;; returns NIL.  The operand-format table must match the encodings the codegen emits
;;; and the VM decodes; %VM-DISASSEMBLE consumes the stream octet-for-octet and prints
;;; a warning if it does not decode cleanly (a self-check against table drift).
;;; ===========================================================================

(defparameter *vm-operand-formats*
  '((:push-const  :const)
    (:drop        :u8)
    (:load-local  :uvar :uvar)  (:store-local :uvar :uvar)
    (:load-global :gconst)      (:store-global :gconst)  (:load-function :gconst)
    (:make-closure :tmpl)
    (:jump :svar) (:jump-if-nil :svar) (:jump-if-not-nil :svar)
    (:call :u8) (:call-named :gconst :u8)
    (:tail-call :u8) (:tail-call-named :gconst :u8)
    (:values :u8)
    (:catch-open :svar) (:dynamic-bind :gconst) (:unwind-protect :svar))
  "Opcode keyword -> list of operand kinds (:u8 :uvar :svar :const :gconst :tmpl).
   Opcodes absent from this table take no operands.  :const/:gconst/:tmpl are a :uvar
   index into the constant pool (a value / a symbol / a nested template).")

(defun %vm-opcode-names ()
  "Alist (number . keyword) inverting *VM-OPCODES* (drops the :VERSION pseudo-entry)."
  (let ((out '()))
    (dolist (pair *vm-opcodes* out)
      (unless (eq (car pair) :version)
        (push (cons (cdr pair) (car pair)) out)))))

(defun %vm-code-octet (code i) (char-code (char code i)))

(defun %vm-decode-uvar (code pos)
  "Read an unsigned LEB128 varint at POS; return (values value new-pos)."
  (let ((val 0) (shift 0) (i pos))
    (loop
      (let ((b (%vm-code-octet code i)))
        (incf i)
        (setq val (logior val (ash (logand b #x7f) shift)))
        (incf shift 7)
        (when (zerop (logand b #x80)) (return))))
    (values val i)))

(defun %vm-decode-svar (code pos)
  "Read a zig-zag LEB128 signed varint at POS; return (values value new-pos)."
  (multiple-value-bind (z i) (%vm-decode-uvar code pos)
    (values (if (evenp z) (ash z -1) (- (- (ash z -1)) 1)) i)))

(defun %vm-disassemble (object &optional (stream *standard-output*))
  "Print a byte-code listing of OBJECT (a CompiledFunction / CompiledClosure) to STREAM.
   Returns NIL.  Warns if the code stream does not decode octet-for-octet."
  (multiple-value-bind (name code constants) (%compiled-function-parts object)
    (unless (stringp code)
      (format stream "~&;; ~S is not a byte-compiled function~%" object)
      (return-from %vm-disassemble nil))
    (let ((names (%vm-opcode-names)) (len (length code)) (pc 0))
      (format stream "~&;; disassembly of ~A (~D octets):~%"
              (if name (prin1-to-string name) "an anonymous function") len)
      (block walk
        (loop while (< pc len) do
          (let* ((op (%vm-code-octet code pc)) (kw (cdr (assoc op names))) (start pc))
            (incf pc)
            (when (null kw)
              (format stream ";; ~4D  <unknown opcode ~D> — stopping~%" start op)
              (return-from walk))
            (let ((opnds '()))
              (dolist (fmt (cdr (assoc kw *vm-operand-formats*)))
                (ecase fmt
                  (:u8   (push (format nil "~D" (%vm-code-octet code pc)) opnds) (incf pc))
                  (:uvar (multiple-value-bind (v np) (%vm-decode-uvar code pc)
                           (push (format nil "~D" v) opnds) (setq pc np)))
                  (:svar (multiple-value-bind (v np) (%vm-decode-svar code pc)
                           (push (format nil "@~D" (+ np v)) opnds) (setq pc np)))
                  ((:const :gconst :tmpl)
                   (multiple-value-bind (v np) (%vm-decode-uvar code pc)
                     (let ((k (and (vectorp constants) (< v (length constants)) (aref constants v))))
                       (push (if (eq fmt :tmpl)
                                 (format nil "~D=~:[<template>~;~:*~S~]" v
                                         (and (%compiled-function-p k)
                                              (nth-value 0 (%compiled-function-parts k))))
                                 (format nil "~D=~S" v k))
                             opnds))
                     (setq pc np)))))
              (format stream ";; ~4D  ~A~{ ~A~}~%"
                      start (string-downcase (symbol-name kw)) (nreverse opnds))))))
      (when (> pc len)
        (format stream ";; WARNING: decoded ~D octets past the ~D-octet code vector~%" pc len))))
  nil)

;;; DISASSEMBLE (redefined; vm-compiler loads last) now prints byte-code for a compiled
;;; function and still falls back to the macroexpanded lambda for an interpreted one.
(defun disassemble (fn)
  "Write FN's disassembly to *STANDARD-OUTPUT* and return NIL (CLHS 25.1).  For a
   byte-compiled function this is the VM opcode listing (%VM-DISASSEMBLE); for an
   interpreted function it is the macroexpanded lambda expression (bocl keeps no native
   code).  FN is a function, a function-name, or a (LAMBDA ...) expression (compiled
   first)."
  (let ((f (cond ((and (consp fn) (eq (car fn) 'lambda)) (compile nil fn))
                 ((and (symbolp fn) (fboundp fn))        (symbol-function fn))
                 (t                                       fn))))
    (if (%compiled-function-p f)
        (%vm-disassemble f)
        (let ((expr (function-lambda-expression f)))
          (when expr (fresh-line) (prin1 expr) (terpri))))
    nil))
