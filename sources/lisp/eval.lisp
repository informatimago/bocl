;;;; -*- mode:lisp;coding:iso-8859-1 -*-

;;; This file contains a prototype of the interpreter EVAL function
;;; equivalent to the KERNEL:EVAL function, implemented in COMMON-LISP.

(defpackage "COM.INFORMATIMAGO.BOCL.PROTOTYPE.EVAL"
  (:use "COMMON-LISP")
  (:use "COM.INFORMATIMAGO.COMMON-LISP.LISP-SEXP.SOURCE-FORM"))
(in-package "COM.INFORMATIMAGO.BOCL.PROTOTYPE.EVAL")



(defun check-arg-count (arguments min max)
  (assert (<= min (length arguments) max)))

(defun check-arg-count/even (arguments)
  (assert (evenp (length arguments))))

(defun check-arg-count/min (arguments min)
  (assert (<= min (length arguments))))

(defstruct (eval-counters
            (:conc-name ec-))
  (call                 0)
  (variable             0)
  (literal              0)
  (compiler-macro       0)
  (global-function      0)
  (local-function       0)
  (global-macro         0)
  (local-macro          0)
  (lambda-function      0)
  (quote                0)
  (function             0)
  (setq                 0)
  (if                   0)
  (block                0)
  (return-from          0)
  (catch                0)
  (throw                0)
  (unwind-protect       0)
  (tagbody              0)
  (go                   0)
  (let                  0)
  (let*                 0)
  (flet                 0)
  (labels               0)
  (symbol-macrolet      0)
  (macrolet             0)
  (multiple-value-call  0)
  (multiple-value-prog1 0)
  (progn                0)
  (progv                0)
  (locally              0)
  (the                  0)
  (eval-when            0)
  (load-time-value      0))

(defparameter *eval-counters* (make-eval-counters))
(defvar kernel:*load-pathname*     nil)
(defvar kernel:*loading-fasl-file* nil)

(defun kernel:evale (form frame)
  ;; This eval function is called on forms that have been minimally-compiled,
  ;; ie. fully macroexpanded.

  (cond

    ((symbolp form)
     (incf (ec-variable *eval-counters*))
     ;; Symbol macros must have been expanded by the minimal
     ;; compilation, so we only have variables.  Variables are
     ;; substituted by minimal-compilation by calls to
     ;; kernel:variable-value or kernel:symbol-value.
     ;; TODO: What about dynamically bound variables (eg. with PROGV?)
     (if (kernel:symbol-boundp form)
         (values (kernel:symbol-value form))
         (error "Dynamic variable ~S is not bound" form)))

    ((atom form)
     ;; The other atoms are unchanged literals:
     (values form))

    (t
     ;; Now we have a list.
     (let ((operator  (car form))
           (arguments (cdr form)))
       (case operator
         ;; First we check the special operators:

         ((bocl:QUOTE)
          (incf (ec-quote *eval-counters*))
          (check-arg-count arguments 1 1)
          (values (car arguments)))

         ((bocl:FUNCTION)
          (incf (ec-function *eval-counters*))
          (check-arg-count arguments 1 1)
          ;; We only are left here only with symbols naming global functions.
          (let ((fname (car arguments)))
            (if (symbolp fname)
                (values (kernel:symbol-function fname))
                (error "No such function ~S" fname))))

         ((bocl:IF)
          (incf (ec-if *eval-counters*))
          (check-arg-count arguments 2 3)
          (if (kernel:evale (second form) frame)
              (kernel:evale (third  form) frame)
              (kernel:evale (fourth form) frame)))

         ((bocl:SETQ)
          (incf (ec-setq *eval-counters*))
          ;; SETQ forms are replaced by the minimal-compilation with
          ;; calls to kernel:symbol-value or kernel:variable-value.
          (error "There should remain no SETQ after minimal-compilation. ~S" form))

         ;; ((bocl:SETQ)
         ;;  (incf (ec-setq *eval-counters*))
         ;;  ;; After minimal compilation, the variables are not symbol-macros.
         ;;  (check-arg-count/even arguments)
         ;;  (loop
         ;;    with result = nil
         ;;    for (varname expr) on (cdr form) by (function cddr)
         ;;    do (let ((primary-value (kernel:evale expr frame)))
         ;;         (env-set-variable-value varname frame)
         ;;         (setf result primary-value))
         ;;    finally (return (values result))))

         ((bocl:PROGN)
          (incf (ec-progn *eval-counters*))
          (let ((results nil))
            (dolist (subform arguments (values-list results))
              (setf results (multiple-value-list (kernel:evale subform frame))))))


         ((bocl:LOCALLY)
          (incf (ec-locally *eval-counters*))
          ;; LOCALLY forms are normally replaced by the
          ;; minimal-compilation with PROGN forms.  But we'll just
          ;; skip the declaration and process like a PROGN.
          (let ((results nil))
            (loop
              :with body := nil
              :for subform :in arguments
              :if (and (consp subform)
                       (eql 'bocl:declare (car subform)))
                :do (if body
                        (error "DECLARE in the middle of a LOCALLY body."))
              :else
                :do (setf body t)
                    (setf results (multiple-value-list (kernel:evale subform frame))))
            (values-list results)))

         ((bocl:LET)
          (incf (ec-let *eval-counters*))
          (check-arg-count/min arguments 1)
          (let ((bindings (car arguments))
                (specials '())
                (body     (cdr arguments)))
            ;; find the special declarations
            (loop
              :while (and (consp body)
                          (consp (car body))
                          (eql 'bocl:declare (caar body)))
              :do (dolist (decl (cdr (pop body)))
                    (if (eql 'bocl:special (car decl))
                        (setf specials (append (cdr decl) specials)))))
            ;; evaluate the initial-values in frame, and
            ;; bind the symbols to the values in new-frame
            (let ((new-frame (kernel:make-protect-frame
                              :next frame
                              :bindings (kernel:make-bindings (and frame (kernel:frame-bindings frame))
                                                              (length bindings)
                                                              0)))
                  (has-dynamic-binding nil))
              (dolist (binding bindings)
                (let (name value var)
                  (if (consp binding)
                      (setf name (car binding)
                            value (kernel:evale (cadr binding) frame))
                      (setf name binding
                            value nil))
                  (let ((kernel:*frame* new-frame))
                    (if (or (special-variable-p name)
                            (member name specials))
                        (progn
                          (setf has-dynamic-binding t)
                          (let* ((saved-boundp (kernel:symbol-boundp name))
                                 (saved-value (if saved-boundp
                                                  (kernel:symbol-value name)
                                                  nil)))
                            ;; TODO get the vindex and findex from the minimal-compile !
                            (kernel:set-variable-value (cons saved-boundp saved-value) vindex findex)
                            (kernel:set-symbol-value value name)))
                        ;; TODO get the vindex and findex from the minimal-compile !
                        (kernel:set-variable-value value vindex findex)))))
              (let ((kernel:*frame* new-frame))
                (if has-dynamic-binding
                    (unwind-protect
                         (let ((results nil))
                           (dolist (subform body (values-list results))
                             (setf results (multiple-value-list (kernel:evale subform frame)))))
                      (restore-dynamic-variables))
                    (let ((results nil))
                      (dolist (subform body (values-list results))
                        (setf results (multiple-value-list (kernel:evale subform frame))))))))))

         ;; No LET* since it's minimal-compiled out into LET.
         ;; ((LET*))

         ((bocl:PROGV)
          (incf (ec-progv *eval-counters*))
          (check-arg-count/min arguments 2)
          ;; Dynamically bind the symbols to the values.
          (let ((symbols (kernel:evale (car  arguments) kernel:*frame*))
                (values  (kernel:evale (cadr arguments) kernel:*frame*))
                (body    (cddr arguments))
                ;; We create a frame to save the old values, but we don't
                ;; put it on the frame list, because the variables are
                ;; only known at run-time.
                (new-frame (kernel:make-protect-frame
                            :next frame
                            :bindings (and frame (kernel:frame-bindings frame)))))
            (let ((saved-values '()))
              (dolist (name symbols)
                (let* ((boundp  (kernel:symbol-boundp name))
                       (value   (if boundp (kernel:symbol-value name) nil)))
                  (push (list name boundp value) saved-values)
                  (if (consp values)
                      (kernel:set-symbol-value name (pop values))
                      (kernel:symbol-make-unbound name))))
              ;; Unwind-protect the body evaluation
              (unwind-protect
                   (let ((results nil))
                     (dolist (subform body (values-list results))
                       (setf results (multiple-value-list (kernel:evale subform frame)))))
                ;; restore the dynamic bindings
                (dolist (saved saved-values)
                  (if (second saved)
                      (kernel:set-symbol-value (first saved) (third saved))
                      (kernel:symbol-make-unbound (first saved))))))))

         ((bocl:FLET)
          (incf (ec-flet *eval-counters*))
          (error "Not implemented yet FLET"))

         ((bocl:LABELS)
          (incf (ec-labels *eval-counters*))
          (error "Not implemented yet LABELS"))

         ((bocl:SYMBOL-MACROLET)
          (incf (ec-symbol-macrolet *eval-counters*))
          (error "Not implemented yet SYMBOL-MACROLET"))

         ((bocl:MACROLET)
          (incf (ec-macrolet *eval-counters*))
          (error "Not implemented yet MACROLET"))

         ((bocl:LOAD-TIME-VALUE)
          (incf (ec-load-time-value *eval-counters*))
          (error "Not implemented yet LOAD-TIME-VALUE"))

         ((bocl:MULTIPLE-VALUE-CALL)
          (incf (ec-multiple-value-call *eval-counters*))
          (check-arg-count/min arguments 1)
          (let* ((function (kernel:evale (car arguments) frame))
                 (forms    (cdr arguments))
                 (args     (list '()))
                 (last     args))
            (dolist (subform forms)
              (setf (cdr last) (multiple-value-list (kernel:evale subform frame))
                    last (last last)))
            (apply function args)))

         ((bocl:MULTIPLE-VALUE-PROG1)
          (incf (ec-multiple-value-prog1 *eval-counters*))
          (check-arg-count/min arguments 1)
          (let ((first-form (car arguments))
                (other-forms (cdr arguments))
                (results1))
            (setf results1 (multiple-value-list (kernel:evale first-form frame)))
            (dolist (subform other-forms)
              (kernel:evale subform frame))
            (values-list results1)))

         ((bocl:THE)
          (incf (ec-the *eval-counters*))
          (check-arg-count arguments 2 2)
          (kernel:evale (cadr arguments) frame))

         ((bocl:EVAL-WHEN)
          (incf (ec-eval-when *eval-counters*))
          (check-arg-count/min arguments 1)
          (let ((situations (car arguments))
                (body       (cdr arguments)))
            (if (null situations)
                (values nil)
                (let ((doit nil))
                  (dolist (situation situations)
                    (cond
                      ((or (eql situation :compile-toplevel)
                           (eql situation 'kernel:compile))
                       (when (frame-is-toplevel frame)
                         (setf doit t)
                         (return)))
                      ((or (eql situation :load-toplevel)
                           (eql situation 'kernel:load))
                       (when (and (frame-is-toplevel frame)
                                  (not (null kernel:*load-pathname*))
                                  (not (null kernel:*loading-fasl-file*)))
                         (setf doit t)
                         (return)))
                      ((or (eql situation :execute)
                           (eql situation 'kernel:eval))
                       (when (null kernel:*loading-fasl-file*)
                         (setf doit t)
                         (return)))
                      (t
                       (error "Invalid EVAL-WHEN situation: ~S" situation))))
                  (if doit
                      (let ((results nil))
                        (dolist (subform body (values-list results))
                          (setf results (multiple-value-list
                                         (kernel:evale subform frame)))))
                      (values nil))))))

         ((bocl:UNWIND-PROTECT)
          (incf (ec-unwind-protect *eval-counters*))
          (check-arg-count/min arguments 1)
          (let ((proform (car arguments))
                (cleanup (cdr arguments)))
            (unwind-protect
                 (kernel:evale proform frame)
              (dolist (subform cleanup)
                (kernel:evale subform frame)))))

         ((bocl:BLOCK)
          (incf (ec-block *eval-counters*))
          (check-arg-count/min arguments 1)
          (let ((name (car arguments))
                (body (cdr arguments)))
            (let ((frame (kernel:make-block-frame :name name :next frame)))
              (error "Not implemented yet ~S" 'bocl:block))))

         ((bocl:RETURN-FROM)
          (check-arg-count arguments 1 2)
          (error "Not implemented yet ~S" 'bocl:return-from))

         ((bocl:CATCH)   (error "Not implemented yet ~S" 'bocl:catch))
         ((bocl:THROW)   (error "Not implemented yet ~S" 'bocl:throw))

         ((bocl:TAGBODY) (error "Not implemented yet ~S" 'bocl:tagbody))
         ((bocl:GO)      (error "Not implemented yet ~S" 'bocl:go))

         (otherwise
          ;; there should not remain any macro, so only a function call:
          (kernel:call-function (car form) (cdr form))))))))

(defun kernel:eval (form)
  (kernel:evale form nil))


;;;; THE END ;;;;
