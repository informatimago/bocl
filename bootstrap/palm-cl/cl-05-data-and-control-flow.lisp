;;****************************************************************************
;;FILE:               cl-05-data-and-control-flow.lisp
;;LANGUAGE:           Common-Lisp
;;SYSTEM:             Common-Lisp
;;USER-INTERFACE:     NONE
;;DESCRIPTION
;;    
;;    XXX
;;    
;;AUTHORS
;;    <PJB> Pascal Bourguignon <pjb@informatimago.com>
;;MODIFICATIONS
;;    2003-12-28 <PJB> Created.
;;BUGS
;;LEGAL
;;    Proprietary
;;    
;;    Copyright Pascal Bourguignon 2003 - 2003
;;    
;;    All Rights Reserved.
;;    
;;    This program and its documentation constitute intellectual property 
;;    of Pascal J. Bourguignon and is protected by the copyright laws of 
;;    the European Union and other countries.
;;****************************************************************************


;; ------------------------------
;; - 5 -  Data and Control Flow -
;; ------------------------------

;; Primitives: NIL T APPLY FUNCTION-LAMBDA-EXPRESSION EQ EQL VALUES 


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
;;    LET ({var | (var [init-form])}*) declaration* form* => result*
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



;;;; cl-05-data-and-control-flow.lisp --                     --          ;;;;
