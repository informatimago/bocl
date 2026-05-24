;;;; -*- mode:lisp;coding:utf-8 -*-
;;;;**************************************************************************
;;;;FILE:               j-language.lisp
;;;;LANGUAGE:           Common-Lisp
;;;;SYSTEM:             Common-Lisp
;;;;USER-INTERFACE:     NONE
;;;;DESCRIPTION
;;;;    
;;;;    This package provides the primitives found in Objecteering J language.
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


(defpackage "COM.INFORMATIMAGO.J-LANGUAGE"
  (:use "COMMON-LISP")
  (:shadow "CLASS" "CLASS-OF" "SPACE" "SUBSTITUTE"
           "STREAM" "OPEN" "WRITE" "READ" "CLOSE"
           "SORT" "REMOVE" "LENGTH" "BLOCK" "EVAL" "SET"
           "RETURN" "METHOD"
           "/")
  (:export

   "OBJECT" "CLASS-OF" "METACLASS-NAME" "GET-FEATURE" "==" "!="
   "VOID" "NOT-VOID" 

   "META-CLASS" "NAME" "PARENT-OF"

   "DEFINE-CLASS"

   "<" "<=" ">" ">="  "+" "-" "*" "/"  "%"
   "TO-BOOLEAN" "TO-INT"  "TO-FLOAT" "TO-STRING"

   "INT"

   "FLOAT" "SQRT" "POW" "EXP"

   "BOOLEAN" "AND" "OR" "NOT" "TRUE" "FALSE"

   "STRING" "SIZE" "SPACE" "SUBSTITUTE" "TO-UPPER" "TO-LOWER"
   "FIND-FIRST" "FIND-LAST" "SEGMENT" "FIND-TOKEN"
   "ERASE-BEFORE" "ERASE-AFTER" "ERASE" "STRING-ASSIGN"
   "STRNEQUAL" "STRCAT" "CONCAT" "STRNCPY" "STRNCAT"
   "STRIP" "PREPEND"  "INSERT-STR" "OVERWRITE"
   "ENUMERATE"

   "STREAM" "IN-STREAM" "OUT-STREAM" "IS-FILE" "IS-DIRECTORY"
   "OPEN" "READ" "WRITE" "CLOSE" "EXIST-FILE" "NL" "STD-OUT" "STD-ERR"
   
   
   "THIS" "ASEND" "SEND" "DIFFUSE" "SELECT" "WHILE"
   "BLOCK" "VAR" "SET?" "SET"

   "JSET" "MAKE-JSET" "JSET-ASSIGN" "GET-ITEM-SET" "SET-ITEM-SET"
   "SORT" "CONTAINS" "REMOVE" "CLEAR" "SIZE" "LENGTH" "ADD-ELEMENT"
   "ADD" "RETRACT"

   "IF" "COND" "FOR" 
   "EVAL"
   "DEFINE-VARIABLE"
   "DEFINE-FUNCTION"
   "DEFINE-METHOD"
   "RETURN")
  (:documentation "
This package provides the primitives found in Objecteering J language.

Copyright Pascal Bourguignon 2008 - 2008

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version
2 of the License, or (at your option) any later version.
"))


(defpackage  "COM.INFORMATIMAGO.J-LANGUAGE-USER"
  (:nicknames "J-LANGUAGE-USER" "JL-USER")
  (:use "COM.INFORMATIMAGO.J-LANGUAGE")
  (:documentation "
This package is a playfield for J programmers.

Copyright Pascal Bourguignon 2008 - 2008

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version
2 of the License, or (at your option) any later version.
"))


(in-package "COM.INFORMATIMAGO.J-LANGUAGE")


;; TODO: move split-sequence-between-elements to some utility package.
(DEFUN split-sequence-between-elements (INDICATOR sequence)
  "
RETURN: A list of subsequences of SEQUENCE,
        the SEQUENCE is splited between items a and b for
        which (funcall INDICATOR a b) returns true.
"
  (DECLARE (TYPE (FUNCTION (T T) T) INDICATOR))
  (if (listp sequence)
      (loop
         :with result = '()
         :with range = '()
         :for a :in sequence
         :for b :in (cdr sequence)
         :do (progn (push a range)
                    (when (funcall indicator a b)
                      (push (nreverse range) result)
                      (setf range '())))
         :finally (progn (push a range)
                         (push (nreverse range) result)
                         (cl:return (nreverse result))))
      (loop
         :with result = '()
         :with start = 0
         :for end :from 1 :below (cl:length sequence)
         :do (progn
               (when (funcall indicator
                              (aref sequence (1- end)) (aref sequence end))
                 (push (subseq sequence start end) result)
                 (setf start end)))
         :finally (progn (push (subseq sequence start) result)
                         (cl:return (nreverse result))))))

(defun lisp-name (name)
  (intern
   (with-output-to-string (out)
     (format out "~{~:@(~A~^-~)~}"
             (split-sequence-between-elements
              (lambda (a b) (declare (ignore a)) (upper-case-p b))
              name)))
   #.*package*))


(defun j-name (name &optional lowcase)
  (let* ((name  (string name))
         (items (split-sequence:split-sequence #\- name)))
    (format nil
            (if lowcase
                "~(~A~)~{~:(~A~)~}"
                "~:(~A~)~{~:(~A~)~}")
            (first items) (rest items))))


(defgeneric == (a b)
  (:method (a b)                   (equal a b)) ;; ?
  (:method ((a string) (b string)) (string= a b)))

(defgeneric != (a b)
  (:method (a b)  (not (== a b))))



(defclass object ()
  ((class-of :reader class-of :initform (find-meta-class 'object))))

(defclass meta-class (object)
  ((class-of   :initform (find-meta-class 'meta-class)
               :reader class-of)
   (parent-of  :initarg :parent-of
               :reader parent-of
               :initform (find-meta-class 'object))
   (name       :initarg :name
               :reader name)))

(defmethod print-object ((self object) stream)
  (print-unreadable-object (self stream :identity t :type nil)
    (format stream "J object ~A" (name (class-of self))))
  self)

(defmethod print-object ((self meta-class) stream)
  (print-unreadable-object (self stream :identity t :type nil)
    (format stream "J class ~A" (name self)))
  self)


(defparameter *meta-classes*  (make-hash-table))

(defun find-meta-class (class)
  (gethash class *meta-classes*))

(let ((object    (make-instance 'meta-class
                     :name "Object"
                     :parent-of nil))
      (meta-class (make-instance 'meta-class
                      :name "Meta-Class")))
  (setf (gethash 'object     *meta-classes*)  object
        (gethash 'meta-class *meta-classes*)  meta-class
        (slot-value object     'class-of)     meta-class
        (slot-value meta-class 'class-of)     meta-class
        (slot-value meta-class 'parent-of)    object))


(defmacro define-class (name parent slots)
  (let ((meta-class (gensym)))
    `(let ((,meta-class (make-instance 'meta-class
                            :name ,(j-name name)
                            :parent-of (find-meta-class ',parent))))
       (defclass ,name (,parent)
         ,(cons
           `(class-of :initform ,meta-class)
           (mapcar (lambda (slot)
                     `(,slot :initarg ,(intern (string slot) "KEYWORD")
                             :accessor ,slot))
                   slots)))
       (setf (gethash ',name *meta-classes*) ,meta-class)
       ',name)))


(defmethod metaclass-name ((self object)) (name (class-of self)))



(defmethod %get-feature ((self object) (feature string))
  (let ((name (lisp-name feature)))
    (if (slot-boundp self name)
        (values t (slot-value self name))
        (values nil nil))))



(defmacro get-feature (object feature value)
  (let ((present (gensym))
        (val     (gensym)))
    `(multiple-value-bind (,present ,val) (%get-feature ,object ,feature)
       (setf ,value (when ,present ,val)))))



(defmacro define-meta-class (j-name lisp-type)
  (let ((s-name (intern (string-upcase j-name) #.*package*)))
    `(let ((meta-class (make-instance 'meta-class
                           :name ,j-name
                           :parent-of (find-meta-class 'object))))
       (setf (gethash ',s-name *meta-classes*) meta-class)
       (defmethod class-of ((self ,lisp-type)) meta-class)
       ',s-name)))


(define-meta-class "int"       integer)
(define-meta-class "float"     float)
(define-meta-class "boolean"   (eql t))
(define-meta-class "boolean"   (eql nil))
(define-meta-class "String"    string)
(define-meta-class "enumerate" symbol)

(deftype int () 'integer)


(defun / (a b) (values (floor a b)))
(defun % (a b) (values (mod   a b)))

(defmethod to-float   ((self integer)) (coerce self 'double-float))
(defmethod to-string  ((self integer)) (prin1-to-string self))
(defmethod to-int     ((self integer)) self)
(defmethod to-boolean ((self integer)) (if (zerop self) 'nil 't))

(defmethod pow ((self float) (exponent float))
  (expt self exponent))

(defmethod to-float   ((self float)) self)
(defmethod to-string  ((self float)) (prin1-to-string self))
(defmethod to-int     ((self float)) (round self))
(defmethod to-boolean ((self float)) (if (zerop self) 'nil 't))

(defconstant true  't)
(defconstant false 'nil)

(defmethod to-float   ((self (eql t)))   1.0)
(defmethod to-float   ((self (eql nil))) 0.0)
(defmethod to-string  ((self (eql t)))    "true")
(defmethod to-string  ((self (eql nil)))  "false")
(defmethod to-int     ((self (eql t)))   1)
(defmethod to-int     ((self (eql nil))) 0)
(defmethod to-boolean ((self (eql t)))   't)
(defmethod to-boolean ((self (eql nil))) 'nil)

(defun %update (string value &key (start1 0))
  (assert (adjustable-array-p string))
  (replace (adjust-array string (+ start1 (cl:length value))
                         :fill-pointer (+ start1 (cl:length value))
                         :element-type 'character
                         :initial-element #\space)
           value :start1 start1))

(defmethod size  ((self string)) (cl:length self))
(defmethod space ((self string)) (%update self (format nil "~VA" space self)))
(defmethod substitute ((self string) (to-substitute string) (new-value string))
  (%update self (COM.INFORMATIMAGO.COMMON-LISP.STRING:STRING-REPLACE self to-substitute new-value)))
(defmethod to-upper ((self string)) (%update self (string-upcase self)))
(defmethod to-lower ((self string)) (%update self (string-downcase self)))
(defmethod find-first ((self string) (pattern string) (start-index integer))
  (or (search pattern string :start2 start-index) -1))
(defmethod find-last ((self string) (pattern string))
  (or (search pattern string :from-end t) -1))
(defmethod segment ((self string) (separator string))
  (jset-from-list
   'string
   (loop
      :for start = 0 :then next
      :for sepos = (and start (search separator self :start2 start))
      :for next  = (and sepos (+ sepos (cl:length separator)))
      :while (or sepos start)
      :collect (subseq self start sepos))))
(defmethod find-token ((self string) (separators-set string))
  (jset-from-list
   'string
   (COM.INFORMATIMAGO.COMMON-LISP.STRING:SPLIT-STRING self separators-set)))
(defmethod erase-before ((self string) (start-index integer))
  (%update self (subseq self start-index)))
(defmethod erase-after ((self string) (start-index integer))
  (%update self (subseq self 0 start-index)))
(defmethod erase ((self string) (start-index integer) (stop-index integer))
  (%update self (concatenate 'string (subseq self start-index) (subseq self 0 stop-index))))
(defmethod string-assign ((self string) (origin string)
                          (start-index integer) (stop-index integer))
  (let ((before (%copy self))
        (after  (%copy self)))
    (erase-after before start-index)
    (erase-before after stop-index)
    (%update self (concatenate 'string before origin after))))
(defmethod strnequal ((self string) (to-compare string) (stop-index integer))
  (string= self to-compare :end1 stop-index))
(defmethod strcat ((self string) &rest args)
  (%update self (with-output-to-string (out)
                  (princ self out)
                  (dolist (item args)
                    (princ item out)))))
(defmethod concat ((self string) &rest args)
  (%update self (with-output-to-string (out)
                  (princ self out)
                  (dolist (item args)
                    (princ item out)))))
(defmethod strncpy ((self string) (origin string) (length integer))
  (%update self (subseq origin 0 length)))
(defmethod strncat ((self string) (to-add string) (length integer))
  (%update self (concat self (subseq to-add 0 length))))
(defmethod strip ((self string) (border-set string))
  (%update self (string-trim border-set self)))
(defmethod prepend ((self string) (to-add string))
  (%update self (concat to-add self)))
(defmethod insert-str ((self string) (to-add string) (position integer))
  (%update self (concat (subseq self 0 position)
                        to-add
                        (subseq self position))))
(defmethod overwrite ((self string) (to-write string) (position integer))
  (%update self to-write :start1 position))

(defmethod to-boolean ((self string)) (if (string= "true") 't 'nil))
(defmethod to-int     ((self string)) (parse-integer self :junk-allowed t))
(defmethod to-float   ((self string)) (coerce (read-from-string self) 'double-float))
(defmethod to-string  ((self string)) self)



(define-class stream Object
  (lisp-stream))
(define-class in-stream  stream ())
(define-class out-stream stream ())

(defun is-file (file-name)
  (ignore-errors (probe-file file-name)))
(defun is-directory (directory-name)
  #+clisp (ext:probe-directory directory-name)
  #-clisp (error "IS-DIRECTORY  is not implemented yet on ~A"
                 (lisp-implementation-type)))

;; out-stream

(defmethod open ((self out-stream) (file-name string) &optional (append-mode nil))
  (setf (lisp-stream self) nil)
  (ignore-errors
    (setf (lisp-stream self)
          (cl:open file-name :direction :output
                   :if-does-not-exist :create
                   :if-exists (if append-open :append :supersede)))
    t))
(defmethod write ((self out-stream) &rest arguments)
  (dolist (item arguments)
    (princ (to-string item)  (lisp-stream self))))

(defmethod close ((self out-stream))
  (cl:close (lisp-stream self)))

(defmacro exist-file (file-name answer)
  `(setf ,answer (probe-file ,file-name)))


;; in-stream

(defmethod open ((self in-stream) (file-name string) &optional (append-mode t))
  (setf (lisp-stream self) nil)
  (ignore-errors
    (setf (lisp-stream self)
          (cl:open file-name :direction :input :if-does-not-exist :error))
    t))

(defmethod %read ((self in-stream))
  (COM.INFORMATIMAGO.COMMON-LISP.FILE:TEXT-FILE-CONTENTS (lisp-stream self)))

(defmacro read (stream buffer)
  `(setf ,buffer (%read ,stream)))

(defmethod close ((self in-stream))
  (cl:close (lisp-stream self)))

(defparameter NL (string #\newline))

(defparameter std-out (make-instance 'out-stream :lisp-stream *standard-output*))
(defparameter std-err (make-instance 'out-stream :lisp-stream *error-output*))

(defmethod void     (object) (null object))
(defmethod not-void (object) (not (void object)))




(define-class jset object
  (element-type elements))

(deftype jset (&optional (element-type nil etp))
  (let ((vfun (gensym)))
    (setf (symbol-function vfun)
          (coerce
           `(lambda (object)
              (and (eql 'jset (class-name (cl:class-of object)))
                   (or ,(not etp)
                       (subtypep (slot-value object 'element-type)
                                 ',element-type))))
           'function))
    `(satisfies ,vfun)))

(defun make-jset (element-type)
  (make-instance 'jset :element-type element-type))

(defmethod initialize-instance :after ((self jset) &rest args)
  (with-slots ((element-type element-type)
               (elements     elements))
      self
    (setf elements (make-array 0
                               :element-type element-type
                               :initial-element (initial-value element-type)
                               :adjustable t
                               :fill-pointer 0)))
  self)

(defmethod jset-assign ((self jset) (other jset))
  (assert (eql (slot-value self  'element-type)
               (slot-value other 'element-type)))
  (setf (slot-value self 'elements)
        (adjust-array (slot-value self 'elements)
                      (cl:length (slot-value other 'elements))
                      :element-type (slot-value self  'element-type)
                      :fill-pointer (cl:length (slot-value other 'elements))))
  (replace (slot-value self 'elements)  (slot-value other 'elements))
  self)

(defmethod void     ((self jset)) (zerop (size self)))

(defmethod %jset-get ((self jset) index)
  (aref (slot-value self 'elements) index))
(defmacro  get-item-set (jset index place)
  `(setf ,place (%jset-get ,jset ,index)))
(defmethod set-item-set ((self jset) index value)
  (setf (aref (slot-value self 'elements) index) value))

(defmethod sort (object ascending-sort (meta-attribute-name string))
  object)


(defgeneric lessp (a b)
  (:method ((a (eql nil)) (b (eql nil))) nil)
  (:method ((a (eql t))   (b (eql nil))) nil)
  (:method ((a (eql nil)) (b (eql t)))   t)
  (:method ((a (eql t))   (b (eql t)))   nil)
  (:method ((a real)      (b real))      (< a b))
  (:method ((a string)    (b string))    (string< a b))
  (:method (a b) nil))

(defgeneric greaterp (a b)
  (:method (a b) (lessp b a)))


(defmethod sort ((self jset) ascending-sort (meta-attribute-name string))
  (with-slots ((element-type element-type)
               (elements     elements))
      self
    (setf elements
          (cl:sort elements
                   (if ascending-sort
                       (function lessp)
                       (function greaterp))
                   :key (let ((slot (intern meta-attribute-name #.*package*)))
                          (lambda (x) (slot-value x slot))))))
  self)

(defgeneric contains (object element)
  (:method (object element) (eql object element))
  (:method ((self jset) element)
    (find element (slot-value self 'elements) :test (function equal))))

(defgeneric remove (object element)
  (:method (o e) (declare (ignore o e)) (values))
  (:method ((self jset) element)
    (assert (typep element (slot-value self 'element-type)))
    (setf (slot-value self 'elements)
          (cl:delete (slot-value self 'elements) element
                     :test (function equal)))
    self))

(defgeneric clear (x)
  (:method (x) x)
  (:method ((self jset))
    (setf (slot-value self 'elements)
          (adjust-array (slot-value self 'elements)
                        0
                        :element-type (slot-value self  'element-type)
                        :fill-pointer 0))
    self))



(defmethod size   ((self jset)) (cl:length (slot-value self 'elements)))
(defmethod length ((self jset)) (cl:length (slot-value self 'elements)))

(defmethod add-element ((self jset) element)
  (assert (typep element (slot-value self 'element-type)))
  (vector-push-extend element (slot-value self 'elements)))

(defmethod add ((self jset) (other jset))
  (assert (eql (slot-value self  'element-type)
               (slot-value other 'element-type)))
  (let ((original (cl:length (slot-value self 'elements))))
    (setf (slot-value self 'elements)
          (adjust-array (slot-value self 'elements)
                        (+ original
                           (cl:length (slot-value other 'elements)))
                        :element-type (slot-value self  'element-type)
                        :fill-pointer (+ original
                                         (cl:length (slot-value other 'elements)))))
    (replace (slot-value self 'elements) (slot-value other 'elements)
             :start1 original)
    self))

(defmethod add ((self jset) element)
  (add-element self element)
  self)

(defmethod retract ((self jset))
  (when (plusp (cl:length (slot-value self 'elements)))
    (decf (fill-pointer (slot-value self 'elements)))
    (setf (aref (slot-value self 'elements)
                (fill-pointer (slot-value self 'elements))) nil))
  self)



(defgeneric diffuse (obj thunk)
  (:method ((object t) thunk)
    (funcall thunk object)
    (values))
  (:method ((object null) thunk)
    (declare (ignore object thunk))
    (values))
  (:method ((object jset) thunk)
    (map nil thunk (slot-value object 'elements))
    (values)))


(defmacro asend (recipient &body body)
  `(diffuse ,recipient (lambda (this) ,@body)))

(defmethod send ((recipient object) message &rest argments)
  (apply message recipient arguments))


(defun initial-value (type)
  (if (atom type)
      (case type
        ((int) 0)
        ((float) 0.0d0)
        ((boolean) nil)
        ((string)  (%copy ""))
        ((in-stream out-stream) (make-instance type))
        (t nil))
      (ecase (first type)
        ((jset) `(make-jset ',(second type))))))


(defgeneric %copy (object)
  (:method ((self t)) self)
  (:method ((self string))
    (let ((new (make-array (cl:length self)
                           :element-type 'character
                           :initial-element #\space
                           :fill-pointer (cl:length self)
                           :adjustable t)))
      (replace new self)
      new))
  (:method ((self stream))
    (make-instance (class-of self) :lisp-stream  (slot-value self 'lisp-stream)))
  (:method ((self jset))
    (let ((new-elements (make-array (cl:length (slot-value self 'elements))
                                    :element-type (slot-value self 'element-type)
                                    :initial-element (initial-value element-type)
                                    :adjustable t
                                    :fill-pointer (cl:length (slot-value self 'elements)))))
      (replace new-elements  (slot-value self 'elements))
      (make-instance 'jset
          :element-type (slot-value self 'element-type)
          :elements new-elements))))


(defparameter *types* '())

(defmacro define-variable (name type &optional (initial-value nil ivp))
  (let ((val (gensym)))
    `(progn
       (push (cons ',name ',type) *types*)
       (defparameter ,name
         (let ((,val ,(if ivp initial-value (initial-value type))))
           (check-type ,val ,type)
           ,val)))))

(defmacro set? (var expression)
  (let ((vbind (gensym "BINDINGS-"))
        (vexpr (gensym "EXPRESSION-")))
    `(setf ,var (%copy (let ((,vbind (assoc ',var *types*))
                             (,vexpr ,expression))
                         (if ,vbind
                             (when (typep ,vexpr (cdr ,vbind))
                               ,vexpr)
                             ,vexpr))))))

(defmacro set (var expression)
  (let ((vbind (gensym "BINDINGS-"))
        (vexpr (gensym "EXPRESSION-")))
    `(setf ,var (%copy (let ((,vbind (assoc ',var *types*))
                             (,vexpr ,expression))
                         (when ,vbind
                           (unless (typep ,vexpr (cdr ,vbind))
                             (error "Trying to assign ~S of type ~S to ~S which expects a value of type ~S"
                                    ,vexpr (type-of ,vexpr) ',var (cdr ,vbind))))
                         ,vexpr)))))

(defmacro block (&body body)
  (loop
     :while (and (listp body)
                 (listp (first body))
                 (eql 'var (first (first body))))
     :collect (pop body) :into decls
     :finally (cl:return
                `(let*
                     ((*types* *types*)
                      ,@(mapcar
                         (lambda (var)
                           (destructuring-bind (var name type
                                                    &optional (initial-value nil ivp))
                               var
                             (let ((val (gensym "VALUE-")))
                               `(,name (let ((,val ,(if ivp
                                                        (%copy initial-value)
                                                        (initial-value type))))
                                         (push (cons ',name ',type) *types*)
                                         (check-type ,val ,type)
                                         ,val)))))
                         decls))
                   ,@body))))


(defmacro define-function (name arguments &body body)
  `(cl:defun ,name ,arguments
     (cl:block method
       (block                           ; j-block
           ,@body))))


(defmacro define-method (class name arguments &body body)
  `(cl:defmethod ,name ((this ,class) ,@arguments)
     (cl:block method
       (block                           ; j-block
           ,@body))))


(defmacro return (&optional (value nil vp))
  (if vp
      `(return-from method ,value)
      `(return-from method (values))))





(defmacro select (object condi &body body)
  `(diffuse ,object (lambda (this)
                      (when ,condi
                        ,@body))))

(defmacro while (object condi &body body)
  (let ((done (gensym)))
    `(cl:block ,done
       (diffuse ,object (lambda (this)
                          (unless ,condi (return-from ,done (values)))
                          ,@body)))))

(defmacro for ((init test increment) &body body)
  `(loop
      :initially ,init
      :while ,test
      :do (progn ,@body)
      :do ,increment))


(defun eval (string)
  (assert (stringp string))
  (error "~S not implemented yet." 'eval))


(when nil
  ;; examples
  
  (defmethod dump ((self object))
    (let ((mclass (class-of self)))
      (format t "MetaClass : ~A ~%" (name mclass))
      (asend (parent-of mclass)
             (format t "Parent metclass : ~A~%" (name this)))))

  (dump (make-instance 'object))
  (dump (make-instance 'toto))


  (define-variable storage (jset int))
  (add storage 1)
  (add storage 3)
  (add storage 5)
  (add storage 7)
  (add storage 11)
  (asend storage (write std-out "Item: " this NL))

  )




(when nil
  (load"j-language.lisp")
  (in-package  "COM.INFORMATIMAGO.J-LANGUAGE")
  (define-class toto object (a b c))
  (inspect *meta-classes*)
  )


;;;; THE END ;;;;
