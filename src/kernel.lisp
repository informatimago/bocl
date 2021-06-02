(defpackage "COM.INFORMATIMAGO.BOCL.KERNEL"
  (:nicknames "KERNEL" "K")
  (:use))

(defpackage "COM.INFORMATIMAGO.BOCL.KERNEL.PROTOTYPE"
  (:use "COMMON-LISP")
  (:import-from "COM.INFORMATIMAGO.COMMON-LISP.CESARUM.LIST"
                "IOTA"))
(in-package "COM.INFORMATIMAGO.BOCL.KERNEL.PROTOTYPE")


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

(defmacro niy (&rest stuff)
  `(progn
     ,@stuff
     (error "Not implemented yet.")))

(defun collect-garbage (rootset)
  (niy rootset)
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
    (prog1 (+ *free* +word-size+)
      (store-word *memory* block-size)
      (incf *free* block-size))))

(defun allocate-words (size)
  (let ((block-size (* +word-size+ (+ 1 size))))
    (when (< (length *memory*) (+ *free* block-size))
      (gc))
    (when (< (length *memory*) (+ *free* block-size))
      (error 'out-of-memory))
    (prog1 (+ *free* +word-size+)
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




(defparameter *initial-object-graph*
  ;; Each entry:: (class-name (is-a) . direct-superclasses)
  ;; eg. CHARACTER is a BUILT-IN-CLASS, has T as direct-superclass.
  '(
    (k::t                                  (k::built-in-class)) ; root
    (k::character                          (k::built-in-class)             (k::t))
    (k::array                              (k::built-in-class)             (k::t))
    (k::string                             (k::built-in-class)             (k::vector))
    (k::vector                             (k::built-in-class)             (k::array                       k::sequence))
    (k::sequence                           (k::built-in-class)             (k::t))
    (k::cons                               (k::built-in-class)             (k::list))
    (k::null                               (k::built-in-class)             (k::symbol                      k::list))
    (k::symbol                             (k::built-in-class)             (k::t))
    (k::list                               (k::built-in-class)             (k::sequence))
    (k::complex                            (k::built-in-class)             (k::number))
    (k::number                             (k::built-in-class)             (k::t))
    (k::float                              (k::built-in-class)             (k::real))
    (k::real                               (k::built-in-class)             (k::number))
    (k::ratio                              (k::built-in-class)             (k::rational))
    (k::rational                           (k::built-in-class)             (k::real))
    (k::integer                            (k::built-in-class)             (k::rational))
    (k::signed-byte                        (k::built-in-class)             (k::integer))
    (k::bit                                (k::built-in-class)             (k::unsigned-byte))
    (k::unsigned-byte                      (k::built-in-class)             (k::signed-byte))
    (k::hash-table                         (k::built-in-class)             (k::t))
    (k::restart                            (k::built-in-class)             (k::t))
    (k::random-state                       (k::built-in-class)             (k::t))
    (k::package                            (k::built-in-class)             (k::t))
    (k::readtable                          (k::built-in-class)             (k::t))
    (k::logical-pathname                   (k::built-in-class)             (k::pathname))
    (k::physical-pathname                  (k::built-in-class)             (k::pathname))
    (k::pathname                           (k::built-in-class)             (k::t))
    (k::file-stream                        (k::built-in-class)             (k::stream))
    (k::string-stream                      (k::built-in-class)             (k::stream))
    (k::two-way-stream                     (k::built-in-class)             (k::stream))
    (k::broadcast-stream                   (k::standard-class)             (k::stream))
    (k::echo-stream                        (k::standard-class)             (k::stream))
    (k::concatenated-stream                (k::standard-class)             (k::stream))
    (k::synonym-stream                     (k::built-in-class)             (k::stream))
    (k::fd-stream                          (k::built-in-class)             (k::stream))
    (k::stream                             (k::standard-class)             (k::t))
    (k::compiled-function                  (k::built-in-class)             (k::function))
    (k::function                           (k::built-in-class)             (k::t))
    (k::standard-object                    (k::standard-class)             (k::t))
    (k::funcallable-standard-object        (k::funcallable-standard-class) (k::function k::standard-object))
    (k::standard-generic-function          (k::funcallable-standard-class) (k::generic-function))
    (k::generic-function                   (k::funcallable-standard-class) (k::funcallable-standard-object k::metaobject))
    (k::slot-definition                    (k::standard-class)             (k::metaobject))
    (k::standard-direct-slot-definition    (k::standard-class)             (k::direct-slot-definition      k::standard-slot-definition))
    (k::direct-slot-definition             (k::standard-class)             (k::slot-definition))
    (k::standard-effective-slot-definition (k::standard-class)             (k::effective-slot-definition   k::standard-slot-definition))
    (k::effective-slot-definition          (k::standard-class)             (k::slot-definition))
    (k::standard-slot-definition           (k::standard-class)             (k::slot-definition))
    (k::method-combination                 (k::standard-class)             (k::metaobject))
    (k::eql-specializer                    (k::standard-class)             (k::specializer))
    (k::specializer                        (k::standard-class)             (k::metaobject))
    (k::funcallable-standard-class         (k::standard-class)             (k::class))
    (k::standard-class                     (k::standard-class)             (k::class))
    (k::built-in-class                     (k::standard-class)             (k::class))
    (k::structure-class                    (k::standard-class)             (k::class))
    (k::forward-referenced-class           (k::standard-class)             (k::class))
    (k::class                              (k::standard-class)             (k::specializer))
    (k::metaobject                         (k::standard-class)             (k::standard-object))
    (k::method                             (k::standard-class)             (k::metaobject))
    (k::standard-method                    (k::standard-class)             (k::method))
    (k::standard-writer-method             (k::standard-class)             (k::standard-accessor-method))
    (k::standard-reader-method             (k::standard-class)             (k::standard-accessor-method))
    (k::standard-accessor-method           (k::standard-class)             (k::standard-method))
    (k::style-warning                      (k::standard-class)             (k::warning))
    (k::simple-warning                     (k::standard-class)             (k::simple-condition            warning))
    (k::warning                            (k::standard-class)             (k::condition))
    (k::condition                          (k::standard-class)             (k::t))
    (k::storage-condition                  (k::standard-class)             (k::serious-condition))
    (k::serious-condition                  (k::standard-class)             (k::condition))
    (k::simple-error                       (k::standard-class)             (k::simple-condition            error))
    (k::simple-type-error                  (k::standard-class)             (k::simple-condition            type-error))
    (k::simple-condition                   (k::standard-class)             (k::condition))
    (k::type-error                         (k::standard-class)             (k::error))
    (k::control-error                      (k::standard-class)             (k::error))
    (k::parse-error                        (k::standard-class)             (k::error))
    (k::reader-error                       (k::standard-class)             (k::error))
    (k::end-of-file                        (k::standard-class)             (k::stream-error))
    (k::stream-error                       (k::standard-class)             (k::error))
    (k::file-error                         (k::standard-class)             (k::error))
    (k::print-not-readable                 (k::standard-class)             (k::error))
    (k::division-by-zero                   (k::standard-class)             (k::arithmetic-error))
    (k::floating-point-underflow           (k::standard-class)             (k::arithmetic-error))
    (k::floating-point-invalid-operation   (k::standard-class)             (k::arithmetic-error))
    (k::floating-point-inexact             (k::standard-class)             (k::arithmetic-error))
    (k::floating-point-overflow            (k::standard-class)             (k::arithmetic-error))
    (k::arithmetic-error                   (k::standard-class)             (k::error))
    (k::package-error                      (k::standard-class)             (k::error))
    (k::program-error                      (k::standard-class)             (k::error))
    (k::error                              (k::standard-class)             (k::serious-condition))
    (k::unbound-variable                   (k::standard-class)             (k::cell-error))
    (k::unbound-slot                       (k::standard-class)             (k::cell-error))
    (k::undefined-function                 (k::standard-class)             (k::cell-error))
    (k::cell-error                         (k::standard-class)             (k::error))))

(defun cinfo-class-name                (entry) (car entry))
(defun cinfo-class-metaclass           (entry) (caadr entry))
(defun cinfo-class-direct-superclasses (entry) (caddr entry))
(defun find-class-info (class-name)
  (find class-name *initial-object-graph* :key (function first)))
(defun k::class (class-name) (find-class-info class-name))


(eval-when (:compile-toplevel :load-toplevel :execute)
  (defun scat (&rest string-designators)
    (intern (apply (function concatenate)
                   'string
                   (mapcar (function string) string-designators))
            (if (symbolp (first string-designators))
                (symbol-package  (first string-designators))
                *package*))))

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

(defvar *kernel-obarray* (list 'k::kernel 'k::nil))




(defun ensure-kernel-class (class-name)
  (let ((class (get class-name 'kernel-class)))
    (or class
        (setf (get class-name 'kernel-class) (k::make-class class-name)))))


(defun k::make-string (string)
  (%make-object (ensure-kernel-class 'k::string)
                (%make-slots :length 2
                             :data (vector (length string)
                                     string))))

(defun k::make-symbol (sname obarray)
  (%make-object (ensure-kernel-class 'k::symbol)
                (%make-slots :length 5
                             :data (vector sname
                                     obarray
                                     '$unbound
                                     '$unbound
                                     nil))))



(defun k::find-symbol (sname obarray)
  (find sname (cdr obarray) :key (function symbol-name)))

(defun k::symbol-intern (sname obarray)
  (let ((symbol (k::find-symbol sname obarray)))
    (unless symbol
      (setf symbol (k::make-symbol sname obarray))
      (push symbol (cdr obarray)))
    symbol))

(defun kernel-initialize-objects ()
  (let* ((class-name )
         (sc-info (find-class-info 'k::standard-class))
         (sc-object (%make-object :class nil :slots  nil))
         (bic-info (find-class-info 'k::built-in-class))
         (bic-object (%make-object :class nil :slots  nil)))
    (setf (get 'k::standard-class 'kernel-class) sc-object)
    (setf (get 'k::built-in-class 'kernel-class) bic-object)
    (setf (object-class sc-object) sc-object
          (object-class bic-object) bic-object
          (object-slots sc-object) (%make-slots :length 2
                                             :data (vector
                                                    (k::symbol-intern (k::make-string "STANDARD-CLASS") *kernel-obarray*)
                                                    (mapcar (function ensure-kernel-class)
                                                            (cinfo-class-direct-superclasses sc-info))))
          (object-slots sc-object) (%make-slots :length 2
                                             :data (vector
                                                    (k::symbol-intern (k::make-string "BUILT-IN-CLASS") *kernel-obarray*)
                                                    (mapcar (function ensure-kernel-class)
                                                            (cinfo-class-direct-superclasses bic-info)))))
    object))

(defun k::make-class (name)
  (let ((info (find-class-info name)))
    (%make-object (ensure-kernel-class (cinfo-class-metaclass info))
                  (%make-slots :length 2
                               :data (vector
                                      (k::symbol-intern (k::make-string (string (cinfo-class-name info)))
                                                        *kernel-obarray*)
                                      (mapcar (function ensure-kernel-class)
                                              (cinfo-class-direct-superclasses info)))))))
(defun k::class-name (class)
  (aref (slots-data (object-slots class)) 0))

(defun k::class-direct-superclasses (class)
  (aref (slots-data (object-slots class)) 1))

(defun (setf k::class-direct-superclasses) (new-direct-superclasses class)
  (setf (aref (slots-data (object-slots class)) 1) new-direct-superclasses))




(defun k::check-type (object class)
  (let ((object-class (object-class object)))
    (or (eql object-class class)
        (find-if (lambda (superclass)
                   (k::check-type object superclass))
                 (k::class-direct-superclasses class)))))

(defmacro define-object (name &rest fields)
  `(progn
     (defun ,(scat 'k::make- name) (,@(mapcar (function first) fields))
       (%make-object (ensure-kernel-class ',name)
                     (%make-slots :length ,(length fields)
                                  :data (vector ,@(mapcar (function first) fields)))))
     (defun ,(scat name '-p) (object)
       (eql (object-class object) (ensure-kernel-class ',name)))
     ,@(mapcar (lambda (index field)
                 (destructuring-bind (fname &key type) field
                   `(progn
                      (defun ,(scat name '- fname) (object)
                        (aref (slots-data (object-slots object)) ,index))
                      (defun (setf ,(scat name '- fname)) (new-value object)
                        (k::check-type new-value (ensure-kernel-class ',type))
                        (setf (aref (slots-data (object-slots object))  ,index) new-value)))))
               (iota (length fields))
               fields)
     ',name))



(define-object k::character
    (code :type k::integer)
  (name :type k::string))

(define-object k::integer
    (value :type integer))


(kernel-initialize-objects)
;; (k::symbol-intern (k::make-string "T") *kernel-obarray*)

(defun subclassp (class superclass)
  (when (eql class superclass)
    (return t))
  (dolist (direct-superclass  (class-direct-superclasses class))
    (when (subclassp direct-superclass superclass)
      (return t)))
  nil)
