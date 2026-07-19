;; The intercalling lambda could stack any number of frame (here 1 or
;; 2, but we could have more fun).

(loop for x from 42 to 43
      collect (list 'block-odd
                    (block odd
                      (list 'block-even
                            (block even
                              (let (o e)
                                (setf o (lambda (x e)
                                          (if (oddp x)
                                              (return-from odd x)
                                              (funcall e x o)))
                                      e (lambda (x o)
                                          (if (evenp x)
                                              (return-from even x)
                                              (funcall o x e))))
                      (funcall o x e)))))))
--> ((BLOCK-ODD (BLOCK-EVEN 42))
     (BLOCK-ODD 43))

;; In this example, two embedded blocks with the same name are
;; referenced in two return-from; lexically, the reference is clear.
;; The intercalling lambda could stack any number of frame (here 1 or
;; 2, but we could have more fun).

(loop for x from 42 to 43
      collect (let (o e)
                (list 'block-odd
                      (block operation
                        (setf o (lambda (x e)
                                  (if (oddp x)
                                      (return-from operation x)
                                      (funcall e x o))))
                        (list 'block-even
                              (block operation
                                (setf e (lambda (x o)
                                          (if (evenp x)
                                              (return-from operation x)
                                              (funcall o x e))))
                                (funcall o x e)))))))
--> ((BLOCK-ODD (BLOCK-EVEN 42))
     (BLOCK-ODD 43))

(defun one-of (seq)
  (elt seq (random (length seq))))

(let (closures selected)
  (setf selected
        (block return-once
          (setf closures (loop for i below 3
                               collect (let ((i i))
                                         (lambda () (return-from return-once i)))))
          (funcall (one-of closures))))

  (mapcar 'funcall closures) ; ==> run-time error

  selected)

;; When returning from a block that has expired, 
;;  ==> CCL says: Can't throw to tag (71452419 . 71601174)
;; [Condition of type ccl::cant-throw-error]

;; 1- we want to signal a more descriptive condition:
;; "Cannot return-from the block FOO, it has already exited."

(define-condition RETURN-FROM-EXITED-BLOCK-ERROR (program-error simple-error)
  ((block-name :initarg :name :reader RETURN-FROM-EXITED-BLOCK-ERROR-block-name)))

(error 'return-from-edited-block-error
       :block-name 'return-once
       :format-control "Cannot return-from the block ~S, it has already exited."
       :format-arguments (list 'return-once))

;; 2- The error must be signaled in the lexical and dynamic context of
;; the RETURN-FROM form.  Ie. before unwinding the stack-frames, in
;; case a HANDLER-BIND is present and provides an alternative
;; non-local exit. So we must search first for the block, if found,
;; unwind the stack and jump, if not found signal the error.

;; 3- Similarly for GO.


;;; RESOLUTION (2026-07-17) — verified on vm head (v1.8.49)
;;; ------------------------------------------------------
;;; All the scenarios above now behave as specified, interpreted AND
;;; byte-compiled (COMPILE):
;;;
;;; - Both LOOP examples (mutually-escaping closures across two blocks;
;;;   two same-named nested blocks resolved lexically) return
;;;   ((BLOCK-ODD (BLOCK-EVEN 42)) (BLOCK-ODD 43)).
;;;
;;; 1- RETURN-FROM on an exited block signals the descriptive condition
;;;    BOCL:RETURN-FROM-EXITED-BLOCK-ERROR, a subclass of PROGRAM-ERROR,
;;;    with the reader BOCL:RETURN-FROM-EXITED-BLOCK-ERROR-BLOCK-NAME
;;;    giving the block name.
;;;
;;; 2- It is signaled in the dynamic context of the RETURN-FROM, before
;;;    any unwinding: a HANDLER-BIND handler can perform an alternative
;;;    non-local exit (verified: returning from a live outer block from
;;;    inside the handler works).
;;;
;;; 3- GO to an exited TAGBODY likewise signals
;;;    BOCL:GO-EXITED-TAGBODY-ERROR (interpreted and compiled).
;;;
;;; Covered by the test suite: tests/test_misc.c "vm escaping blocks" and
;;; "vm exited extent".  Remaining nit — (format nil "~A" condition)
;;; prints #<RETURN-FROM-EXITED-BLOCK-ERROR 0x...> instead of the report
;;; string; that is the separate issues/open/printing-conditions.issue.
