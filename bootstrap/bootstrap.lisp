;;;; -*- mode:lisp;coding:utf-8 -*-
;;;;**************************************************************************
;;;;FILE:               bootstrap.lisp
;;;;LANGUAGE:           Common-Lisp
;;;;SYSTEM:             Common-Lisp
;;;;USER-INTERFACE:     NONE
;;;;DESCRIPTION
;;;;    
;;;;    Implement a mini lisp on some inferior languages.
;;;;    We don't assume a heap!
;;;;    We don't assume recursion!
;;;;    
;;;;AUTHORS
;;;;    <PJB> Pascal Bourguignon <pjb@informatimago.com>
;;;;MODIFICATIONS
;;;;    2008-05-07 <PJB> Created.
;;;;BUGS
;;;;LEGAL
;;;;    AGPL3
;;;;    
;;;;    Copyright Pascal Bourguignon 2008 - 2008
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


(in-package "BOOTSTRAP")


(use-package :com.informatimago.common-lisp.interactive)



(defun enbox-string    (native-string)
  (assert (stringp native-string))
  (make-box *tag-string* native-string))

(defun enbox-fixnum    (native-fixnum)
  (assert (integerp native-fixnum))
  (make-box *tag-fixnum* (prin1-to-string native-fixnum)))

(defun enbox-character (native-character)
  (assert (and (stringp native-character)
               (= 1 (length native-character))))
  (make-box *tag-character* native-character))


(defun unbox-string (object)
  (assert (eql *tag-string* (box-tag object)))
  (box-value object))

(defun unbox-fixnum (object)
  (assert (eql *tag-fixnum* (box-tag object)))
  (parse-integer (box-value object)))

(defun unbox-character (object)
  (assert (eql *tag-character* (box-tag object)))
  (box-value object))


(defgeneric size-of (type))

(defmacro define-structure (name type-tag &rest fields)
  `(progn
     (defmethod size-of ((self (eql ',name))) ,(length fields))
     (defun ,(intern (format nil "-MAKE~A" name) (symbol-package name)) ,fields
       (let* ((object (make-box ,type-tag (allocate ,(length fields))))
              (base (1- (box-value object))))
         ,@(loop
              :for field :in fields
              :collect `(store-box (incf base) ,field))
         object))
     ,@(loop
          :for offset :from 0
          :for field :in fields
          :collect  `(defun ,(intern (format nil "~A-~A" name field)
                                    (symbol-package name))
                        (object)
                      (fetch-box (+ (box-value object) ,offset)))
          :collect `(defun (setf ,(intern (format nil "~A-~A" name field)
                                    (symbol-package name)))
                        (new-value object)
                      (store-box (+ (box-value object) ,offset) new-value)))
     ',name))

(define-structure -symbol  *tag-symbol*  name value function plist package) 
(define-structure -cons    *tag-cons*    car cdr)
(define-structure -package *tag-package* name nicknames exported-symbols interned-symbols)

(defvar @nil       (make-box *tag-symbol* (* (size-of '-symbol) 0)))
(defvar @t         (make-box *tag-symbol* (* (size-of '-symbol) 1)))
(defvar @fixnum    (make-box *tag-symbol* (* (size-of '-symbol) 2)))
(defvar @character (make-box *tag-symbol* (* (size-of '-symbol) 3)))
(defvar @string    (make-box *tag-symbol* (* (size-of '-symbol) 4)))
(defvar @symbol    (make-box *tag-symbol* (* (size-of '-symbol) 5)))
(defvar @cons      (make-box *tag-symbol* (* (size-of '-symbol) 6)))
(defvar @package   (make-box *tag-symbol* (* (size-of '-symbol) 7)))

(defun initialize-symbol (object name value func plist pack)
  (store-box (+ (box-value object) +symbol-name-offset+)     name)
  (store-box (+ (box-value object) +symbol-value-offset+)    value)
  (store-box (+ (box-value object) +symbol-function-offset+) func)
  (store-box (+ (box-value object) +symbol-plist-offset+)    plist)
  (store-box (+ (box-value object) +symbol-package-offset+)  pack)
  object)

(defun initialize ()
  (setf *free-storage* 0)
  (dotimes (i 8) (allocate (size-of '-symbol)))
  (initialize-symbol @nil       (enbox-string "NIL")       @nil @nil @nil @nil)
  (initialize-symbol @t         (enbox-string "T")         @t   @nil @nil @nil)
  (initialize-symbol @fixnum    (enbox-string "FIXNUM")    @nil @nil @nil @nil)
  (initialize-symbol @character (enbox-string "CHARACTER") @nil @nil @nil @nil)
  (initialize-symbol @string    (enbox-string "STRING")    @nil @nil @nil @nil)
  (initialize-symbol @symbol    (enbox-string "SYMBOL")    @nil @nil @nil @nil)
  (initialize-symbol @cons      (enbox-string "CONS")      @nil @nil @nil @nil)
  (initialize-symbol @package   (enbox-string "PACAKGE")   @nil @nil @nil @nil)
  (values))



(defun -cons (car cdr)
  (-make-cons car cdr))

(defun -car (object)
  (cond
    ((eql object @nil) @nil)
    ((eql *tag-cons* (box-tag object))
     (-cons-car object))
    (t
     (error "Called CAR on a non CONS object of type ~A"
            (box-tag object)))))

(defun -cdr (object)
  (cond
    ((eql object @nil) @nil)
    ((eql *tag-cons* (box-tag object))
     (-cons-cdr object))
    (t
     (error "Called CDR on a non CONS object of type ~A"
            (box-tag object)))))

(defun -null (object)
  (if (and (eql (box-tag   object) *tag-symbol*)
           (eql (box-value object) @nil))
      @t
      @nil))





(initialize)
