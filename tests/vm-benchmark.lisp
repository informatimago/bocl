;;;; vm-benchmark.lisp — plan-2 byte-code VM vs the tree-walker (vm-9 validation).
;;;;
;;;; Runs a few representative compute kernels twice — interpreted (the tree-walker) and
;;;; COMPILE'd (the byte-code VM) — from the SAME source, and prints the speedup.  Not a
;;;; unit test (timings are machine/load dependent); a documented, reproducible A/B.
;;;;
;;;;   bocl --norc -b --load tests/vm-benchmark.lisp --quit </dev/null
;;;;
;;;; Representative results (see issues/closed/vm-9-debug-bootstrap-validate.issue):
;;;;   sumsq (tight arithmetic loop) ~9.8x   fib/tak (call-bound) ~1.0x
;;;;   revbuild (push in a loop)     ~0.7x   — call/alloc-bound; fast-apply (vm-6b) is the fix.
;;;; The VM wins on operation-/dispatch-bound bodies; call-bound code hits the shared
;;;; kernel_apply ~150us/funcall floor either way.

;;; vm-9 benchmark: tree-walker vs byte-code VM on representative compute kernels.
(defmacro elapsed (&body body)
  `(let ((start (get-internal-run-time))) ,@body
     (/ (float (- (get-internal-run-time) start)) internal-time-units-per-second)))

(defun bench1 (name interp-fn vm-fn call)
  (let* ((ti (elapsed (funcall call interp-fn)))
         (tv (elapsed (funcall call vm-fn))))
    (format t "~&~12A  interp ~8,3Fs   vm ~8,3Fs   speedup ~5,1Fx~%"
            name ti tv (if (zerop tv) 0.0 (/ ti tv)))))

;; naive fib (deep call-heavy recursion)
(defun fib (n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2)))))
;; tak (classic call benchmark)
(defun tak (x y z) (if (not (< y x)) z
                       (tak (tak (- x 1) y z) (tak (- y 1) z x) (tak (- z 1) x y))))
;; tight arithmetic loop
(defun loopsum (n) (let ((s 0)) (dotimes (i n s) (setq s (+ s (* i i))))))
;; list building/reversing
(defun revbuild (n) (let ((l nil)) (dotimes (i n) (push i l)) (length (reverse l))))

(defun vm (f) (compile nil (function-lambda-expression (symbol-function f))))
(let ((ifib #'fib)          (vfib (vm 'fib))
      (itak #'tak)          (vtak (vm 'tak))
      (iloop #'loopsum)     (vloop (vm 'loopsum))
      (irev #'revbuild)     (vrev (vm 'revbuild)))
  (format t "~&compiled? fib=~S tak=~S loopsum=~S revbuild=~S~%"
          (compiled-function-p vfib) (compiled-function-p vtak)
          (compiled-function-p vloop) (compiled-function-p vrev))
  (bench1 "fib 28"   ifib  vfib  (lambda (f) (funcall f 28)))
  (bench1 "tak"      itak  vtak  (lambda (f) (funcall f 22 12 6)))
  (bench1 "loopsum"  iloop vloop (lambda (f) (funcall f 2000000)))
  (bench1 "revbuild" irev  vrev  (lambda (f) (funcall f 300000))))
