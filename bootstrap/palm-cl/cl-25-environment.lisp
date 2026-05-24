;;****************************************************************************
;;FILE:               cl-25-environment.lisp
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
;; - 25 -  Environment -
;; ---------------------

;; Primitives: SLEEP GET-UNIVERSAL-TIME ROOM LISP-IMPLEMENTATION-TYPE LISP-IMPLEMENTATION-VERSION SHORT-SITE-NAME LONG-SITE-NAME MACHINE-INSTANCE MACHINE-TYPE MACHINE-VERSION SOFTWARE-TYPE SOFTWARE-VERSION USER-HOMEDIR-PATHNAME



;; Function DECODE-UNIVERSAL-TIME                           
;;    decode-universal-time universal-time &optional time-zone
;;      => second, minute, hour, date, month, year, day, daylight-p, zone
;;
;; function ENCODE-UNIVERSAL-TIME                           
;;    encode-universal-time second minute hour date month year 
;;                         &optional time-zone => universal-time
;;
;; Function GET-UNIVERSAL-TIME                              
;;    get-universal-time <no arguments> => universal-time
;;
;; Function GET-DECODED-TIME                                
;;    get-decoded-time <no arguments>
;;    => second, minute, hour, date, month, year, day, daylight-p, zone
;;
;; Function SLEEP                                           
;;    sleep seconds => nil
;;
;; Function APROPOS                                         
;;    apropos string &optional package => <no values>
;;
;; Function APROPOS-LIST                                    
;;    apropos-list string &optional package => symbols
;;
;; Function DESCRIBE

;; Standard Generic Function DESCRIBE-OBJECT

;; Macro TRACE
;; Macro UNTRACE

;; Macro STEP

;; Macro TIME

;; Constant Variable INTERNAL-TIME-UNITS-PER-SECOND         

;; Function GET-INTERNAL-REAL-TIME                          

;; Function GET-INTERNAL-RUN-TIME                           

;; Function DISASSEMBLE                                     
;;    disassemble fn => nil
;;
;; Standard Generic Function DOCUMENTATION
;; Standard Generic Function (SETF DOCUMENTATION)

;; Function ROOM                                            

;; Function ED                                              

;; Function INSPECT

;; Function DRIBBLE

;; Variable -

;; Variable +
;; Variable ++
;; Variable +++

;; Variable *
;; Variable **
;; Variable ***

;; Variable /
;; Variable //
;; Variable ///

;; Function LISP-IMPLEMENTATION-TYPE                        
;; Function LISP-IMPLEMENTATION-VERSION                     

;; Function SHORT-SITE-NAME                                 
;; Function LONG-SITE-NAME                                  

;; Function MACHINE-INSTANCE                                

;; Function MACHINE-TYPE                                    

;; Function MACHINE-VERSION                                 

;; Function SOFTWARE-TYPE
;; Function SOFTWARE-VERSION                 

;; Function USER-HOMEDIR-PATHNAME                           


;;;; cl-25-environment.lisp           -- 2003-12-28 20:50:33 -- pascal   ;;;;
