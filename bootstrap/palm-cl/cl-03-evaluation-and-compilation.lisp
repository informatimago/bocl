;;****************************************************************************
;;FILE:               cl-03-evaluation-and-compilation.lisp
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

;; -----------------------------------
;; - 3 -  Evaluation and Compilation -
;; -----------------------------------

;; Primitives: COMPILER-MACRO-FUNCTION MACRO-FUNCTION


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



;;;; cl-03-evaluation-and-compilation.lisp -- 2004-01-26 14:54:17 -- pascal   ;;;;
