;;****************************************************************************
;;FILE:               cl-23-reader.lisp
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
;;    
;; ----------------
;; - 23 -  Reader -
;; ----------------

;; Primitives:



;; System Class READTABLE
;; 
;; Function COPY-READTABLE
;; 
;; Function MAKE-DISPATCH-MACRO-CHARACTER
;; 
;; Function READ
;; Function READ-PRESERVING-WHITESPACE
;; 
;; Function READ-DELIMITED-LIST
;; 
;; Function READ-FROM-STRING
;; 
;; Accessor READTABLE-CASE
;; 
;; Function READTABLEP
;; 
;; Function SET-DISPATCH-MACRO-CHARACTER
;; Function GET-DISPATCH-MACRO-CHARACTER
;; 
;; Function SET-MACRO-CHARACTER
;; Function GET-MACRO-CHARACTER
;; 
;; Function SET-SYNTAX-FROM-CHAR
;; 
;; Macro WITH-STANDARD-IO-SYNTAX
;; 
;; Variable *READ-BASE*
;; 
;; Variable *READ-DEFAULT-FLOAT-FORMAT*
;; 
;; Variable *READ-EVAL*
;; 
;; Variable *READ-SUPPRESS*
;; 
;; Variable *READTABLE*
;; 
;; Condition Type READER-ERROR
;;


;;;; cl-23-reader.lisp                -- 2003-12-28 20:48:46 -- pascal   ;;;;
