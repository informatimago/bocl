;;****************************************************************************
;;FILE:               cl-04-types-and-classes.lisp
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

;; --------------------------
;; - 4 -  Types and Classes -
;; --------------------------

;; Primitives: 


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

;;;; cl-04-types-and-classes.lisp     -- 2003-12-28 20:20:47 -- pascal   ;;;;
