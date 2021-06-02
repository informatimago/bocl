(defpackage "COM.INFORMATIMAGO.BOCL.KERNEL.GENERATE.KERNEL-FUNCTIONS"
  (:use "COMMON-LISP")
  (:export "GENERATE-BUILT-IN-FUNCTIONS"
           "GENERATE-KERNEL-FUNCTION"))
(in-package "COM.INFORMATIMAGO.BOCL.KERNEL.GENERATE.KERNEL-FUNCTIONS")

(defun parse-lambda-list (lambda-list)
  (loop
    :with mandatories      := '()
    :with optionals        := '()
    :with rest             := nil
    :with keys             := '()
    :with allow-other-keys := '()
    :with state            := :mandatory
    :with current          := lambda-list
    :while current
    :do (let ((par (pop current)))
          (case par
            ((&optional)
             (assert (eq state :mandatory))
             (setf state :optional))
            ((&rest)
             (assert (member state '(:mandatory :optional)))
             (setf state :rest))
            ((&key)
             (assert (member state '(:mandatory :optional :after-rest)))
             (setf state :keys))
            ((&allow-other-keys)
             (assert (eq state :keys))
             (setf allow-other-keys t)
             (assert (null current)))
            (otherwise
             (case state
               (:mandatory (push par mandatories))
               (:optional  (push par optionals))
               (:rest      (setf rest par
                                 state :after-rest))
               (:after-rest (error "Too many symbols after &rest: ~S in ~S"
                                   par lambda-list))
               (:keys     (push par keys))))))
    :finally (return (values (nreverse mandatories) (nreverse optionals) rest (nreverse keys) allow-other-keys))))



(defun keywordize (sym)
  (intern (string sym) "KEYWORD"))

(defun k-fun-name (fname)
  (concatenate 'string "kernel_"
               (with-output-to-string (*standard-output*)
                 (loop :for ch :across (string fname)
                       :do (case ch
                             ((#\-) (write-char #\_))
                             ((#\<) (write-string "LT"))
                             ((#\=) (write-string "EQ"))
                             ((#\>) (write-string "GT"))
                             (otherwise (write-char ch)))))))


(defun c-fun-name (cfun)
  (substitute #\_ #\- (string-downcase cfun)))

(defun c-par-name (pname)
  (concatenate 'string "par_" (substitute #\_ #\- (string-downcase pname))))

(defun c-default (pname)
  (concatenate 'string "def_" (substitute #\_ #\- (string-downcase pname))))

(defvar *header* :source)

(defun generate-kernel-function (fname lambda-list &key (values '(t)) cfun)
  (multiple-value-bind (mandatories optionals rest keys allow-other-keys) (parse-lambda-list lambda-list)
    (declare (ignore allow-other-keys))
    (let (;; (nman (length mandatories))
          ;; (nopt (length optionals))
          (keywords (mapcar (lambda (key)
                              (cond
                                ((null key) (error "Invalid keyword: ~S" key))
                                ((symbolp key) (list (keywordize key) nil))
                                ((listp key)
                                 (cond
                                   ((null (first key))
                                    (error "Invalid keyword: ~S" key))
                                   ((symbolp (first key))
                                    (list (keywordize (first key)) (second key)))
                                   ((listp (first key))
                                    (list (first (first key)) (second key)))
                                   (t
                                    (error "Invalid keyword: ~S" key))))
                                (t
                                 (error "Invalid keyword: ~S" key))))
                            keys)))

      
      (case *header*

        (:kernel-functions
         (cond
           ((equal values '())         (write-string "void "))
           ((equal values '(boolean))  (write-string "bool "))
           ((equal values '(uword))    (write-string "uword "))
           ((equal values '(word))     (write-string "word "))
           ((equal values '(t))        (write-string "Object* "))
           (t (error "Multiple-values not supported yet. ~S" values)))
         (format t "~A(" (if cfun (c-fun-name cfun) (c-fun-name fname)))
         (let ((sep ""))
           (dolist (man mandatories)
             (format t "~AObject* ~A" sep (c-par-name man))
             (setf sep ", "))
           (dolist (opt optionals)
             (let ((opt-name  (if (listp opt)
                                  (first opt)
                                  opt)))
               (format t "~AObject* ~A" sep (c-par-name opt-name))
               (setf sep ", ")))
           (when rest
             (format t "~AObject* ~A" sep (c-par-name rest))
             (setf sep ", "))
           (dolist (key keywords)
             (let ((key-name (first key)))
               (format t "~AObject* ~A" sep (c-par-name key-name))
               (setf sep ", ")))
           (when (string= sep "")
             (write-string "void")))
         (write-line ");"))

        (:header
         (format t "void ~A(Object* function,Object* arguments,MultipleValues* results);~%"
                 (k-fun-name fname)))
        
        (otherwise
         (format t "void ~A(Object* function,Object* arguments,MultipleValues* results){~%"
                 (k-fun-name fname))
         (write-line "    push_to_rootset(function);")
         (write-line "    push_to_rootset(arguments);")
         (write-line "    Object* current=arguments;")
         (dolist (man mandatories)
           (format t "    Object* ~A = pop_mandatory_argument(function,&current);~%"
                   (c-par-name man)))
         (dolist (opt optionals)
           (let ((opt-name (if (listp opt)
                               (first opt)
                               opt))
                 (opt-default (if (listp opt)
                                  (second opt)
                                  nil)))
             (cond
               ((keywordp opt-default)
                (format t "    Object* ~A = K(~A);~%"
                        (c-default opt-name) (string opt-default)))
               ((symbolp opt-default)
                (format t "    Object* ~A = variable_value(S(~A));~%"
                        (c-default opt-name)  (string opt-default)))
               ((integerp opt-default)
                (format t "    Object* ~A = I(~A);~%"
                        (c-default opt-name)  opt-default))
               ((and (listp opt-default)
                     (eq 'quote (first opt-default))
                     (symbolp (second opt-default)))
                (format t "    Object* ~A = S(~A);~%"
                        (c-default opt-name)  (string (second opt-default))))
               (t
                (error "Not Implemented Yet: non-nil optional default ~S" opt-default)))
             (format t "    Object* ~A = pop_optional_argument(function,&current,~A);~%"
                     (c-par-name opt-name) (c-default opt-name))))
         (when rest
           (format t "    Object* ~A = current;~%"
                   (c-par-name rest)))
         (dolist (key keywords)
           (let ((key-name (first key))
                 (key-default (second key)))
             (cond
               ((keywordp key-default)
                (format t "    Object* ~A = K(~A);~%"
                        (c-default key-name) (string key-default)))
               ((symbolp key-default)
                (format t "    Object* ~A = variable_value(S(~A));~%"
                        (c-default key-name)  (string key-default)))
               ((integerp key-default)
                (format t "    Object* ~A = I(~A);~%"
                        (c-default key-name)  key-default))
               ((and (listp key-default)
                     (eq 'quote (first key-default))
                     (symbolp (second key-default)))
                (format t "    Object* ~A = S(~A);~%"
                        (c-default key-name)  (string (second key-default))))
               (t
                (error "Not Implemented Yet: non-nil optional default ~S" key-default)))
             (format t "    Object* ~A = pop_key_argument(function,&current,K(~A),~A);~%"
                     (c-par-name key-name) (string key-name) (c-default key-name))))
         (unless (or rest keywords)
           (write-line  "    check_no_more_arguments(function,current);"))
         (cond
           ((equal values '())
            (write-line "    results->count=0;")
            (write-string "    "))
           ((equal values '(boolean))
            (write-line   "    results->count=1;")
            (write-string "    results->values[0] = as_boolean("))
           ((equal values '(uword))
            (write-line   "    results->count=1;")
            (write-string "    results->values[0] = integer_from_uword("))
           ((equal values '(word))
            (write-line   "    results->count=1;")
            (write-string "    results->values[0] = integer_from_word("))
           ((equal values '(t))
            (write-line   "    results->count=1;")
            (write-string "    results->values[0] = "))
           (t (error "Multiple-values not supported yet. ~S" values)))
         (format t "~A(" (if cfun (c-fun-name cfun) (c-fun-name fname)))
         (let ((sep ""))
           (dolist (man mandatories)
             (format t "~A~A" sep (c-par-name man))
             (setf sep ", "))
           (dolist (opt optionals)
             (let ((opt-name  (if (listp opt)
                                  (first opt)
                                  opt)))
               (format t "~A~A" sep (c-par-name opt-name))
               (setf sep ", ")))
           (when rest
             (format t "~A~A" sep (c-par-name rest))
             (setf sep ", "))
           (dolist (key keywords)
             (let ((key-name (first key)))
               (format t "~A~A" sep (c-par-name key-name))
               (setf sep ", "))))
         (if (member values '((boolean) (uword) (word)) :test (function equal))
             (format t "));~%")
             (format t ");~%"))
         (write-line "    delete_from_rootset(arguments);")
         (write-line "    delete_from_rootset(function);")
         (write-line "}")
         (terpri))))))




(defun generate-built-in-functions ()
  (flet ((generate (header)
           (let ((*header* header))

             (generate-kernel-function 'kernel-rootset '())
             

             ;; CLASS
             (generate-kernel-function 'class-new '(name))
             (generate-kernel-function 'class-p '(object) :values '(boolean))
             (generate-kernel-function 'class-name '(class))
             (generate-kernel-function 'class-direct-superclasses '(class))
             (generate-kernel-function 'class-add-superclass '(class superclass) :values '())
             (generate-kernel-function 'class-of '(object))
             (generate-kernel-function 'find-class '(class-name))
             (generate-kernel-function 'subclassp '(class superclass)  :values '(boolean))
             (generate-kernel-function 'typep '(object class) :values '(boolean)) ; not cl:typep
             (generate-kernel-function 'check-type '(object class) :values '())
             (generate-kernel-function 'check-class '(object class) :values '())

             ;; BUILT-IN-CLASS
             (generate-kernel-function 'built-in-class-new '(name))

             ;; CONS
             (generate-kernel-function 'cons '(car cdr))
             (generate-kernel-function 'consp '(object) :values '(boolean) :cfun 'cons-p)

             ;; Those are internal cons functions (less type check) ; we should probably remove them from here.
             (generate-kernel-function 'cons-car '(cell))
             (generate-kernel-function 'cons-cdr '(cell))
             (generate-kernel-function 'cons-rplaca '(cell newcar))
             (generate-kernel-function 'cons-rplacd '(cell newcdr))

             ;; INTEGER
             (generate-kernel-function 'integerp   '(object) :values '(boolean) :cfun 'integer-p)

             ;; FLOAT
             (generate-kernel-function 'floatp     '(object) :values '(boolean) :cfun 'float-p)

             ;; CHARACTER
             (generate-kernel-function 'characterp '(object) :values '(boolean) :cfun 'character-p)
             (generate-kernel-function 'char-code '(character) :cfun 'character-code)
             (generate-kernel-function 'char-name '(character) :cfun 'character-name)
             (generate-kernel-function 'code-char '(code)      :cfun 'code-character)
             (generate-kernel-function 'name-char '(name)      :cfun 'name-character)


             ;; STRING
             (generate-kernel-function 'string-length '(string)  :values '(uword))
             (generate-kernel-function 'string=       '(a b)     :values '(boolean) :cfun 'string-eq)
             (generate-kernel-function 'string-equal  '(a b)     :values '(boolean))
             (generate-kernel-function 'stringp       '(object)  :values '(boolean) :cfun 'string-p)


             ;; SYMBOL

             (generate-kernel-function 'find-symbol '(name obarray))
             (generate-kernel-function 'make-symbol '(name) :cfun 'symbol-new)
             (generate-kernel-function 'intern      '(name) :cfun 'symbol-intern)

             (generate-kernel-function 'symbol-name     '(symbol))
             (generate-kernel-function 'symbol-package  '(symbol))
             (generate-kernel-function 'symbol-value    '(symbol))
             (generate-kernel-function 'symbol-function '(symbol))
             (generate-kernel-function 'symbol-plist    '(symbol))

             (generate-kernel-function 'symbol-set-package  '(symbol new-package))
             (generate-kernel-function 'symbol-set-value    '(symbol new-value))
             (generate-kernel-function 'symbol-set-function '(symbol new-function))
             (generate-kernel-function 'symbol-set-plist    '(symbol new-plist))

             (generate-kernel-function 'symbolp    '(object) :values '(boolean) :cfun 'symbol-p)
             (generate-kernel-function 'boundp     '(symbol) :values '(boolean) :cfun 'symbol-boundp)
             (generate-kernel-function 'fboundp    '(symbol) :values '(boolean) :cfun 'symbol-fboundp)

             (generate-kernel-function 'intern-keyword  '(name) :cfun 'keyword-intern)
             (generate-kernel-function 'keywordp        '(object) :values '(boolean) :cfun 'keyword-p)

             ;; NULL
             (generate-kernel-function 'null        '(object) :values '(boolean) :cfun 'null-p)

             ;; EXTERNAL-FORMAT
             (generate-kernel-function 'normalize-external-format  '(external-format))

             ;; PHYSICAL-PATHNAME 
             (generate-kernel-function 'physical-pathname  '(namestring))

             ;;  FILESTREAM 

             (generate-kernel-function 'make-file-stream '(pathname element-type external-format) :cfun 'file-stream-new)
             (generate-kernel-function 'stream-pathname '(stream))
             (generate-kernel-function 'stream-element-type '(stream))
             (generate-kernel-function 'stream-external-format '(stream))
             (generate-kernel-function 'stream-direction '(stream))
             (generate-kernel-function 'open-stream-p  '(stream) :values '(boolean) :cfun 'stream-open-p)

             (generate-kernel-function 'standard-input-stream   '())
             (generate-kernel-function 'standard-output-stream  '())
             (generate-kernel-function 'standard-error-stream   '())

             ;; CONS ====================

             (generate-kernel-function 'rplaca  '(cell new-car))
             (generate-kernel-function 'rplacd  '(cell new-car))

             (dolist (op '(car cdr caar cadr cdar cddr caaar caadr cadar caddr
                           cdaar cdadr cddar cdddr))
               (generate-kernel-function op '(cons)))
             
             ;; List 

             (generate-kernel-function 'list-length  '(list) :values '(word))

             (generate-kernel-function 'nconc  '(list tail))
             (generate-kernel-function 'last  '(list))
             (generate-kernel-function 'memq  '(item list))

             ;; Plist

             (generate-kernel-function 'getf  '(plist indicator &optional default-value))
             (generate-kernel-function 'putf  '(plist indicator new-value))
             (generate-kernel-function 'remf  '(plist indicator) :values '(boolean))

             ;; DATA ====================

             (generate-kernel-function 'eql '(a b) :values '(boolean))

             ;; FILENAME ====================

             (generate-kernel-function 'pathname  '(pathname-designator))
             (generate-kernel-function 'namestring  '(pathname))

             ;; PRINTER ====================

             (generate-kernel-function 'prin1-to-string '(object))
             (generate-kernel-function 'print '(object &optional (stream *standard-output*)))
             (generate-kernel-function 'prin1 '(object &optional (stream *standard-output*)))
             (generate-kernel-function 'princ '(object &optional (stream *standard-output*)))
             
             (generate-kernel-function 'terpri '(&optional (stream *standard-output*)))

             ;; READER ====================

             (generate-kernel-function 'kernel-read '(&optional (input-stream *standard-input*) (eof-error-p t) eof-value recursive-p))
             (generate-kernel-function 'kernel-read-preserving-whitespace '(&optional (input-stream *standard-input*) (eof-error-p t) eof-value recursive-p))
             (generate-kernel-function 'kernel-read-delimited-list '(character &optional (input-stream *standard-input*) recursive-p))
             (generate-kernel-function 'kernel-read-from-string '(input-string &optional (eof-error-p t) eof-value (start 0) end preserve-whitespace))

             ;; STREAM ====================


             (generate-kernel-function 'open '(filespec
                                               &key (direction :input)
                                               (element-type 'character)
                                               (if-exists :error)
                                               (if-does-not-exist :create)
                                               (external-format :default))
                                       :cfun 'stream-open)

             

             )))

    (with-open-file (*standard-output* "kernel_functions.h"
                                       :direction :output
                                       :if-does-not-exist :create
                                       :if-exists :supersede)
      (write-line "#ifndef kernel_functions_h")
      (write-line "#define kernel_functions_h")
      (write-line "#include \"kernel_types.h\"")
      (terpri)
      (generate :kernel-functions)
      (terpri)
      (write-line "#endif"))

    (with-open-file (*standard-output* "built_in_functions.h"
                                       :direction :output
                                       :if-does-not-exist :create
                                       :if-exists :supersede)
      (write-line "#ifndef built_in_functions_h")
      (write-line "#define built_in_functions_h")
      (write-line "#include \"kernel_types.h\"")
      (write-line "#include \"kernel.h\"")
      (terpri)
      (generate :header)
      (terpri)
      (write-line "#endif"))
    
    
    (with-open-file (*standard-output* "built_in_functions.c"
                                       :direction :output
                                       :if-does-not-exist :create
                                       :if-exists :supersede)
      (write-line "#include \"built_in_functions.h\"")
      (write-line "#include \"kernel_functions.h\"")
      (write-line "#include \"kernel.h\"")
      (write-line "#include \"cons.h\"")
      (write-line "#include \"variable.h\"")
      (write-line "#include \"stream.h\"")
      (write-line "#include \"reader.h\"")
      (write-line "#include \"printer.h\"")
      (terpri)
      (generate :source)
      (terpri)
      (write-line "/**** THE END ****/"))
    (values)))



