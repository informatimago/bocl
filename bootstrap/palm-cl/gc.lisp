

#||

DmGetRecord(db,index:UInt16) --> void**
DmNewRecord(db,index:UInt16*,UInt32 size) --> void**
DmReleaseRecord(db,index:UInt16,dirty:Boolean)

DmRemoveRecord(db,index:UInt16)


||#

#||

The virtual machine is compiled and stored in code resources.

The lisp system is byte-compiled and stored in the lisp image, a
separate database.

Here, we're concerned by the lisp image memory.

||#

;; "physical memory": is composed of "pages", ie db records:

;; public interface:

(deftype address (integer 0 (1- (expt 2 32))))

(defconstant *log-page-size* 15)
(defconstant *page-size*     (expt 2 *log-page-size*) "The page size")

(defun page-map-db   (card-no db-name))
(defun page-unmap-db ())

(defun page-alloc  ()) ;; --> address
(defun page-free   (page-address))


;; implementation:

(defvar *card-no*    )
(defvar *db-name*    )
(defvar *db-local-id*)
(defvar *db-ref*     )
(defvar *num-pages*  )
(defvar *page-map*   )


(defun page-map-db (card-no db-name)
  (setf *card-no*     card-no
        *db-name*     db-name
        *db-local-id* (dm-find-database card-no db-name))
  (when (zerop *db-local-id*)
      (os-error (dm-get-last-err)))
  (setf *db-ref* (dm-open-database *card-no* *db-local-id*
                                   (+ dm-mode-read-write
                                      dm-mode-exclusive)))
  (setf *num-pages* (dm-num-records *db-ref*))
  (setf *page-map*  (make-array (list *num-pages*)
                                :element-type 'address
                                :adjustable t))
  (do ((i 0 (1+ i))) ((>= i *num-pages*))
    (setf (aref *page-map* i) (mem-handle-lock (dm-get-record *db-ref* i)))))


(defun page-unmap-db ()
  (do ((i 0 (1+ i))) ((>= i *num-pages*))
    (dm-release-record *db-ref* i t))
  (dm-close-database *db-ref*))


(defun page-alloc  ()
  (let ((new-page-handle (dm-new-record *db-ref* *page-size*)))
    (when (zerop new-page-handle)
      (os-error (dm-get-last-err)))
    (adjust-array *page-map* (list (incf *num-pages*)))
    (setf (aref *page-map* (1- *num-pages*)) (mem-handle-lock new-page))
    new-page))


(defun page-free   (page-address)
  (if (equal page-address (aref *page-map* (1- *num-pages*)))
    (progn
      (decf *num-pages*)
      (may-be-os-error (mem-ptr-unlock (aref *page-map* *num-pages*)))
      (may-be-os-error (dm-remove-record *db-ref*  *num-pages*))
      (adjust-array *page-map* (list *num-pages*)))
    (error "Can't free page in the middle.")))





(defconstant *offset-mask*  (1- *page-size*) "00001111")
(defconstant *page-mask*    (- (1- *word-size*) *offset-mask*) "11110000")


(defun virtual-load-word  (vaddress)
  (let ((page-index (ash    vaddress (- *log-page-size*)))
        (offset     (logand vaddress *offset-mask* )))
    (deref-word (+ (aref *page-map* page-index) offset))))


(defun virtual-load-byte  (vaddress)
  (let ((page-index (ash    vaddress (- *log-page-size*)))
        (offset     (logand vaddress *offset-mask* )))
    (deref-byte (+ (aref *page-map* page-index) offset))))


(defun virtual-store-word (vaddress word)
  (let ((page-index (ash    vaddress (- *log-page-size*)))
        (offset     (logand vaddress *offset-mask* )))
    (setf (deref-word (+ (aref *page-map* page-index) offset)) word)))


(defun virtual-store-byte (vaddress byte)
  (let ((page-index (ash    vaddress (- *log-page-size*)))
        (offset     (logand vaddress *offset-mask* )))
    (setf (deref-byte (+ (aref *page-map* page-index) offset)) byte)))

  

;;;; gc.lisp                          -- 2004-01-07 03:06:04 -- pascal   ;;;;ch
