(in-package  "COM.INFORMATIMAGO.BOCL.MACROS")


(values
 (generate-do 'let 'psetf
              '()                       ; no binding
              'nil                      ; nil end-test-form
              'nil                      ; result-form
                                        ; no declaration
              '((print 'hi)))

 (generate-do 'let 'psetf
              '()                       ; no binding
              'nil                      ; nil end-test-form
              'nil                      ; result-form
                                        ; with declaration
              '((declare (stepper disable)) (print 'hi)))

 (generate-do 'let 'psetf
              '()                       ; no binding
              '(zerop (random 2))       ; non-nil end-test-form
              'nil                      ; result-form
                                        ; no declaration
              '((print 'hi)))

 (generate-do 'let 'psetf
              '()                       ; no binding
              '(zerop (random 2))       ; nil end-test-form
              'nil                      ; result-form
                                        ; with declaration
              '((declare (stepper disable)) (print 'hi)))

 (generate-do 'let 'psetf
              '((a 1) (b 2))            ; bindings, no increment
              'nil                      ; nil end-test-form
              '(list a b)               ; result-form
                                        ; no declaration
              '((print 'hi)))

 (generate-do 'let 'psetf
              '((a 1) (b 2))            ; bindings, no increment
              'nil                      ; nil end-test-form
              '(list a b)               ; result-form
                                        ; with declaration
              '((declare (stepper disable)) (print 'hi)))

 (generate-do 'let 'psetf
              '((a 1) (b 2) c)          ; bindings, no increment
              '(zerop (random 2))       ; non-nil end-test-form
              '(list a b)               ; result-form
                                        ; no declaration
              '((print 'hi)))

 (generate-do 'let 'psetf
              '((a 1) (b 2) c)          ; bindings, no increment
              '(zerop (random 2))       ; nil end-test-form
              '(list a b)               ; result-form
                                        ; with declaration
              '((declare (stepper disable)) (print 'hi)))

 (generate-do 'let 'psetf
              '((a 1 (+ a b)) (b 2 (+ b)) c (d 4)) ; bindings, increment
              'nil                                 ; nil end-test-form
              '(list a b)                          ; result-form
                                        ; no declaration
              '((print 'hi)))

 (generate-do 'let 'psetf
              '((a 1 (+ a b)) (b 2 (+ b)) c (d 4)) ; bindings, increment
              'nil                                 ; nil end-test-form
              '(list a b)                          ; result-form
                                        ; with declaration
              '((declare (stepper disable)) (print 'hi)))

 (generate-do 'let 'psetf
              '((a 1 (+ a b)) (b 2 (+ b)) c (d 4)) ; bindings, increment
              '(zerop (random 2))       ; non-nil end-test-form
              '(list a b)               ; result-form
                                        ; no declaration
              '((print 'hi)))

 (generate-do 'let 'psetf
              '((a 1 (+ a b)) (b 2 (+ b)) c (d 4)) ; bindings, increment
              '(zerop (random 2))                  ; nil end-test-form
              '(list a b)                          ; result-form
                                        ; with declaration
              '((declare (stepper disable)) (print 'hi))))


(values
 (macroexpand-1 '(and-))
 (macroexpand-1 '(and- (values 1 2 3)))
 (macroexpand-1 '(and- (values 1 2 3) t (values t t) (values 1 2 3)))
 (macroexpand-1 '(and- (values nil nil) nil nil (values nil nil) (values 1 2 3)))
 (macroexpand-1 '(and- t t (values t t) (values 1 2 3)))
 (macroexpand-1 '(and- nil nil (values nil nil) (values 1 2 3)))
 (macroexpand-1 '(and- a b c d))
 (macroexpand-1 '(and- (values a a) (values b b) (values c c) (values d dd)))
 
 (macroexpand-1 '(or-))
 (macroexpand-1 '(or- (values 1 2 3)))
 (macroexpand-1 '(or- (values 1 2 3) t (values t t) (values 1 2 3)))
 (macroexpand-1 '(or- (values nil nil) nil nil (values nil nil) (values 1 2 3)))
 (macroexpand-1 '(or- t t (values t t) (values 1 2 3)))
 (macroexpand-1 '(or- nil nil (values nil nil) (values 1 2 3)))
 (macroexpand-1 '(or- a b c d))
 (macroexpand-1 '(or- (values a a) (values b b) (values c c) (values d dd)))
 )


(progn
  (pprint (macroexpand-1 '(cond-)))
  (pprint (macroexpand-1 '(cond- (nil))))
  (pprint (macroexpand-1 '(cond- (a) (b) (c))))
  (pprint (macroexpand-1 '(cond-
                           ((= a 1) (foo b) (values 1 2 3))
                           ((= a 2) (goo b) (values 4 5 6))
                           (t       (hoo b))))))

(let ((p (list :a 1 :b 2 :c 3 :d 4)))
  (values (remf- p :c)
          p
          (remf- p :a)
          p
          (remf- p :a)
          p))
t
(:a 1 :b 2 :d 4)
t
(:b 2 :d 4)
nil
(:b 2 :d 4)


(macroexpand-1 '(shiftf- (values a b c) (values d e) (values g) (values 1 2 3)))
(let (#:g144988 #:g144989 #:g144990 #:g144991 #:g144992 #:g144993 #:g144994)
  (multiple-value-setq (#:g144988 #:g144989 #:g144990) (values a b c))
  (multiple-value-setq (#:g144991 #:g144992) (values d e))
  (setq #:g144993 (values g))
  (setq #:g144994 (values 1 2 3))
  (let ((#:g144995 #:g144988)
        (#:g144996 #:g144989)
        (#:g144997 #:g144990))
    (setq #:g144988 #:g144991
          #:g144989 #:g144992
          #:g144990 nil
          #:g144991 #:g144993
          #:g144992 nil)
    (setq #:g144993 #:g144994)
    (values (setq a #:g144988) (setq b #:g144989) (setq c #:g144990))
    (values (setq d #:g144991) (setq e #:g144992))
    (values (setq g #:g144993))
    (values #:g144995 #:g144996 #:g144997))
  )
t


(macroexpand-1 '(rotatef- (values a b) (values c d) (values d e)))
(let (#:g145118 #:g145119 #:g145120 #:g145121 #:g145122 #:g145123 #:g145124 #:g145125)
  (multiple-value-setq (#:g145118 #:g145119) (values a b))
  (multiple-value-setq (#:g145120 #:g145121) (values c d))
  (multiple-value-setq (#:g145122 #:g145123) (values d e))
  (setq #:g145124 #:g145118
        #:g145125 #:g145119)
  (setq #:g145118 #:g145120
        #:g145119 #:g145121
        #:g145120 #:g145122
        #:g145121 #:g145123
        #:g145122 #:g145124
        #:g145123 #:g145125)
  (values (setq a #:g145118) (setq b #:g145119))
  (values (setq c #:g145120) (setq d #:g145121))
  (values (setq d #:g145122) (setq e #:g145123))
  nil)

(macroexpand-1 '(cl:psetq a (+ a b) b (- a b)))
(progn (let* ((#:g145137 (+ a b))) (setq b (- a b) a #:g145137)) nil)
t
(macroexpand-1 '(psetq- a (+ a b) b (- a b)))
(let ((#:g145252 (+ a b))
      (#:g145251 (- a b)))
  (setq a #:g145252
        b #:g145251)
  nil)
