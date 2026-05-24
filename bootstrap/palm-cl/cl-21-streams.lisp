;; -----------------
;; - 21 -  Streams -
;; -----------------



#||"

                 +-------------------------------------+
                 |           stream                    |
                 +-------------------------------------+
                 |   kind:{character,binary}           |
                 |   direction:{input,output,io}       |
                 |   state:{open,closed}               |
                 |   interactive:boolean               |
                 |                                     |
                 +-------------------------------------+
                 |                                     |
                 |                                     |
                 |                                     |
                 |                                     |
                 |                                     |
                 |                                     |
                 |                                     |
                 +-------------------------------------+
                      |
                     / \
                    /___\
                      |
                      |         +-----------------------+
                      +---------| file-stream           |
                      |         +-----------------------+
                      |         |   pathname:pathname   |
                      |         +-----------------------+
                      |
                      |         +-----------------------+
                      +---------| broadcast-stream      |
                      |         +-----------------------+
                      |
                      |         +-----------------------+
                      +---------| concatenated-stream   |
                      |         +-----------------------+
                      |
                      |         +-----------------------+
                      +---------| echo-stream           |
                      |         +-----------------------+
                      |
                      |         +-----------------------+
                      +---------| string-stream         |
                      |         +-----------------------+
                      |
                      |         +-----------------------+
                      +---------| synonym-stream        |
                      |         +-----------------------+
                      |
                      |         +-----------------------+
                      +---------| two-way-stream        |
                                +-----------------------+

"||#


;; Primitives: Not determined yet. Cf. underlying I/O?



;; System Class STREAM
;; 
;; System Class BROADCAST-STREAM
;; 
;; System Class CONCATENATED-STREAM
;; 
;; System Class ECHO-STREAM
;; 
;; System Class FILE-STREAM
;; 
;; System Class STRING-STREAM
;; 
;; System Class SYNONYM-STREAM
;; 
;; System Class TWO-WAY-STREAM
;; 
;; Function INPUT-STREAM-P
;; Function OUTPUT-STREAM-P
;; 
;; Function INTERACTIVE-STREAM-P
;; 
;; Function OPEN-STREAM-P
;; 
;; Function STREAM-ELEMENT-TYPE
;; 
;; Function STREAMP
;; 
;; Function READ-BYTE
;; 
;; Function WRITE-BYTE
;; 
;; Function PEEK-CHAR
;; 
;; Function READ-CHAR
;; 
;; Function READ-CHAR-NO-HANG
;; 
;; Function TERPRI
;; Function FRESH-LINE
;; 
;; Function UNREAD-CHAR
;; 
;; Function WRITE-CHAR
;; 
;; Function READ-LINE
;; 
;; Function WRITE-STRING
;; Function WRITE-LINE
;; 
;; Function READ-SEQUENCE
;; 
;; Function WRITE-SEQUENCE
;; 
;; Function FILE-LENGTH
;; 
;; Function FILE-POSITION
;; 
;; Function FILE-STRING-LENGTH
;; 
;; Function OPEN
;; 
;; Function STREAM-EXTERNAL-FORMAT
;; 
;; macro WITH-OPEN-FILE
;; 
;; Function CLOSE
;; 
;; Macro WITH-OPEN-STREAM
;; 
;; Function LISTEN
;; 
;; Function CLEAR-INPUT
;; 
;; Function FINISH-OUTPUT
;; Function FORCE-OUTPUT
;; Function CLEAR-OUTPUT
;; 
;; Function Y-OR-N-P
;; Function YES-OR-NO-P
;; 
;; Function MAKE-SYNONYM-STREAM
;; 
;; Function SYNONYM-STREAM-SYMBOL
;; 
;; Function BROADCAST-STREAM-STREAMS
;; 
;; Function MAKE-BROADCAST-STREAM
;; 
;; Function MAKE-TWO-WAY-STREAM
;; 
;; Function TWO-WAY-STREAM-INPUT-STREAM
;; Function TWO-WAY-STREAM-OUTPUT-STREAM
;; 
;; Function ECHO-STREAM-INPUT-STREAM
;; Function ECHO-STREAM-OUTPUT-STREAM
;; 
;; Function MAKE-ECHO-STREAM
;; 
;; Function CONCATENATED-STREAM-STREAMS
;; 
;; Function MAKE-CONCATENATED-STREAM
;; 
;; Function GET-OUTPUT-STREAM-STRING
;; 
;; Function MAKE-STRING-INPUT-STREAM
;; 
;; Function MAKE-STRING-OUTPUT-STREAM
;; 
;; Macro WITH-INPUT-FROM-STRING
;; 
;; Macro WITH-OUTPUT-TO-STRING
;; 
;; Variable *DEBUG-IO*
;; Variable *ERROR-OUTPUT*
;; Variable *QUERY-IO*
;; Variable *STANDARD-INPUT*
;; Variable *STANDARD-OUTPUT*
;; Variable *TRACE-OUTPUT*
;; 
;; Variable *TERMINAL-IO*
;; 
;; Condition Type STREAM-ERROR
;; 
;; Function STREAM-ERROR-STREAM
;; 
;; Condition Type END-OF-FILE
;;


;;;; cl-21-streams.lisp               -- 2003-12-28 20:46:56 -- pascal   ;;;;
