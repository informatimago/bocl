;;;; -*- mode:lisp;coding:utf-8 -*-
;;;;**************************************************************************
;;;;FILE:               tiny-reader.lisp
;;;;LANGUAGE:           Common-Lisp
;;;;SYSTEM:             Common-Lisp
;;;;USER-INTERFACE:     NONE
;;;;DESCRIPTION
;;;;    
;;;;    Tiny Lisp Reader.
;;;;
;;;;    This reader implements a fixed subset of the Common Lisp reader.
;;;;    It can read proper lists, semi-colon comments, strings, 
;;;;    integers, keywords and symbols. 
;;;;    
;;;;AUTHORS
;;;;    <PJB> Pascal J. Bourguignon <pjb@informatimago.com>
;;;;MODIFICATIONS
;;;;    2011-04-17 <PJB> Reduced loop loops with do loops, added
;;;;                     reading strings and comments.
;;;;    2009-08-02 <PJB> Created.
;;;;BUGS
;;;;LEGAL
;;;;    GPL
;;;;    
;;;;    Copyright Pascal J. Bourguignon 2009 - 2011
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


(defconstant +newline+           (code-char 10))
(defconstant +space+             (code-char 32))
(defconstant +double-quote+      (code-char 34))
(defconstant +left-parenthesis+  (code-char 40))
(defconstant +right-parenthesis+ (code-char 41))
(defconstant +minus+             (code-char 45))
(defconstant +dot+               (code-char 46))
(defconstant +colon+             (code-char 58))
(defconstant +semi-colon+        (code-char 59))
(defconstant +back-slash+        (code-char 92))


(defun tiny-read (stream &optional (eof-error-p t) eof-value)
  "
   Tiny Lisp Reader

   This reader implements a fixed subset of the Common Lisp reader.
   It can read proper lists, semi-colon comments, strings, 
   integers, keywords and symbols. 
"  
  (let ((ch (peek-char t stream nil nil)))
    (cond
      ((null ch)
       (if eof-error-p
           eof-value
           (error 'end-of-file :stream stream)))
      ((char= ch +left-parenthesis+)
       (read-char stream)
       (do ((result '() (cons (tiny-read stream) result)))
           ((char= +right-parenthesis+ (peek-char t stream))
            (read-char stream)
            (reverse result))))
      ((char= ch +semi-colon+)
       (read-line stream)
       (tiny-read stream eof-error-p eof-value))
      ((char= ch +double-quote+)
       (read-char stream)
       (do ((result '() (cons (if (char= ch +back-slash+)
                                  (read-char stream)
                                  ch)
                              result))
            (ch (read-char stream)
                (read-char stream)))
           ((char= ch +double-quote+)
            (coerce (reverse result) 'string))))
      (t
       (do ((buffer '() (cons (read-char stream) buffer))
            (ch (peek-char nil stream nil +space+)
                (peek-char nil stream nil +space+)))
           ((or (char= ch +space+)
                (char= ch +right-parenthesis+)
                (char= ch +newline+))
            (let ((buffer (coerce (reverse buffer) 'string)))
             (cond
               ((digit-char-p (aref buffer 0))
                (parse-integer buffer))
               ((char= +colon+ (aref buffer 0))
                (intern (string-upcase (subseq buffer 1)) :keyword))
               (t
                (intern (string-upcase buffer)))))))))))



(defun tiny-read-from-string (string &optional (eof-error-p t) eof-value
                              &key (start 0) end)
  (with-input-from-string (stream string :start start :end end)
    (tiny-read stream eof-error-p eof-value)))


;;;; THE END ;;;;
