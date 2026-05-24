;;;; -*- mode:lisp;coding:utf-8 -*-
;;;;**************************************************************************
;;;;FILE:               heap.lisp
;;;;LANGUAGE:           Common-Lisp
;;;;SYSTEM:             Common-Lisp
;;;;USER-INTERFACE:     NONE
;;;;DESCRIPTION
;;;;    
;;;;    Implement a mini lisp on some inferior languages.
;;;;    We don't assume a heap!
;;;;    We don't assume recursion!
;;;;    
;;;;AUTHORS
;;;;    <PJB> Pascal Bourguignon <pjb@informatimago.com>
;;;;MODIFICATIONS
;;;;    2008-05-07 <PJB> Created.
;;;;BUGS
;;;;LEGAL
;;;;    GPL
;;;;    
;;;;    Copyright Pascal Bourguignon 2008 - 2008
;;;;    
;;;;    This program is free software; you can redistribute it and/or
;;;;    modify it under the terms of the GNU General Public License
;;;;    as published by the Free Software Foundation; either version
;;;;    2 of the License, or (at your option) any later version.
;;;;    
;;;;    This program is distributed in the hope that it will be
;;;;    useful, but WITHOUT ANY WARRANTY; without even the implied
;;;;    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
;;;;    PURPOSE.  See the GNU General Public License for more details.
;;;;    
;;;;    You should have received a copy of the GNU General Public
;;;;    License along with this program; if not, write to the Free
;;;;    Software Foundation, Inc., 59 Temple Place, Suite 330,
;;;;    Boston, MA 02111-1307 USA
;;;;**************************************************************************


(defgeneric size-of (type))


(defmacro define-structure (name type-tag &rest fields)
  "

TYPE-TAG:  either a fixnum containing the bits of the type tag (in
           which case no type is stored with the structure),
           or a list (type-tag obj-type), in which case the structure
           contains an implicit type field.
"
  (let ((tag-bits (if (listp type-tag)
                      (first type-tag)
                      type-tag))
        (obj-type (if (listp type-tag)
                      (second type-tag)
                      nil)))
    `(progn
       (defmethod size-of ((self (eql ',name)))
         ,(+ (if obj-type 1 0) (length fields)))
       (defun ,(intern (format nil "%MAKE-~A" name) (symbol-package name)) ,fields
         (let* ((object (allocate ,(+ (if obj-type 1 0) (length fields))))
                (base   (1- object)))
           ,@(when obj-type
                   `((store-box (incf base) ,obj-type)))
           ,@(loop
                :for field :in fields
                :collect `(store-box (incf base) ,field))
           (make-box ,tag-bits object)))
       ,@(loop
            :for offset :from (if obj-type 1 0)
            :for field :in fields
            :for reader-name = (intern (format nil "~A-~A"     name field) (symbol-package name))
            :for writer-name = (intern (format nil "SET-~A-~A" name field) (symbol-package name))
            :collect `(defun ,reader-name (object)
                        (fetch-box (+ (box-value object) ,offset)))
            :collect `(defun ,writer-name (object new-value)
                        (store-box (+ (box-value object) ,offset) new-value)))
       ',name)))

(define-structure bootstrap-cl:cons          *tag-cons*
  car cdr)

(define-structure bootstrap-cl:symbol        *tag-symbol*
  name value function plist package) 

(define-structure bootstrap-cl:simple-vector *tag-simple-vector*
  length slots)

(define-structure bootstrap-cl:hash-table    *tag-hash-table*
  test rehash-size rehash-threshold buckets)


;; Some symbols:
(defvar @nil 0)
(defvar @t)
(defvar @fixnum)
(defvar @character)
(defvar @string)
(defvar @cons)
(defvar @symbol)
(defvar @simple-vector)
(defvar @hash-table)
(defvar @package)
(defvar @quote)
(defvar @eql)
(defvar @equal)
(defvar @equalp)
(defvar @kw-test)

;; The root set (list)
(defvar *cl-sym* '())
(defvar *kw-sym* '())



(defun str (native-string) (enbox-string native-string))

(defun initialize-heap ()
  (initialize-storage)
  (setf
   ;; allocate essential symbols. @NIL must be first!
   @nil           (make-box *tag-symbol* 0)
   @nil           (bootstrap-cl::%make-symbol @nil @nil @nil @nil @nil)
   @t             (bootstrap-cl::%make-symbol @nil @nil @nil @nil @nil)
   @fixnum        (bootstrap-cl::%make-symbol @nil @nil @nil @nil @nil)
   @character     (bootstrap-cl::%make-symbol @nil @nil @nil @nil @nil)
   @string        (bootstrap-cl::%make-symbol @nil @nil @nil @nil @nil)
   @cons          (bootstrap-cl::%make-symbol @nil @nil @nil @nil @nil)
   @symbol        (bootstrap-cl::%make-symbol @nil @nil @nil @nil @nil)
   @simple-vector (bootstrap-cl::%make-symbol @nil @nil @nil @nil @nil)
   @hash-table    (bootstrap-cl::%make-symbol @nil @nil @nil @nil @nil)
   ;;---
   @package       (bootstrap-cl::%make-symbol @nil @nil @nil @nil @nil)
   @quote         (bootstrap-cl::%make-symbol @nil @nil @nil @nil @nil)
   @eql           (bootstrap-cl::%make-symbol @nil @nil @nil @nil @nil)
   @equal         (bootstrap-cl::%make-symbol @nil @nil @nil @nil @nil)
   @equalp        (bootstrap-cl::%make-symbol @nil @nil @nil @nil @nil)
   @kw-test       (bootstrap-cl::%make-symbol @nil @nil @nil @nil @nil)
   @kw-internal   (bootstrap-cl::%make-symbol @nil @nil @nil @nil @nil)
   @kw-inherited  (bootstrap-cl::%make-symbol @nil @nil @nil @nil @nil))
  (bootstrap-cl::set-symbol-name  @nil           (str "NIL"))
  (bootstrap-cl::set-symbol-name  @t             (str "T"))
  (bootstrap-cl::set-symbol-value @t             @t)
  (bootstrap-cl::set-symbol-name  @fixnum        (str "FIXNUM"))
  (bootstrap-cl::set-symbol-name  @character     (str "CHARACTER"))
  (bootstrap-cl::set-symbol-name  @string        (str "STRING"))
  (bootstrap-cl::set-symbol-name  @cons          (str "CONS"))
  (bootstrap-cl::set-symbol-name  @symbol        (str "SYMBOL"))
  (bootstrap-cl::set-symbol-name  @simple-vector (str "SIMPLE-VECTOR"))
  (bootstrap-cl::set-symbol-name  @hash-table    (str "HASH-TABLE"))
  ;;---
  (bootstrap-cl::set-symbol-name  @package       (str "PACKAGE"))
  (bootstrap-cl::set-symbol-name  @quote         (str "QUOTE"))
  (bootstrap-cl::set-symbol-name  @eql           (str "EQL"))
  (bootstrap-cl::set-symbol-name  @equal         (str "EQUAL"))
  (bootstrap-cl::set-symbol-name  @equalp        (str "EQUALP"))
  (bootstrap-cl::set-symbol-name  @kw-test       (str "TEST"))
  (bootstrap-cl::set-symbol-value @kw-test       @kw-test)
  (bootstrap-cl::set-symbol-name  @kw-internal   (str "INTERNAL"))
  (bootstrap-cl::set-symbol-value @kw-internal   @kw-internal)
  (bootstrap-cl::set-symbol-name  @kw-inherited  (str "INHERITED"))
  (bootstrap-cl::set-symbol-value @kw-inherited  @kw-inherited)
  (setf *cl-syms* (list @nil @t @fixnum @character @string @symbol
                                 @cons @simple-vector @hash-table
                                 ;;---
                                 @package @quote @eql @equal @equalp)
        *kw-syms* (list @kw-test @kw-internal @kw-inherited))
  (values))


(defvar *type-map*)

(defun bootstrap-cl:type-of (object)
  (let ((ass (assoc (box-tag object) *type-map*)))
    (if ass
        (second ass)
        (fetch-box (box-value object)))))


(defun ubool (bool) (not (eql @nil bool)))
(defun bool (ubool) (if ubool @t @nil))

(defun bootstrap-cl:eq (a b)
  (bool (eql a b)))

(defun bootstrap-cl:eql (a b)
  (bool (eql a b)))


(declaim (inline bs-symbolp bs-null bs-consp bs-vectorp bs-stringp))
(defun bs-null       (object) (eql @nil object))
(defun bs-fixnump    (object) (eql *tag-fixnum*        (box-tag object)))
(defun bs-characterp (object) (eql *tag-character*     (box-tag object)))
(defun bs-symbolp    (object) (eql *tag-symbol*        (box-tag object)))
(defun bs-consp      (object) (eql *tag-cons*          (box-tag object)))
(defun bs-vectorp    (object) (eql *tag-simple-vector* (box-tag object)))
(defun bs-stringp    (object) (eql *tag-string*        (box-tag object)))



(defun bootstrap-cl:equal (a b)
  ;; (terpri) (princ  (%prin1-to-string (bootstrap-cl:list a b)))
  (bool (or (eql a b)
            (and (eql (box-tag a) (box-tag b))
                 (cond ((bs-consp a)
                        (and (ubool (bootstrap-cl:equal (bootstrap-cl:car a)  (bootstrap-cl:car b)))
                             (ubool (bootstrap-cl:equal (bootstrap-cl:cdr a)  (bootstrap-cl:cdr b)))))
                       ((bs-stringp a)
                        (COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.NATIVE:string= (unbox-string a) (unbox-string b)))
                       ((bs-characterp a) nil)
                       ((bs-fixnump    a) nil)
                       ((bs-symbolp    a) nil)
                       (t (error "equal not implemented for ~A" (%prin1-to-string a))))))))



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; FIXNUM

(defun fixnum%sxhash (fixnum)
  fixnum)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; CHARACTER

(defun character%sxhash (character)
  (char-code (aref character 0)))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; STRING

(defun string%sxhash (string)
  (loop
     :with hash = 0
     :for c :across string
     :do (setf hash (logand #xffffffff
                            (- (+ (char-code c) (ash hash 6) (ash hash 16)) hash)))
     :finally (return hash)))

(defun string%ulength (string)
  (COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.NATIVE:string-length
   (unbox-string string)))

(defun string%length (string)
  (enbox-fixnum (string%ulength string)))

(defun string%nreverse (string)
  (error "string%nreverse not implemented yet"))

(defun string%reverse (string)
  (string%nreverse (string%copy string)))

(defun string%copy (string)
  (enbox-string (COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.NATIVE:string-copy
                 (unbox-string string))))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; SYMBOL

(defun bootstrap-cl:make-symbol (name)
  (bootstrap-cl::%make-symbol name @nil @nil @nil @nil))


(defun bootstrap-cl:null (object)
  (bool (bs-null object)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; CONS

(defun bootstrap-cl:cons (car cdr)
  (bootstrap-cl::%make-cons car cdr))

(defun bootstrap-cl:consp (object)
  (bool (bs-consp object)))

(defun bootstrap-cl:atom (object)
  (bool (not (bs-consp object))))

(defun bootstrap-cl:car (object)
  (cond
    ((bs-null  object)  @nil)
    ((bs-consp object)  (bootstrap-cl::cons-car object))
    (t (error "Called CAR on a non CONS object of type ~A"
              (%prin1-to-string (bootstrap-cl:type-of object))))))

(defun bootstrap-cl:cdr (object)
  (cond
    ((bs-null  object)  @nil)
    ((bs-consp object)  (bootstrap-cl::cons-cdr object))
    (t (error "Called CDR on a non CONS object of type ~A"
              (%prin1-to-string (bootstrap-cl:type-of object))))))

(defun bootstrap-cl:rplaca (cons object)
  (if (bs-consp cons)
      (bootstrap-cl::set-cons-car cons object)
      (error "Called RPLACA on a non CONS object of type ~A"
             (%prin1-to-string (bootstrap-cl:type-of cons))))
  cons)

(defun bootstrap-cl:rplacd (cons object)
  (if (bs-consp cons)
      (bootstrap-cl::set-cons-cdr cons object)
      (error "Called RPLACD on a non CONS object of type ~A"
             (%prin1-to-string (bootstrap-cl:type-of cons))))
  cons)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; LIST

(defun list%ulength (list)
  (loop
     :with length = 0
     :do (cond
           ((bs-null  list) (return length))
           ((bs-consp list) (setf list (bootstrap-cl:cdr list)) (incf length))
           (t               (return length)))))

(defun list%length (object)
  (enbox-fixnum (list%ulength list)))

(defun list%nreverse (list)
  (let ((result @nil))
    (loop
       :for cur = list :then next
       :for next = (bootstrap-cl:cdr cur)
       :while (bs-consp cur)
       :do (bootstrap-cl:rplacd cur result) (setf result cur)
       :finally (return result))))

(defun list%reverse (list)
  (let ((result @nil))
    (loop
       :for cur = list :then (bootstrap-cl:cdr cur)
       :while (bs-consp cur)
       :do (setf result (bootstrap-cl:cons (bootstrap-cl:car cur) result))
       :finally (return result))))

(defun list%copy (list)
  (list%nreverse (list%reverse list)))



(defun bootstrap-cl::ensure-list (object)
  (cond
    ((bs-null object)   object)
    ((bs-consp object)  object)
    (t                  (bootstrap-cl:list object))))

(defun bootstrap-cl:list (&rest elements)
  (let ((result @nil))
    (dolist (item (reverse elements) result)
      (setf result (bootstrap-cl:cons item result)))))

(defun bootstrap-cl:list* (&rest elements)
  (let* ((elements (reverse elements))
         (result (pop elements)))
    (dolist (item elements result)
      (setf result (bootstrap-cl:cons item result)))))




(defun bootstrap-cl:assoc (object alist &key (key @nil) (test @eql) (test-not @nil))
  (unless (and (eql @nil key)
               (or (eql test @eql) (eql test @equal) (eql test @equalp))
               (eql @nil test-not))
    (error "ASSOC doesn't implement yet keyword arguments."))
  (loop
     :with cur = alist
     :while   (bs-consp cur)
     :do      (if (ubool
                   (cond
                     ((eql test @eql)
                      (bootstrap-cl:eql    (bootstrap-cl:car (bootstrap-cl:car cur))
                                           object))
                     ((eql test @equal)
                      (bootstrap-cl:equal  (bootstrap-cl:car (bootstrap-cl:car cur))
                                           object))
                     ((eql test @equalp)
                      (bootstrap-cl:equalp (bootstrap-cl:car (bootstrap-cl:car cur))
                                           object))
                     (t (error "Unexpected :test parameter in ASSOC"))))
                  (return (values (bootstrap-cl:car cur) @t))
                  (setf cur (bootstrap-cl:cdr cur)))
     :finally (return (values @nil @nil))))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;

(defun %make-slots (length)
  (let* ((object (allocate (1+ length)))
         (base   (1- object)))
    (store-box (incf base) (enbox-fixnum length))
    (loop
       :repeat length
       :do (store-box (incf base) @nil))
    (make-box *tag-other* object)))

(defun %slot-length (slots)
  (unbox-fixnum (fetch-box (box-value slots))))

(defun %slot-ref (slots index)
  (let ((len (%slot-length slots)))
    (unless (and (<= 0 index) (< index len))
      (error "Index ~A out of range (max ~A)" index len))
    (fetch-box (+ 1 index (box-value slots)))))

(defun %slot-set (slots index value)
  (let ((len (%slot-length slots)))
    (unless (and (<= 0 index) (< index len))
      (error "Index ~A out of range (max ~A)" index len))
    (store-box (+ 1 index (box-value slots)) value)
    value))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; VECTOR

(defun bootstrap-cl:make-array (dimension &key (element-type bootstrap-cl:t)
                                (initial-element  bootstrap-cl:nil)
                                (initial-contents bootstrap-cl:nil)
                                (adjustable bootstrap-cl:nil)
                                (fill-pointer bootstrap-cl:nil)
                                (displaced-to  bootstrap-cl:nil)
                                (displaced-index-offset bootstrap-cl:nil))
  (let ((dimension (bootstrap-cl::ensure-list dimension)))
    (unless (= 1 (unbox-fixnum (bootstrap-cl:length dimension)))
      (error "bootstrap-cl:make-array not implemented yet for non simple-vectors"))
    (unless (eql element-type bootstrap-cl:t)
      (error "bootstrap-cl:make-array not implemented yet for non simple-arrays"))
    (unless (eql adjustable bootstrap-cl:nil)
      (error "bootstrap-cl:make-array not implemented yet for adjustable arrays"))
    (unless (eql fill-pointer bootstrap-cl:nil)
      (error "bootstrap-cl:make-array not implemented yet for fill-pointers"))
    (unless (eql displaced-to bootstrap-cl:nil)
      (error "bootstrap-cl:make-array not implemented yet for displaced-arrays"))
    (let* ((len  (unbox-fixnum (bootstrap-cl:car dimension)))
           (slots (%make-slots len))
           (result (%make-simple-vector len slots)))
      (if (not (eql initial-contents bootstrap-cl:nil))
          (loop
            ;; fill the vector with the initial-contents
             :repeat len
             :for addr = (1+ (box-value slots))
             :for cur = initial-contents :then (bootstrap-cl:cdr cur)
             :until (eql @nil cur)
             :do (store-box addr (bootstrap-cl:car cur)) (incf addr))
          (loop
            ;; fill the vector with the initial-contents
             :repeat len
             :for addr = (1+ (box-value slots))
             :do (store-box addr initial-element) (incf addr)))
      result)))


(defun vector%ulength (vector)
  (%slot-length (simple-vector-slots vector)))

(defun vector%length (vector)
  (enbox-fixnum (%slot-length (simple-vector-slots vector))))

(defun vector%nreverse (vector)
  (loop
     :with slots = (simple-vector-slots vector)
     :for i :from 0
     :for j :from (1- (vector%ulength vector)) :by -1
     :while (< i j)
     :do (let ((temp (%slot-ref slots i)))
           (%slot-set slots i (%slot-ref slots j))
           (%slot-set slots j temp))))

(defun vector%reverse (vector)
  (let* ((len  (vector%ulength vector))
         (copy (bootstrap-cl:make-array len))
         (src  (simple-vector-slots vector))
         (dst  (simple-vector-slots copy)))
    (loop
       :for i :from 0 
       :for j :from (1- (vector%ulength vector)) :downto 0
       :do (%slot-set dst i (%slot-ref src j)))
    copy))

(defun vector%copy (vector)
  (let* ((len  (vector%ulength vector))
         (copy (bootstrap-cl:make-array len))
         (src  (simple-vector-slots vector))
         (dst  (simple-vector-slots copy)))
    (dotimes (i len)
      (%slot-set dst i (%slot-ref src i)))
    copy))



(defun bootstrap-cl:svref (vector index)
  (let ((uindex  (unbox-fixnum index)))
    (%slot-ref (simple-vector-slots vector) uindex)))


(defun bootstrap-cl::svset (vector index value)
  (let ((uindex  (unbox-fixnum index)))
    (%slot-set (simple-vector-slots vector) uindex value)))


(defun bootstrap-cl:aref (array &rest indexes)
  (cond
    ((eql *tag-simple-vector* (box-tag array))
     (if (= 1 (length indexes))
         (bootstrap-cl:svref array (first indexes))
         (error "Too many indexes given to AREF a SIMPLE-VECTOR: ~S" indexes)))
    ;; ((eql *tag-string* (box-tag array))
    ;;  )
    (t
     (error "AREF on arrays or strings not implemented yet."))))


;; (vector%length (%make-simple-vector 5))
;; 
;; (bootstrap-cl:prin1-to-string
;;  (bootstrap-cl:length (%make-simple-vector 5)))
;; 
;; (bootstrap-cl:prin1-to-string
;;  (BOOTSTRAP-CL::svref (%make-simple-vector 5)
;;                       (enbox-fixnum 2)))
;; 
;; (bootstrap-cl:prin1-to-string
;;  (bootstrap-cl::SVSET  (%make-simple-vector 3)
;;                        (enbox-fixnum 2)
;;                        @PACKAGE))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; SEQUENCE


(defmacro sequence-dispatch (object operator)
  `(cond
     ((bs-consp   ,object)
      (,(intern (format nil "LIST%~A"   operator)) ,object))
     ((bs-vectorp ,object)
      (,(intern (format nil "VECTOR%~A" operator)) ,object))
     ((bs-stringp ,object)
      (,(intern (format nil "STRING%~A" operator)) ,object))
     (t
      (error "~A: ~A is not a sequence" ',operator
             (%prin1-to-string ,object)))))


(defun bootstrap-cl:nreverse (seq) (sequence-dispatch seq nreverse))
(defun bootstrap-cl:reverse  (seq) (sequence-dispatch seq reverse))
(defun bootstrap-cl:length   (seq) (sequence-dispatch seq length))
(defun bootstrap-cl:copy-seq (seq) (sequence-dispatch seq copy))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; HASH-TABLE


(defun bootstrap-cl:make-hash-table (&key (test @eql)
                                     (size bootstrap-cl:nil)
                                     (rehash-size bootstrap-cl:nil)
                                     (rehash-threshold bootstrap-cl:nil))
  (unless (and (eql bootstrap-cl:nil size)
               (eql bootstrap-cl:nil rehash-size)
               (eql bootstrap-cl:nil rehash-threshold))
      (error "bootstrap-cl:make-hash-table doesn't implemented yet all the keywords"))
  (let* ((usize (if (eql bootstrap-cl:nil size)
                   7
                   (unbox-fixnum size)))
         (buckets (%make-slots usize)))
    (loop
       :repeat usize
       :for addr = (1+ (box-value buckets))
       :do (store-box addr @nil) (incf addr))
    (bootstrap-cl::%make-hash-table test rehash-size rehash-threshold buckets)))


(defun bootstrap-cl:hash-table-size (table)
  (enbox-fixnum (%slot-length (bootstrap-cl::hash-table-buckets table))))


(defun bucket-index (key table)
  (let* ((test (bootstrap-cl::hash-table-test table))
         (key (cond
                ((eql test @eql)    key)
                ((eql test @equal)  key)
                ((eql test @equalp) (cond
                                      ((bs-stringp key)
                                       (bootstrap-cl:string-upcase key))
                                      (t key)))
                (t                  key)))
         (hk   (bootstrap-cl:sxhash key))
         (size (%slot-length (bootstrap-cl::hash-table-buckets table))))
    (mod hk size)))


(defun bootstrap-cl:gethash (key table &optional (default bootstrap-cl:nil))
  (let* ((index (bucket-index key table))
         (bucket (%slot-ref (bootstrap-cl::hash-table-buckets table) index)))
    (if (eql bootstrap-cl:nil bucket)
       (values default bootstrap-cl:nil)
       (let ((ass (bootstrap-cl:assoc
                   key bucket
                   :test (bootstrap-cl::hash-table-test table))))
         (if (eql bootstrap-cl:nil ass)
             (values default bootstrap-cl:nil)
             (values (bootstrap-cl:cdr ass) bootstrap-cl:t))))))

(defun bootstrap-cl::sethash (key table value)
  (let* ((index  (bucket-index key table))
         (bucket (%slot-ref (bootstrap-cl::hash-table-buckets table) index)))
    (if (eql bootstrap-cl:nil bucket)
        (%slot-set (bootstrap-cl::hash-table-buckets table) index
                   (bootstrap-cl:list (bootstrap-cl:cons key value)))
        (let ((ass (bootstrap-cl:assoc key bucket)))
          (if (eql bootstrap-cl:nil ass)
              (%slot-set (bootstrap-cl::hash-table-buckets table) index
                         (bootstrap-cl:cons (bootstrap-cl:cons key value) bucket))
              (bootstrap-cl:rplacd ass value))))))



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; PACKAGE


(defun break-on-write (address operation box)
  (ecase operation
    ((1) (cerror "Continue writing ~A at ~D" "About to write ~A at ~D"
                 (%prin1-to-string box) address))
    ((2) box)))

(define-structure bootstrap-cl:package       (*tag-other* @package)
  name nicknames exported-symbols interned-symbols use-list)

(defvar bootstrap-cl::*list-all-packages*)


(defun bootstrap-cl:list-all-packages ()
  (bootstrap-cl::copy-seq bootstrap-cl::*list-all-packages*))

(defun bootstrap-cl:find-package (name)
  (COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.J-MACRO.STORAGE:trap-address
   (function break-on-write)
   (box-value bootstrap-cl::*list-all-packages*)
   (1+ (box-value bootstrap-cl::*list-all-packages*)))
  ;; (terpri) (princ (%prin1-to-string bootstrap-cl::*list-all-packages*))
  (loop
     :for cur = bootstrap-cl::*list-all-packages* :then (bootstrap-cl:cdr cur)
     :for pack = (bootstrap-cl:car cur)
     :until (bs-null cur)
     ;; :do (print (list (%prin1-to-string cur)
     ;;                  (%prin1-to-string pack)
     ;;                  (%prin1-to-string name)
     ;;                  (%prin1-to-string (bootstrap-cl:package-name pack))
     ;;                  (%prin1-to-string (bootstrap-cl:package-nicknames pack))))
     :do (if (COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.NATIVE:string=
              (unbox-string name)
              (unbox-string (bootstrap-cl:package-name pack)))
             (return-from  bootstrap-cl:find-package pack)
             (loop
                :with uname = (unbox-string name)
                :for nicks = (bootstrap-cl:package-nicknames pack)
                :then (bootstrap-cl:cdr nicks)
                :for nick = (bootstrap-cl:car nicks)
                :until (bs-null nicks)
                :do (when (COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.NATIVE:string=
                           uname (unbox-string nick))
                      (return-from  bootstrap-cl:find-package pack))))
     :finally (return-from  bootstrap-cl:find-package @nil)))


(defun bootstrap-cl:make-package (name &key (nicknames @nil) (use @nil))
  (let* ((existing-name @nil)
         (existing-package
          (or (let ((existing-package (bootstrap-cl:find-package name)))
                (unless (eql @nil existing-package)
                  (setf existing-name name))
                existing-package)
              (loop
                 :for cur = nicknames :then (bootstrap-cl:cdr cur)
                 :for pack = (bootstrap-cl:find-package
                              (bootstrap-cl:car cur))
                 :do (unless (eql @nil pack)
                       (setf existing-name (bootstrap-cl:car cur))
                       (return pack))
                 :finally (return @nil)))))
    (unless (eql @nil existing-package)
      (cerror "return the existing package"
              "MAKE-PACKAGE: a package with name ~S already exists"
              (%print1-to-string existing-name))
      (return-from bootstrap-cl:make-package existing-package)))
  (let ((new-package (bootstrap-cl::%make-package
                      name nicknames @nil
                      (bootstrap-cl:make-hash-table :test @equal)
                      use)))
    (setf bootstrap-cl::*list-all-packages*
          (bootstrap-cl:cons new-package bootstrap-cl::*list-all-packages*))
    new-package))


(defvar *common-lisp-package*)
(defvar *common-lisp-user-package*)
(defvar *keyword-package*)
(defvar bootstrap-cl:*package*)


(defun make-keyword (name)
  (let ((sym (bootstrap-cl:intern name *keyword-package*)))
    (bootstrap-cl::set-symbol-value sym sym)
    sym))


(defun bootstrap-cl:find-symbol (name &optional (package bootstrap-cl:*package*))
  (multiple-value-bind (sym foundp)
      (bootstrap-cl::gethash name (bootstrap-cl::package-interned-symbols package))
    (if (eql foundp @nil)
        (values @nil @nil)
        (values sym (if (eql (bootstrap-cl::symbol-package sym) package)
                        ;; TODO: check for external symbols
                        @kw-internal @kw-inherited)))))


(defun bootstrap-cl:intern (name &optional (package bootstrap-cl:*package*))
  (let ((package 
         (cond ((bs-stringp package) (bootstrap-cl:find-package package))
               (t package))))
    (multiple-value-bind (sym found) (bootstrap-cl:find-symbol name package)
      (when (eql found @nil)
        (setf sym (bootstrap-cl::%make-symbol name @nil @nil @nil package))
        (bootstrap-cl::sethash name (bootstrap-cl::package-interned-symbols package) sym))
      sym)))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; prin1-to-string

(defmacro concat (string-var &rest string-expressions)
  (let ((exprs (gensym)))
    `(let ((,exprs (list ,@string-expressions)))
       (setf ,string-var (format nil "~A~{~A~}" ,string-var ,exprs)))))


(defun base-prin1-to-string (type-tag cur-obj)
  (cond
    
    ((eql *tag-fixnum*        type-tag)
     (multiple-value-bind (errorp result)
         (COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.NATIVE:fixnum-to-string
          (unbox-fixnum cur-obj))
       (when errorp
         (error "Error while converting a fixnum ~D to a string"
                (box-value cur-obj)))
       result))

    ((eql *tag-character*     type-tag)
     (let ((result "#\\"))
       (multiple-value-bind (errorp charstr)
           (COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.NATIVE:character-to-string
            (unbox-character cur-obj))
         (when errorp
           (error "Error while converting a fixnum ~D to a string"
                  (unbox-character cur-obj)))
         (concat result charstr)
         result)))
    
    ((eql *tag-string*        type-tag)
     (let* ((value (unbox-string cur-obj))
            (result (make-string 
                     (+ 2 (loop
                              :for ch :across value
                              :sum (if (or (char= #\\ ch) (char= #\" ch)) 2 1))))))
       (loop
          :with i = 0
          :for ch :across value
          :initially (setf (aref result i) #\") (incf i)
          :do (when (or (char= #\\ ch) (char= #\" ch))
                (setf (aref result i) #\\) (incf i))
          :do (setf (aref result i) ch) (incf i)
          :finally (setf (aref result i) #\"))
       result))
    
    ((eql *tag-SYMBOL*        type-tag)
     ;; TODO: package:symbol
     (unbox-string (bootstrap-cl:symbol-name cur-obj)))
        
    ((eql *tag-other*         type-tag)
     ;; TODO
     (let ((type (bootstrap-cl:type-of cur-obj)))
       (cond
         ((eql @package type)
          (concatenate 'string "#<PACKAGE " (unbox-string (bootstrap-cl:package-name cur-obj)) ">"))
         (t
          (concatenate 'string "#<" (unbox-string (bootstrap-cl:symbol-name type)) ">")))))
    
    (t
     (error "base-prin1-to-string called on unexpected base type ~D."
            type-tag))))


(defun bootstrap-cl:prin1-to-string (obj)
  (enbox-string (%prin1-to-string obj)))

(defun %prin1-to-string (obj)
  (let ((output "")
        (stack (list 'element obj)))
    (loop
       :while stack
       :do (let ((kind    (pop stack))
                 (cur-obj (pop stack)))
             (let ((type-tag (box-tag cur-obj)))
               (cond
                 ((eql @nil cur-obj)
                  ;; (print '(cur-obj = nil))
                  (cond
                    ((eql kind 'element)          (concat output "NIL"))
                    ((eql kind 'list)             (concat output ")"))
                    ((eql kind 'vector-element)   (concat output " NIL"))
                    ((eql kind 'vector-close)     (concat output ")"))
                    ((eql kind 'hash-table-close) (concat output ")"))))
                 ((eql *tag-CONS*          type-tag)
                  ;; (print '(cur-obj is a cons))
                  (if (or (eql kind 'element)
                          (eql kind 'vector-element)
                          (eql kind 'hash-table-bucket))
                      (progn
                        (concat output "(")
                        (push (bootstrap-cl:cdr cur-obj) stack)
                        (push 'list         stack)
                        (push (bootstrap-cl:car cur-obj) stack)
                        (push 'element stack))
                      (progn
                        (concat output " ")
                        (push (bootstrap-cl:cdr cur-obj) stack)
                        (push 'list         stack)
                        (push (bootstrap-cl:car cur-obj) stack)
                        (push 'element stack))))
                 ((eql *tag-simple-vector* type-tag)
                  (concat output "#(")
                  (push @nil stack)
                  (push 'vector-close stack)
                  (loop
                     :for i :from (1- (unbox-fixnum (bootstrap-cl:length cur-obj))) :downto 0
                     :do (push (bootstrap-cl:svref cur-obj (enbox-fixnum i)) stack)
                     :do (push 'vector-element stack)))
                 ((eql *tag-hash-table* type-tag)
                  (concat output "#S(HASH-TABLE ")
                  (push @nil stack)
                  (push 'hash-table-close stack)
                  (loop
                     :with buckets = (bootstrap-cl::hash-table-buckets cur-obj)
                     :for i :from (1-  (%slot-length buckets)) :downto 0
                     :do (progn (push (%slot-ref buckets i) stack)
                                (push 'hash-table-bucket stack))))
                 (t
                  ;; (print `(cur-obj is an atom ,(base-prin1-to-string type-tag cur-obj)))
                  (cond
                    ((eql kind 'element)
                     (concat output       (base-prin1-to-string type-tag cur-obj)))
                    ((eql kind 'list)
                     (concat output " . " (base-prin1-to-string type-tag cur-obj) ")"))
                    ((eql kind 'vector-element)
                     (concat output " "   (base-prin1-to-string type-tag cur-obj)))
                    ((eql kind 'hash-table-bucket)
                     (concat output " "  (base-prin1-to-string type-tag cur-obj)))))))))
    output))


;; (bootstrap-cl:prin1-to-string (bootstrap-cl:make-array (enbox-fixnum 4) :initial-element (bootstrap-cl:cons @symbol (enbox-fixnum 42))))
;; (bootstrap-cl:prin1-to-string (bootstrap-cl:make-array (enbox-fixnum 4) :initial-element @symbol))
;; (bootstrap-cl:prin1-to-string (bootstrap-cl:make-array (enbox-fixnum 4)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; read-from-string

(defun bootstrap-cl:read-from-string (string &optional eof-error-p eof-value &key start end preserve-whitespace)
  (when preserve-whitespace (error "(bootstrap-cl:read-from-string :preserve-whitespace t)  is not implemented yet"))
  (unless eof-error-p       (error "(bootstrap-cl:read-from-string (not eof-error-p))  is not implemented yet"))
  (if (or start end)
      (%read-from-string (subseq string start end))
      (%read-from-string string)))


(defun make-str (str) (enbox-string str))
(defun make-sym (str) (bootstrap-cl:intern (enbox-string str)))
(defun make-int (int) (enbox-fixnum int))

(defvar *scanner-text*)
(defvar *scanner-position*)
(defun init-scanner (text)
  (setf *scanner-text* text
        *scanner-position* 0))
(defconstant +eot+ 'nil)
(defun eotp (ch) (eql +eot+ ch))
(defun next-char ()
  (when (< *scanner-position* (length *scanner-text*))
    (prog1 (aref *scanner-text* *scanner-position*)
      (incf *scanner-position*))))
(defun parse-token (kind token)
  (case kind
    ((symbol) (values 'symbol (make-sym token)))
    (otherwise
     (let ((value (parse-integer token :junk-allowed t)))
       (if (string= token (prin1-to-string value))
           (values 'integer (make-int value))
           (values 'symbol  (make-sym token)))))))


(defun %read-from-string (text)
  (init-scanner text)
  (let ((ch #\space)
        type value
        (stack '(parse done eot nil)))
    (macrolet ((stop () `(car stack))
               (spop ()
                 '(pop stack)
                 #-(and)
                 (let ((val (gensym)))
                   `(let ((,val (pop stack)))
                      (format t "(pop ~A) --> ~S~%~A = ~S~%~%"
                              'stack ,val 'stack stack)
                      ,val)))
               (spush (expr)
                 `(push ,expr stack)
                 #-(and)
                 (let ((vexpr (gensym)))
                   `(let ((,vexpr ,expr))
                      (format t "~A = ~S~%(push  ~S = ~S  ~A)~%"
                              'stack stack ',expr  ,vexpr 'stack)
                      (prog1 (push ,vexpr stack)
                        (format t  "~A = ~S~2%"  'stack stack))))))
      (loop
         :for state = (spop)
         :until (eql state 'done)
         ;; :do (terpri) (print state) (print stack) 
         :do (ecase state
               ((parse)
                (progn
                  (loop
                     :named skip-spaces
                     :while (and (not (eotp ch)) (or (char= #\space ch) (char= #\newline ch)))
                     :do (setf ch (next-char)))
                  (case ch
                    ((#.+eot+)
                     (spush nil)
                     (spush 'eot)
                     (spush 'done))
                    ((#\')
                     (spush 'quote)
                     (spush 'parse))
                    ((#\")
                     (loop
                        :with strstate = 'in-string
                        :with str = ""
                        :until (or (eotp ch) (eql strstate 'done))
                        :do (setf ch (next-char))
                        :do (ecase strstate
                              ((in-string)
                               (case ch
                                 ((#\") (setf strstate 'done))
                                 ((#\\) (setf strstate 'escaped))
                                 (otherwise (concat str ch))))
                              ((escaped)
                               (concat str ch)
                               (setf strstate 'in-string)))
                        :finally (if (eotp ch)
                                     (error "End-Of-Text in the string ~S" str)
                                     (progn
                                       (spush (make-str str))
                                       (spush 'string)
                                       (spush 'token)
                                       (setf ch (next-char))))))
                    ((#\()
                     (spush @nil)
                     (spush 'list)
                     (spush 'parse)
                     (setf ch (next-char)))
                    ((#\))
                     (if (eql 'list (stop))
                         (progn (spop)
                                (spush (bootstrap-cl:nreverse (spop)))
                                (spush 'cons)
                                (spush 'token)
                                (setf ch (next-char)))
                         (error "An object cannot begin with #\\)")))
                    ((#\;)
                     (loop :named comment :until (or (scanner-eot) (char= #\newline (next-char))))
                     (spush 'parse))
                    ((#\` #\, #\#)
                     (error "Reader macro for character #\\~C is not implemented yet." ch))
                    (otherwise
                     (loop
                        :with kind = nil
                        :with tokenstate = 'in-token
                        :with token = ""
                        :until (eql tokenstate 'done)
                        :do (ecase tokenstate
                              ((in-token)
                               (case ch
                                 ((#.+eot+)
                                  (setf tokenstate 'done)
                                  (multiple-value-bind (type value) (parse-token kind token)
                                    (progn (spush value)
                                           (spush type)
                                           (spush 'token))))
                                 ((#\space #\' #\" #\; #\, #\` #\( #\) #\newline)
                                  (setf tokenstate 'done)
                                  (multiple-value-bind (type value) (parse-token kind token)
                                    (progn (spush value)
                                           (spush type)
                                           (spush 'token))))
                                 ((#\\)
                                  (setf tokenstate 'escape1 kind 'symbol)
                                  (setf ch (next-char)))
                                 ((#\|)
                                  (setf tokenstate 'escape2 kind 'symbol)
                                  (setf ch (next-char)))
                                 (otherwise
                                  (unless (or (eotp ch)
                                              (digit-char-p ch)
                                              (position ch "+-.EeSsFfDdLl"))
                                    (setf kind 'symbol))
                                  (concat token (char-upcase ch))
                                  (setf ch (next-char)))))
                              ((escape1)
                               (when (eotp ch)
                                 (error "Escaping EOT"))
                               (concat token ch)
                               (setf tokenstate 'in-token)
                               (setf ch (next-char)))
                              ((escape2)
                               (case ch
                                 ((#.+eot+) (error "EOT in multiple escape"))
                                 ((#\\)     (setf tokenstate 'escape21))
                                 ((#\|)     (setf tokenstate 'in-token))
                                 (otherwise (concat token ch)))
                               (setf ch (next-char)))
                              ((escape21)
                               (when (eotp ch)
                                 (error "Escaping EOT"))
                               (concat token ch)
                               (setf tokenstate 'escape2)
                               (setf ch (next-char)))))))))
               ((token)
                (let ((type  (spop))
                      (value (spop)))
                  (ecase (setf state (spop))
                    ((done)
                     (spop)
                     (spop)
                     (spush value)
                     (spush type)
                     (spush 'done))
                    ((list)
                     (spush (bootstrap-cl:cons value (spop)))
                     (spush 'list)
                     (spush 'parse))
                    ((quote)
                     (spush (bootstrap-cl:list @quote value))
                     (spush 'cons)
                     (spush 'token))))))
         :finally
         ;; (terpri) (print state) (print stack)
         (let ((type  (spop))
               (value (spop)))
           (return (values value *scanner-position*)))))))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;


(defun bootstrap-cl:sxhash (object)
  (cond
    ((bs-stringp object)      (string%sxhash    (unbox-string object)))
    ((bs-fixnump object)      (fixnum%sxhash    (unbox-fixnum object)))
    ((bs-characterp object)   (character%sxhash (unbox-character object)))
    ((bs-consp object)        (cons%sxhash object))
    ((bs-vectorp object)      (vector%sxhash object))
    ((bs-hash-table-p object) (hash-table%sxhash object))
    (t (error "Cannot compute sxhash of ~A" (%prin1-to-string object)))))



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;

(initialize-heap)

(defparameter *type-map*
  (list (list *tag-fixnum*        @FIXNUM)
        (list *tag-character*     @CHARACTER)
        (list *tag-string*        @STRING)
        (list *tag-CONS*          @CONS)
        (list *tag-SYMBOL*        @SYMBOL)
        (list *tag-simple-vector* @SIMPLE-VECTOR)
        (list *tag-hash-table*    @HASH-TABLE)))

(defparameter bootstrap-cl:nil @nil)
(defparameter bootstrap-cl:t   @t)

(defparameter bootstrap-cl::*list-all-packages* @nil)

(defparameter *common-lisp-package*
  (bootstrap-cl:make-package (str "COMMON-LISP")
                             :nicknames (bootstrap-cl:list (str "CL"))))

(defparameter *common-lisp-user-package*
  (bootstrap-cl:make-package (str "COMMON-LISP-USER")
                             :nicknames (bootstrap-cl:list (str "CL-USER"))
                             :use  (bootstrap-cl:list (str "COMMON-LISP"))))

(defparameter *keyword-package* (bootstrap-cl:make-package (str "KEYWORD")))

(defparameter bootstrap-cl:*package* (bootstrap-cl:find-package (str "CL-USER")))

(dolist (sym *cl-syms*)
  (bootstrap-cl::set-symbol-package sym *common-lisp-package*)
  (bootstrap-cl::sethash (bootstrap-cl:symbol-name sym)
                         (bootstrap-cl::package-interned-symbols
                          *common-lisp-package*)
                         sym))
(dolist (sym *kw-syms*)
  (bootstrap-cl::set-symbol-package sym *keyword-package*)
  (bootstrap-cl::sethash (bootstrap-cl:symbol-name sym)
                         (bootstrap-cl::package-interned-symbols
                          *keyword-package*)
                         sym))





;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;

;;;; THE END ;;;;
