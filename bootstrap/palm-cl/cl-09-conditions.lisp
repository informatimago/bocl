;;****************************************************************************
;;FILE:               cl-09-conditions.lisp
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

;; -------------------
;; - 9 -  Conditions -
;; -------------------

;; Primitives:  


;; Condition Type CONDITION
;; 
;; Condition Type WARNING
;; 
;; Condition Type STYLE-WARNING
;; 
;; Condition Type SERIOUS-CONDITION
;; 
;; Condition Type ERROR
;; 
;; Condition Type CELL-ERROR
;; 
;; Function CELL-ERROR-NAME
;; 
;; Condition Type PARSE-ERROR
;; 
;; Condition Type STORAGE-CONDITION
;; 
;; Macro ASSERT
;; 
;; Function ERROR
;; 
;; Function CERROR
;; 
;; Macro CHECK-TYPE
;; 
;; Condition Type SIMPLE-ERROR
;; 
;; Function INVALID-METHOD-ERROR
;; 
;; Function METHOD-COMBINATION-ERROR
;; 
;; Function SIGNAL
;; 
;; Condition Type SIMPLE-CONDITION
;; 
;; Function SIMPLE-CONDITION-FORMAT-CONTROL
;; Function SIMPLE-CONDITION-FORMAT-ARGUMENTS
;; 
;; Function WARN
;; 
;; Condition Type SIMPLE-WARNING
;; 
;; Function INVOKE-DEBUGGER
;; 
;; Function BREAK
;; 
;; Variable *DEBUGGER-HOOK*
;; 
;; Variable *BREAK-ON-SIGNALS*
;; 
;; Macro HANDLER-BIND
;; 
;; Macro HANDLER-CASE
;; 
;; Macro IGNORE-ERRORS
;; 
;; Macro DEFINE-CONDITION
;; 
;; Function MAKE-CONDITION
;; 
;; System Class RESTART
;; 
;; Function COMPUTE-RESTARTS
;; 
;; Function FIND-RESTART
;; 
;; Function INVOKE-RESTART
;; 
;; Function INVOKE-RESTART-INTERACTIVELY
;; 
;; Macro RESTART-BIND
;; 
;; Macro RESTART-CASE
;; 
;; Function RESTART-NAME
;; 
;; Macro WITH-CONDITION-RESTARTS
;; 
;; Macro WITH-SIMPLE-RESTART
;; 
;; Restart ABORT
;; 
;; Restart CONTINUE
;; 
;; Restart MUFFLE-WARNING
;; 
;; Restart STORE-VALUE
;; 
;; Restart USE-VALUE
;; 
;; Function ABORT
;; Function CONTINUE
;; Function MUFFLE-WARNING
;; Function STORE-VALUE
;; Function USE-VALUE
;; 
;;



;;;; cl-09-conditions.lisp            -- 2004-01-26 15:03:27 -- pascal   ;;;;
