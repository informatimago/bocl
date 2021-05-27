(define-condition out-of-memory (storage-condition)
  ())

#-64-bit (defconstant +word-size+ 4)
#+64-bit (defconstant +word-size+ 8)

(defconstant +memory-size+ (* 64 1024 1024))

(deftype word () `(unsigned-byte ,(* 8 +word-size+)))
(deftype byte () `(unsigned-byte 8))

(defvar *memory* (make-array +memory-size+
                             :element-type 'byte
                             :initial-element 0))
(defvar *free*   0)

(defun store-byte (address byte)
  (setf (aref *memory* address) byte))

(defun load-byte (address)
  (aref *memory* address))

(defun store-word (address word)
  (setf (aref *memory* address)        (ldb (byte 8 56) word)
        (aref *memory* (incf address)) (ldb (byte 8 48) word)
        (aref *memory* (incf address)) (ldb (byte 8 40) word)
        (aref *memory* (incf address)) (ldb (byte 8 32) word)
        (aref *memory* (incf address)) (ldb (byte 8 24) word)
        (aref *memory* (incf address)) (ldb (byte 8 16) word)
        (aref *memory* (incf address)) (ldb (byte 8  8) word)
        (aref *memory* (incf address)) (ldb (byte 8  0) word)))

(defun load-word (address)
  (dpb (aref *memory* address) (byte 8 56)
       (dpb (aref *memory* (incf address)) (byte 8 48)
            (dpb (aref *memory* (incf address)) (byte 8 40)
                 (dpb (aref *memory* (incf address)) (byte 8 32)
                      (dpb (aref *memory* (incf address)) (byte 8 24)
                           (dpb (aref *memory* (incf address)) (byte 8 16)
                                (dpb (aref *memory* (incf address)) (byte 8  8)
                                     (aref *memory* (incf address))))))))))

;; (store-word 0 #x0102030405060708)
;; (assert (equalp (subseq *memory* 0 16) #(1 2 3 4 5 6 7 8 0 0 0 0 0 0 0 0)))
;; (assert (= (load-word 0) #x0102030405060708))




(defvar *garbage-collector-enabled* t)
(defvar *rootset* 0)

(defun collect-garbage (rootset)
  
  0)

(defun gc (&optional command)
  (ecase command
    ((nil) (when *garbage-collector-enabled*
             (collect-garbage *rootset*)))
    ((:on) (prog1 (if *garbage-collector-enabled* :on :off)
             (setf *garbage-collector-enabled* t)))
    ((:off) (prog1 (if *garbage-collector-enabled* :on :off)
              (setf *garbage-collector-enabled* nil)))))

(defun allocate-bytes (size)
  (let ((block-size (* +word-size+ (1+ (ceiling size +word-size+)))))
    (when (< (length *memory*) (+ *free* block-size))
      (gc))
    (when (< (length *memory*) (+ *free* block-size))
      (error 'out-of-memory))
    (prog1 (+ *byte* +word-size+)
      (store-word *memory* block-size)
      (incf *free* block-size))))

(defun allocate-words (size)
  (let ((block-size (* +word-size+ (+ 1 size))))
    (when (< (length *memory*) (+ *free* block-size))
      (gc))
    (when (< (length *memory*) (+ *free* block-size))
      (error 'out-of-memory))
    (prog1 (+ *free* +word-size)
      (store-word *memory* block-size)
      (incf *free* block-size))))

(defun make-word-object (class length)
  (let ((object (allocate-words 2))
        (slots  (allocate-words length)))
    (store-word object class)
    (store-word (+ object 1) slots)))

(defun make-byte-object (class length)
  (let ((object (allocate-words 2))
        (bytes  (allocate-bytes length)))
    (store-word object class)
    (store-word (+ object 1) bytes)))


(defun scat (&rest string-designators)
  (intern (apply (function concatenate)
                 'string
                 (mapcar (function string) string-designators))))

(defmacro define-record (name &rest fields)
  `(defstruct (,name
               (:type vector)
               (:constructor ,(scat '%make- name)))
     ,@fields))

                      
(define-record object
  class
  slots)


(define-record slots
  length
  data)

(define-object character
  (code :type integer)
  (name :type string))

(define-object integer
  (v))
(defvar *nil* nil)



(defun set-class (object class)
  (setf (object-class object) class))

(defun make-object (class length width)
  (%make-object :class class
                :slots (make-vector (* 2 ))))



