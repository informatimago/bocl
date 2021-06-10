(in-package "BOCL")

(eval-when (:compile-toplevel :load-toplevel :execute)
  (defun label-to-functionname (label)
    (intern (format nil "%__tag__%-~A" label))))

(defmacro go (label)
  (let ((name (label-to-functionname label)))
    `(throw ,name (function ,name))))

(defmacro tagbody (&body body)
  (let* ((init-tag   (gensym))
         (go-tag     (gensym))
         (return-tag (gensym))
         (functions  (mapcon (lambda (seq &aux (label (car seq)) (s (cdr seq)))
                               (when (atom label)
                                 (let ((p (position-if #'atom s)))
                                   `((,(label-to-functionname label) ()
                                      ,@(subseq s 0 (or p (length s)))
                                      ,(if p
                                           `(,(label-to-functionname (elt s p)))
                                           `(throw ,return-tag 'nil)))))))
                             `(,init-tag ,@body))))
    `(let* ((,go-tag (list nil)) (,return-tag (list nil))
            ,@(mapcar (lambda (f) `(,(car f) ,go-tag)) functions))
       (catch ,return-tag
         (labels ,functions
           (let ((nxt-label (function ,(caar functions))))
             (loop (setq nxt-label (catch ,go-tag (funcall nxt-label))))))))))


#|
(let (val)
  (tagbody
     (setq val 1)
     (go point-a)
     (incf val 16)
   point-c
     (incf val 04)
     (go point-b)
     (incf val 32)
   point-a
     (incf val 02)
     (go point-c)
     (incf val 64)
   point-b
     (incf val 08))
  val)
;; --> 15

(defun f2 (flag escape)
  (if flag (funcall escape) 2))

(defun f1 (flag)
  (let ((n 1))
    (tagbody 
       (setq n (f2 flag (lambda () (go out))))
     out
       (prin1 n))))

(f1 nil)
;; 2
;; --> nil
(f1 t)
;; 1
;; --> nil
|#
