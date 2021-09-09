

(defvar *deses* '())

(defstruct des name slots)
(defun des-named (name)
  (find name *deses* :key (function des-name)))
(defmacro des (name &rest slots)
  `(progn (pushnew (make-des :name ',name :slots ',slots) *deses*
                   :key (function des-name))
          ',name))
(progn
 (des object
      (size halfword) (objcount halfword)
      (class object) (slots slots))

 (des vector-of-object
      (size halfword) (objcount halfword)
      (elements (vector object)))

 (des vector-of-uword
      (size halfword) (objcount halfword)
      (elements (vector uword)))

 (des vector-of-octet
      (size halfword) (objcount halfword)
      (length uword)
      (elements (vector octet)))

 (des character
      (size halfword) (objcount halfword)
      (name object)
      (code object))

 (des integer
      (size halfword) (objcount halfword)
      (value word))

 (des float
      (size halfword) (objcount halfword)
      (value floating))

 (des array
      (size halfword) (objcount halfword)
      (element-type object)
      (elements vector-of-object)
      (dimension (vector uword)))

 (des octet-array
      (size halfword) (objcount halfword)
      (element-type object)
      (elements vector-of-octet)
      (dimension (vector uword)))

 (des cons
      (size halfword) (objcount halfword)
      (car object)
      (cdr object))

 (des symbol
      (size halfword) (objcount halfword)
      (name object)
      (package object)
      (value object)
      (function object)
      (plist object))

 (des class
      (size halfword) (objcount halfword)
      (name object)
      (direct-superclasses object))
 (des standard-class
      (size halfword) (objcount halfword)
      (name object)
      (direct-superclasses object))
 (des built-in-class
      (size halfword) (objcount halfword)
      (name object)
      (direct-superclasses object))
 (des structure-class
      (size halfword) (objcount halfword)
      (name object)
      (direct-superclasses object))
 (des funcallable-standard-class
      (size halfword) (objcount halfword)
      (name object)
      (direct-superclasses object))

 (des closure
      (size halfword) (objcount halfword)
      (function object)
      (environment object))

 (des function
      (size halfword) (objcount halfword)
      (lisp-name object)
      (lambda-list object)
      (body object))
 (des built-in-function
      (size halfword) (objcount halfword)
      (lisp-name object)
      (lambda-list object)
      (kernel-function kernel-function-ptr))
 (des funcallable-standard-object
      (size halfword) (objcount halfword)
      (object object)
      (kernel-function kernel-function-ptr))
 )

(defstruct graph
  deses)

(defparameter *graph* (make-graph :deses *deses*))

(defgeneric gendot (graph))

(defmethod gendot ((graph graph))
  (write-line "digraph structs {")
  (write-line "rankdir=LR")
  (write-line "node [shape=record]")
  (dolist (des (graph-deses graph))
    (gendot des))
  (write-line "}")
  (values))

(defmethod gendot ((des des))
  (format t "~A [label=\"{<~A>~(~:*~A~)|<~A>~(~:*~A~)}~{|<~A>~(~:*~A~)~}\"];~%"
          (des-name des)
          (first (first (des-slots des))) (first (second (des-slots des)))
          (mapcar (function first) (cddr (des-slots des))))
  (values))

(defstruct inst des values)
(defmethod make ((des des) &rest field-values)
  (assert (= (length (des-slots des))
             (length field-values)))
  (loop
    :for slot :in (des-slots des)
    :for value :in field-values
    :always (and (typep value 'inst)
                 (equalp (inst-des value) (second slot))))
  (make-inst :des des :values field-values))
(make )
(make (des-named 'cons) (* 3 8) 2 a d)
(make (des-named 'object)
      #S(des :name object :slots ((size halfword) (objcount halfword) (class object) (slots slots)))
      (* 3 8) 2  class slots)
(mapcar 'sxhash *deses*)
*deses*

(773525442 773525442 773525442 773525442 773525442 773525442 773525442 773525442 773525442 773525442 773525442 773525442 773525442 773525442 773525442 773525442 773525442 773525442 773525442 773525442)
(pprint *deses*)

(#S(des :name funcallable-standard-object
        :slots ((size halfword) (objcount halfword) (object object) (kernel-function kernel-function-ptr)))
 #S(des :name built-in-function
        :slots ((size halfword) (objcount halfword) (lisp-name object) (lambda-list object)
                (kernel-function kernel-function-ptr)))
 #S(des :name function
        :slots ((size halfword) (objcount halfword) (lisp-name object) (lambda-list object) (body object)))
 #S(des :name closure :slots ((size halfword) (objcount halfword) #'object (environment object)))
 #S(des :name funcallable-standard-class
        :slots ((size halfword) (objcount halfword) (name object) (direct-superclasses object)))
 #S(des :name structure-class :slots ((size halfword) (objcount halfword) (name object) (direct-superclasses object)))
 #S(des :name built-in-class :slots ((size halfword) (objcount halfword) (name object) (direct-superclasses object)))
 #S(des :name standard-class :slots ((size halfword) (objcount halfword) (name object) (direct-superclasses object)))
 #S(des :name class :slots ((size halfword) (objcount halfword) (name object) (direct-superclasses object)))
 #S(des :name symbol
        :slots ((size halfword) (objcount halfword) (name object) (package object) (value object) #'object
                (plist object)))
 #S(des :name cons :slots ((size halfword) (objcount halfword) (car object) (cdr object)))
 #S(des :name octet-array
        :slots ((size halfword) (objcount halfword) (element-type object) (elements vector-of-octet)
                (dimension (vector uword))))
 #S(des :name array
        :slots ((size halfword) (objcount halfword) (element-type object) (elements vector-of-object)
                (dimension (vector uword))))
 #S(des :name float :slots ((size halfword) (objcount halfword) (value floating)))
 #S(des :name integer :slots ((size halfword) (objcount halfword) (value word)))
 #S(des :name character :slots ((size halfword) (objcount halfword) (name object) (code object)))
 #S(des :name vector-of-octet :slots ((size halfword) (objcount halfword) (length uword) (elements (vector octet))))
 #S(des :name vector-of-uword :slots ((size halfword) (objcount halfword) (elements (vector uword))))
 #S(des :name vector-of-object :slots ((size halfword) (objcount halfword) (elements (vector object))))
 #S(des :name object :slots ((size halfword) (objcount halfword) (class object) (slots slots))))

(make )
(gendot *graph*)



            (case (second slot)
              ((vector-of-object)   (and (typep value 'inst) (eql (inst-des value) 'vector-of-object)))
              ((vector-of-octet)    (and (typep value 'inst) (eql (inst-des value) 'vector-of-octet)))

              ((octet)              (and (typep value 'inst) (eql (inst-des value) 'octet)))
              ((halfword)           (and (typep value 'inst) (eql (inst-des value) 'octet)))
              ((uword))
              ((word))
              ((floating))
              ((vector))
              ((kernel-function-ptr))

              ((object))
              ((slots))

              )


digraph structs {
rankdir=LR
node [shape=record]
funcallable-standard-object [label="{<size>size|<objcount>objcount}|<object>object|<kernel-function>kernel-function"];
built-in-function [label="{<size>size|<objcount>objcount}|<lisp-name>lisp-name|<lambda-list>lambda-list|<kernel-function>kernel-function"];
function [label="{<size>size|<objcount>objcount}|<lisp-name>lisp-name|<lambda-list>lambda-list|<body>body"];
closure [label="{<size>size|<objcount>objcount}|<function>function|<environment>environment"];
funcallable-standard-class [label="{<size>size|<objcount>objcount}|<name>name|<direct-superclasses>direct-superclasses"];
structure-class [label="{<size>size|<objcount>objcount}|<name>name|<direct-superclasses>direct-superclasses"];
built-in-class [label="{<size>size|<objcount>objcount}|<name>name|<direct-superclasses>direct-superclasses"];
standard-class [label="{<size>size|<objcount>objcount}|<name>name|<direct-superclasses>direct-superclasses"];
class [label="{<size>size|<objcount>objcount}|<name>name|<direct-superclasses>direct-superclasses"];
symbol [label="{<size>size|<objcount>objcount}|<name>name|<package>package|<value>value|<function>function|<plist>plist"];
cons [label="{<size>size|<objcount>objcount}|<car>car|<cdr>cdr"];
octet-array [label="{<size>size|<objcount>objcount}|<element-type>element-type|<elements>elements|<dimension>dimension"];
array [label="{<size>size|<objcount>objcount}|<element-type>element-type|<elements>elements|<dimension>dimension"];
float [label="{<size>size|<objcount>objcount}|<value>value"];
integer [label="{<size>size|<objcount>objcount}|<value>value"];
character [label="{<size>size|<objcount>objcount}|<name>name|<code>code"];
vector-of-octet [label="{<size>size|<objcount>objcount}|<length>length|<elements>elements"];
vector-of-uword [label="{<size>size|<objcount>objcount}|<elements>elements"];
vector-of-object [label="{<size>size|<objcount>objcount}|<elements>elements"];
object [label="{<size>size|<objcount>objcount}|<class>class|<slots>slots"];
}






(pprint *deses*)

(#S(des :name funcallable-standard-object
        :slots ((size halfword) (objcount halfword) (object object) (kernel-function kernel-function-ptr)))
 #S(des :name built-in-function
        :slots ((size halfword) (objcount halfword) (lisp-name object) (lambda-list object)
                (kernel-function kernel-function-ptr)))
 #S(des :name function
        :slots ((size halfword) (objcount halfword) (lisp-name object) (lambda-list object) (body object)))
 #S(des :name closure :slots ((size halfword) (objcount halfword) #'object (environment object)))
 #S(des :name funcallable-standard-class
        :slots ((size halfword) (objcount halfword) (name object) (direct-superclasses object)))
 #S(des :name structure-class :slots ((size halfword) (objcount halfword) (name object) (direct-superclasses object)))
 #S(des :name built-in-class :slots ((size halfword) (objcount halfword) (name object) (direct-superclasses object)))
 #S(des :name standard-class :slots ((size halfword) (objcount halfword) (name object) (direct-superclasses object)))
 #S(des :name class :slots ((size halfword) (objcount halfword) (name object) (direct-superclasses object)))
 #S(des :name symbol
        :slots ((size halfword) (objcount halfword) (name object) (package object) (value object) #'object
                (plist object)))
 #S(des :name cons :slots ((size halfword) (objcount halfword) (car object) (cdr object)))
 #S(des :name octet-array
        :slots ((size halfword) (objcount halfword) (element-type object) (elements vector-of-octet)
                (dimension (vector uword))))
 #S(des :name array
        :slots ((size halfword) (objcount halfword) (element-type object) (elements vector-of-object)
                (dimension (vector uword))))
 #S(des :name float :slots ((size halfword) (objcount halfword) (value floating)))
 #S(des :name integer :slots ((size halfword) (objcount halfword) (value word)))
 #S(des :name character :slots ((size halfword) (objcount halfword) (name object) (code object)))
 #S(des :name vector-of-octet :slots ((size halfword) (objcount halfword) (length uword) (elements (vector octet))))
 #S(des :name vector-of-uword :slots ((size halfword) (objcount halfword) (elements (vector uword))))
 #S(des :name vector-of-object :slots ((size halfword) (objcount halfword) (elements (vector object))))
 #S(des :name object :slots ((size halfword) (objcount halfword) (class object) (slots slots))))

