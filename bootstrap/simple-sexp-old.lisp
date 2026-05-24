
(defmacro concat (string-var &rest string-expressions)
  (let ((out (gensym)))
    `(setf ,string-var (with-output-to-string (,out)
                         (princ ,string-var ,out)
                         ,@(mapcar (lambda (expr) `(princ ,expr ,out))
                                   string-expressions)))))

(defun print-to-str (obj output)
  (let ((stack (list 'element obj)))
    (loop
       :while stack
       :do (let ((kind    (pop stack))
                 (cur-obj (pop stack)))
             (typecase cur-obj
               (cons
                (if (eql kind 'element)
                    (progn
                      (concat output "(")
                     (push (cdr cur-obj) stack)
                      (push 'list         stack)
                      (push (car cur-obj) stack)
                      (push 'element stack))
                    (progn
                      (concat output " ")
                      (push (cdr cur-obj) stack)
                      (push 'list         stack)
                      (push (car cur-obj) stack)
                      (push 'element stack))))
               (null
                (if (eql kind 'element)
                    (concat output "NIL")
                    (concat output ")")))
               (t
                (if (eql kind 'element)
                    (concat output (prin1-to-string cur-obj))
                    (concat output " . "  (prin1-to-string cur-obj) ")"))))))
    output))


;; comment ::= ';' line-char '\n' .
;; sexp ::= atom | '(' ')' | '(' sexp+ [ '.' sexp ] ')' | '\'''(' sexp ')'
;; atom ::= [-+]digit+ | [-+]digit+.digit+[[eE][-+]digit+] | '"' string-char* '"' | symbol-char(symbol-char|digit)* 

(setf text "(attributes (lock \"toto\"))")

(defun make-str (str) (copy-seq   str))
(defun make-sym (str) (intern     str))

(defvar *scanner-text*)
(defvar *scanner-position*)
(defun init-scanner (text)
  (setf *scanner-text* text
        *scanner-position* 0))
(defun scanner-eot ()
  (<= (length *scanner-text*) *scanner-position*))
(defun next-char ()
  (when (< *scanner-position* (length *scanner-text*))
    (prog1 (aref *scanner-text* *scanner-position*)
      (incf *scanner-position*))))
(defun top (stack) (car stack))
(defun parse-token (kind token)
  (case kind
    ((symbol) (values 'symbol (make-sym token)))
    (otherwise
     (let ((value  (parse-integer token :junk-allowed t)))
       (if (string= token (prin1-to-string value))
           (values 'integer value)
           (values 'symbol  token))))))

(defun read-from-str (text)
  (init-scanner text)
  (let (ch type value
           (stack '(parse done eot nil)))
    (macrolet ((spop ()
                 (let ((val (gensym)))
                   `(let ((,val (pop stack)))
                      (format t "(pop ~A) --> ~S~%~A = ~S~%~%"
                              'stack ,val 'stack stack)
                      ,val)))
               (spush (expr)
                 (let ((vexpr (gensym)))
                   `(let ((,vexpr ,expr))
                      (format t "~A = ~S~%(push  ~S = ~S  ~A)~%"
                              'stack stack ',expr  ,vexpr 'stack)
                      (prog1 (push ,vexpr stack)
                        (format t  "~A = ~S~2%"  'stack stack))))))
      (loop
         :for state =  (spop)
         :until (eql state 'done)
         :do (ecase state
               ((parse)
                (progn
                  (loop
                     :named skip-spaces
                     :until (scanner-eot)
                     :do (setf ch (next-char))
                     :while (or (char= #\space ch) (char= #\newline ch)))
                  (format t "~% ch = #\\~C ~%" ch)
                  (if (scanner-eot)
                      (progn
                        (spush nil)
                        (spush 'eot)
                        (spush 'done))
                      (case ch
                        ((#\')
                         (spush 'quote)
                         (spush 'parse))
                        ((#\")
                         (loop
                            :with strstate = 'in-string
                            :with str = ""
                            :until (eql strstate 'done)
                            :do (let ((ch (next-char)))
                                  (ecase strstate
                                    ((in-string)
                                     (case ch
                                       ((#\") (setf strstate 'done))
                                       ((#\\) (setf strstate 'escaped))
                                       (otherwise (concat str ch))))
                                    ((escaped)
                                     (concat str ch)
                                     (setf strstate 'in-string))))
                            :finally (progn
                                       (spush (make-str str))
                                       (spush 'string)
                                       (spush 'token))))
                        ((#\()
                         (spush 'nil)
                         (spush 'list)
                         (spush 'parse))
                        ((#\))
                         (if (eql 'list (top stack))
                             (progn (spop)
                                    (spush (reverse (spop)))
                                    (spush 'cons)
                                    (spush 'token))
                             (error "An object cannot begin with #\\)")))
                        ((#\;)
                         (loop :named comment :until (or (scanner-eot) (char= #\newline (next-char))))
                         (spush 'parse))
                        ((#\` #\, #\#)
                         (error "Reader macro for character #\\~C is not implemented yet." ch))
                        (otherwise
                         (loop
                            :with kind = nil
                            :with tokenstate = 'in-token
                            :with token = (string ch)
                            :until (eql tokenstate 'done)
                            :do (if (scanner-eot)
                                    (progn
                                      (setf tokenstate 'done)
                                      (if (and (null kind) (string= token ""))
                                          (progn (spush nil)
                                                 (spush 'eot)
                                                 (spush 'done))
                                          (multiple-value-bind (type value) (parse-token kind token)
                                            (progn (spush value)
                                                   (spush type)
                                                   (spush 'token)))))
                                    (let ((ch (next-char)))
                                      (ecase tokenstate
                                        ((in-token)
                                         (case ch
                                           ((#\space #\' #\" #\; #\, #\` #\( #\) #\newline)
                                            (setf tokenstate 'done)
                                            (multiple-value-bind (type value) (parse-token kind token)
                                              (progn (spush value)
                                                     (spush type)
                                                     (spush 'token))))
                                           ((#\\) (setf tokenstate 'escape1 kind 'symbol))
                                           ((#\|) (setf tokenstate 'escape2 kind 'symbol))
                                           (otherwise
                                            (unless (or (digit-char-p ch)
                                                        (position ch "+-.EeSsFfDdLl"))
                                              (setf kind 'symbol))
                                            (concat token ch))))
                                        ((escape1)
                                         (concat token ch)
                                         (setf tokenstate 'in-token))
                                        ((escape2)
                                         (case ch
                                           ((#\|)     (setf tokenstate 'in-token))
                                           (otherwise (concat token ch)))
                                         (setf tokenstate 'in-token)))))))))))
               ((token)
                (let ((type  (spop))
                      (value (spop)))
                  (ecase (setf state (spop))
                    ((done)
                     (spush value)
                     (spush type)
                     (spush 'done))
                    ((list)
                     (spush (cons value (spop)))
                     (spush 'list)
                     (spush 'parse))
                    ((quote)
                     (spush (list 'quote value))
                     (spush 'cons)
                     (spush 'token))))))
         :finally (let ((type  (spop))
                        (value (spop)))
                    (return (values value *scanner-position*)))))))



(dolist (test '((|symbol|  "symbol")
                (|symbol|  "symbol and another")
                (1234      "1234")
                (1234      "1234 integer")
                (1234      "01234 integer")
                ("string"  "\"string\"")
                ("string"  "\"string\" and something else")
                ("string\"with dblquote" 
                 "\"string\\\"with dblquote\" and something else")
                ( "string
with newline"
                  "\"string
with newline\" and something else")
                (() "()")
                (() "  (   )   ")
                ((1) "  ( 1  )   ")
                ((1 2 3 4) "  ( 1 2 3 4 )   ")
                ((|a| |bc| |def|) " (a bc def)   ")
                ((|attributes| (|lock| "toto")) "(attributes (lock \"toto\"))")))
  (let ((results  (multiple-value-list (read-from-str (second test)))))
    (format t "(read-from-str ~S) ~% --> ~{~S~^, ~}~%" (second test) results)
    (if (equal (first results) (first test))
        (format t "    as expected.~%")
        (format t "    BUT EXPECTED: ~S~%" (first test)))))
