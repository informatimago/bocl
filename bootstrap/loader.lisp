;;;; -*- mode:lisp;coding:utf-8 -*-
;;;;**************************************************************************
;;;;FILE:               loader.lisp
;;;;LANGUAGE:           Common-Lisp
;;;;SYSTEM:             Common-Lisp
;;;;USER-INTERFACE:     NONE
;;;;DESCRIPTION
;;;;    
;;;;    Loader for the bootstrap lisp.
;;;;    
;;;;AUTHORS
;;;;    <PJB> Pascal Bourguignon <pjb@informatimago.com>
;;;;MODIFICATIONS
;;;;    2008-05-26 <PJB> Created.
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

(in-package "COMMON-LISP-USER")

(defparameter *backend* :j-macro
  "
Available backends are:  :j-macro
Planed backends are:     :perl :ruby :sh :javascript :ecmascript
")


(load "bootstrap-cl-package.lisp")
(load "backend/storage-package.lisp")
(load "backend/native-package.lisp")
(load "implementation-package.lisp")


(case *backend*
  ((:j-macro)    (progn
                   ;; "languages/j-language.lisp" is used to simulate the Objecteering
                   ;; J macro language, as well as to define a s-exp format for J language
                   ;; forms for the J translator (not implemented yet).
                   (load "languages/j-language.lisp")
                   (load "backend/j-macro/storage.lisp")
                   (load "backend/j-macro/native.lisp")
                   #+#:not-implemented-yet (load "backend/j-macro/translator.lisp")))
  ((:perl)       (progn
                   (load "backend/perl/storage.lisp")
                   #+#:not-implemented-yet (load "backend/perl/translator.lisp")))
  ((:ruby)       (progn
                   (load "backend/ruby/storage.lisp")
                   #+#:not-implemented-yet (load "backend/ruby/translator.lisp")))
  ((:sh)         (progn
                   (load "backend/sh/storage.lisp")
                   #+#:not-implemented-yet (load "backend/sh/translator.lisp")))
  ((:emacscript
    :javascript) (progn
    #+#:not-fetched-yet (load "languages/parenscript.lisp")
    (load "backend/emacsscript/storage.lisp")))
  (otherwise     (error "No backend ~S" *backend*)))



(in-package "COM.INFORMATIMAGO.BOOTSTRAP.IMPLEMENTATION")

(load "simple-heap.lisp")
;; (load "simple-sexp.lisp")



(defun dump-storage (&key raw)
  (loop
     :for i :from 0 :below (COM.INFORMATIMAGO.J-LANGUAGE::size
                            COM.INFORMATIMAGO.BOOTSTRAP.BACKEND.J-MACRO.STORAGE::*storage*)
     :for box = (fetch-box i)
     :initially (terpri)
     :do (format
             t "~8D : [ ~:[ ~;*~] ~14A | ~8D ] ~:[~;--> ~:*~A~]~:[~;--> ~:*~S~]~%"
             i
             (markedp i)
             (second (assoc (box-tag box) *type-map*))
             (box-value box)
             (if raw
               nil
               (cond
                 ((and (= (box-tag box) *tag-symbol*)
                       (= (box-tag (bootstrap-cl:symbol-name box)) *tag-string*))
                  (unbox-string (bootstrap-cl:symbol-name box)))
                 (t                                  nil)))
             (if raw
               nil
               (cond
                 ((= (box-tag box) *tag-character*)  (unbox-character box))
                 ((= (box-tag box) *tag-string*)     (unbox-string box))
                 (t                                   nil))))))


(bootstrap-cl:cons
 COM.INFORMATIMAGO.BOOTSTRAP.IMPLEMENTATION::@FIXNUM
 COM.INFORMATIMAGO.BOOTSTRAP.IMPLEMENTATION::@CHARACTER)
;; (initialize)
;; (dump-storage :raw t)
(dump-storage)

(in-package "COM.INFORMATIMAGO.BOOTSTRAP.IMPLEMENTATION")

(progn
  (print (unbox-string (bootstrap-cl:symbol-name
                        (bootstrap-cl:type-of
                         (bootstrap-cl:cons
                          COM.INFORMATIMAGO.BOOTSTRAP.IMPLEMENTATION::@FIXNUM
                          COM.INFORMATIMAGO.BOOTSTRAP.IMPLEMENTATION::@CHARACTER)))))

  (print (unbox-string (bootstrap-cl:symbol-name
                        (bootstrap-cl:type-of
                         (bootstrap-cl:car
                          (bootstrap-cl:cons
                           COM.INFORMATIMAGO.BOOTSTRAP.IMPLEMENTATION::@FIXNUM
                           COM.INFORMATIMAGO.BOOTSTRAP.IMPLEMENTATION::@CHARACTER))))))


  (terpri) (princ (%prin1-to-string
                   (bootstrap-cl:cons
                    (bootstrap-cl:symbol-name COM.INFORMATIMAGO.BOOTSTRAP.IMPLEMENTATION::@FIXNUM)
                    COM.INFORMATIMAGO.BOOTSTRAP.IMPLEMENTATION::@CHARACTER)))

  (terpri) (princ (%prin1-to-string
                   (bootstrap-cl:symbol-name COM.INFORMATIMAGO.BOOTSTRAP.IMPLEMENTATION::@FIXNUM)))

  (terpri) (princ (%prin1-to-string
                   (bootstrap-cl:list
                    (enbox-string "Hello \"World\"")
                    (enbox-character "X")
                    (enbox-fixnum 42)
                    (bootstrap-cl:cons @fixnum @character))))

  (terpri) (princ (%prin1-to-string
                   (bootstrap-cl:nreverse
                    (bootstrap-cl:list
                     (enbox-string "Hello \"World\"")
                     (enbox-character "X")
                     (enbox-fixnum 42)
                     (bootstrap-cl:cons @fixnum @character)))))


  (let ((h (bootstrap-cl::make-hash-table))) 
    (bootstrap-cl::sethash (enbox-fixnum 42) h (enbox-string "fourty-two"))
    (terpri) (princ (%prin1-to-string (bootstrap-cl::gethash (enbox-fixnum 42) h)))
    (terpri) (princ (%prin1-to-string (bootstrap-cl::gethash (enbox-fixnum 0) h)))
    (bootstrap-cl::sethash (enbox-fixnum 42) h (enbox-string "quarante deux"))
    (terpri) (princ (%prin1-to-string (bootstrap-cl::gethash (enbox-fixnum 42) h)))
    (values))
  

  (terpri) (princ (%prin1-to-string (bootstrap-cl:list-all-packages)))
  (terpri) (princ (%prin1-to-string (bootstrap-cl:find-package (enbox-string "COMMON-LISP"))))
  (terpri) (princ (%prin1-to-string (bootstrap-cl:find-package (enbox-string "COMMON-LISP-USER"))))

  (let ((sym (%read-from-string "ABC")))
    (terpri) (princ (%prin1-to-string sym)) 
    (bootstrap-cl::set-symbol-value sym (enbox-fixnum 42))
    (terpri) (princ (%prin1-to-string (bootstrap-cl::symbol-value sym)))
    (terpri) (princ (%prin1-to-string (bootstrap-cl:intern (enbox-string "ABC"))))
    (terpri) (princ (%prin1-to-string (bootstrap-cl::symbol-value sym))))

  (princ (%prin1-to-string (%read-from-string "(1234 ABCD (sub list) \"abc \\\"def\\\"\" (1 2 3))")))
  (princ (%prin1-to-string (%read-from-string "(defun fact (x) (if (< x 1) 1 (* x (fact (1- x)))))")))
  
  (values))

(defun repl ()
  (loop
     :named repl
     :with quit = (%read-from-string "(quit)")
     :do
     (terpri) (princ "> ") (finish-output)
     (handler-case (let* ((line (read-line))
                          (expr (%read-from-string line)))
                     ;; (print (ubool (bootstrap-cl:equal quit expr)))
                     (when (ubool (bootstrap-cl:equal quit expr))
                       (return-from repl))
                     (terpri)  (princ (%prin1-to-string expr)))
       (error (err) (terpri) (princ err) (terpri) nil))))
 

(bootstrap-cl:find-symbol(enbox-string "ABC")bootstrap-cl:*package*)

(defun dump-package (&optional (package bootstrap-cl:*package*))
  (map nil (lambda (a)
            (terpri)
            (princ (symbol-name a))
            (princ " ")
            (princ (%prin1-to-string (funcall a package))))
          '(bootstrap-cl::package-name
            bootstrap-cl::package-nicknames
            bootstrap-cl::package-exported-symbols
            bootstrap-cl::package-interned-symbols
            bootstrap-cl::package-use-list))
  (values))

;;;; THE END ;;;;
