;;****************************************************************************
;;FILE:               cl-18-hash-tables.lisp
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
;; ---------------------
;; - 18 -  Hash Tables -
;; ---------------------


;; Primitives: MAKE-HASH-TABLE HASH-TABLE-P  HASH-TABLE-COUNT  HASH-TABLE-REHASH-SIZE  HASH-TABLE-REHASH-THRESHOLD  HASH-TABLE-SIZE  HASH-TABLE-TEST  GETHASH  REMHASH  MAPHASH SXHASH




;; System Class HASH-TABLE
;; 
;; Function MAKE-HASH-TABLE
;; 
;; Function HASH-TABLE-P
;; 
;; Function HASH-TABLE-COUNT
;; 
;; Function HASH-TABLE-REHASH-SIZE
;; 
;; Function HASH-TABLE-REHASH-THRESHOLD
;; 
;; Function HASH-TABLE-SIZE
;; 
;; Function HASH-TABLE-TEST
;; 
;; Accessor GETHASH
;; 
;; Function REMHASH
;; 
;; Function MAPHASH
;; 
;; Macro WITH-HASH-TABLE-ITERATOR
;; 
;; Function CLRHASH
;; 
;; Function SXHASH
;;


;;;; cl-18-hash-tables.lisp           -- 2003-12-28 20:44:09 -- pascal   ;;;;
