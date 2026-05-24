;;;; -*- mode:lisp;coding:utf-8 -*-
;;;;**************************************************************************
;;;;FILE:               tiny-printer.lisp
;;;;LANGUAGE:           Common-Lisp
;;;;SYSTEM:             Common-Lisp
;;;;USER-INTERFACE:     NONE
;;;;DESCRIPTION
;;;;    
;;;;    Tiny Lisp Printer.
;;;;
;;;;    This printer implements a fixed subset of the Common Lisp printer.
;;;;    It can print:
;;;;
;;;;        - proper lists,
;;;;        - dotted lists,
;;;;        - strings,
;;;;        - integers,
;;;;        - unqualified symbols, and
;;;;        - keywords.
;;;;
;;;;AUTHORS
;;;;    <PJB> Pascal J. Bourguignon <pjb@informatimago.com>
;;;;MODIFICATIONS
;;;;    2011-04-17 <PJB> Created.
;;;;BUGS
;;;;LEGAL
;;;;    GPL
;;;;    
;;;;    Copyright Pascal J. Bourguignon 2011 - 2011
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


(defconstant +digits+ "0123456789")


(defun tiny-prin1-to-buffer (object buffer)
  (cond
    ((integerp object)  (cond
                          ((zerop object)
                           (push (aref +digits+ 0) buffer))
                          (t
                           (if (minusp object)
                               (tiny-prin1-to-buffer (- object) (cons +minus+ buffer))
                               (do ((n object)
                                    (digit)
                                    (numbuf '()))
                                   ((zerop n)
                                    (setf buffer (revappend numbuf buffer)))
                                 (multiple-value-setq (n digit) (truncate n 10))
                                 (push (aref +digits+ digit) numbuf))))))
    ((stringp  object)  (progn
                          (push +double-quote+ buffer)
                          (do* ((len (length object))
                                (i 0 (1+ i)))
                               ((<= len i)
                                (push +double-quote+ buffer)
                                buffer)
                            (let ((ch (aref object i)))
                              (cond ((or (char= ch +double-quote+)
                                         (char= ch +back-slash+))
                                     (push +back-slash+ buffer)
                                     (push ch buffer))
                                    (t
                                     (push ch buffer)))))))
    ((symbolp  object)  (progn
                          (if (keywordp object)
                              (push +colon+ buffer))
                          (do* ((name (symbol-name object))
                                (len  (length name))
                                (i 0 (1+ i)))
                               ((<= len i)
                                buffer)
                            (push (aref name i) buffer))))    
    ((consp    object)  (progn
                          (push +left-parenthesis+ buffer)
                          (setf buffer (tiny-prin1-to-buffer (car object) buffer))
                          (do ((cell (cdr object) (cdr cell)))
                              ((atom cell)
                               (if cell
                                   (setf buffer (tiny-prin1-to-buffer cell (list* +space+ +dot+ +space+ buffer))))
                               (push +right-parenthesis+ buffer)
                               buffer)
                            (setf buffer (tiny-prin1-to-buffer (car cell) (cons +space+ buffer))))))
    (t (error "TINY-PRIN1-TO-STRING is not implemented for objects of type ~A"
              (type-of object)))))


(defun tiny-prin1-to-string (object)
  (coerce (reverse (tiny-prin1-to-buffer object '())) 'string))




;;;; THE END ;;;;
