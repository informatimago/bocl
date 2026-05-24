;;****************************************************************************
;;FILE:               cl-10-symbols.lisp
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

;; -----------------
;; - 10 -  Symbols -
;; -----------------

;; Primitives: SYMBOLP MAKE-SYMBOL  SYMBOL-FUNCTION  SYMBOL-NAME  SYMBOL-PACKAGE  SYMBOL-PLIST  SYMBOL-VALUE MAKUNBOUND BOUNDP


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

;;;; cl-10-symbols.lisp               -- 2003-12-28 20:30:00 -- pascal   ;;;;
