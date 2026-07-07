
(defpackage "FACTORIAL"
  (:use "COMMON-LISP")
  (:export "FACT"))
(in-package  "FACTORIAL")

(defun recursive-factorial (n)
  (if (= 1 n)
      1
      (* n (fact (- n 1)))))

(defun iterative-factorial (n)
  (loop
    :with factorial := 1
    :while (< 1 n)
    :do (setf factorial (* factorial n)
              n (1- n))
    :finally (return factorial)))

(defun fact (n)
  (if (plusp n)
      (if (< n 32)
          (recursive-factorial n)
          (iterative-factorial n))
      (error 'simple-type-error :datum n :expected-type '(integer 1)
                                :format-control "FACT enpects a positive integer, not ~A"
                                :format-arguments (list n))))
 (provide 'factorial)

