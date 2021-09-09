

(eval-when (:compile-toplevel)
  (print '(in test-compile :compile-toplevel))
  (print `(*COMPILE-FILE-PATHNAME* ,*COMPILE-FILE-PATHNAME*))
  (print `(*LOAD-PATHNAME* ,*LOAD-PATHNAME*))
  (load "test-load.lisp"))

(eval-when (:load-toplevel)
  (print '(in test-compile :load-toplevel))
  (print `(*COMPILE-FILE-PATHNAME* ,*COMPILE-FILE-PATHNAME*))
  (print `(*LOAD-PATHNAME* ,*LOAD-PATHNAME*))
  (load "test-load.dx64fsl"))

(eval-when (:execute)
  (print '(in test-compile :execute))
  (print `(*COMPILE-FILE-PATHNAME* ,*COMPILE-FILE-PATHNAME*))
  (print `(*LOAD-PATHNAME* ,*LOAD-PATHNAME*))
  (load "test-load.lisp"))

(defun foo () 'foo)
