;;****************************************************************************
;;FILE:               cl-19-filenames.lisp
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
;; - 19 -  Filenames -
;; -------------------

;; Primitives:


;; System Class PATHNAME
;; 
;; System Class LOGICAL-PATHNAME
;; 
;; Function PATHNAME
;; 
;; Function MAKE-PATHNAME
;; 
;; Function PATHNAMEP
;; 
;; Function PATHNAME-HOST
;; Function PATHNAME-DEVICE
;; Function PATHNAME-DIRECTORY
;; Function PATHNAME-NAME
;; Function PATHNAME-TYPE
;; Function PATHNAME-VERSION
;; 
;; Function LOAD-LOGICAL-PATHNAME-TRANSLATIONS
;; 
;; Accessor LOGICAL-PATHNAME-TRANSLATIONS
;; 
;; Function LOGICAL-PATHNAME
;; 
;; Variable *DEFAULT-PATHNAME-DEFAULTS*
;; 
;; Function NAMESTRING
;; Function FILE-NAMESTRING
;; Function DIRECTORY-NAMESTRING
;; Function HOST-NAMESTRING
;; Function ENOUGH-NAMESTRING
;; 
;; Function PARSE-NAMESTRING
;; 
;; Function WILD-PATHNAME-P
;; 
;; Function PATHNAME-MATCH-P
;; 
;; Function TRANSLATE-LOGICAL-PATHNAME
;; 
;; Function TRANSLATE-PATHNAME
;; 
;; Function MERGE-PATHNAMES
;;



;;;; cl-19-filenames.lisp             -- 2003-12-28 20:44:48 -- pascal   ;;;;
