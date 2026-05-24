;;;; -*- mode:lisp;coding:utf-8 -*-
;;;;**************************************************************************
;;;;FILE:               storage.lisp
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
   ;; Basic lisp objects and storage:
   "*TAG-FIXNUM*" "*TAG-CHARACTER*" "*TAG-STRING*"
   "*TAG-SYMBOL*" "*TAG-CONS*"      "*TAG-SIMPLE-VECTOR*"
   "*TAG-PACKAGE*"
   "ENBOX-FIXNUM" "ENBOX-CHARACTER" "ENBOX-STRING"
   "UNBOX-FIXNUM" "UNBOX-CHARACTER" "UNBOX-STRING"
   "MAKE-BOX" "FETCH-BOX-TAG" "FETCH-BOX-VALUE"
   "ALLOCATE" "STORE-BOX" "FETCH-BOX"
   ;; For the garbage collector:
   "COLLECT-GARBAGE" "MARK" "UNMARK" "MARKEDP"))
(in-package "COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.J-MACRO.STORAGE")



;; Since we cannot allocate objects on a heap in this implementation,
;; we manage our own heap in J sets.
;; The basic storage is a J set of integers.
;; To store characters and strings, we'll allocate slots in a J set of strings.

(define-variable *marks*         (jset int))
(define-variable *type-tags*     (jset int))
(define-variable *storage*       (jset int))
(define-variable *strings*       (jset string))
(define-variable *free-strings*  (jset int))





(define-variable *tag-fixnum*          int 1)
(define-variable *tag-character*       int 2)
(define-variable *tag-string*          int 3)

(define-variable *tag-cons*            int 4)
(define-variable *tag-simple-vector*   int 5)
(define-variable *tag-symbol*          int 6)
(define-variable *tag-package*         int 7)


(define-function allocate (size)
  (var result   int  (size *type-tags*))
  (var i int)
  (for ( (set i 0) (< i size) (set i (+ i 1)) )
       (add-element *type-tags* *tag-fixnum*)
       (add-element *marks*     0)
       (add-element *storage*   0))
  (return result))

(define-function free (address size)
  (if (and (== 1 size)
           (== *tag-string* (fetch-box-tag address)))
    (block 
        (var address int (fetch-box-value address))
      (set-item-set *strings* address "")
      (add-element  *free-strings* address))))

(define-function store-box  (address tag ref)
  (set-item-set *type-tags* address tag)
  (set-item-set *storage*   address ref)
  (return))


(define-function fetch-box-tag    (address)
  (var tag int)
  (get-item-set *type-tags* address tag)
  (return tag))

(define-function fetch-box-value  (address)
  (var value int)
  (get-item-set *storage* address value)
  (return value))


(define-function enbox-fixnum    (address native-fixnum)
  (cl:assert (cl:integerp native-fixnum))
  (store-box address *tag-fixnum* native-fixnum))

(define-function enbox-character (address native-character)
  (var string-ref int (size *strings*))
  (cl:assert (and (cl:stringp native-character)
                  (== 1 (cl:length native-character))))
  (add-element *strings* native-string)
  (store-box address  *tag-character* string-ref))

(define-function enbox-string    (address native-string)
  (var string-ref int (size *strings*))
  (cl:assert (cl:stringp native-string))
  (add-element *strings* native-string)
  (store-box address  *tag-string* string-ref))

(define-function unbox-fixnum    (address)
  (cl:assert (== *tag-fixnum* (fetch-box-tag address)))
  (return (fetch-box-value box)))

(define-function unbox-character (address)
  (var result string)
  (cl:assert (== *tag-character* (fetch-box-tag address)))
  (get-item-set *strings* (fetch-box-value address) result)
  (return result))

(define-function unbox-string    (address)
  (var result string)
  (cl:assert (== *tag-string* (fetch-box-tag address)))
  (get-item-set *strings* (fetch-box-value address) result)
  (return result))


(define-function mark     (address) (set-item-set *marks* address 1))
(define-function unmark   (address) (set-item-set *marks* address 0))
(define-function markedp  (address)
  (var mark int)
  (get-item-set *marks* address mark)
  (return (< 0 mark)))


;;;; THE END ;;;;
