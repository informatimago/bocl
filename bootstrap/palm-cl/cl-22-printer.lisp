;;****************************************************************************
;;FILE:               cl-22-printer.lisp
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
;; -----------------
;; - 22 -  Printer -
;; -----------------

;; Primitives:



;; Function COPY-PPRINT-DISPATCH                            
;;    copy-pprint-dispatch &optional table => new-table
;;
;; Macro FORMATTER                                          
;;    formatter control-string => function
;;
;; Function PPRINT-DISPATCH                                 
;;    pprint-dispatch object &optional table => function, found-p
;;
;; Local Macro PPRINT-EXIT-IF-LIST-EXHAUSTED                
;;    pprint-exit-if-list-exhausted <no arguments> => nil
;;
;; Function PPRINT-FILL                                     
;;    pprint-fill stream object &optional colon-p at-sign-p => nil
;;
;; Function PPRINT-LINEAR                                   
;;    pprint-linear stream object &optional colon-p at-sign-p => nil
;;
;; Function PPRINT-TABULAR                                  
;;    pprint-tabular stream object &optional colon-p at-sign-p tabsize => nil
;; 
;; Function PPRINT-INDENT                                   
;;    pprint-indent relative-to n &optional stream => nil
;;
;; Macro PPRINT-LOGICAL-BLOCK                               
;;    pprint-logical-block (stream-symbol object &key prefix
;;                   per-line-prefix suffix) declaration* form*
;;
;; Function PPRINT-NEWLINE                                  
;;    pprint-newline kind &optional stream => nil
;;
;; Local Macro PPRINT-POP                                   
;;    pprint-pop <no arguments> => object
;;
;; Function PPRINT-TAB                                      
;;    pprint-tab kind colnum colinc &optional stream => nil
;;
;; Standard Generic Function PRINT-OBJECT
;;    print-object object stream => object
;;
;; Macro PRINT-UNREADABLE-OBJECT                            
;;    print-unreadable-object (object stream &key type identity) form* => nil
;; 
;; Function SET-PPRINT-DISPATCH                             
;;    set-pprint-dispatch type-specifier func &optional priority table => nil
;;
;; Function WRITE
;;    write object &key array base case circle escape gensym 
;;                      length level lines miser-width 
;;                      pprint-dispatch pretty radix readably
;;                      right-margin stream  => object
;;
;; Function PRIN1                                           
;;    prin1 object &optional output-stream => object
;;
;; Function PRINT                                           
;;     print object &optional output-stream => object
;;
;; Function PPRINT                                          
;;      pprint object &optional output-stream => <no values>
;;
;; Function PRINC                                           
;;     princ object &optional output-stream => object
;; 
;; Function WRITE-TO-STRING
;;     write-to-string object &key array base case circle escape gensym 
;;                                 length level lines miser-width
;;                                 pprint-dispatch pretty radix readably
;;                                 right-margin  => string
;;
;; Function PRIN1-TO-STRING
;;    prin1-to-string object => string
;;
;; Function PRINC-TO-STRING
;;    princ-to-string object => string
;;
;; Variable *PRINT-ARRAY*                                   
;; Variable *PRINT-BASE*                                    
;; Variable *PRINT-RADIX*                                   
;; Variable *PRINT-CASE*                                    
;; Variable *PRINT-CIRCLE*                                  
;; Variable *PRINT-ESCAPE*                                  
;; Variable *PRINT-GENSYM*                                  
;; Variable *PRINT-LEVEL*                                   
;; Variable *PRINT-LENGTH*                                  
;; Variable *PRINT-LINES*                                   
;; Variable *PRINT-MISER-WIDTH*                             
;; Variable *PRINT-PPRINT-DISPATCH*                         
;; Variable *PRINT-PRETTY*                                  
;; Variable *PRINT-READABLY*                                
;; Variable *PRINT-RIGHT-MARGIN*                            
;; 
;; Condition Type PRINT-NOT-READABLE
;; 
;; Function PRINT-NOT-READABLE-OBJECT
;; 
;; Function FORMAT                                          
;; 


;;;; cl-22-printer.lisp               -- 2003-12-28 20:48:11 -- pascal   ;;;;
