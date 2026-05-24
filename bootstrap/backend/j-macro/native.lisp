;;;;  -*- mode:lisp;coding:utf-8 -*-
;;;;**************************************************************************
;;;; FILE:               j-macro/native.lisp
;;;; LANGUAGE:           Common-Lisp
;;;; SYSTEM:             Common-Lisp
;;;; USER-INTERFACE:     NONE
;;;; DESCRIPTION
;;;;     
;;;;    Target (Objecteering) J macros.
;;;;    Implementation of the backend native data types.
;;;;
;;;;    Note: this is a common lisp implementation of the storage, as it
;;;;          will be implemented in J macro language.
;;;;
;;;; AUTHORS
;;;;     <PJB> Pascal Bourguignon <pjb@informatimago.com>
;;;; MODIFICATIONS
;;;;     2008-05-26 <PJB> Created.
;;;; BUGS
;;;; LEGAL
;;;;     GPL
;;;;     
;;;;     Copyright Pascal Bourguignon 2008 - 2008
;;;;     
;;;;     This program is free software; you can redistribute it and/or
;;;;     modify it under the terms of the GNU General Public License
;;;;     as published by the Free Software Foundation; either version
;;;;     2 of the License, or (at your option) any later version.
;;;;     
;;;;     This program is distributed in the hope that it will be
;;;;     useful, but WITHOUT ANY WARRANTY; without even the implied
;;;;     warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
;;;;     PURPOSE.  See the GNU General Public License for more details.
;;;;     
;;;;     You should have received a copy of the GNU General Public
;;;;     License along with this program; if not, write to the Free
;;;;     Software Foundation, Inc., 59 Temple Place, Suite 330,
;;;;     Boston, MA 02111-1307 USA
;;;; *************************************************************************

(defpackage "COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.J-MACRO.NATIVE"
  (:use "COM.INFORMATIMAGO.J-LANGUAGE"
        "COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.NATIVE")
  (:export
   "FIXNUM+" "FIXNUM-" "FIXNUM*" "FIXNUM-TRUNCATE" "FIXNUM<" "FIXNUM<="
   "FIXNUM>" "FIXNUM>=" "FIXNUM=" "FIXNUM/=" "CHARACTER<" "CHARACTER<="
   "CHARACTER>" "CHARACTER>=" "CHARACTER=" "CHARACTER/="
   "CHARACTER-EQUALP" "CHARACTER-LESSP" "CHARACTER-GREATERP"
   "STRING-LENGTH" "STRING-REF" "STRING-SET" "STRING-SUBSEQ" "STRING<"
   "STRING<=" "STRING>" "STRING>=" "STRING=" "STRING/=" "STRING-EQUALP"
   "STRING-LESSP" "STRING-GREATERP" "STRING-COPY" "FIXNUM-TO-STRING"
   "FIXNUM-TO-CHARACTER" "STRING-TO-FIXNUM" "STRING-TO-CHARACTER"
   "CHARACTER-TO-FIXNUM" "CHARACTER-TO-STRING" 
   )
  (:documentation "
Target (Objecteering) J macros.
Implementation of the backend native data types.

Note: this is a common lisp implementation of the storage, as it
      will be implemented in J macro language.

Copyright Pascal Bourguignon 2008 - 2008
 
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version
2 of the License, or (at your option) any later version.
"))

(in-package "COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.J-MACRO.NATIVE")


(define-function fixnum+         (a b) (return (cl:values cl:nil (+ a b))))
(define-function fixnum-         (a b) (return (cl:values cl:nil (- a b))))
(define-function fixnum*         (a b) (return (cl:values cl:nil (* a (/ b 16)))))
(define-function fixnum-truncate (a b) (return (cl:values cl:nil (* 16 (/ a b)) (* 16  (% (/ a 16) (/ b 16))))))

(define-function fixnum<         (a b) (return (cl:values (< a b))))
(define-function fixnum<=        (a b) (return (cl:values (<= a b))))
(define-function fixnum>         (a b) (return (cl:values (> a b))))
(define-function fixnum>=        (a b) (return (cl:values (>= a b))))
(define-function fixnum=         (a b) (return (cl:values (== a b))))
(define-function fixnum/=        (a b) (return (cl:values (!= a b))))


(define-function character<         (a b) (return (cl:values (< a b))))
(define-function character<=        (a b) (return (cl:values (<= a b))))
(define-function character>         (a b) (return (cl:values (> a b))))
(define-function character>=        (a b) (return (cl:values (>= a b))))
(define-function character=         (a b) (return (cl:values (== a b))))
(define-function character/=        (a b) (return (cl:values (!= a b))))
(define-function character-equalp   (a b) (return (cl:values (== (to-lower a) (to-lower b)))))
(define-function character-lessp    (a b) (return (cl:values (< (to-lower a) (to-lower b)))))
(define-function character-greaterp (a b) (return (cl:values (> (to-lower a) (to-lower b)))))


(define-function string-length   (a)     (return (cl:values cl:nil (size a))))
(define-function string-ref      (a i)
  (var res string)
  (if (and (<= 0 i) (< i (size a)))
    (progn
      (string-assign res a i (+ i 1))
      (return (cl:values cl:nil res)))
    (return (cl:values t ""))))
(define-function string-set      (a i c)
  (if (and (<= 0 i) (< i (size a)))
    (progn
      (string-overwrite a c i)
      (return (cl:values cl:nil)))
    (return (cl:values t))))
(define-function string-subseq   (a s e)
  (var res string)
  (if (and (<= 0 s) (<= s e) (< e (size a)))
    (progn
      (string-assign res a s (- e 1))
      (return (cl:values cl:nil res)))
    (return (cl:values t ""))))


(define-function string-copy (str)
  (var x string str)
  x)

(define-function string<         (a b) (return (cl:values (< a b))))
(define-function string<=        (a b) (return (cl:values (<= a b))))
(define-function string>         (a b) (return (cl:values (> a b))))
(define-function string>=        (a b) (return (cl:values (>= a b))))
(define-function string=         (a b) (return (cl:values (== a b))))
(define-function string/=        (a b) (return (cl:values (!= a b))))
(define-function string-equalp   (a b) (return (cl:values (== (to-lower a) (to-lower b)))))
(define-function string-lessp    (a b) (return (cl:values (< (to-lower a) (to-lower b)))))
(define-function string-greaterp (a b) (return (cl:values (> (to-lower a) (to-lower b)))))



(define-function fixnum-to-string    (a) (return (cl:values cl:nil (to-string a))))

(define-function fixnum-to-character (a)
  (var result string)
  (cond
    ((= 10 a) (return (cl:values cl:nil NL)))
    ((= 9 a)  (return (cl:values cl:nil HT)))
    ((and (<= 32 a) (<= a 126))
     (get-item-set *characters* (- a 32) result)
     (return (cl:values cl:nil result)))))

(define-function string-to-fixnum    (a) (return (cl:values cl:nil (to-int a))))

(define-function string-to-character (a)
  (var result string)
  (if (<= 1 (size a))
    (progn
      (string-assign result a 0 0)
      (return (cl:values cl:nil result)))
    (return (cl:values cl:nil "x"))))

(define-function character-to-fixnum (a)
  (var result int)
  (var found boolean false)
  (var char string)
  (cond
    ((== NL a) (return (cl:values cl:nil 10)))
    ((== HT a) (return (cl:values cl:nil 9)))
    (t (for ( (set result 0)
              (and (not found) (< result (size *characters*)))
              (set result (+ result 1)) )
            (get-item-set *characters* result char)
            (if (== char a)
              (progn
                (set result (+ result 32))
                (set found true))))
       (return (cl:values cl:nil result)))))

(define-function character-to-string (a)
  (var result string)
  (string-assign result a 0 (- (size a) 1))
  (return (cl:values cl:nil result)))


;;;; THE END ;;;;
