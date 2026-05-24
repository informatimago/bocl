;;;; -*- mode:lisp;coding:utf-8 -*-
;;;;**************************************************************************
;;;;FILE:               storage-package.lisp
;;;;LANGUAGE:           Common-Lisp
;;;;SYSTEM:             Common-Lisp
;;;;USER-INTERFACE:     NONE
;;;;DESCRIPTION
;;;;    
;;;;    This package defines the API to the backend storage.
;;;;    The symbols exported by this packages will be implemented
;;;;    by the actual backend.
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

(defpackage "COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.STORAGE"
  (:use "COMMON-LISP")
  (:export
    "*TAG-FIXNUM*" "*TAG-CHARACTER*" "*TAG-STRING*" "*TAG-CONS*"
    "*TAG-SIMPLE-VECTOR*" "*TAG-SYMBOL*" "*TAG-HASH-TABLE*" "*TAG-OTHER*"
    "MAKE-BOX" "BOX-TAG" "BOX-VALUE" "ENBOX-FIXNUM" "ENBOX-CHARACTER"
    "ENBOX-STRING" "UNBOX-FIXNUM" "UNBOX-CHARACTER" "UNBOX-STRING"
    "ALLOCATE" "STORE-BOX" "FETCH-BOX" "TRAP-ADDRESS"
    "MARK" "UNMARK" "MARKEDP" "COLLECT-GARBAGE"
    "INITIALIZE-STORAGE"
    ))



#|| **********************************************************************

The backend must implement a tagged storage.  This is a vector of
boxes, that can store boxes of "immediate" values.

The immediate values are of type:

    fixnum        (make-box *tag-fixnum*    native-integer-value)
    character     (make-box *tag-character* native-character-value)
    string        (make-box *tag-string*    native-string-value)

Boxes for immediate value can be stored in one storage slot (allocate 1).


The SIMPLE-HEAP package will implement over this storage a heap
storing compound objects. In compound objects, the value is referenced
from the box:

The reference values are of type:

    cons          (make-box *tag-cons*          reference-to-cons-pair)
    symbol        (make-box *tag-symbol*        reference-to-symbol-structure)
    simple-vector (make-box *tag-simple-vector* reference-to-vector-structure)

Boxes for reference values can be stored in one storage slot too
(allocate 1), but will refer to bigger memory blocks (allocate N).


The BACKEND.STORAGE package shall define the following values:

    (defvar *tag-fixnum*)
    (defvar *tag-character*)
    (defvar *tag-string*)

    (defvar *tag-cons*)
    (defvar *tag-symbol*)
    (defvar *tag-simple-vector*)
    (defvar *tag-hash-table*)
    (defvar *tag-other*)

and the following functions:


Boxes are encoding an integer value with a type tag into a single
value.  Usually a box is itself an integer value, but if the inferior
language has structures, it could be a structure.

    (defun make-box  (tag value))           ; --> box 
    (defun box-tag   (box))                 ; --> tag 
    (defun box-value (box))                 ; --> ref 

Note: boxes may have space for the garbage collector mark bit, but
it's used only for boxes stored in memory.


The immediate data types are enboxed by these functions.  Of course,
the inferior language may store some or all of these native values in
a separate data structure, referenced by the box.

    (defun enbox-fixnum    (native-fixnum))        ; --> box
    (defun enbox-character (native-character))     ; --> box
    (defun enbox-string    (native-string))        ; --> box
    (defun unbox-fixnum    (box))                  ; --> native-fixnum
    (defun unbox-character (box))                  ; --> native-character
    (defun unbox-string    (box))                  ; --> native-string


The memory can store boxes (and only boxes):
 
    (defun allocate  (size))                ; --> address 
    (defun store-box (address box))
    (defun fetch-box (address))             ; --> box 


Access to the memory can be trapped (or untrapped with NIL for fun):

    (defun trap-address (fun start-address end-address))
will call:
    (fun address 1 box) ; store
    (fun address 2 box) ; fetch


ALLOCATE may call the COLLECT-GARBAGE function.  

Each memory block can be marked and unmarked, by the garbage
collector:

    (defun mark     (address))
    (defun unmark   (address))
    (defun markedp  (address))              ; --> boolean


The function COLLECT-GARBAGE is defined by the BACKEND.STORAGE package.

    (defun COLLECT-GARBAGE ())


Reset the storage to the empty state.

    (defun INITIALIZE-STORAGE ())
 ********************************************************************** ||#

;;;; THE END ;;;;
