;;;; -*- mode:lisp;coding:utf-8 -*-
;;;;**************************************************************************
;;;;FILE:               j-macro.lisp
;;;;LANGUAGE:           Common-Lisp
;;;;SYSTEM:             Common-Lisp
;;;;USER-INTERFACE:     NONE
;;;;DESCRIPTION
;;;;    
;;;;    
;;;;    
;;;;AUTHORS
;;;;    <PJB> Pascal J. Bourguignon <pjb@informatimago.com>
;;;;MODIFICATIONS
;;;;    2009-03-07 <PJB> Extracted from bootstrap.lisp
;;;;BUGS
;;;;LEGAL
;;;;    GPL
;;;;    
;;;;    Copyright Pascal J. Bourguignon 2009 - 2009
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
(defpackage "J-MACRO"
  (:use "CL")
  (:export
   ;; Basic lisp objects and storage:
   "*TAG-FIXNUM*" "*TAG-CHARACTER*" "*TAG-STRING*"
   "*TAG-SYMBOL*" "*TAG-CONS*"      "*TAG-PACKAGE*"
   "MAKE-BOX" "BOX-TAG" "BOX-VALUE"
   "ALLOCATE" "STORE-BOX" "FETCH-BOX"
   ;; Garbage collector:
   "*GARBAGE-COLLECTOR*" "MARK" "UNMARK" "MARKEDP"))
(in-package "J-MACRO")


#||

In J-STORAGE,
  immediate values:
    fixnum        = *tag-fixnum*    + integer-value                  (int, int)
    character     = *tag-character* + 1-char-string-value            (int, string)
    string        = *tag-string*    + string-value                   (int, string)
  reference values:
    package       = *tag-package*   + reference-to-package-structure (int, int)
    symbol        = *tag-symbol*    + reference-to-symbol-structure  (int, int)
    cons          = *tag-cons*      + reference-to-cons-pair         (int, int)
    simple-vector = *tag-simple-vector* + reference-to-length        (int, int)

(defvar *tag-fixnum*    'fixnum)
(defvar *tag-character* 'character)
(defvar *tag-string*    'string)
(defvar *tag-symbol*    'symbol)
(defvar *tag-cons*      'cons)
(defvar *tag-package*   'package)

(defun make-box (tag value) ; --> box
  )
(defun box-tag   (box)
  )
(defun box-value (box)
  )
(defun allocate () ; --> address
  )
(defun store-box (address box)
  )
(defun fetch-box (address) ; --> box
  )

||#


(defparameter *tag-fixnum*    1)
(defparameter *tag-character* 2)
(defparameter *tag-string*    3)
(defparameter *tag-symbol*    4)
(defparameter *tag-cons*      5)

;; Since we cannot allocate objects on a heap in this implementation,
;; we manage our own heap in vectors:

(defconstant +storage-size+ (* 128 1024))
(defvar *marks*      (make-array +storage-size+ :element-type 'bit    :initial-element 0))
(defvar *type-tags*  (make-array +storage-size+ :element-type 'fixnum :initial-element 0))
(defvar *storage*    (make-array +storage-size+ :element-type 'string :initial-element ""))
(defvar *free-storage*  0)
(defvar *garbage-collector* nil)


#+boot-strap::debug (setf *print-length* 100)
;; (setf *marks*      (make-array +storage-size+ :element-type 'bit    :initial-element 0)
;;       *type-tags*  (make-array +storage-size+ :element-type 'fixnum :initial-element 0)
;;       *storage*    (make-array +storage-size+ :element-type 'string :initial-element "")
;;       *free-storage*  0)


(defstruct (box (:constructor make-box (tag value))) tag value)


(defun allocate (size)
  (let ((result *free-storage*))
    (incf *free-storage* size)
    (when (<= +storage-size+ *free-storage*)
      (when *garbage-collector*
        (decf *free-storage* size)
        (funcall *garbage-collector*)
        (setf result *free-storage*)
        (incf *free-storage* size))
      (when (<= +storage-size+ *free-storage*)
        (error "Out of memory")))
    result))


(defun store-box (address box)
  (let ((tag (box-tag box)))
    (setf (aref *type-tags* address) tag
          (aref *storage* address) (if  (or (eql tag *tag-string*)  (eql tag *tag-character*))
                                        (box-value box)
                                        (prin1-to-string (box-value box))))
    box))


(defun fetch-box (address)              ; --> box
  (let ((tag (aref *type-tags* address)))
    (make-box tag
              (if (or (eql tag *tag-string*)  (eql tag *tag-character*))
                  (aref *storage* address)
                  (parse-integer (aref *storage* address))))))

(defun mark     (address) (setf (aref *marks* address) 1))
(defun unmark   (address) (setf (aref *marks* address) 0))
(defun markedp  (address) (plusp (aref *marks* address)))

;;;; THE END ;;;;
