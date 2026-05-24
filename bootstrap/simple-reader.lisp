;;;; -*- mode:lisp;coding:utf-8 -*-
;;;;**************************************************************************
;;;;FILE:               simple-reader.lisp
;;;;LANGUAGE:           Common-Lisp
;;;;SYSTEM:             Common-Lisp
;;;;USER-INTERFACE:     NONE
;;;;DESCRIPTION
;;;;    
;;;;    Simple Lisp Reader.
;;;;    This reader implements a subset of the Common Lisp reader,
;;;;    but it should be extensible enough to be able to read most
;;;;    of Common Lisp syntax.
;;;;    Not supported: the preserving whitespace flag, the recursive flag
;;;;    (therefore no references), character traits (escapes).
;;;;    Otherwise, reader macros and dispatching  macros can be written
;;;;    to read most of CL syntax.
;;;;    Only integers, keywords and symbol tokens are parsed (a more
;;;;    sophisticated parse-token function can be configured).
;;;;
;;;;    http://paste.lisp.org/display/165367
;;;;
;;;;AUTHORS
;;;;    <PJB> Pascal J. Bourguignon <pjb@informatimago.com>
;;;;MODIFICATIONS
;;;;    2015-12-05 <PJB> Changed license to AGPL3; Added package.
;;;;    2009-08-02 <PJB> Created.
;;;;BUGS
;;;;LEGAL
;;;;    AGPL3
;;;;    
;;;;    Copyright Pascal J. Bourguignon 2009 - 2015
;;;;    
;;;;    This program is free software: you can redistribute it and/or modify
;;;;    it under the terms of the GNU Affero General Public License as published by
;;;;    the Free Software Foundation, either version 3 of the License, or
;;;;    (at your option) any later version.
;;;;    
;;;;    This program is distributed in the hope that it will be useful,
;;;;    but WITHOUT ANY WARRANTY; without even the implied warranty of
;;;;    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;;;    GNU Affero General Public License for more details.
;;;;    
;;;;    You should have received a copy of the GNU Affero General Public License
;;;;    along with this program.  If not, see <http://www.gnu.org/licenses/>.
;;;;**************************************************************************

(defpackage "COM.INFORMATIMAGO.SIMPLE-LISP.READER"
  (:use "COMMON-LISP")

  (:shadow
   "COPY-READTABLE"
   "*READTABLE*"
   "READTABLE" "READTABLEP"
   "GET-MACRO-CHARACTER" "SET-MACRO-CHARACTER"
   "GET-DISPATCH-MACRO-CHARACTER" "SET-DISPATCH-MACRO-CHARACTER"
   "READ")

  (:export
   ;; Standard CL symbols:
   "COPY-READTABLE"
   "*READTABLE*"
   "READTABLE" "READTABLEP"
   "GET-MACRO-CHARACTER" "SET-MACRO-CHARACTER"
   "GET-DISPATCH-MACRO-CHARACTER" "SET-DISPATCH-MACRO-CHARACTER"
   "READ")
  
  (:export
   ;; extensions:
   "READTABLE-PARSE-TOKEN")
  
  (:documentation "

Simple Lisp Reader
==================

This reader implements a subset of the Common Lisp reader,
but it should be extensible enough to be able to read most
of Common Lisp syntax.

Not supported: the preserving whitespace flag, the recursive flag
\(therefore no references), character traits (escapes).
Otherwise, reader macros and dispatching  macros can be written
to read most of CL syntax.

Only integers, keywords and symbol tokens are parsed (a more
sophisticated parse-token function can be configured).


Legal
=====

AGPL3

Copyright Pascal J. Bourguignon 2009 - 2015

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

"))

(in-package "COM.INFORMATIMAGO.SIMPLE-LISP.READER")

(defstruct (character-description
            (:copier nil))
  "Describes the syntax of a character."
  (non-terminating-p t)
  reader-macro
  dispatching-macro-characters)

(defun copy-hash-table (table &optional (copy-value (function identity)))
  (let ((to (make-hash-table
             :size (hash-table-size table)
             :test (hash-table-test table)
             :rehash-size (hash-table-rehash-size table)
             :rehash-threshold (hash-table-rehash-threshold table))))
    (maphash (lambda (k v) (setf (gethash k to) (funcall copy-value v)))
             table)
    to))

(defun copy-character-description (cd)
  (make-character-description
   :non-terminating-p (character-description-non-terminating-p cd)
   :reader-macro (character-description-reader-macro cd)
   :dispatching-macro-characters (copy-hash-table (character-description-dispatching-macro-characters cd))))

(defvar *whitespaces* #(#\space #\tab #\newline #\linefeed #\return #\page #\vt))

(defstruct (readtable
            (:constructor %make-readtable)
            (:predicate readtablep)
            (:copier nil))
  "Maps characters to syntaxes and reader macros."
  default-character-description
  macro-characters ; a hash-table mapping characters to character-descriptions.
  parse-token      ; a function to parse the tokens.
  whitespaces)     ; a sequence of whitespace characters.

(defun get-macro-character (character &optional (readtable *readtable*))
  "Returns the reader macro function and whether it's a non-terminating reader macro character."
  (let ((description (or (gethash character (readtable-macro-characters readtable))
                         (readtable-default-character-description readtable))))
    (values (character-description-reader-macro description)
            (character-description-non-terminating-p description))))

(defun set-macro-character (character function &optional non-terminating-p
                                                 (readtable *readtable*))
  "Sets the reader macro function and whether it's a non-terminating reader macro character."
  (setf (gethash character (readtable-macro-characters readtable))
        (make-character-description :non-terminating-p non-terminating-p
                                    :reader-macro function))
  't)

(defun get-dispatch-macro-character (character subchar 
                                     &optional (readtable *readtable*))
  "Returns the reader dispatching macro function for the pair of characters."
  (setf subchar (char-upcase subchar))
  (let ((description (gethash character (readtable-macro-characters readtable))))
    (unless (and description
                 (character-description-dispatching-macro-characters description))
      (error "#\\~C is not a dispatching macro character" character))
    (gethash subchar (character-description-dispatching-macro-characters description))))

(defun set-dispatch-macro-character (character subchar function
                                     &optional (readtable *readtable*))
  "Sets the reader dispatching macro function for the pair of characters."
  (setf subchar (char-upcase subchar))
  (let ((description (gethash character (readtable-macro-characters readtable))))
    (when (or (null description)
              (null (character-description-dispatching-macro-characters description)))
      (setf (gethash character (readtable-macro-characters readtable))
            (make-character-description :non-terminating-p t
                                        :reader-macro (function reader-dispatching-macro)
                                        :dispatching-macro-characters (make-hash-table)))))
  (let ((description (gethash character (readtable-macro-characters readtable))))
    (setf (gethash subchar (character-description-dispatching-macro-characters description))
          function))
  't)

(defun reader-dispatching-macro (character stream)
  "Calls the reader dispatching macro associated with the character."
  (let* ((subchar  (read-char stream))
         (macro    (get-dispatch-macro-character character subchar)))
    (assert macro () "~C~C is not a dispatching macro" character subchar)
    (funcall macro character subchar stream)))

(defun read-vector-dmacro (char subchar stream)
  "Dispatching reader macro to read vectors like: #(1 2 3)."
  (declare (ignore char))
  (coerce (read-list-macro subchar stream) 'vector))

(defvar *character-names* (list (cons "SPACE"    (code-char 32))
                                (cons "NEWLINE"  (code-char 10))
                                (cons "RETURN"   (code-char 13))
                                (cons "PAGE"     (code-char 12))
                                (cons "VT"       (code-char 11))
                                (cons "LINEFEED" (code-char 10))
                                (cons "TAB"      (code-char 9)))
  "An a-list mapping character names to characters.")

(defun read-character-dmacro (char subchar stream)
  "Dispatching reader macro to read characters like: #\\a or #\\space."
  (declare (ignore char subchar))
  (let ((object (read-char stream)))
    (if (alpha-char-p (peek-char nil stream nil #\space))
        (loop ; read a character name
          :with buffer = (make-array 8 :element-type 'character :adjustable t :fill-pointer 1
                                       :initial-element object)
          :do (vector-push-extend (read-char stream) buffer)
          :while (alpha-char-p (peek-char nil stream nil #\space))
          :finally (return (or (cdr (assoc (string-upcase buffer) *character-names*
                                           :test (function string=)))
                               (aref buffer 0))))
        object)))

(defun parse-token (buffer)
  "Parses a token in the buffer.
Note: We only deal with integers, keywords and symbols."
  (or (ignore-errors (parse-integer buffer :junk-allowed nil))
      (if (char= #\: (aref buffer 0))
          (intern (string-upcase (subseq buffer (position #\: buffer :test (function char/=))))
                  "KEYWORD")
          ;; We don't deal with other packages in this simple parse-token.
          (intern (string-upcase buffer)))))

(defun read-quote-macro (character stream)
  "Reader macro to read quoted expressions like: 'x."
  (declare (ignore character))
  (list 'quote (read stream)))

(defun read-list-macro (character stream)
  "Reader macro to read lists like: (1 2 3).
Note: We don't read dotted lists."
  (declare (ignore character))
  (loop
    :until (char= #\) (peek-char t stream))
    :collect (read stream)
    :finally (read-char stream)))

(defun read-string-macro (character stream)
  "Reader macro to read strings like: \"abc\"."
  (handler-case 
      (loop
        :with buffer = (make-array 8 :element-type 'character :adjustable t :fill-pointer 0)
        :for ch = (read-char stream)
        :until (char= character ch)
        :do (vector-push-extend (if (char= #\\ ch)
                                    (read-char stream)
                                    ch)
                                buffer)
        :finally (return (copy-seq buffer)))))

(defun read-comment-macro (character stream)
  "Reader macro to read comments like: ; this is a comment."
  (declare (ignore character))
  (read-line stream)
  (values))

(defun reset-readtable (readtable)
  "Sets the readtable to the default state."
  (setf (readtable-default-character-description readtable) (make-character-description :non-terminating-p t)
        (readtable-macro-characters              readtable) (make-hash-table)
        (readtable-parse-token                   readtable) (function parse-token)
        (readtable-whitespaces                   readtable) (copy-seq *whitespaces*))
  (set-macro-character #\( (function read-list-macro)     nil readtable)
  (set-macro-character #\) nil                            nil readtable)
  (set-macro-character #\' (function read-quote-macro)    nil readtable)
  (set-macro-character #\" (function read-string-macro)   nil readtable)
  (set-macro-character #\; (function read-comment-macro)  nil readtable)
  (set-dispatch-macro-character #\# #\\ (function read-character-dmacro) readtable)
  (set-dispatch-macro-character #\# #\( (function read-vector-dmacro)    readtable)
  readtable)


(defun copy-readtable (&optional from-readtable to-readtable)
  "Copies the readtable FROM-READTABLE into the readtable TO-READTABLE,
and returns TO-READTABLE.
If FROM-READTABLE is NIL, then the default readtable is copied;
if TO-READTABLE is NIL, then a new readtable is returned."
  (flet ((copy (from to)
           (setf (readtable-default-character-description to) (copy-character-description (readtable-default-character-description from))
                 (readtable-macro-characters              to) (copy-hash-table (readtable-macro-characters from)
                                                                               (function copy-character-description))
                 (readtable-parse-token                   to) (readtable-parse-token from)
                 (readtable-whitespaces                   to) (copy-seq (readtable-whitespaces from)))))
    (if to-readtable
        (if from-readtable
            (copy from-readtable to-readtable)
            (reset-readtable to-readtable))
        (if from-readtable
            (copy from-readtable (%make-readtable))
            (reset-readtable (%make-readtable))))))

(defparameter *readtable* (copy-readtable)
  "A readtable definining a subset of the standard CL syntax.")

(defun read (&optional (stream *standard-input*) (eof-error-p t) eof-value)
  "Reads and returns a lisp object."
  (peek-char t stream nil)
  (let ((char (read-char stream nil nil)))
    (cond
      (char
       (multiple-value-bind (macro non-terminating-p) (get-macro-character char)
         (if macro
             (let ((object (multiple-value-list (funcall macro char stream))))
               (if object (first object) (read stream eof-error-p eof-value)))         
             (loop
               :with buffer = (make-array 8 :element-type 'character :adjustable t :fill-pointer 0)
               :for ch = (peek-char nil stream nil #\space)
                 :initially (vector-push-extend char buffer)
               :until (or (find ch (readtable-whitespaces *readtable*))
                          (not (nth-value 1 (get-macro-character ch))))
               :do (vector-push-extend (read-char stream eof-error-p eof-value) buffer)
               :finally (return (funcall (readtable-parse-token *readtable*) buffer))))))
      (eof-error-p (error 'end-of-file :stream stream))
      (t eof-value))))


(defun test/reader ()
  (let ((test-file (merge-pathnames "src/lisp/bootstrap/simple-reader.lisp"
                                    (user-homedir-pathname))))
    (assert (equalp
             (with-open-file (input test-file)
               (loop
                 :for sexp = (read input nil input)
                 :until (eq sexp input)
                 :collect sexp))
             (with-open-file (input test-file)
               (loop
                 :for sexp = (read input nil input)
                 :until (eq sexp input)
                 :collect sexp))))
    :success))

(test/reader)

;;;; THE END ;;;;
