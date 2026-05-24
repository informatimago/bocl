;;;; -*- mode:lisp;coding:utf-8 -*-
;;;;**************************************************************************
;;;;FILE:               j-macro/storage.lisp
;;;;LANGUAGE:           Common-Lisp
;;;;SYSTEM:             Common-Lisp
;;;;USER-INTERFACE:     NONE
;;;;DESCRIPTION
;;;;    
;;;;    Target (Objecteering) J macros.
;;;;    Implementation of the backend storage.
;;;;
;;;;    Note: this is a common lisp implementation of the storage, as it
;;;;          will be implemented in J macro language.
;;;;    
;;;;AUTHORS
;;;;    <PJB> Pascal Bourguignon <pjb@informatimago.com>
;;;;MODIFICATIONS
;;;;    2008-05-25 <PJB> Created.
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

(defpackage "COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.J-MACRO.STORAGE"
  (:use "COM.INFORMATIMAGO.J-LANGUAGE"
        "COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.STORAGE")
  (:export
    "*TAG-FIXNUM*" "*TAG-CHARACTER*" "*TAG-STRING*" "*TAG-CONS*"
    "*TAG-SIMPLE-VECTOR*" "*TAG-SYMBOL*" "*TAG-HASH-TABLE*" "*TAG-OTHER*"
    "MAKE-BOX" "BOX-TAG" "BOX-VALUE" "ENBOX-FIXNUM" "ENBOX-CHARACTER"
    "ENBOX-STRING" "UNBOX-FIXNUM" "UNBOX-CHARACTER" "UNBOX-STRING"
    "ALLOCATE" "STORE-BOX" "FETCH-BOX" "TRAP-ADDRESS"
    "MARK" "UNMARK" "MARKEDP" "COLLECT-GARBAGE" "INITIALIZE-STORAGE"
   )
  (:documentation "

Target (Objecteering) J macros.
Implementation of the backend storage.

Note: this is a common lisp implementation of the storage, as it will
      be implemented in J macro language.  It can be translated
      automatically by the j-macro/translator.lisp
 
Copyright Pascal Bourguignon 2008 - 2008

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version
2 of the License, or (at your option) any later version.
"))
(in-package "COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.J-MACRO.STORAGE")



;; Since we cannot allocate objects on a heap in this implementation,
;; we manage our own heap in J sets.
;; The basic storage is a J set of integers.
;; To store characters and strings, we'll allocate slots in a J set of strings.

(define-variable *storage*       (jset int))
(define-variable *strings*       (jset string))
(define-variable *free-strings*  (jset int))

;; (define-variable *free-storage*  0)
(define-function INITIALIZE-STORAGE ()
  (clear *storage*)
  (clear *strings*)
  (clear *free-strings*))

;; Box format:
;; +--------------------------------------+---+-+
;; |   the width of J int  is unknown     |TAG|M|
;; +--------------------------------------+---+-+
;;  The type tag is stored in bits 1-3.
;;  The garbage collector mark bit is stored in bit 0.
;;  Usually, the mark bit is 0.
;;  The fixnum type-tag is 0 so fixnums can be added directly.

(define-variable *tag-fixnum*          int  0)
(define-variable *tag-cons*            int  2)
(define-variable *tag-character*       int  4)
(define-variable *tag-simple-vector*   int  6)
(define-variable *tag-string*          int  8)
(define-variable *tag-symbol*          int 10)
(define-variable *tag-hash-table*      int 12)
(define-variable *tag-other*           int 14)



(define-function make-box  (tag value)   (return (+ (* value 16) tag)))
(define-function box-tag   (box)         (return (% box 16)))
(define-function box-value (box)         (return (/ box 16)))



(define-function enbox-fixnum    (native-fixnum)
  (cl:assert (cl:integerp native-fixnum))
  ;; (make-box *tag-fixnum* native-fixnum)
  (return (* native-fixnum 16)))

(define-function enbox-character (native-character)
  (var string-ref int (size *strings*))
  (cl:assert (and (cl:stringp native-character)
                  (== 1 (cl:length native-character))))
  (add-element *strings* native-character)
  (return (make-box *tag-character* string-ref)))

(define-function enbox-string    (native-string)
  (var string-ref int (size *strings*))
  (cl:assert (cl:stringp native-string))
  (add-element *strings* native-string)
  (return (make-box *tag-string* string-ref)))

(define-function unbox-fixnum    (box)
  (cl:assert (== *tag-fixnum* (box-tag box)))
  (return (/ box 16)))

(define-function unbox-character (box)
  (var result string)
  (cl:assert (== *tag-character* (box-tag box)))
  (get-item-set *strings* (box-value box) result)
  (return result))

(define-function unbox-string    (box)
  (var result string)
  (cl:assert (== *tag-string* (box-tag box)))
  (get-item-set *strings* (box-value box) result)
  (return result))





(define-function allocate (size)
  (var result   int  (size *storage*))
  (var i int)
  (for ( (set i 0) (< i size) (set i (+ i 1)) )
       (add-element *storage*   0))
  (return result))


(cl:defparameter *traps* (cl:make-hash-table))

(define-function trap-address (fun start-address end-address)
  (cl:loop :for a :from start-address :below end-address
     :do (cl:setf (cl:gethash a *traps*) fun)))

(define-function store-box  (address box)
  (cl:let ((trap (cl:gethash address *traps*)))
    (cl:when trap (cl:funcall trap address 1 box)))
  (set-item-set *storage* address box)
  (return))

(define-function fetch-box  (address)
  (var box int)
  (get-item-set *storage* address box)
  (cl:let ((trap (cl:gethash address *traps*)))
    (cl:when trap (cl:funcall trap address 2 box)))
  (return box))





(define-function mark     (address)
  (var box int)
  (get-item-set *storage* address box)
  (set-item-set *storage* address (+ (- box (% box 2)) 1))
  (return))

(define-function unmark   (address)
  (var box int)
  (get-item-set *storage* address box)
  (set-item-set *storage* address (- box (% box 2)))
  (return))

(define-function markedp  (address)
  (var box int)
  (get-item-set *storage* address box)
  (return (!= 0 (% box 2))))


;;;; THE END ;;;;
