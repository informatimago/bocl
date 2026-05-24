;;****************************************************************************
;;FILE:               cl-07-objects.lisp
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
;; - 7 -  Objects -
;; ----------------


;; Primitives:



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




;;;; cl-07-objects.lisp               -- 2004-01-26 14:56:11 -- pascal   ;;;;
