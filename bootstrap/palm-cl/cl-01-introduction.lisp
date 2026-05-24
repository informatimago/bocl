(defun cl-hyperspec-for-symbol (symbol-name)
  (interactive 
   (list (let ((completion-ignore-case t)
               (symbol-at-point (thing-at-point 'symbol)))
           (completing-read
            "Look up symbol in Common Lisp HyperSpec: "
            common-lisp-hyperspec-symbols #'boundp
            t symbol-at-point
            'common-lisp-hyperspec-history))))
  (mapcar
   (lambda (entry)
     (let ((browse-url-browser-function common-lisp-hyperspec-browser))
       (concat common-lisp-hyperspec-root "Body/"  entry)))
   (let ((symbol (intern-soft (downcase symbol-name)
                              common-lisp-hyperspec-symbols)))
     (if (and symbol (boundp symbol))
       (symbol-value symbol)
       (error "The symbol `%s' is not defined in Common Lisp"
              symbol-name))))
  );;cl-hyperspec-for-symbol


(show (delete-duplicates (cl-hyperspec-for-symbol "cddar") :test (function string=)))

==> ("http://thalassa.informatimago.com/local/lisp/HyperSpec/Body/f_car_c.htm")

(show (shell-command-to-string
 (format "lynx -dump %s" "http://thalassa.informatimago.com/local/lisp/HyperSpec/Body/f_car_c.htm")))




cl-01-introduction.lisp
cl-02-syntax.lisp
cl-03-evaluation-and-compilation.lisp
cl-04-types-and-classes.lisp
cl-05-data-and-control-flow.lisp
cl-06-iteration.lisp
cl-07-objects.lisp
cl-08-structures.lisp
cl-09-conditions.lisp
cl-10-symbols.lisp
cl-11-packages.lisp
cl-12-numbers.lisp
cl-13-characters.lisp
cl-15-arrays.lisp
cl-14-conses.lisp
cl-16-strings.lisp
cl-17-sequences.lisp
cl-18-hash-tables.lisp
cl-19-filenames.lisp
cl-20-files.lisp
cl-21-streams.lisp
cl-22-printer.lisp
cl-23-reader.lisp
cl-24-system-construction.lisp
cl-25-environment.lisp




;; ---------------------
;; - 1 -  Introduction -
;; ---------------------

;; ---------------
;; - 2 -  Syntax -
;; ---------------

;; -----------------------------------
;; - 3 -  Evaluation and Compilation -
;; -----------------------------------


;; Symbol LAMBDA                                            
;; Macro LAMBDA                                    
;;   LAMBDA lambda-list [[declaration* | documentation]] form* => function
;;
;; Function COMPILE                                         
;;   COMPILE name &optional definition => function, warnings-p, failure-p
;;
;; Function EVAL                                            
;;   EVAL form => result*              
;;
;; Special Operator EVAL-WHEN                               
;;    EVAL-WHEN (situation*) form* => result*
;;
;; Special Operator LOAD-TIME-VALUE                         
;;    LOAD-TIME-VALUE form &optional read-only-p => object
;;
;; Special Operator QUOTE                                   
;;    QUOTE object => object
;;
;; Accessor COMPILER-MACRO-FUNCTION                         
;;    COMPILER-MACRO-FUNCTION name &optional environment => function
;;
;; Macro DEFINE-COMPILER-MACRO                              
;;    DEFINE-COMPILER-MACRO name lambda-list [[decl* | doc]] form* => name
;;
;; Macro DEFMACRO                                  
;;    DEFMACRO name lambda-list [[decl* | doc]] form* => name
;;
;; Accessor MACRO-FUNCTION                                  
;;    MACRO-FUNCTION symbol &optional environment => function
;;
;; Function MACROEXPAND
;;    MACROEXPAND form &optional env => expansion, expanded-p
;;                     
;; Function MACROEXPAND-1                                   
;;    MACROEXPAND-1 form &optional env => expansion, expanded-p
;;                     
;;
;; Macro DEFINE-SYMBOL-MACRO                                
;;    DEFINE-SYMBOL-MACRO symbol expansion => symbol
;;                     
;;
;; Special Operator SYMBOL-MACROLET                         
;; symbol-macrolet ((symbol expansion)*) declaration* form* => result*
;;
;; Variable *MACROEXPAND-HOOK* 
;;
;; Function PROCLAIM                                        
;;    PROCLAIM declaration-specifier => implementation-dependent
;;
;; Macro DECLAIM                                            
;;    DECLAIM declaration-specifier* => implementation-dependent
;;
;; Symbol DECLARE                                           
;;    DECLARE declaration-specifier
;;
;; Declaration IGNORE                                       
;;    (ignore {var | (function fn)}*)
;;
;; Declaration IGNORABLE                                    
;;    (ignorable {var | (function fn)}*)
;;                                            
;;
;; Declaration DYNAMIC-EXTENT                               
;;    (dynamic-extent [[var* | (function fn)*]])
;;                                            
;;
;; Declaration TYPE                                         
;;    (type typespec var*)
;;                                            
;;
;; Declaration INLINE                                       
;;    (inline function-name*)
;;                                            
;;
;; Declaration NOTINLINE                                    
;;    (notinline function-name*)
;;                                            
;;
;; Declaration FTYPE                                        
;;    (ftype type function-name*)
;;                                            
;;
;; Declaration DECLARATION                                  
;;    (declaration name*)
;;                                            
;;
;; Declaration OPTIMIZE                                     
;;    (optimize {quality | (quality value)}*)
;;                                            
;;
;; Declaration SPECIAL                          
;;    (special var*)
;;    
;;  
;;
;; Special Operator LOCALLY                                 
;;    LOCALLY declaration* form* => result*
;;
;; Special Operator THE                                     
;;    THE value-type form => result*
;;
;; Function SPECIAL-OPERATOR-P                              
;;    SPECIAL-OPERATOR-P symbol => generalized-boolean
;;
;; Function CONSTANTP                                       
;;    constantp form &optional environment => generalized-boolean
;;



;; --------------------------
;; - 4 -  Types and Classes -
;; --------------------------


;; Type NIL

;; Type BOOLEAN

;; System Class FUNCTION

;; Type COMPILED-FUNCTION

;; System Class GENERIC-FUNCTION

;; System Class STANDARD-GENERIC-FUNCTION

;; System Class CLASS

;; System Class BUILT-IN-CLASS

;; System Class STRUCTURE-CLASS

;; System Class STANDARD-CLASS

;; System Class METHOD

;; System Class STANDARD-METHOD

;; Class STRUCTURE-OBJECT

;; Class STANDARD-OBJECT

;; System Class METHOD-COMBINATION

;; System Class T

;; Type Specifier SATISFIES

;; Type Specifier MEMBER

;; Type Specifier NOT

;; Type Specifier AND

;; Type Specifier OR

;; Type Specifier VALUES

;; Type Specifier EQL

;; Function COERCE

;; Macro DEFTYPE                                            

;; Function SUBTYPEP                                        

;; Function TYPE-OF                                         
;;  
;;          for example: (TYPE-OF (MAKE-ARRAY 2)) ==> 'vector !

;; Function TYPEP                                           

;; Condition Type TYPE-ERROR

;; Function TYPE-ERROR-DATUM                                
;; Function TYPE-ERROR-EXPECTED-TYPE                        

;; Condition Type SIMPLE-TYPE-ERROR



;; ------------------------------
;; - 5 -  Data and Control Flow -
;; ------------------------------


;; Function APPLY                                           
;;    APPLY function &rest args+ => result*
;;
;; Macro DEFUN                                              
;;                                                      
;;    DEFUN function-name lambda-list [[decl* | doc]] form* => function-name
;;                                               
;;
;; Accessor FDEFINITION                                     
;;    FDEFINITION function-name => definition
;;    (setf (fdefinition function-name) new-definition)
;;
;; Function FBOUNDP                                         
;;    FBOUNDP name => generalized-boolean
;;
;; Function FMAKUNBOUND                                     
;;    FMAKUNBOUND name => name
;;
;; Special Operator FLET                                    
;;    FLET ((f-name lambda-list [[loc-decl* | loc-doc]] loc-form*)*) 
;;         decl* form* => result*
;;
;; Special Operator LABELS                                  
;;    LABELS ((f-name lambda-list [[loc-decl* | loc-doc]] loc-form*)*) 
;;         decl* form* => result*
;;
;; Special Operator MACROLET                                
;;    MACROLET ((name lambda-list [[loc-decl* | loc-doc]] local-form*)*) 
;;        decl* form* => result*
;;
;; Function FUNCALL                                         
;;    FUNCALL function &rest args => result*

;; Special Operator FUNCTION                
;;    FUNCTION name => function
;;                                      
;; Function FUNCTION-LAMBDA-EXPRESSION                      
;;    FUNCTION-LAMBDA-EXPRESSION function => lambda-expression, closure-p, name
;;

;; Function FUNCTIONP                                       
;;    FUNCTIONP object => generalized-boolean
;;
;; Function COMPILED-FUNCTION-P                             
;;

;; Constant Variable CALL-ARGUMENTS-LIMIT                   
;; Constant Variable LAMBDA-LIST-KEYWORDS                   
;; Constant Variable LAMBDA-PARAMETERS-LIMIT                
;; 50<=LAMBDA-PARAMETERS-LIMIT<=CALL-ARGUMENTS-LIMIT


;; Macro DEFCONSTANT                                        
;;    DEFCONSTANT name initial-value [documentation] => name
;;
;; Macro DEFPARAMETER                                       
;;    DEFPARAMETER name initial-value [documentation]  => name
;;
;; Macro DEFVAR                                             
;;    DEFVAR name [initial-value [documentation]] => name
;;

;; Macro DESTRUCTURING-BIND                                 
;;    DESTRUCTURING-BIND lambda-list expression declaration* form* => result*
;;
;; Special Operator LET                                     
;;     LET ({var | (var [init-form])}*) declaration* form* => result*
;;
;; Special Operator LET*                                    
;;    LET* ({var | (var [init-form])}*) declaration* form* => result
;;
;; Special Operator PROGV                                   
;;    PROGV symbols values form* => result*
;;
;; Special Form SETQ                                        
;;    SETQ {pair}* => result
;;
;; Macro PSETQ                                              
;;    PSETQ {pair}* => nil
;;
;; Special Operator BLOCK                                   
;;    BLOCK name form* => result*
;;
;; Special Operator CATCH                                   
;;    CATCH tag form* => result*
;;
;; Special Operator GO                                      
;;    GO tag =>|
;;
;; Special Operator RETURN-FROM                             
;;    RETURN-FROM name [result] =>|
;;
;; Macro RETURN                                             
;;    RETURN [result] =>|
;;
;; Special Operator TAGBODY                                 
;;    TAGBODY {tag | statement}* => nil
;;
;; Special Operator THROW                                   
;;    THROW tag result-form =>|
;;
;; Special Operator UNWIND-PROTECT                          
;;    UNWIND-PROTECT protected-form cleanup-form* => result*
;;
;; Constant Variable NIL                                    
;;
;; Function NOT                                             
;;    NOT x => boolean
;;
;; Constant Variable T                                      
;;
;; Function EQ                                              
;;    EQ x y => generalized-boolean
;;
;; Function EQL                                             
;;    EQL x y => generalized-boolean
;;
;; Function EQUAL                                           
;;    EQUAL x y => generalized-boolean
;;
;; Function EQUALP                                          
;;    EQUALP x y => generalized-boolean
;;
;; Function IDENTITY                                        
;;    IDENTITY object => object
;;
;; Function COMPLEMENT                                      
;;    COMPLEMENT function => complement-function
;;
;; Function CONSTANTLY                                      
;;    constantly value => function
;;
;; Function EVERY                                           
;;    EVERY predicate &rest sequences+ => generalized-boolean
;;
;; Function SOME                                            
;;    SOME predicate &rest sequences+ => result
;;
;; Function NOTEVERY                                        
;;    NOTEVERY predicate &rest sequences+ => generalized-boolean 
;;
;; Function NOTANY                                          
;;    NOTANY predicate &rest sequences+ => generalized-boolean
;;
;; Macro AND                                                
;;    AND form* => result*
;;
;; Macro COND                                               
;;    COND {clause}* => result*
;;    clause::= (test-form form*) 
;;
;; Special Operator IF                                      
;;    IF test-form then-form [else-form] => result*
;;
;; Macro OR                                                 
;;    OR form* => results*
;;
;; Macro WHEN                                               
;;    WHEN test-form form* => result*
;;
;; Macro UNLESS                                             
;;    UNLESS test-form form* => result*
;;
;; Macro CASE                                               
;;    CASE keyform {normal-clause}* [otherwise-clause] => result*
;;
;; Macro CCASE                                              
;;    CCASE keyplace {normal-clause}* => result*
;;
;; Macro ECASE                                              
;;    ECASE keyform {normal-clause}* => result*
;;
;; Macro TYPECASE                                           
;;    TYPECASE keyform {normal-clause}* [otherwise-clause] => result*
;;
;; Macro CTYPECASE                                          
;;    CTYPECASE keyplace {normal-clause}* => result*
;;
;; Macro ETYPECASE                                          
;;    ETYPECASE keyform {normal-clause}* => result*
;;
;; Macro MULTIPLE-VALUE-BIND                                
;;    MULTIPLE-VALUE-BIND (var*) values-form declaration* form* => result*
;;
;; Special Operator MULTIPLE-VALUE-CALL
;;    MULTIPLE-VALUE-CALL function-form form* => result*
;;

;;; (DEFMACRO MULTIPLE-VALUE-CALL (FUNCTION-FORM &rest FORM)
;;;   `(APPLY ,FUNCTION-FORM (APPLY (FUNCTION APPEND) (LIST ,@FORM)))
;;;   ) ;;MULTIPLE-VALUE-CALL

;;; (show (macroexpand (quote
;;;  (MULTIPLE-VALUE-CALL (function list) 1 '/ (values 2 3) '/ (values) '/ (floor 2.5))
;;; )))


;;; multiple values:
;;;
;;; return the  first value  normally, to stay  compatible with  emacs and
;;; keep the semantic that by default only the first value is received.
;;;
;;; return the  other values  in an global  array. The assignment  to that
;;; array must  be atomic and  the last operation  of VALUES to  allow for
;;; imbricated VALUES calls.
;;;
;;; For functions returning only one value without VALUES, this array is not
;;; modified! 
;;;  DEFUN could add a (VALUES ...) around the last sexp, 
;;;  but then check RETURN in DEFUN !
;;;
;;; For emacs functions, there is only single values and this array is never
;;; modified!




;; Macro MULTIPLE-VALUE-LIST

;; Special Operator MULTIPLE-VALUE-PROG1

;; Macro MULTIPLE-VALUE-SETQ

;; Accessor VALUES

;; Function VALUES-LIST

;; Constant Variable MULTIPLE-VALUES-LIMIT

;; Macro NTH-VALUE

;; Macro PROG
;; Macro PROG*

;;; (defmacro prog (variable-list &rest decl-and-body)
;;;  `(BLOCK NIL (LET ,variable-list ,declaration (tagbody . body)))


;; Macro PROG1
;; Macro PROG2

;; Special Operator PROGN

;; Macro DEFINE-MODIFY-MACRO

;; Macro DEFSETF

;; Macro DEFINE-SETF-EXPANDER

;; Function GET-SETF-EXPANSION

;; Macro SETF
;; Macro PSETF

;; Macro SHIFTF

;; Macro ROTATEF

;; Condition Type CONTROL-ERROR

;; Condition Type PROGRAM-ERROR

;; Condition Type UNDEFINED-FUNCTION

;;  ABOVE: (defalias 'DEFUN        'defun*) ;; does not implement &key!
;;  (DEFMACRO DEFUN (): Handle &OPTIONAL, &KEY, &REST, etc...

;; ------------------
;; - 6 -  Iteration -
;; ------------------


;; Macro DO
;; Macro DO*
;; 
;; Macro DOTIMES
;; 
;; Macro DOLIST
;; 
;; Macro LOOP
;; 
;; Local Macro LOOP-FINISH
;; 
;;


;; ----------------
;; - 7 -  Objects -
;; ----------------

;; Standard Generic Function FUNCTION-KEYWORDS
;; 
;; Function ENSURE-GENERIC-FUNCTION
;; 
;; Standard Generic Function ALLOCATE-INSTANCE
;; 
;; Standard Generic Function REINITIALIZE-INSTANCE
;; 
;; Standard Generic Function SHARED-INITIALIZE
;; 
;; Standard Generic Function UPDATE-INSTANCE-FOR-DIFFERENT-CLASS
;; 
;; Standard Generic Function UPDATE-INSTANCE-FOR-REDEFINED-CLASS
;; 
;; Standard Generic Function CHANGE-CLASS
;; 
;; Function SLOT-BOUNDP
;; 
;; Function SLOT-EXISTS-P
;; 
;; Function SLOT-MAKUNBOUND
;; 
;; Standard Generic Function SLOT-MISSING
;; 
;; Standard Generic Function SLOT-UNBOUND
;; 
;; Function SLOT-VALUE
;; 
;; Standard Generic Function METHOD-QUALIFIERS
;; 
;; Standard Generic Function NO-APPLICABLE-METHOD
;; 
;; Standard Generic Function NO-NEXT-METHOD
;; 
;; Standard Generic Function REMOVE-METHOD
;; 
;; Standard Generic Function MAKE-INSTANCE
;; 
;; Standard Generic Function MAKE-INSTANCES-OBSOLETE
;; 
;; Standard Generic Function MAKE-LOAD-FORM
;; 
;; Function MAKE-LOAD-FORM-SAVING-SLOTS
;; 
;; Macro WITH-ACCESSORS
;; 
;; Macro WITH-SLOTS
;; 
;; Macro DEFCLASS
;; 
;; Macro DEFGENERIC
;; 
;; Macro DEFMETHOD
;; 
;; Accessor FIND-CLASS
;; 
;; Local Function NEXT-METHOD-P
;; 
;; Local Macro CALL-METHOD
;; Local Macro MAKE-METHOD
;; 
;; Local Function CALL-NEXT-METHOD
;; 
;; Standard Generic Function COMPUTE-APPLICABLE-METHODS
;; 
;; Macro DEFINE-METHOD-COMBINATION
;; 
;; Standard Generic Function FIND-METHOD
;; 
;; Standard Generic Function ADD-METHOD
;; 
;; Standard Generic Function INITIALIZE-INSTANCE
;; 
;; Standard Generic Function CLASS-NAME
;; 
;; Standard Generic Function (SETF CLASS-NAME)
;; 
;; Function CLASS-OF
;; 
;; Condition Type UNBOUND-SLOT
;; 
;; Function UNBOUND-SLOT-INSTANCE
;; 
;;



;; -------------------
;; - 8 -  Structures -
;; -------------------


;; Macro DEFSTRUCT
;; 
;; Function COPY-STRUCTURE
;; 
;; 

;; -------------------
;; - 9 -  Conditions -
;; -------------------

;; Condition Type CONDITION
;; 
;; Condition Type WARNING
;; 
;; Condition Type STYLE-WARNING
;; 
;; Condition Type SERIOUS-CONDITION
;; 
;; Condition Type ERROR
;; 
;; Condition Type CELL-ERROR
;; 
;; Function CELL-ERROR-NAME
;; 
;; Condition Type PARSE-ERROR
;; 
;; Condition Type STORAGE-CONDITION
;; 
;; Macro ASSERT
;; 
;; Function ERROR
;; 
;; Function CERROR
;; 
;; Macro CHECK-TYPE
;; 
;; Condition Type SIMPLE-ERROR
;; 
;; Function INVALID-METHOD-ERROR
;; 
;; Function METHOD-COMBINATION-ERROR
;; 
;; Function SIGNAL
;; 
;; Condition Type SIMPLE-CONDITION
;; 
;; Function SIMPLE-CONDITION-FORMAT-CONTROL
;; Function SIMPLE-CONDITION-FORMAT-ARGUMENTS
;; 
;; Function WARN
;; 
;; Condition Type SIMPLE-WARNING
;; 
;; Function INVOKE-DEBUGGER
;; 
;; Function BREAK
;; 
;; Variable *DEBUGGER-HOOK*
;; 
;; Variable *BREAK-ON-SIGNALS*
;; 
;; Macro HANDLER-BIND
;; 
;; Macro HANDLER-CASE
;; 
;; Macro IGNORE-ERRORS
;; 
;; Macro DEFINE-CONDITION
;; 
;; Function MAKE-CONDITION
;; 
;; System Class RESTART
;; 
;; Function COMPUTE-RESTARTS
;; 
;; Function FIND-RESTART
;; 
;; Function INVOKE-RESTART
;; 
;; Function INVOKE-RESTART-INTERACTIVELY
;; 
;; Macro RESTART-BIND
;; 
;; Macro RESTART-CASE
;; 
;; Function RESTART-NAME
;; 
;; Macro WITH-CONDITION-RESTARTS
;; 
;; Macro WITH-SIMPLE-RESTART
;; 
;; Restart ABORT
;; 
;; Restart CONTINUE
;; 
;; Restart MUFFLE-WARNING
;; 
;; Restart STORE-VALUE
;; 
;; Restart USE-VALUE
;; 
;; Function ABORT
;; Function CONTINUE
;; Function MUFFLE-WARNING
;; Function STORE-VALUE
;; Function USE-VALUE
;; 
;;


;; -----------------
;; - 10 -  Symbols -
;; -----------------

;; System Class SYMBOL
;; 
;; Type KEYWORD
;; 
;; Function SYMBOLP
;; 
;; Function KEYWORDP
;; 
;; Function MAKE-SYMBOL
;; 
;; Function COPY-SYMBOL
;; 
;; Function GENSYM
;; 
;; Variable *GENSYM-COUNTER*
;; 
;; Function GENTEMP
;; 
;; Accessor SYMBOL-FUNCTION
;; 
;; Function SYMBOL-NAME
;; 
;; Function SYMBOL-PACKAGE
;; 
;; Accessor SYMBOL-PLIST
;; 
;; Accessor SYMBOL-VALUE
;; 
;; Accessor GET
;; 
;; Function REMPROP
;; 
;; Function BOUNDP
;; 
;; Function MAKUNBOUND
;; 
;; Function SET
;; 
;; Condition Type UNBOUND-VARIABLE
;; 
;;


;; ------------------
;; - 11 -  Packages -
;; ------------------



;; -----------------
;; - 12 -  Numbers -
;; -----------------

;; System Class NUMBER
;; 
;; System Class COMPLEX
;; 
;; System Class REAL
;; 
;; System Class FLOAT
;; 
;; Type SHORT-FLOAT
;; Type SINGLE-FLOAT
;; Type DOUBLE-FLOAT
;; Type LONG-FLOAT
;; 
;; System Class RATIONAL
;; 
;; System Class RATIO
;; 
;; System Class INTEGER
;; 
;; Type SIGNED-BYTE
;; 
;; Type UNSIGNED-BYTE
;; 
;; Type Specifier MOD
;; 
;; Type BIT
;; 
;; Type FIXNUM
;; 
;; Type BIGNUM
;; 
;; 
;; Function =
;; Function /=
;; Function <
;; Function >
;; Function <=
;; Function >=
;; 
;; 
;; Function MAX
;; Function MIN
;; 
;; 
;; Function MINUSP
;; Function PLUSP
;; 
;; 
;; Function ZEROP
;; 
;; 
;; Function FLOOR
;; Function FFLOOR
;; Function CEILING
;; Function FCEILING
;; Function TRUNCATE
;; Function FTRUNCATE
;; Function ROUND
;; Function FROUND
;; 
;; 
;; Function SIN
;; Function COS
;; Function TAN
;; 
;; 
;; Function ASIN
;; Function ACOS
;; Function ATAN
;; 
;; 
;; Constant Variable PI
;; 
;; 
;; Function SINH
;; Function COSH
;; Function TANH
;; Function ASINH
;; Function ACOSH
;; Function ATANH
;; 
;; 
;; Function *
;; 
;; 
;; Function +
;; 
;; 
;; Function -
;; 
;; 
;; Function /
;; 
;; 
;; Function 1+
;; Function 1-
;; 
;; 
;; Function ABS
;; 
;; 
;; Function EVENP
;; Function ODDP
;; 
;; 
;; Function EXP
;; Function EXPT
;; 
;; 
;; Function GCD
;; 
;; 
;; Macro INCF
;; Macro DECF
;; 
;; 
;; Function LCM
;; 
;; 
;; Function LOG
;; 
;; 
;; Function MOD
;; Function REM
;; 
;; 
;; Function SIGNUM
;; 
;; 
;; Function SQRT
;; Function ISQRT
;; 
;; 
;; System Class RANDOM-STATE
;; 
;; 
;; Function MAKE-RANDOM-STATE
;; 
;; 
;; Function RANDOM
;; 
;; 
;; Function RANDOM-STATE-P
;; 
;; 
;; Variable *RANDOM-STATE*
;; 
;; 
;; Function NUMBERP
;; 
;; 
;; Function CIS
;; 
;; 
;; Function COMPLEX
;; 
;; 
;; Function COMPLEXP
;; 
;; 
;; Function CONJUGATE
;; 
;; 
;; Function PHASE
;; 
;; 
;; Function REALPART
;; Function IMAGPART
;; 
;; 
;; Function UPGRADED-COMPLEX-PART-TYPE
;; 
;; 
;; Function REALP
;; 
;; 
;; Function NUMERATOR
;; Function DENOMINATOR
;; 
;; 
;; Function RATIONAL
;; Function RATIONALIZE
;; 
;; 
;; Function RATIONALP
;; 
;; 
;; Function ASH
;; 
;; 
;; Function INTEGER-LENGTH                                    
;; 
;; 
;; Function INTEGERP
;; 
;; 
;; Function PARSE-INTEGER
;; 
;; 
;; Function BOOLE
;; 
;; 
;; Constant Variable BOOLE-1 
;; Constant Variable BOOLE-2 
;; Constant Variable BOOLE-AND 
;; Constant Variable BOOLE-ANDC1 
;; Constant Variable BOOLE-ANDC2 
;; Constant Variable BOOLE-C1 
;; Constant Variable BOOLE-C2 
;; Constant Variable BOOLE-CLR 
;; Constant Variable BOOLE-EQV 
;; Constant Variable BOOLE-IOR 
;; Constant Variable BOOLE-NAND 
;; Constant Variable BOOLE-NOR 
;; Constant Variable BOOLE-ORC1 
;; Constant Variable BOOLE-ORC2 
;; Constant Variable BOOLE-SET 
;; Constant Variable BOOLE-XOR
;; 
;; 
;; Function LOGAND
;; Function LOGANDC1
;; Function LOGANDC2
;; Function LOGEQV
;; Function LOGIOR
;; Function LOGNAND
;; Function LOGNOR
;; Function LOGNOT
;; Function LOGORC1
;; Function LOGORC2
;; Function LOGXOR
;; 
;; 
;; Function LOGBITP
;; 
;; 
;; Function LOGCOUNT
;; 
;; 
;; Function LOGTEST
;; 
;; 
;; Function BYTE
;; Function BYTE-SIZE
;; Function BYTE-POSITION
;; 
;; 
;; Function DEPOSIT-FIELD
;; 
;; 
;; Function DPB
;; 
;; 
;; Accessor LDB
;; 
;; 
;; Function LDB-TEST
;; 
;; 
;; Accessor MASK-FIELD
;; 
;; 
;; Constant Variable MOST-POSITIVE-FIXNUM                      
;; Constant Variable MOST-NEGATIVE-FIXNUM                      
;; 
;; 
;; Function DECODE-FLOAT
;; Function SCALE-FLOAT
;; Function FLOAT-RADIX
;; Function FLOAT-SIGN
;; Function FLOAT-DIGITS
;; Function FLOAT-PRECISION
;; Function INTEGER-DECODE-FLOAT
;; 
;; 
;; Function FLOAT
;; 
;; 
;; Function FLOATP
;; 
;; 
;; Constant Variable MOST-POSITIVE-SHORT-FLOAT 
;; Constant Variable LEAST-POSITIVE-SHORT-FLOAT 
;; Constant Variable LEAST-POSITIVE-NORMALIZED-SHORT-FLOAT 
;; Constant Variable MOST-POSITIVE-DOUBLE-FLOAT 
;; Constant Variable LEAST-POSITIVE-DOUBLE-FLOAT 
;; Constant Variable LEAST-POSITIVE-NORMALIZED-DOUBLE-FLOAT 
;; Constant Variable MOST-POSITIVE-LONG-FLOAT 
;; Constant Variable LEAST-POSITIVE-LONG-FLOAT 
;; Constant Variable LEAST-POSITIVE-NORMALIZED-LONG-FLOAT 
;; Constant Variable MOST-POSITIVE-SINGLE-FLOAT 
;; Constant Variable LEAST-POSITIVE-SINGLE-FLOAT 
;; Constant Variable LEAST-POSITIVE-NORMALIZED-SINGLE-FLOAT 
;; Constant Variable MOST-NEGATIVE-SHORT-FLOAT 
;; Constant Variable LEAST-NEGATIVE-SHORT-FLOAT 
;; Constant Variable LEAST-NEGATIVE-NORMALIZED-SHORT-FLOAT 
;; Constant Variable MOST-NEGATIVE-SINGLE-FLOAT 
;; Constant Variable LEAST-NEGATIVE-SINGLE-FLOAT 
;; Constant Variable LEAST-NEGATIVE-NORMALIZED-SINGLE-FLOAT 
;; Constant Variable MOST-NEGATIVE-DOUBLE-FLOAT 
;; Constant Variable LEAST-NEGATIVE-DOUBLE-FLOAT 
;; Constant Variable LEAST-NEGATIVE-NORMALIZED-DOUBLE-FLOAT 
;; Constant Variable MOST-NEGATIVE-LONG-FLOAT 
;; Constant Variable LEAST-NEGATIVE-LONG-FLOAT 
;; Constant Variable LEAST-NEGATIVE-NORMALIZED-LONG-FLOAT
;; 
;; 
;; Constant Variable SHORT-FLOAT-EPSILON 
;; Constant Variable SHORT-FLOAT-NEGATIVE-EPSILON 
;; Constant Variable SINGLE-FLOAT-EPSILON 
;; Constant Variable SINGLE-FLOAT-NEGATIVE-EPSILON 
;; Constant Variable DOUBLE-FLOAT-EPSILON 
;; Constant Variable DOUBLE-FLOAT-NEGATIVE-EPSILON 
;; Constant Variable LONG-FLOAT-EPSILON 
;; Constant Variable LONG-FLOAT-NEGATIVE-EPSILON
;; 
;; 
;; Condition Type ARITHMETIC-ERROR
;; 
;; Function ARITHMETIC-ERROR-OPERANDS
;; Function ARITHMETIC-ERROR-OPERATION
;; 
;; 
;; Condition Type DIVISION-BY-ZERO
;; 
;; Condition Type FLOATING-POINT-INVALID-OPERATION
;; 
;; Condition Type FLOATING-POINT-INEXACT
;; 
;; Condition Type FLOATING-POINT-OVERFLOW
;; 
;; Condition Type FLOATING-POINT-UNDERFLOW



;; --------------------
;; - 13 -  Characters -
;; --------------------

;; ----------------
;; - 15 -  Arrays -
;; ----------------

;; TODO: We should store the dimensions of the array somewhere!
;; TODO: We should add the fill-pointer!


;; ----------------
;; - 14 -  Conses -
;; ----------------




;; -----------------
;; - 16 -  Strings -
;; -----------------




;; -------------------
;; - 17 -  Sequences -
;; -------------------

;; ---------------------
;; - 18 -  Hash Tables -
;; ---------------------

;; -------------------
;; - 19 -  Filenames -
;; -------------------

;; --------------------------------
;; - 19.4 - Pathnames - Filenames -
;; --------------------------------

;; ---------------
;; - 20 -  Files -
;; ---------------


;; -----------------
;; - 21 -  Streams -
;; -----------------


;; System Class STREAM
;;
;; System Class BROADCAST-STREAM
;;
;; System Class CONCATENATED-STREAM
;;
;; System Class ECHO-STREAM
;;
;; System Class FILE-STREAM
;;
;; System Class STRING-STREAM
;;
;; System Class SYNONYM-STREAM
;;
;; System Class TWO-WAY-STREAM
;;

#||"

                 +-------------------------------------+
                 |           stream                    |
                 +-------------------------------------+
                 |   kind:{character,binary}           |
                 |   direction:{input,output,io}       |
                 |   state:{open,closed}               |
                 |   interactive:boolean               |
                 |                                     |
                 +-------------------------------------+
                 |                                     |
                 |                                     |
                 |                                     |
                 |                                     |
                 |                                     |
                 |                                     |
                 |                                     |
                 +-------------------------------------+
                      |
                     / \
                    /___\
                      |
                      |         +-----------------------+
                      +---------| file-stream           |
                      |         +-----------------------+
                      |         |   pathname:pathname   |
                      |         +-----------------------+
                      |
                      |         +-----------------------+
                      +---------| broadcast-stream      |
                      |         +-----------------------+
                      |
                      |         +-----------------------+
                      +---------| concatenated-stream   |
                      |         +-----------------------+
                      |
                      |         +-----------------------+
                      +---------| echo-stream           |
                      |         +-----------------------+
                      |
                      |         +-----------------------+
                      +---------| string-stream         |
                      |         +-----------------------+
                      |
                      |         +-----------------------+
                      +---------| synonym-stream        |
                      |         +-----------------------+
                      |
                      |         +-----------------------+
                      +---------| two-way-stream        |
                                +-----------------------+

"||#



;; Function INPUT-STREAM-P
;; Function OUTPUT-STREAM-P
;;
;; Function INTERACTIVE-STREAM-P
;;
;; Function OPEN-STREAM-P
;;
;; Function STREAM-ELEMENT-TYPE
;;
;; Function STREAMP
;;
;; Function READ-BYTE
;;
;; Function WRITE-BYTE
;;
;; Function PEEK-CHAR
;;
;; Function READ-CHAR
;;
;; Function READ-CHAR-NO-HANG
;;
;; Function TERPRI
;; Function FRESH-LINE
;;
;; Function UNREAD-CHAR
;;
;; Function WRITE-CHAR
;;
;; Function READ-LINE
;;
;; Function WRITE-STRING
;; Function WRITE-LINE
;;
;; Function READ-SEQUENCE
;;
;; Function WRITE-SEQUENCE
;;
;; Function FILE-LENGTH
;;
;; Function FILE-POSITION
;;
;; Function FILE-STRING-LENGTH
;;
;; Function OPEN
;;
;; Function STREAM-EXTERNAL-FORMAT
;;
;; Macro WITH-OPEN-FILE
;;
;; Function CLOSE
;;
;; Macro WITH-OPEN-STREAM
;;
;; Function LISTEN
;;
;; Function CLEAR-INPUT
;;
;; Function FINISH-OUTPUT
;; Function FORCE-OUTPUT
;; Function CLEAR-OUTPUT
;;
;; Function Y-OR-N-P
;; Function YES-OR-NO-P
;;
;; Function MAKE-SYNONYM-STREAM
;;
;; Function SYNONYM-STREAM-SYMBOL
;;
;; Function BROADCAST-STREAM-STREAMS
;;
;; Function MAKE-BROADCAST-STREAM
;;
;; Function MAKE-TWO-WAY-STREAM
;;
;; Function TWO-WAY-STREAM-INPUT-STREAM
;; Function TWO-WAY-STREAM-OUTPUT-STREAM
;;
;; Function ECHO-STREAM-INPUT-STREAM
;; Function ECHO-STREAM-OUTPUT-STREAM
;;
;; Function MAKE-ECHO-STREAM
;;
;; Function CONCATENATED-STREAM-STREAMS
;;
;; Function MAKE-CONCATENATED-STREAM
;;
;; Function GET-OUTPUT-STREAM-STRING
;;
;; Function MAKE-STRING-INPUT-STREAM
;;
;; Function MAKE-STRING-OUTPUT-STREAM
;;
;; Macro WITH-INPUT-FROM-STRING
;;
;; Macro WITH-OUTPUT-TO-STRING
;;
;; Variable *DEBUG-IO*
;; Variable *ERROR-OUTPUT*
;; Variable *QUERY-IO*
;; Variable *STANDARD-INPUT*
;; Variable *STANDARD-OUTPUT*
;; Variable *TRACE-OUTPUT*
;;
;; Variable *TERMINAL-IO*
;;
;; Condition Type STREAM-ERROR
;;
;; Function STREAM-ERROR-STREAM
;;
;; Condition Type END-OF-FILE



;; -----------------
;; - 22 -  Printer -
;; -----------------


;; Function COPY-PPRINT-DISPATCH                            
;;    copy-pprint-dispatch &optional table => new-table
;;
;; Macro FORMATTER                                          
;;    formatter control-string => function
;;
;; Function PPRINT-DISPATCH                                 
;;    pprint-dispatch object &optional table => function, found-p
;;
;; Local Macro PPRINT-EXIT-IF-LIST-EXHAUSTED                
;;    pprint-exit-if-list-exhausted <no arguments> => nil
;;
;; Function PPRINT-FILL                                     
;;    pprint-fill stream object &optional colon-p at-sign-p => nil
;;
;; Function PPRINT-LINEAR                                   
;;    pprint-linear stream object &optional colon-p at-sign-p => nil
;;
;; Function PPRINT-TABULAR                                  
;;    pprint-tabular stream object &optional colon-p at-sign-p tabsize => nil
;; 
;; Function PPRINT-INDENT                                   
;;    pprint-indent relative-to n &optional stream => nil
;;
;; Macro PPRINT-LOGICAL-BLOCK                               
;;    pprint-logical-block (stream-symbol object &key prefix
;;                   per-line-prefix suffix) declaration* form*
;;
;; Function PPRINT-NEWLINE                                  
;;    pprint-newline kind &optional stream => nil
;;
;; Local Macro PPRINT-POP                                   
;;    pprint-pop <no arguments> => object
;;
;; Function PPRINT-TAB                                      
;;    pprint-tab kind colnum colinc &optional stream => nil
;;
;; Standard Generic Function PRINT-OBJECT
;;    print-object object stream => object
;;
;; Macro PRINT-UNREADABLE-OBJECT                            
;;    print-unreadable-object (object stream &key type identity) form* => nil
;; 
;; Function SET-PPRINT-DISPATCH                             
;;    set-pprint-dispatch type-specifier func &optional priority table => nil
;;
;; Function WRITE
;;    write object &key array base case circle escape gensym 
;;                      length level lines miser-width 
;;                      pprint-dispatch pretty radix readably
;;                      right-margin stream  => object
;;
;; Function PRIN1                                           
;;    prin1 object &optional output-stream => object
;;
;; Function PRINT                                           
;;     print object &optional output-stream => object
;;
;; Function PPRINT                                          
;;      pprint object &optional output-stream => <no values>
;;
;; Function PRINC                                           
;;     princ object &optional output-stream => object
;; 
;; Function WRITE-TO-STRING
;;     write-to-string object &key array base case circle escape gensym 
;;                                 length level lines miser-width
;;                                 pprint-dispatch pretty radix readably
;;                                 right-margin  => string
;;
;; Function PRIN1-TO-STRING
;;    prin1-to-string object => string
;;
;; Function PRINC-TO-STRING
;;    princ-to-string object => string
;;
;; Variable *PRINT-ARRAY*                                   
;; Variable *PRINT-BASE*                                    
;; Variable *PRINT-RADIX*                                   
;; Variable *PRINT-CASE*                                    
;; Variable *PRINT-CIRCLE*                                  
;; Variable *PRINT-ESCAPE*                                  
;; Variable *PRINT-GENSYM*                                  
;; Variable *PRINT-LEVEL*                                   
;; Variable *PRINT-LENGTH*                                  
;; Variable *PRINT-LINES*                                   
;; Variable *PRINT-MISER-WIDTH*                             
;; Variable *PRINT-PPRINT-DISPATCH*                         
;; Variable *PRINT-PRETTY*                                  
;; Variable *PRINT-READABLY*                                
;; Variable *PRINT-RIGHT-MARGIN*                            
;; 
;; Condition Type PRINT-NOT-READABLE
;; 
;; Function PRINT-NOT-READABLE-OBJECT
;; 
;; Function FORMAT                                          
;; 

;; ----------------
;; - 23 -  Reader -
;; ----------------

;; -----------------------------
;; - 24 -  System Construction -
;; -----------------------------


;; (DEFUN COMPILE-FILE
;; (DEFUN COMPILE-FILE-PATHNAME
;; (DEFUN LOAD
;; (DEFUN WITH-COMPILATION-UNIT

;; (DEFVAR *COMPILE-FILE-PATHNAME*
;; (DEFVAR *COMPILE-FILE-TRUENAME*
;; (DEFVAR *LOAD-PATHNAME*
;; (DEFVAR *LOAD-TRUENAME*
;; (DEFVAR *COMPILE-PRINT*
;; (DEFVAR *COMPILE-VERBOSE*
;; (DEFVAR *LOAD-PRINT*
;; (DEFVAR *LOAD-VERBOSE*
;; (DEFVAR *MODULES*

;; (DEFUN PROVIDE



;; ---------------------
;; - 25 -  Environment -
;; ---------------------


;; Function DECODE-UNIVERSAL-TIME                           
;;    decode-universal-time universal-time &optional time-zone
;;      => second, minute, hour, date, month, year, day, daylight-p, zone
;;
;; Function ENCODE-UNIVERSAL-TIME                           
;;    encode-universal-time second minute hour date month year 
;;                         &optional time-zone => universal-time
;;
;; Function GET-UNIVERSAL-TIME                              
;;    get-universal-time <no arguments> => universal-time
;;
;; Function GET-DECODED-TIME                                
;;    get-decoded-time <no arguments>
;;    => second, minute, hour, date, month, year, day, daylight-p, zone
;;
;; Function SLEEP                                           
;;    sleep seconds => nil
;;
;; Function APROPOS                                         
;;    apropos string &optional package => <no values>
;;
;; Function APROPOS-LIST                                    
;;    apropos-list string &optional package => symbols
;;
;; Function DESCRIBE

;; Standard Generic Function DESCRIBE-OBJECT

;; Macro TRACE
;; Macro UNTRACE

;; Macro STEP

;; Macro TIME

;; Constant Variable INTERNAL-TIME-UNITS-PER-SECOND         

;; Function GET-INTERNAL-REAL-TIME                          

;; Function GET-INTERNAL-RUN-TIME                           

;; Function DISASSEMBLE                                     
;;    disassemble fn => nil
;;
;; Standard Generic Function DOCUMENTATION
;; Standard Generic Function (SETF DOCUMENTATION)

;; Function ROOM                                            

;; Function ED                                              

;; Function INSPECT

;; Function DRIBBLE

;; Variable -

;; Variable +
;; Variable ++
;; Variable +++

;; Variable *
;; Variable **
;; Variable ***

;; Variable /
;; Variable //
;; Variable ///

;; Function LISP-IMPLEMENTATION-TYPE                        
;; Function LISP-IMPLEMENTATION-VERSION                     

;; Function SHORT-SITE-NAME                                 
;; Function LONG-SITE-NAME                                  

;; Function MACHINE-INSTANCE                                

;; Function MACHINE-TYPE                                    

;; Function MACHINE-VERSION                                 

;; Function SOFTWARE-TYPE
;; Function SOFTWARE-VERSION                 

;; Function USER-HOMEDIR-PATHNAME                           



;; ------------------------------------------------------------------------
(DEFPACKAGE "COMMON-LISP"
  (:NICKNAME "CL")
  (:EXPORT 
   &ALLOW-OTHER-KEYS *PRINT-MISER-WIDTH* &AUX *PRINT-PPRINT-DISPATCH*
   &BODY *PRINT-PRETTY* &ENVIRONMENT *PRINT-RADIX* &KEY *PRINT-READABLY*
   &OPTIONAL *PRINT-RIGHT-MARGIN* &REST *QUERY-IO* &WHOLE *RANDOM-STATE*
   * *READ-BASE* ** *READ-DEFAULT-FLOAT-FORMAT* *** *READ-EVAL*
   *BREAK-ON-SIGNALS* *READ-SUPPRESS* *COMPILE-FILE-PATHNAME* *READTABLE*
   *COMPILE-FILE-TRUENAME* *STANDARD-INPUT* *COMPILE-PRINT*
   *STANDARD-OUTPUT* *COMPILE-VERBOSE* *TERMINAL-IO* *DEBUG-IO*
   *TRACE-OUTPUT* *DEBUGGER-HOOK* + *DEFAULT-PATHNAME-DEFAULTS* ++
   *ERROR-OUTPUT* +++ *FEATURES* - *GENSYM-COUNTER* / *LOAD-PATHNAME* //
   *LOAD-PRINT* /// *LOAD-TRUENAME* /= *LOAD-VERBOSE* 1+
   *MACROEXPAND-HOOK* 1- *MODULES* < *PACKAGE* <= *PRINT-ARRAY* =
   *PRINT-BASE* > *PRINT-CASE* >= *PRINT-CIRCLE* ABORT *PRINT-ESCAPE* ABS
   *PRINT-GENSYM* ACONS *PRINT-LENGTH* ACOS *PRINT-LEVEL* ACOSH
   *PRINT-LINES* ADD-METHOD ADJOIN ATOM BOUNDP ADJUST-ARRAY BASE-CHAR
   BREAK ADJUSTABLE-ARRAY-P BASE-STRING BROADCAST-STREAM
   ALLOCATE-INSTANCE BIGNUM BROADCAST-STREAM-STREAMS ALPHA-CHAR-P BIT
   BUILT-IN-CLASS ALPHANUMERICP BIT-AND BUTLAST AND BIT-ANDC1 BYTE APPEND
   BIT-ANDC2 BYTE-POSITION APPLY BIT-EQV BYTE-SIZE APROPOS BIT-IOR CAAAAR
   APROPOS-LIST BIT-NAND CAAADR AREF BIT-NOR CAAAR ARITHMETIC-ERROR
   BIT-NOT CAADAR ARITHMETIC-ERROR-OPERANDS BIT-ORC1 CAADDR
   ARITHMETIC-ERROR-OPERATION BIT-ORC2 CAADR ARRAY BIT-VECTOR CAAR
   ARRAY-DIMENSION BIT-VECTOR-P CADAAR ARRAY-DIMENSION-LIMIT BIT-XOR
   CADADR ARRAY-DIMENSIONS BLOCK CADAR ARRAY-DISPLACEMENT BOOLE CADDAR
   ARRAY-ELEMENT-TYPE BOOLE-1 CADDDR ARRAY-HAS-FILL-POINTER-P BOOLE-2
   CADDR ARRAY-IN-BOUNDS-P BOOLE-AND CADR ARRAY-RANK BOOLE-ANDC1
   CALL-ARGUMENTS-LIMIT ARRAY-RANK-LIMIT BOOLE-ANDC2 CALL-METHOD
   ARRAY-ROW-MAJOR-INDEX BOOLE-C1 CALL-NEXT-METHOD ARRAY-TOTAL-SIZE
   BOOLE-C2 CAR ARRAY-TOTAL-SIZE-LIMIT BOOLE-CLR CASE ARRAYP BOOLE-EQV
   CATCH ASH BOOLE-IOR CCASE ASIN BOOLE-NAND CDAAAR ASINH BOOLE-NOR
   CDAADR ASSERT BOOLE-ORC1 CDAAR ASSOC BOOLE-ORC2 CDADAR ASSOC-IF
   BOOLE-SET CDADDR ASSOC-IF-NOT BOOLE-XOR CDADR ATAN BOOLEAN CDAR ATANH
   BOTH-CASE-P CDDAAR CDDADR CLEAR-INPUT COPY-TREE CDDAR CLEAR-OUTPUT COS
   CDDDAR CLOSE COSH CDDDDR CLRHASH COUNT CDDDR CODE-CHAR COUNT-IF CDDR
   COERCE COUNT-IF-NOT CDR COMPILATION-SPEED CTYPECASE CEILING COMPILE
   DEBUG CELL-ERROR COMPILE-FILE DECF CELL-ERROR-NAME
   COMPILE-FILE-PATHNAME DECLAIM CERROR COMPILED-FUNCTION DECLARATION
   CHANGE-CLASS COMPILED-FUNCTION-P DECLARE CHAR COMPILER-MACRO
   DECODE-FLOAT CHAR-CODE COMPILER-MACRO-FUNCTION DECODE-UNIVERSAL-TIME
   CHAR-CODE-LIMIT COMPLEMENT DEFCLASS CHAR-DOWNCASE COMPLEX DEFCONSTANT
   CHAR-EQUAL COMPLEXP DEFGENERIC CHAR-GREATERP
   COMPUTE-APPLICABLE-METHODS DEFINE-COMPILER-MACRO CHAR-INT
   COMPUTE-RESTARTS DEFINE-CONDITION CHAR-LESSP CONCATENATE
   DEFINE-METHOD-COMBINATION CHAR-NAME CONCATENATED-STREAM
   DEFINE-MODIFY-MACRO CHAR-NOT-EQUAL CONCATENATED-STREAM-STREAMS
   DEFINE-SETF-EXPANDER CHAR-NOT-GREATERP COND DEFINE-SYMBOL-MACRO
   CHAR-NOT-LESSP CONDITION DEFMACRO CHAR-UPCASE CONJUGATE DEFMETHOD
   CHAR/= CONS DEFPACKAGE CHAR< CONSP DEFPARAMETER CHAR<= CONSTANTLY
   DEFSETF CHAR= CONSTANTP DEFSTRUCT CHAR> CONTINUE DEFTYPE CHAR>=
   CONTROL-ERROR DEFUN CHARACTER COPY-ALIST DEFVAR CHARACTERP COPY-LIST
   DELETE CHECK-TYPE COPY-PPRINT-DISPATCH DELETE-DUPLICATES CIS
   COPY-READTABLE DELETE-FILE CLASS COPY-SEQ DELETE-IF CLASS-NAME
   COPY-STRUCTURE DELETE-IF-NOT CLASS-OF COPY-SYMBOL DELETE-PACKAGE
   DENOMINATOR EQ DEPOSIT-FIELD EQL DESCRIBE EQUAL DESCRIBE-OBJECT EQUALP
   DESTRUCTURING-BIND ERROR DIGIT-CHAR ETYPECASE DIGIT-CHAR-P EVAL
   DIRECTORY EVAL-WHEN DIRECTORY-NAMESTRING EVENP DISASSEMBLE EVERY
   DIVISION-BY-ZERO EXP DO EXPORT DO* EXPT DO-ALL-SYMBOLS EXTENDED-CHAR
   DO-EXTERNAL-SYMBOLS FBOUNDP DO-SYMBOLS FCEILING DOCUMENTATION
   FDEFINITION DOLIST FFLOOR DOTIMES FIFTH DOUBLE-FLOAT FILE-AUTHOR
   DOUBLE-FLOAT-EPSILON FILE-ERROR DOUBLE-FLOAT-NEGATIVE-EPSILON
   FILE-ERROR-PATHNAME DPB FILE-LENGTH DRIBBLE FILE-NAMESTRING
   DYNAMIC-EXTENT FILE-POSITION ECASE FILE-STREAM ECHO-STREAM
   FILE-STRING-LENGTH ECHO-STREAM-INPUT-STREAM FILE-WRITE-DATE
   ECHO-STREAM-OUTPUT-STREAM FILL ED FILL-POINTER EIGHTH FIND ELT
   FIND-ALL-SYMBOLS ENCODE-UNIVERSAL-TIME FIND-CLASS END-OF-FILE FIND-IF
   ENDP FIND-IF-NOT ENOUGH-NAMESTRING FIND-METHOD
   ENSURE-DIRECTORIES-EXIST FIND-PACKAGE ENSURE-GENERIC-FUNCTION
   FIND-RESTART FIND-SYMBOL GET-INTERNAL-RUN-TIME FINISH-OUTPUT
   GET-MACRO-CHARACTER FIRST GET-OUTPUT-STREAM-STRING FIXNUM
   GET-PROPERTIES FLET GET-SETF-EXPANSION FLOAT GET-UNIVERSAL-TIME
   FLOAT-DIGITS GETF FLOAT-PRECISION GETHASH FLOAT-RADIX GO FLOAT-SIGN
   GRAPHIC-CHAR-P FLOATING-POINT-INEXACT HANDLER-BIND
   FLOATING-POINT-INVALID-OPERATION HANDLER-CASE FLOATING-POINT-OVERFLOW
   HASH-TABLE FLOATING-POINT-UNDERFLOW HASH-TABLE-COUNT FLOATP
   HASH-TABLE-P FLOOR HASH-TABLE-REHASH-SIZE FMAKUNBOUND
   HASH-TABLE-REHASH-THRESHOLD FORCE-OUTPUT HASH-TABLE-SIZE FORMAT
   HASH-TABLE-TEST FORMATTER HOST-NAMESTRING FOURTH IDENTITY FRESH-LINE
   IF FROUND IGNORABLE FTRUNCATE IGNORE FTYPE IGNORE-ERRORS FUNCALL
   IMAGPART FUNCTION IMPORT FUNCTION-KEYWORDS IN-PACKAGE
   FUNCTION-LAMBDA-EXPRESSION INCF FUNCTIONP INITIALIZE-INSTANCE GCD
   INLINE GENERIC-FUNCTION INPUT-STREAM-P GENSYM INSPECT GENTEMP INTEGER
   GET INTEGER-DECODE-FLOAT GET-DECODED-TIME INTEGER-LENGTH
   GET-DISPATCH-MACRO-CHARACTER INTEGERP GET-INTERNAL-REAL-TIME
   INTERACTIVE-STREAM-P INTERN LISP-IMPLEMENTATION-TYPE
   INTERNAL-TIME-UNITS-PER-SECOND LISP-IMPLEMENTATION-VERSION
   INTERSECTION LIST INVALID-METHOD-ERROR LIST* INVOKE-DEBUGGER
   LIST-ALL-PACKAGES INVOKE-RESTART LIST-LENGTH
   INVOKE-RESTART-INTERACTIVELY LISTEN ISQRT LISTP KEYWORD LOAD KEYWORDP
   LOAD-LOGICAL-PATHNAME-TRANSLATIONS LABELS LOAD-TIME-VALUE LAMBDA
   LOCALLY LAMBDA-LIST-KEYWORDS LOG LAMBDA-PARAMETERS-LIMIT LOGAND LAST
   LOGANDC1 LCM LOGANDC2 LDB LOGBITP LDB-TEST LOGCOUNT LDIFF LOGEQV
   LEAST-NEGATIVE-DOUBLE-FLOAT LOGICAL-PATHNAME LEAST-NEGATIVE-LONG-FLOAT
   LOGICAL-PATHNAME-TRANSLATIONS LEAST-NEGATIVE-NORMALIZED-DOUBLE-FLOAT
   LOGIOR LEAST-NEGATIVE-NORMALIZED-LONG-FLOAT LOGNAND
   LEAST-NEGATIVE-NORMALIZED-SHORT-FLOAT LOGNOR
   LEAST-NEGATIVE-NORMALIZED-SINGLE-FLOAT LOGNOT
   LEAST-NEGATIVE-SHORT-FLOAT LOGORC1 LEAST-NEGATIVE-SINGLE-FLOAT LOGORC2
   LEAST-POSITIVE-DOUBLE-FLOAT LOGTEST LEAST-POSITIVE-LONG-FLOAT LOGXOR
   LEAST-POSITIVE-NORMALIZED-DOUBLE-FLOAT LONG-FLOAT
   LEAST-POSITIVE-NORMALIZED-LONG-FLOAT LONG-FLOAT-EPSILON
   LEAST-POSITIVE-NORMALIZED-SHORT-FLOAT LONG-FLOAT-NEGATIVE-EPSILON
   LEAST-POSITIVE-NORMALIZED-SINGLE-FLOAT LONG-SITE-NAME
   LEAST-POSITIVE-SHORT-FLOAT LOOP LEAST-POSITIVE-SINGLE-FLOAT
   LOOP-FINISH LENGTH LOWER-CASE-P LET MACHINE-INSTANCE LET* MACHINE-TYPE
   MACHINE-VERSION MASK-FIELD MACRO-FUNCTION MAX MACROEXPAND MEMBER
   MACROEXPAND-1 MEMBER-IF MACROLET MEMBER-IF-NOT MAKE-ARRAY MERGE
   MAKE-BROADCAST-STREAM MERGE-PATHNAMES MAKE-CONCATENATED-STREAM METHOD
   MAKE-CONDITION METHOD-COMBINATION MAKE-DISPATCH-MACRO-CHARACTER
   METHOD-COMBINATION-ERROR MAKE-ECHO-STREAM METHOD-QUALIFIERS
   MAKE-HASH-TABLE MIN MAKE-INSTANCE MINUSP MAKE-INSTANCES-OBSOLETE
   MISMATCH MAKE-LIST MOD MAKE-LOAD-FORM MOST-NEGATIVE-DOUBLE-FLOAT
   MAKE-LOAD-FORM-SAVING-SLOTS MOST-NEGATIVE-FIXNUM MAKE-METHOD
   MOST-NEGATIVE-LONG-FLOAT MAKE-PACKAGE MOST-NEGATIVE-SHORT-FLOAT
   MAKE-PATHNAME MOST-NEGATIVE-SINGLE-FLOAT MAKE-RANDOM-STATE
   MOST-POSITIVE-DOUBLE-FLOAT MAKE-SEQUENCE MOST-POSITIVE-FIXNUM
   MAKE-STRING MOST-POSITIVE-LONG-FLOAT MAKE-STRING-INPUT-STREAM
   MOST-POSITIVE-SHORT-FLOAT MAKE-STRING-OUTPUT-STREAM
   MOST-POSITIVE-SINGLE-FLOAT MAKE-SYMBOL MUFFLE-WARNING
   MAKE-SYNONYM-STREAM MULTIPLE-VALUE-BIND MAKE-TWO-WAY-STREAM
   MULTIPLE-VALUE-CALL MAKUNBOUND MULTIPLE-VALUE-LIST MAP
   MULTIPLE-VALUE-PROG1 MAP-INTO MULTIPLE-VALUE-SETQ MAPC
   MULTIPLE-VALUES-LIMIT MAPCAN NAME-CHAR MAPCAR NAMESTRING MAPCON
   NBUTLAST MAPHASH NCONC MAPL NEXT-METHOD-P MAPLIST NIL NINTERSECTION
   PACKAGE-ERROR NINTH PACKAGE-ERROR-PACKAGE NO-APPLICABLE-METHOD
   PACKAGE-NAME NO-NEXT-METHOD PACKAGE-NICKNAMES NOT
   PACKAGE-SHADOWING-SYMBOLS NOTANY PACKAGE-USE-LIST NOTEVERY
   PACKAGE-USED-BY-LIST NOTINLINE PACKAGEP NRECONC PAIRLIS NREVERSE
   PARSE-ERROR NSET-DIFFERENCE PARSE-INTEGER NSET-EXCLUSIVE-OR
   PARSE-NAMESTRING NSTRING-CAPITALIZE PATHNAME NSTRING-DOWNCASE
   PATHNAME-DEVICE NSTRING-UPCASE PATHNAME-DIRECTORY NSUBLIS
   PATHNAME-HOST NSUBST PATHNAME-MATCH-P NSUBST-IF PATHNAME-NAME
   NSUBST-IF-NOT PATHNAME-TYPE NSUBSTITUTE PATHNAME-VERSION
   NSUBSTITUTE-IF PATHNAMEP NSUBSTITUTE-IF-NOT PEEK-CHAR NTH PHASE
   NTH-VALUE PI NTHCDR PLUSP NULL POP NUMBER POSITION NUMBERP POSITION-IF
   NUMERATOR POSITION-IF-NOT NUNION PPRINT ODDP PPRINT-DISPATCH OPEN
   PPRINT-EXIT-IF-LIST-EXHAUSTED OPEN-STREAM-P PPRINT-FILL OPTIMIZE
   PPRINT-INDENT OR PPRINT-LINEAR OTHERWISE PPRINT-LOGICAL-BLOCK
   OUTPUT-STREAM-P PPRINT-NEWLINE PACKAGE PPRINT-POP PPRINT-TAB READ-CHAR
   PPRINT-TABULAR READ-CHAR-NO-HANG PRIN1 READ-DELIMITED-LIST
   PRIN1-TO-STRING READ-FROM-STRING PRINC READ-LINE PRINC-TO-STRING
   READ-PRESERVING-WHITESPACE PRINT READ-SEQUENCE PRINT-NOT-READABLE
   READER-ERROR PRINT-NOT-READABLE-OBJECT READTABLE PRINT-OBJECT
   READTABLE-CASE PRINT-UNREADABLE-OBJECT READTABLEP PROBE-FILE REAL
   PROCLAIM REALP PROG REALPART PROG* REDUCE PROG1 REINITIALIZE-INSTANCE
   PROG2 REM PROGN REMF PROGRAM-ERROR REMHASH PROGV REMOVE PROVIDE
   REMOVE-DUPLICATES PSETF REMOVE-IF PSETQ REMOVE-IF-NOT PUSH
   REMOVE-METHOD PUSHNEW REMPROP QUOTE RENAME-FILE RANDOM RENAME-PACKAGE
   RANDOM-STATE REPLACE RANDOM-STATE-P REQUIRE RASSOC REST RASSOC-IF
   RESTART RASSOC-IF-NOT RESTART-BIND RATIO RESTART-CASE RATIONAL
   RESTART-NAME RATIONALIZE RETURN RATIONALP RETURN-FROM READ REVAPPEND
   READ-BYTE REVERSE ROOM SIMPLE-BIT-VECTOR ROTATEF SIMPLE-BIT-VECTOR-P
   ROUND SIMPLE-CONDITION ROW-MAJOR-AREF
   SIMPLE-CONDITION-FORMAT-ARGUMENTS RPLACA
   SIMPLE-CONDITION-FORMAT-CONTROL RPLACD SIMPLE-ERROR SAFETY
   SIMPLE-STRING SATISFIES SIMPLE-STRING-P SBIT SIMPLE-TYPE-ERROR
   SCALE-FLOAT SIMPLE-VECTOR SCHAR SIMPLE-VECTOR-P SEARCH SIMPLE-WARNING
   SECOND SIN SEQUENCE SINGLE-FLOAT SERIOUS-CONDITION
   SINGLE-FLOAT-EPSILON SET SINGLE-FLOAT-NEGATIVE-EPSILON SET-DIFFERENCE
   SINH SET-DISPATCH-MACRO-CHARACTER SIXTH SET-EXCLUSIVE-OR SLEEP
   SET-MACRO-CHARACTER SLOT-BOUNDP SET-PPRINT-DISPATCH SLOT-EXISTS-P
   SET-SYNTAX-FROM-CHAR SLOT-MAKUNBOUND SETF SLOT-MISSING SETQ
   SLOT-UNBOUND SEVENTH SLOT-VALUE SHADOW SOFTWARE-TYPE SHADOWING-IMPORT
   SOFTWARE-VERSION SHARED-INITIALIZE SOME SHIFTF SORT SHORT-FLOAT SPACE
   SHORT-FLOAT-EPSILON SPECIAL SHORT-FLOAT-NEGATIVE-EPSILON
   SPECIAL-OPERATOR-P SHORT-SITE-NAME SPEED SIGNAL SQRT SIGNED-BYTE
   STABLE-SORT SIGNUM STANDARD SIMPLE-ARRAY STANDARD-CHAR
   SIMPLE-BASE-STRING STANDARD-CHAR-P STANDARD-CLASS SUBLIS
   STANDARD-GENERIC-FUNCTION SUBSEQ STANDARD-METHOD SUBSETP
   STANDARD-OBJECT SUBST STEP SUBST-IF STORAGE-CONDITION SUBST-IF-NOT
   STORE-VALUE SUBSTITUTE STREAM SUBSTITUTE-IF STREAM-ELEMENT-TYPE
   SUBSTITUTE-IF-NOT STREAM-ERROR SUBTYPEP STREAM-ERROR-STREAM SVREF
   STREAM-EXTERNAL-FORMAT SXHASH STREAMP SYMBOL STRING SYMBOL-FUNCTION
   STRING-CAPITALIZE SYMBOL-MACROLET STRING-DOWNCASE SYMBOL-NAME
   STRING-EQUAL SYMBOL-PACKAGE STRING-GREATERP SYMBOL-PLIST
   STRING-LEFT-TRIM SYMBOL-VALUE STRING-LESSP SYMBOLP STRING-NOT-EQUAL
   SYNONYM-STREAM STRING-NOT-GREATERP SYNONYM-STREAM-SYMBOL
   STRING-NOT-LESSP T STRING-RIGHT-TRIM TAGBODY STRING-STREAM TAILP
   STRING-TRIM TAN STRING-UPCASE TANH STRING/= TENTH STRING< TERPRI
   STRING<= THE STRING= THIRD STRING> THROW STRING>= TIME STRINGP TRACE
   STRUCTURE TRANSLATE-LOGICAL-PATHNAME STRUCTURE-CLASS
   TRANSLATE-PATHNAME STRUCTURE-OBJECT TREE-EQUAL STYLE-WARNING TRUENAME 
   TRUNCATE VALUES-LIST TWO-WAY-STREAM VARIABLE
   TWO-WAY-STREAM-INPUT-STREAM VECTOR TWO-WAY-STREAM-OUTPUT-STREAM
   VECTOR-POP TYPE VECTOR-PUSH TYPE-ERROR VECTOR-PUSH-EXTEND
   TYPE-ERROR-DATUM VECTORP TYPE-ERROR-EXPECTED-TYPE WARN TYPE-OF WARNING
   TYPECASE WHEN TYPEP WILD-PATHNAME-P UNBOUND-SLOT WITH-ACCESSORS
   UNBOUND-SLOT-INSTANCE WITH-COMPILATION-UNIT UNBOUND-VARIABLE
   WITH-CONDITION-RESTARTS UNDEFINED-FUNCTION WITH-HASH-TABLE-ITERATOR
   UNEXPORT WITH-INPUT-FROM-STRING UNINTERN WITH-OPEN-FILE UNION
   WITH-OPEN-STREAM UNLESS WITH-OUTPUT-TO-STRING UNREAD-CHAR
   WITH-PACKAGE-ITERATOR UNSIGNED-BYTE WITH-SIMPLE-RESTART UNTRACE
   WITH-SLOTS UNUSE-PACKAGE WITH-STANDARD-IO-SYNTAX UNWIND-PROTECT WRITE
   UPDATE-INSTANCE-FOR-DIFFERENT-CLASS WRITE-BYTE
   UPDATE-INSTANCE-FOR-REDEFINED-CLASS WRITE-CHAR
   UPGRADED-ARRAY-ELEMENT-TYPE WRITE-LINE UPGRADED-COMPLEX-PART-TYPE
   WRITE-SEQUENCE UPPER-CASE-P WRITE-STRING USE-PACKAGE WRITE-TO-STRING
   USE-VALUE Y-OR-N-P USER-HOMEDIR-PATHNAME YES-OR-NO-P VALUES ZEROP
   )
  (:USE "SYSTEM")
  );;COMMON-LISP




;;;; cl-01-introduction.lisp          -- 2004-01-26 15:04:08 -- pascal   ;;;;
