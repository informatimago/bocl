;;;;  -*- mode:lisp;coding:utf-8 -*-
;;;;**************************************************************************
;;;; FILE:               native-package.lisp
;;;; LANGUAGE:           Common-Lisp
;;;; SYSTEM:             Common-Lisp
;;;; USER-INTERFACE:     NONE
;;;; DESCRIPTION
;;;;     
;;;;     This package defines the API to the backend native data types.
;;;;     Fixnum, characters and strings.
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

(defpackage "COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.NATIVE"
  (:use "COMMON-LISP")
  (:shadow
   "STRING<" "STRING<=" "STRING>" "STRING>=" "STRING=" "STRING/=" 
   "STRING-LESSP" "STRING-GREATERP")
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
This package defines the API to the backend native data types.
Fixnum, characters and strings.  All the arguments and results
are native values.

Copyright Pascal Bourguignon 2008 - 2008
 
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version
2 of the License, or (at your option) any later version.
"))

#||

(defun fixnum+         (a b) (values errorp result))
(defun fixnum-         (a b) (values errorp result))
(defun fixnum*         (a b) (values errorp result))
(defun fixnum-truncate (a b) (values errorp quotient remainder))

(defun fixnum<         (a b) (values predicate))
(defun fixnum<=        (a b) (values predicate))
(defun fixnum>         (a b) (values predicate))
(defun fixnum>=        (a b) (values predicate))
(defun fixnum=         (a b) (values predicate))
(defun fixnum/=        (a b) (values predicate))


(defun character<         (a b) (values predicate))
(defun character<=        (a b) (values predicate))
(defun character>         (a b) (values predicate))
(defun character>=        (a b) (values predicate))
(defun character=         (a b) (values predicate))
(defun character/=        (a b) (values predicate))
(defun character-equalp   (a b) (values predicate))
(defun character-lessp    (a b) (values predicate))
(defun character-greaterp (a b) (values predicate))


(defun string-length   (a)     (values errorp fixnum))
(defun string-ref      (a i)   (values errorp character))
(defun string-set      (a i c) (values errorp))
(defun string-subseq   (a s e) (values errorp string))
       
(defun string<         (a b) (values predicate))
(defun string<=        (a b) (values predicate))
(defun string>         (a b) (values predicate))
(defun string>=        (a b) (values predicate))
(defun string=         (a b) (values predicate))
(defun string/=        (a b) (values predicate))
(defun string-equalp   (a b) (values predicate))
(defun string-lessp    (a b) (values predicate))
(defun string-greaterp (a b) (values predicate))
(defun string-copy     (str) (values copy))


(defun fixnum-to-string    (a) (values errorp string))
(defun fixnum-to-character (a) (values errorp character))
(defun string-to-fixnum    (a) (values errorp fixnum))
(defun string-to-character (a) (values errorp character))
(defun character-to-fixnum (a) (values errorp fixnum))
(defun character-to-string (a) (values errorp string))


||#

;;;; THE END ;;;;
