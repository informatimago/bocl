;;****************************************************************************
;;FILE:               cl-14-conses.lisp
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


;; ----------------
;; - 14 -  Conses -
;; ----------------

;; Primitives: ATOM CONSP CONS RPLACA RPLACD CAR CDR


;; System Class LIST
;; 
;; System Class NULL
;; 
;; System Class CONS
;; 
;; Type ATOM
;; 
;; Function CONS
;; 
;; Function CONSP
;; 
;; Function ATOM
;; 
;; Function RPLACA
;; Function RPLACD
;; 
;; Accessor CAR
;; Accessor CDR
;; Accessor CAAR
;; Accessor CADR
;; Accessor CDAR
;; Accessor CDDR
;; Accessor CAAAR
;; Accessor CAADR
;; Accessor CADAR
;; Accessor CADDR
;; Accessor CDAAR
;; Accessor CDADR
;; Accessor CDDAR
;; Accessor CDDDR
;; Accessor CAAAAR
;; Accessor CAAADR
;; Accessor CAADAR
;; Accessor CAADDR
;; Accessor CADAAR
;; Accessor CADADR
;; Accessor CADDAR
;; Accessor CADDDR
;; Accessor CDAAAR
;; Accessor CDAADR
;; Accessor CDADAR
;; Accessor CDADDR
;; Accessor CDDAAR
;; Accessor CDDADR
;; Accessor CDDDAR
;; Accessor CDDDDR
;; 
;; Function COPY-TREE
;; 
;; Function SUBLIS
;; Function NSUBLIS
;; 
;; Function SUBST
;; Function SUBST-IF
;; Function SUBST-IF-NOT
;; Function NSUBST
;; Function NSUBST-IF
;; Function NSUBST-IF-NOT
;; 
;; Function TREE-EQUAL
;; 
;; Function COPY-LIST
;; 
;; Function LIST
;; Function LIST*
;; 
;; Function LIST-LENGTH
;; 
;; Function LISTP
;; 
;; Function MAKE-LIST
;; 
;; Macro PUSH
;; 
;; Macro POP
;; 
;; Accessor FIRST
;; Accessor SECOND
;; Accessor THIRD
;; Accessor FOURTH
;; Accessor FIFTH
;; Accessor SIXTH
;; Accessor SEVENTH
;; Accessor EIGHTH
;; Accessor NINTH
;; Accessor TENTH
;; 
;; Accessor NTH
;; 
;; Function ENDP
;; 
;; Function NULL
;; 
;; Function NCONC
;; 
;; Function APPEND
;; 
;; Function REVAPPEND
;; Function NRECONC
;; 
;; Function BUTLAST
;; Function NBUTLAST
;; 
;; Function LAST
;; 
;; Function LDIFF
;; Function TAILP
;; 
;; Function NTHCDR
;; 
;; Accessor REST
;; 
;; Function MEMBER
;; Function MEMBER-IF
;; Function MEMBER-IF-NOT
;; 
;; Function MAPC
;; Function MAPCAR
;; Function MAPCAN
;; Function MAPL
;; Function MAPLIST
;; Function MAPCON
;; 
;; Function ACONS
;; 
;; Function ASSOC
;; Function ASSOC-IF
;; Function ASSOC-IF-NOT
;; 
;; Function COPY-ALIST
;; 
;; Function PAIRLIS
;; 
;; Function RASSOC
;; Function RASSOC-IF
;; Function RASSOC-IF-NOT
;; 
;; Function GET-PROPERTIES
;; 
;; Accessor GETF
;; 
;; Macro REMF
;; 
;; Function INTERSECTION
;; Function NINTERSECTION
;; 
;; Function ADJOIN
;; 
;; Macro PUSHNEW
;; 
;; Function SET-DIFFERENCE
;; Function NSET-DIFFERENCE
;; 
;; Function SET-EXCLUSIVE-OR
;; Function NSET-EXCLUSIVE-OR
;; 
;; Function SUBSETP
;; 
;; Function UNION
;; Function NUNION
;; 
 
;;;; cl-14-conses.lisp                -- 2003-12-28 20:34:28 -- pascal   ;;;;
