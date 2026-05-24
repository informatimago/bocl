
(defun operators (form)
  ;; A Q&D code walker to collect the operators used in the FORM.
  (if (atom form)
      nil
      (remove-duplicates
       (case (first form)
         ;; First we check the special operators:
         ((declare)              '())
         ((FUNCTION)             (cons 'function
                                       (if (symbolp (second form))
                                           (list (second form))
                                           (mapcan (function operators) (cddr (second form))))))
         ((QUOTE)                (list 'quote))
         ((IF)                   (cons 'if     (append (operators (second form))
                                                       (operators (third  form))
                                                       (operators (fourth form)))))
         ((GO
           THROW
           LOAD-TIME-VALUE)      (cons (first form)    (operators (second form))))
         ((RETURN-FROM
           THE
            EVAL-WHEN)           (cons (first form)    (operators (third  form))))
         ((PROGN
            MULTIPLE-VALUE-CALL
            MULTIPLE-VALUE-PROG1
            CATCH
            UNWIND-PROTECT
            LOCALLY
            TAGBODY)             (cons (first form)    (mapcan (function operators) (cdr form))))
         ((BLOCK PROGV)          (cons (first form)    (mapcan (function operators) (cddr form))))
         ((FLET LABELS
            SYMBOL-MACROLET
            MACROLET)            (cons (first form)    (mapcan (function operators)
                                                               (append (mapcar (function cddr) (second form))
                                                                       (cddr form)))))
         ((SETQ)                 (cons 'setq (loop
                                                :for (nil e) :on (cdr form) :by (function cddr)
                                                :nconc (operators e))))
         ((LET LET*)             (cons (first form)    (mapcan (function operators)
                                                               (append (mapcan (lambda (binding)
                                                                                 (if (atom binding)
                                                                                     '()
                                                                                     (list (second binding))))
                                                                               (second form))
                                                                       (cddr form)))))
         ;; Then the functions and macros:
         (otherwise
          (if (macro-function (first form))
              (operators (macroexpand form))
              (cons (first form) (mapcan (function operators) (cdr form)))))))))



(defun clops (form)
  (intersection (list-external-symbols "CL") (operators form)))


;; tiny-printer
;; (ZEROP VALUES TYPE-OF TRUNCATE TAGBODY SYMBOLP SYMBOL-NAME STRINGP SETQ
;;        REVERSE REVAPPEND QUOTE PROGN NOT MULTIPLE-VALUE-CALL MINUSP
;;        LIST* LET* LET LENGTH KEYWORDP INTEGERP IF GO FUNCTION EVAL-WHEN
;;        ERROR CONSP CONS COERCE CHAR= CDR CAR BLOCK ATOM AREF <= 1+ -)
;;
;; tiny-reader
;; (UNWIND-PROTECT TAGBODY SUBSEQ STRING-UPCASE SETQ REVERSE READ-LINE
;;                 READ-CHAR QUOTE PROGN PEEK-CHAR PARSE-INTEGER NULL
;;                 NOT MULTIPLE-VALUE-PROG1 MAKE-STRING-INPUT-STREAM
;;                 LET INTERN IF GO FUNCTION EVAL-WHEN ERROR DIGIT-CHAR-P
;;                 CONS COERCE CODE-CHAR CLOSE CHAR= BLOCK AREF)
