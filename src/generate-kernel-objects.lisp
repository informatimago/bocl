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


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Some library code, copied from com.informatimago.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

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

(defgeneric adjacency-list (object)
  (:documentation
   "Return the list of vertices connected to vertex thru a single edge.")
  (:method ((vertex t)) '()))


(defgeneric reachable-list (vertex)
  (:documentation
   "Return the list of vertices rechable from vertex.")
  (:method ((vertex t)) '()))


;; dependency graphs should be trees, so we run topological sorts on
;; them.


(defun reachable (root successors)
  "
SUCCESSORS:  A function of one node returning a list of nodes.
RETURN:      A list of objects reachable from root traversing SUCCESSORS.
"
  (loop
     :with reachable = '()
     :with old = (funcall successors root)
     :while old
     :do (let ((current (pop old)))
           (pushnew current reachable)
           (dolist (successor (funcall successors current))
             (unless (member successor reachable)
               (pushnew  successor old))))
     :finally (return reachable)))


(defun closest-to-root (nodes ordered-nodes)
  "
RETURN: the node in NODES that is the closest to ROOT according to ordered-nodes
"
  (loop
     :with closest = (pop nodes)
     :with minimum = (position closest ordered-nodes)
     :for node :in nodes
     :for distance = (position node ordered-nodes)
     :initially (assert minimum)
     :when (and distance (< distance minimum))
     :do (setf closest node
               minimum distance)
     :finally (return (values closest minimum))))

(defun find-shortest-path (from to successors)
  "
RETURN: The shortest path of length>0 from FROM to TO if it exists, or NIL.
"
  ;; breadth first search
  (loop
     :with processed = '()
     :for paths = (list (list from)) :then new-paths
     :for new-paths = (remove-if (lambda (head) (member head processed))
                                 (mapcan (lambda (path)
                                           (mapcar (lambda (new-node) (cons new-node path))
                                                   (funcall successors (first path))))
                                         paths)
                                 :key (function first))
     :for shortest-path = (find to new-paths :key (function first))
     :do (setf paths     (nconc paths new-paths)
               processed (nconc (delete-duplicates (mapcar (function first) new-paths)) processed))
     :until (or shortest-path (endp new-paths))
     :finally (return (reverse shortest-path))))

(defun set-equal (a b)
  (and (subsetp a b) (subsetp b a)))

(defun find-cycles (objects)
  (remove-duplicates
   (remove nil
           (map 'list
                (lambda (cycle) (find-shortest-path cycle cycle (function adjacency-list)))
                (remove-if-not (lambda (x) (member x (reachable-list x))) objects)))
   :test (function set-equal)))

(defun print-cycle (path)
  (format t "~%There is a cycle going ~%from ~A" (first path))
  (dolist (node (rest path))
    (format t "~%  to ~A" node))
  (format t " !!!~%"))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


(defparameter *initial-object-graph*
  ;; Each entry: (class-name (is-a) . direct-superclasses)
  ;; eg. CHARACTER is a BUILT-IN-CLASS, has T as direct-superclass.
  '(
    (t                                  (built-in-class)) ; root
    (character                          (built-in-class)             (t))
    (array                              (built-in-class)             (t))
    (string                             (built-in-class)             (vector))
    (vector                             (built-in-class)             (array                       sequence))
    (bit-vector                         (built-in-class)             (vector))
    (sequence                           (built-in-class)             (t))
    (cons                               (built-in-class)             (list))
    (null                               (built-in-class)             (symbol                      list))
    (symbol                             (built-in-class)             (t))
    (list                               (built-in-class)             (sequence))
    (complex                            (built-in-class)             (number))
    (number                             (built-in-class)             (t))
    (float                              (built-in-class)             (real))
    (real                               (built-in-class)             (number))
    (ratio                              (built-in-class)             (rational))
    (rational                           (built-in-class)             (real))
    (integer                            (built-in-class)             (rational))
    (signed-byte                        (built-in-class)             (integer))
    (bit                                (built-in-class)             (unsigned-byte))
    (unsigned-byte                      (built-in-class)             (signed-byte))
    (hash-table                         (built-in-class)             (t))
    (restart                            (built-in-class)             (t))
    (random-state                       (built-in-class)             (t))
    (package                            (built-in-class)             (t))
    (readtable                          (built-in-class)             (t))
    (logical-pathname                   (built-in-class)             (pathname))
    (physical-pathname                  (built-in-class)             (pathname))
    (pathname                           (built-in-class)             (t))
    (file-stream                        (built-in-class)             (stream))
    (string-stream                      (built-in-class)             (file-stream)) ; not exactly, but that's how it's implemented here! still conforming AFAIK
    (two-way-stream                     (built-in-class)             (stream))
    (broadcast-stream                   (standard-class)             (stream))
    (echo-stream                        (standard-class)             (stream))
    (concatenated-stream                (standard-class)             (stream))
    (synonym-stream                     (built-in-class)             (stream))
    (fd-stream                          (built-in-class)             (stream))
    (stream                             (standard-class)             (t))
    (closure                            (built-in-class)             (t))
    (compiled-closure                   (built-in-class)             (closure))
    (function                           (built-in-class)             (t))
    (compiled-function                  (built-in-class)             (function))
    (standard-object                    (standard-class)             (t))
    (funcallable-standard-object        (funcallable-standard-class) (function                     standard-object))
    (standard-generic-function          (funcallable-standard-class) (generic-function))
    (generic-function                   (funcallable-standard-class) (funcallable-standard-object metaobject))
    (slot-definition                    (standard-class)             (metaobject))
    (standard-direct-slot-definition    (standard-class)             (direct-slot-definition      standard-slot-definition))
    (direct-slot-definition             (standard-class)             (slot-definition))
    (standard-effective-slot-definition (standard-class)             (effective-slot-definition   standard-slot-definition))
    (effective-slot-definition          (standard-class)             (slot-definition))
    (standard-slot-definition           (standard-class)             (slot-definition))
    (method-combination                 (standard-class)             (metaobject))
    (eql-specializer                    (standard-class)             (specializer))
    (specializer                        (standard-class)             (metaobject))
    (funcallable-standard-class         (standard-class)             (class))
    (standard-class                     (standard-class)             (class))
    (built-in-class                     (standard-class)             (class))
    (structure-class                    (standard-class)             (class))
    (forward-referenced-class           (standard-class)             (class))
    (class                              (standard-class)             (specializer))
    (metaobject                         (standard-class)             (standard-object))
    (method                             (standard-class)             (metaobject))
    (standard-method                    (standard-class)             (method))
    (standard-writer-method             (standard-class)             (standard-accessor-method))
    (standard-reader-method             (standard-class)             (standard-accessor-method))
    (standard-accessor-method           (standard-class)             (standard-method))
    (style-warning                      (standard-class)             (warning))
    (simple-warning                     (standard-class)             (simple-condition            warning))
    (warning                            (standard-class)             (condition))
    (condition                          (standard-class)             (t))
    (storage-condition                  (standard-class)             (serious-condition))
    (serious-condition                  (standard-class)             (condition))
    (simple-error                       (standard-class)             (simple-condition            error))
    (simple-type-error                  (standard-class)             (simple-condition            type-error))
    (simple-condition                   (standard-class)             (condition))
    (type-error                         (standard-class)             (error))
    (control-error                      (standard-class)             (error))
    (parse-error                        (standard-class)             (error))
    (reader-error                       (standard-class)             (error))
    (end-of-file                        (standard-class)             (stream-error))
    (stream-error                       (standard-class)             (error))
    (file-error                         (standard-class)             (error))
    (print-not-readable                 (standard-class)             (error))
    (division-by-zero                   (standard-class)             (arithmetic-error))
    (floating-point-underflow           (standard-class)             (arithmetic-error))
    (floating-point-invalid-operation   (standard-class)             (arithmetic-error))
    (floating-point-inexact             (standard-class)             (arithmetic-error))
    (floating-point-overflow            (standard-class)             (arithmetic-error))
    (arithmetic-error                   (standard-class)             (error))
    (package-error                      (standard-class)             (error))
    (program-error                      (standard-class)             (error))
    (error                              (standard-class)             (serious-condition))
    (unbound-variable                   (standard-class)             (cell-error))
    (unbound-slot                       (standard-class)             (cell-error))
    (undefined-function                 (standard-class)             (cell-error))
    (cell-error                         (standard-class)             (error))
    ))

(defun cinfo-class-name                (entry) (car entry))
(defun cinfo-class-metaclass           (entry) (caadr entry))
(defun cinfo-class-direct-superclasses (entry) (caddr entry))
(defun find-class-info (class-name)
  (find class-name *initial-object-graph* :key (function first)))



(defparameter *classes*
  (remove-duplicates (append (mapcar (function cinfo-class-name) *initial-object-graph*)
                             (mapcan (function flatten)          *initial-object-graph*))
                     :from-end t))

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



(defun class-c-name (lisp-name)
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
    (write-line "Object  uninitialized_object;")
    (terpri)
    (write-line "Ref kernel_classes;  /* list of class objects */")
    (write-line "Ref NIL_Symbol;")
    (write-line "Ref T_Symbol;")
    (write-line "Ref BIT_Symbol;")
    (write-line "Ref KERNEL_Symbol;")
    (write-line "Ref KEYWORD_Symbol;")
    (write-line "Ref CHARACTER_Symbol;")
    (write-line "Ref KERNEL_obarray;  /* list of interned symbols */")
    (write-line "Ref KEYWORD_obarray; /* list of interned keywords */")
    (write-line "Ref UNBOUND_Object;")
    (terpri)
    (dolist (class *classes*)
      (let* ((var-name (class-c-name class)))
        (format t "Ref ~A;~%" (string var-name))))
    (terpri)
    (write-line "void kernel_objects_initialize(void);")
    (terpri)
    (write-line "#endif"))

  (with-open-file (*standard-output*  "kernel_objects.c"
                                      :direction :output
                                      :if-does-not-exist :create
                                      :if-exists :supersede)

    (write-line "/* Do not edit -- generated from generate-kernel.lisp */")
    (write-line "#include \"kernel_objects.h\"")
    (write-line "#include \"kernel_types.h\"")
    (write-line "#include \"kernel_private.h\"")
    (write-line "#include \"memory.h\"")
    (write-line "#include \"reader.h\"")
    (write-line "#include \"stream.h\"")
    (write-line "#include \"cons.h\"")
    (terpri)
    (write-line "Class   uninitialized_class={24,2,&uninitialized_object,&uninitialized_object};")
    (write-line "Object  uninitialized_object={24,2,(Ref)&uninitialized_class,(Slots*)&uninitialized_class};")
    (terpri)
    (write-line "Ref kernel_classes   = &uninitialized_object;  /* list of class objects */")
    (write-line "Ref NIL_Symbol       = &uninitialized_object;")
    (write-line "Ref T_Symbol         = &uninitialized_object;")
    (write-line "Ref KERNEL_Symbol    = &uninitialized_object;")
    (write-line "Ref KEYWORD_Symbol   = &uninitialized_object;")
    (write-line "Ref CHARACTER_Symbol = &uninitialized_object;")
    (write-line "Ref BIT_Symbol       = &uninitialized_object;")
    (write-line "Ref KERNEL_obarray   = &uninitialized_object;  /* list of interned symbols */")
    (write-line "Ref KEYWORD_obarray  = &uninitialized_object;  /* list of interned keywords */")
    (write-line "Ref UNBOUND_Object   = &uninitialized_object;")
    (terpri)
    (dolist (class *classes*)
      (let* ((var-name (class-c-name class)))
        (format t "Ref ~40A = &uninitialized_object;~%" (string var-name))))
    (terpri)
    (write-line "void kernel_objects_initialize(void){")
    (terpri)
    (dolist (class-name *classes*)
      (let* ((info           (find-class-info class-name))
             (var-name       (class-c-name class-name))
             (class-of-class (cinfo-class-metaclass info)))
        (format t "    ~40A = make_boot_Object((Slots*)make_boot_~A());~%"
                (string var-name) (lisp-to-camel class-of-class))))
    (terpri)
    (dolist (class-name *classes*)
      (let* ((info           (find-class-info class-name))
             (var-name       (class-c-name class-name))
             (class-of-class (cinfo-class-metaclass info))
             (coc-var-name   (class-c-name class-of-class)))
        (format t "    ~40A-> class = ~A;~%"
                (string var-name) (string coc-var-name))))
    (terpri)
    (write-line "    /* Bootstrapping data structures: we need to patch up incomplete initializations: */")
    (terpri)
    (write-line "    NIL_Symbol       = symbol_new_cstring(\"NIL\");")
    (write-line "    NIL_Symbol       ->slots->symbol.package = NIL_Symbol;")
    (write-line "    NIL_Symbol       ->slots->symbol.plist   = NIL_Symbol;")
    (terpri)
    (write-line "    KERNEL_Symbol    = symbol_new_cstring(\"KERNEL\");")
    (write-line "    KERNEL_obarray   = cons(KERNEL_Symbol,cons(NIL_Symbol,cons(KERNEL_Symbol,NIL_Symbol)));")
    (write-line "    KERNEL_Symbol    ->slots->symbol.package = KERNEL_obarray;")
    (write-line "    NIL_Symbol       ->slots->symbol.package = KERNEL_obarray;")
    (terpri)
    (write-line "    CHARACTER_Symbol = symbol_intern_cstring(\"CHARACTER\");")
    (write-line "    NIL_Symbol       ->slots->symbol.name->slots->octet_array.element_type = CHARACTER_Symbol;")
    (write-line "    KERNEL_Symbol    ->slots->symbol.name->slots->octet_array.element_type = CHARACTER_Symbol;")
    (write-line "    CHARACTER_Symbol ->slots->symbol.name->slots->octet_array.element_type = CHARACTER_Symbol;")
    (terpri)
    (write-line "    UNBOUND_Object   = symbol_new_cstring(\"UNBOUND\");")
    (write-line "    NIL_Symbol       ->slots->symbol.value    = UNBOUND_Object;")
    (write-line "    NIL_Symbol       ->slots->symbol.function = UNBOUND_Object;")
    (write-line "    KERNEL_Symbol    ->slots->symbol.value    = UNBOUND_Object;")
    (write-line "    KERNEL_Symbol    ->slots->symbol.function = UNBOUND_Object;")
    (write-line "    CHARACTER_Symbol ->slots->symbol.value    = UNBOUND_Object;")
    (write-line "    CHARACTER_Symbol ->slots->symbol.function = UNBOUND_Object;")
    (write-line "    UNBOUND_Object   ->slots->symbol.value    = UNBOUND_Object;")
    (write-line "    UNBOUND_Object   ->slots->symbol.function = UNBOUND_Object;")
    (terpri)
    (write-line "    KEYWORD_Symbol   = symbol_intern_cstring(\"KEYWORD\");")
    (terpri)
    (write-line "    KEYWORD_obarray  = cons(KEYWORD_Symbol,NIL_Symbol);")
    (write-line "    kernel_classes   = NIL_Symbol;")
    (terpri)
    (dolist (class *classes*)
      (let* ((var-name (class-c-name  class)))
        (format t "    push(~40A,&kernel_classes);~%"
                (string var-name))))
    (terpri)
    (dolist (class-name *classes*)
      (let* ((var-name (class-c-name  class-name)))
          (format t "    ~40A -> slots->class.direct_superclasses = NIL_Symbol;~%"
                  (string var-name))))
    (terpri)
    (loop for info in *initial-object-graph*
          for class-name              = (cinfo-class-name info)
          for direct-superclasses     = (cinfo-class-direct-superclasses info)
          for var-name                = (class-c-name class-name)
          for var-direct-superclasses = (mapcar (function class-c-name) direct-superclasses)
          do (dolist (var-ds var-direct-superclasses)
               (format t "    boot_class_add_superclass(~40A,~40A);~%"
                       (string var-name) (string var-ds))))
    (terpri)
    (dolist (class *classes*)
      (let* ((var-name (class-c-name  class)))
        (format t "    ~40A -> slots->class.name = symbol_intern_cstring(~S);~%"
                (string var-name) (string class))))
    (terpri)
    (write-line "    NIL_Symbol->class = NULL_Class;")
    (write-line "    T_Symbol = symbol_intern_cstring(\"T\");")
    (write-line "    BIT_Symbol = symbol_intern_cstring(\"BIT\");")
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

