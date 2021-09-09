

(eval-when (:compile-toplevel)
  (print '(in test-load :compile-toplevel))
  (print `(*COMPILE-FILE-PATHNAME* ,*COMPILE-FILE-PATHNAME*))
  (print `(*LOAD-PATHNAME* ,*LOAD-PATHNAME*)))

(eval-when (:load-toplevel)
  (print '(in test-load :load-toplevel))
  (print `(*COMPILE-FILE-PATHNAME* ,*COMPILE-FILE-PATHNAME*))
  (print `(*LOAD-PATHNAME* ,*LOAD-PATHNAME*)))

(eval-when (:execute)
  (print '(in test-load :execute))
  (print `(*COMPILE-FILE-PATHNAME* ,*COMPILE-FILE-PATHNAME*))
  (print `(*LOAD-PATHNAME* ,*LOAD-PATHNAME*)))
