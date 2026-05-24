;;****************************************************************************
;;FILE:               cl-17-sequences.lisp
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

;; -------------------
;; - 17 -  Sequences -
;; -------------------

;; Primitives:


;; System Class SEQUENCE
;; 
;; Function COPY-SEQ
;; 
;; Accessor ELT
;; 
;; Function FILL
;; 
;; Function MAKE-SEQUENCE
;; 
;; Accessor SUBSEQ
;; 
;; Function MAP
;; 
;; Function MAP-INTO
;; 
;; Function REDUCE
;; 
;; Function COUNT
;; Function COUNT-IF
;; Function COUNT-IF-NOT
;; 
;; Function LENGTH
;; 
;; Function REVERSE
;; Function NREVERSE
;; 
;; Function SORT
;; Function STABLE-SORT
;; 
;; Function FIND
;; Function FIND-IF
;; Function FIND-IF-NOT
;; 
;; Function POSITION
;; Function POSITION-IF
;; Function POSITION-IF-NOT
;; 
;; Function SEARCH
;; 
;; Function MISMATCH
;; 
;; Function REPLACE
;; 
;; Function SUBSTITUTE
;; Function SUBSTITUTE-IF
;; Function SUBSTITUTE-IF-NOT
;; Function NSUBSTITUTE
;; Function NSUBSTITUTE-IF
;; Function NSUBSTITUTE-IF-NOT
;; 
;; Function CONCATENATE
;; 
;; Function MERGE
;; 
;; Function REMOVE
;; Function REMOVE-IF
;; Function REMOVE-IF-NOT
;; Function DELETE
;; Function DELETE-IF
;; Function DELETE-IF-NOT
;; 
;; Function REMOVE-DUPLICATES
;; Function DELETE-DUPLICATES
;;

;;;; cl-17-sequences.lisp             -- 2003-12-28 20:41:16 -- pascal   ;;;;
