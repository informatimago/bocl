;;;; -*- mode:lisp;coding:utf-8 -*-
;;;;**************************************************************************
;;;;FILE:               j-language-translator.lisp
;;;;LANGUAGE:           Common-Lisp
;;;;SYSTEM:             Common-Lisp
;;;;USER-INTERFACE:     NONE
;;;;DESCRIPTION
;;;;    
;;;;    This package translates j-language s-exp sources into J code.
;;;;    
;;;;AUTHORS
;;;;    <PJB> Pascal J. Bourguignon <pjb@informatimago.com>
;;;;MODIFICATIONS
;;;;    2008-06-13 <PJB> Created.
;;;;BUGS
;;;;LEGAL
;;;;    GPL
;;;;    
;;;;    Copyright Pascal J. Bourguignon 2008 - 2008
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


(defun translate-j-file (input-file &key output-file
                         (verbose *compile-verbose*)
                         (print *compile-print*)
                         (external-format :default))
  (with-open-file (inp input-file :external-format external-format)
    (with-open-file (out (or output-file
                             (make-pathname :type "JMF" :case :common
                                            :defaults input-file))
                         :direction :output
                         :if-exists :supersede
                         :if-does-not-exist :create
                         :external-format :default)
      (when verbose
        (format *trace-output* "~&;; Translating ~A...~%" input-file))
      (loop
         :for form = (read inp nil form)
         :until (eq form inp)
         :for code = (translate-j-form form)
         :when code :do (princ code out)))))


(defun translate-j-form (form)
  (cond
    ((atom form) (warn "Stray atom: ~S" form) nil)
    ((and (symbolp (first form))
          (eq (symbol-package (first form))
              (find-package "COM.INFORMATIMAGO.J-LANGUAGE")))
     
     )
    )
  )
