(defpackage "COM.INFORMATIMAGO.BOCL.KERNEL.GENERATE"
  (:use "COMMON-LISP")
  (:import-from "CCL"
                "FD-STREAM" "DIRECT-SLOT-DEFINITION"
                "EFFECTIVE-SLOT-DEFINITION" "EQL-SPECIALIZER"
                "SPECIALIZER"
                "METAOBJECT"
                "FORWARD-REFERENCED-CLASS"
                "FUNCALLABLE-STANDARD-CLASS"
                "FUNCALLABLE-STANDARD-OBJECT" "SLOT-DEFINITION"
                "STANDARD-ACCESSOR-METHOD"
                "STANDARD-DIRECT-SLOT-DEFINITION"
                "STANDARD-EFFECTIVE-SLOT-DEFINITION"
                "STANDARD-READER-METHOD" "STANDARD-SLOT-DEFINITION"
                "STANDARD-WRITER-METHOD")
  (:export "GENERATE-C-CODE"))
(in-package "COM.INFORMATIMAGO.BOCL.KERNEL.GENERATE")

;; (unintern 'physical-pathname)

;; ========================================================================

(defun flatten (tree)
  "
RETURN: A list containing all the elements of the `tree'.
"
  (loop
    :with result = nil
    :with stack = nil
    :while (or tree stack)
    :do (cond
          ((null tree)
           (setq tree (pop stack)))
          ((atom tree)
           (push tree result)
           (setq tree (pop stack)))
          ((listp (car tree))
           (push (cdr tree) stack)
           (setq tree (car tree)))
          (t
           (push (car tree) result)
           (setq tree (cdr tree))))
    :finally (return (nreverse result))))

(defun remove-empty-subseqs (subsequences remove-empty-subseqs)
  (if remove-empty-subseqs
      (delete-if (lambda (seq)
                   (typecase seq
                     (vector  (zerop (length seq)))
                     (null    t)
                     (t       nil)))
                 subsequences)
      subsequences))

(defun split-sequence-if (predicate sequence &key remove-empty-subseqs)
  "
PREDICATE:      A predicate on elements of the SEQUENCE sequence.  When
                returning true, the sequence is split before and after
                the element, which are removed from the resulting
                subsequences.

SEQUENCE:       A sequence.

REMOVE-EMPTY-SUBSEQS:
                A boolean.  If true, empty subsequences are removed from the result.

RETURN:         A list of subsequences of SEQUENCE, split upon any
                element for which the PREDICATE is true.

EXAMPLES:       (split-sequence-if (function zerop) '(1 2 0 3 4 5 0 6 7 8 0 9))
                --> ((1 2) (3 4 5) (6 7 8) (9))
                (split-sequence-if (function zerop) #(1 2 0 3 4 5 0 6 7 8 0 9))
                --> (#(1 2) #(3 4 5) #(6 7 8) #(9))
                (split-sequence-if (lambda (x) (find x #(#\\space #\\0))) \"1 2 0 3 4 5 0 6 7 8\" )
                --> (\"1\" \"2\" \"\" \"\" \"3\" \"4\" \"5\" \"\" \"\" \"6\" \"7\" \"8\")
"
  (let ((chunks  '()))
    (etypecase sequence
      (vector (loop
                :with position := 0
                :with nextpos  := 0
                :with length   := (length sequence)
                :while (< position length)
                :do (loop :while (and (< nextpos length)
                                      (not (funcall predicate (aref sequence nextpos))))
                          :do (incf nextpos))
                    (push (subseq sequence position nextpos) chunks)
                    (setf position (1+ nextpos)
                          nextpos  position)
                    (when (= position length)
                      (push (subseq sequence 0 0) chunks))))
      (cons   (loop
                :with start := sequence
                :while start
                :do (let ((end (loop
                                 :with current := start
                                 :while (and current
                                             (not (funcall predicate (car current))))
                                 :do (pop current)
                                 :finally (return current))))
                      (push (ldiff start end) chunks)
                      (setf start (cdr end)))))
      (null))
    (remove-empty-subseqs (nreverse chunks) remove-empty-subseqs)))

(defun split-string (string &optional (separators " ") (omit-nulls nil))
  "
STRING:         A sequence.

SEPARATOR:      A sequence.

OMIT-NULLS:     A boolean.  If true, empty subsequences are removed from the result.

RETURN:         A list of subsequence of STRING, split upon any element of SEPARATORS.
                Separators are compared to elements of the STRING with EQL.

EXAMPLES:       (split-string \"1 2 0 3 4 5 0 6 7 8\" '(#\space #\0))
                --> (\"1\" \"2\" \"\" \"\" \"3\" \"4\" \"5\" \"\" \"\" \"6\" \"7\" \"8\")
"
  (split-sequence-if (lambda (ch) (find ch separators)) string :remove-empty-subseqs omit-nulls))

;; ========================================================================


(defparameter *initial-object-graph*
  '(
    (t built-in-class)
    (character t)
    (array t)
    (sequence t)
    (vector array sequence)
    (string vector)
    (symbol t)
    (list sequence)
    (cons list)
    (null symbol list)

    (number t)
    (complex number)
    (real number)
    (float real)
    (rational real)
    (ratio rational)
    (integer rational)
    (signed-byte integer)
    (unsigned-byte signed-byte)
    (bit unsigned-byte)

    (hash-table t)
    (restart t)
    (random-state t)
    (package t)
    (readtable t)
    (pathname t)
    (logical-pathname pathname)
    (physical-pathname pathname)

    (stream t)
    (file-stream stream)
    (string-stream stream)
    (two-way-stream stream)
    (broadcast-stream stream)
    (echo-stream stream)
    (concatenated-stream stream)
    (synonym-stream stream)

    (fd-stream stream)

    (function t)
    (compiled-function function)
    (standard-object t)
    (metaobject standard-object)
    (funcallable-standard-object function standard-object)
    (generic-function  funcallable-standard-object metaobject)
    (standard-generic-function  generic-function)
    (slot-definition metaobject)
    (direct-slot-definition slot-definition)
    (standard-slot-definition slot-definition)
    (effective-slot-definition slot-definition)
    (standard-direct-slot-definition direct-slot-definition standard-slot-definition)
    (standard-effective-slot-definition effective-slot-definition standard-slot-definition)
    (method-combination metaobject)
    (specializer metaobject)
    (eql-specializer specializer)
    (class specializer)
    (funcallable-standard-class class)
    (standard-class class)
    (built-in-class class)
    (structure-class class)
    (forward-referenced-class class)
    (method metaobject)
    (standard-method method)
    (standard-accessor-method standard-method)
    (standard-writer-method standard-accessor-method)
    (standard-reader-method standard-accessor-method)

    (condition t)
    (warning condition)
    (style-warning warning)
    (simple-condition condition)
    (simple-warning simple-condition warning)

    (serious-condition condition)
    (storage-condition serious-condition)
    (error serious-condition)
    (type-error error)
    (simple-error simple-condition error)
    (simple-type-error simple-condition type-error)
    (control-error error)
    (parse-error error)
    (reader-error error)
    (stream-error error)
    (end-of-file stream-error)
    (file-error error)
    (print-not-readable error)
    (arithmetic-error error)
    (division-by-zero arithmetic-error)
    (floating-point-underflow arithmetic-error)
    (floating-point-invalid-operation arithmetic-error)
    (floating-point-inexact arithmetic-error)
    (floating-point-overflow arithmetic-error)
    (package-error error)
    (program-error error)
    (cell-error error)
    (unbound-variable cell-error)
    (unbound-slot cell-error)
    (undefined-function cell-error)))

(defclass physical-pathname (pathname)
  (path)
  (:metaclass built-in-class))

(defparameter *classes*      (remove-duplicates
                              (flatten *initial-object-graph*)))

(defparameter *meta-classes* (remove-duplicates
                              (mapcar (lambda (class-name)
                                        (class-name (class-of (find-class class-name))))
                                      *classes*)))

(defparameter *nodes* '((|Object| (class |Class|) (slots (member |VectorOfObject| |VectorOfUWord| |VectorOfOctet|
                                                                 |Character| |Integer| |Float| |Array| |OctetArray|
                                                                 |Cons| |Symbol| |FileStream| |Class| |StandardClass|
                                                                 |StructureClass| |BuiltInClass| |Function|)))
                        ;; Slots
                        (|VectorOfObject| (elements (vector |Object|)))
                        (|VectorOfUWord|)
                        (|VectorOfOctet|)
                        (|Character| (name |Object|) (code |Object|))
                        (|Integer|)
                        (|Float|)
                        (|Array| (element-type |Object|) (elements |VectorOfObject|))
                        (|OctetArray|)
                        (|Cons| (car |Object|) (cdr |Object|))
                        (|Symbol| (name String) (package |Object|) (value |Object|) (function |Object|) (plist |Object|))
                        (|FileStream| (pathname |Object|) (element-type |Object|) (external-format |Object|))
                        (|Class| (name |Object|) (direct-superclasses |Object|))
                        (|StandardClass| (name |Object|) (direct-superclasses |Object|))
                        (|StructureClass| (name |Object|) (direct-superclasses |Object|))
                        (|BuiltInClass| (name |Object|) (direct-superclasses |Object|))
                        (|Function|)))



(defun c-name (lisp-name)
   (intern (format nil "~A_Class" (substitute #\_ #\- (string lisp-name)))))

(defun lisp-to-camel (sym)
  (format nil "~{~@(~A~)~}" (split-string (string sym) "-" t)))

(defun generate-c-code ()

  (with-open-file (*standard-output*  "kernel_objects.h"
                                      :direction :output
                                      :if-does-not-exist :create
                                      :if-exists :supersede)

    (write-line "/* Do not edit -- generated from generate-kernel.lisp */")
    (write-line "#ifndef kernel_objects_h")
    (write-line "#define kernel_objects_h")
    (write-line "#include \"kernel.h\"")
    (terpri)
    (write-line "Object* kernel_classes;  /* list of class objects */")
    (write-line "Object* NIL_Symbol;")
    (write-line "Object* T_Symbol;")
    (write-line "Object* KERNEL_Symbol;")
    (write-line "Object* KEYWORD_Symbol;")
    (write-line "Object* CHARACTER_Symbol;")
    (write-line "Object* KERNEL_obarray;  /* list of interned symbols */")
    (write-line "Object* KEYWORD_obarray; /* list of interned keywords */")
    (write-line "Object* UNBOUND_Object;")
    (terpri)
    (dolist (class *classes*)
      (let* ((var-name (c-name class)))
        (format t "Object* ~A;~%" (string var-name))))
    (terpri)
    (write-line "void kernel_initialize(void);")
    (terpri)
    (write-line "#endif"))

  (with-open-file (*standard-output*  "kernel_objects.c"
                                      :direction :output
                                      :if-does-not-exist :create
                                      :if-exists :supersede)

    (write-line "/* Do not edit -- generated from generate-kernel.lisp */")
    (write-line "#include \"kernel_objects.h\"")
    (write-line "#include \"kernel_types.h\"")
    (write-line "#include \"memory.h\"")
    (terpri)
    (write-line "Object* kernel_classes   = 0;  /* list of class objects */")
    (write-line "Object* NIL_Symbol       = 0;")
    (write-line "Object* T_Symbol         = 0;")
    (write-line "Object* KERNEL_Symbol    = 0;")
    (write-line "Object* KEYWORD_Symbol   = 0;")
    (write-line "Object* CHARACTER_Symbol = 0;")
    (write-line "Object* KERNEL_obarray   = 0;  /* list of interned symbols */")
    (write-line "Object* KEYWORD_obarray  = 0;  /* list of interned keywords */")
    (write-line "Object* UNBOUND_Object   = 0;")
    (terpri)
    (dolist (class *classes*)
      (let* ((var-name (c-name class)))
        (format t "Object* ~40A = 0;~%" (string var-name))))
    (terpri)
    (write-line "void kernel_initialize(void){")
    (terpri)
    (write-line "    memory_initialize();")
    (terpri)
    (dolist (class *classes*)
      (let* ((var-name (c-name class))
             (class-of-class (class-name (class-of (find-class class)))))
        (format t "    ~40A = make_boot_Object((Slots*)make_boot_~A());~%"
                (string var-name) (lisp-to-camel class-of-class))))

    (terpri)
    (dolist (class *classes*)
      (let* ((var-name (c-name  class))
             (class-of-class (class-name (class-of (find-class class))))
             (coc-var-name (c-name class-of-class)))
        (format t "    ~40A-> class = ~A;~%"
                (string var-name) (string coc-var-name))))
    (terpri)
    (write-line "    UNBOUND_Object   = make_symbol_cstring(\"UNBOUND\");")
    (write-line "    NIL_Symbol       = make_symbol_cstring(\"NIL\");")
    (write-line "    KERNEL_Symbol    = make_symbol_cstring(\"KERNEL\");")
    (write-line "    CHARACTER_Symbol = make_symbol_cstring(\"CHARACTER\");")
    (write-line "    KEYWORD_Symbol   = make_symbol_cstring(\"KEYWORD\");")
    (write-line "    KERNEL_obarray   = cons(KERNEL_Symbol,cons(NIL_Symbol,cons(CHARACTER_Symbol,cons(KEYWORD_Symbol,cons(KERNEL_Symbol,NIL_Symbol)))));")
    (write-line "    NIL_Symbol       ->slots->symbol.package = KERNEL_obarray;")
    (write-line "    KERNEL_Symbol    ->slots->symbol.package = KERNEL_obarray;")
    (write-line "    CHARACTER_Symbol ->slots->symbol.package = KERNEL_obarray;")
    (write-line "    KEYWORD_Symbol   ->slots->symbol.package = KERNEL_obarray;")
    (write-line "    KEYWORD_obarray  = cons(KEYWORD_Symbol,NIL_Symbol);")
    (write-line "    kernel_classes   = NIL_Symbol;")
    (write-line "    set_rootset((Block*)kernel_rootset());")
    (terpri)
    (dolist (class *classes*)
      (let* ((var-name (c-name  class)))
        (format t "    kernel_classes = cons(~40A,kernel_classes);~%"
                (string var-name))))
    (terpri)
    (dolist (class *classes*)
      (let* ((var-name (c-name  class)))
        (format t "    ~40A -> slots->class.direct_superclasses = NIL_Symbol;~%"
                (string var-name))))
    (terpri)
    (loop for (class . direct-superclasses) in *initial-object-graph*
          for var-name = (c-name class)
          for var-direct-superclasses = (mapcar (function c-name) direct-superclasses)
          do (dolist (var-ds var-direct-superclasses)
               (format t "    boot_class_add_superclass(~40A,~40A);~%"
                       (string var-name) (string var-ds))))
    (terpri)
    (dolist (class *classes*)
      (let* ((var-name (c-name  class)))
        (format t "    ~40A -> slots->class.name = symbol_intern(string_new_cstring(~S));~%"
                (string var-name) (string class))))
    (terpri)
    (write-line "    T_Symbol = symbol_intern_cstring(\"T\");")
    (terpri)
    (write-line "}")
    (terpri)
    (write-line "/*** THE END ***/"))

  (values))




#-(and)
(progn
  (class-of (find-class 't))         ; #<standard-class built-in-class>
  (class-of 'character)              ; #<built-in-class symbol>
  (class-of (find-class  'character)) ; #<standard-class built-in-class>
  )

#-(and)
(let (r) (dolist (s   (remove-duplicates (flatten *initial-object-graph*)) r)
   (let ((ss (or (find-symbol (symbol-name s) "CCL")
                 (find-symbol (symbol-name s) "CL"))))

     (if ss
         (push ss r)
         (push s r)))))




#-(and) (pprint generate-lisp-prototype)
