(in-package "COM.INFORMATIMAGO.BOCL.COMPILER")

(progn
  (pprint (multiple-value-list (desgen/null '() 'var (list '()) '((print 'hi)))))
  (pprint (multiple-value-list (desgen/or '(k) 'var (list '())  '((list k)))))
  (pprint (multiple-value-list (desgen/or '((l 24)) 'var (list '()) '((list l)))))
  (pprint (multiple-value-list (desgen/or '(k (l 24)) 'var (list '()) '((list k l)))))
  (pprint (multiple-value-list (desgen/or '(k (l 24) (m 42 mp)) 'var (list '()) '((list k l m mp)))))

  (pprint (multiple-value-list (desgen/ror '(a b c &optional k) 'var (list '())  '((list a b c k)))))
  (pprint (multiple-value-list (desgen/ror '(a b c &optional (l 24)) 'var (list '())  '((list a b c l)))))
  (pprint (multiple-value-list (desgen/ror '(a b c &optional k (l 24)) 'var (list '())  '((list a b c k l)))))
  (pprint (multiple-value-list (desgen/ror '(a b c &optional k (l 24) (m 42 mp)) 'var (list '())  '((list a b c k l m mp)))))

  (pprint (multiple-value-list (desgen/wror '(a b c &optional k) 'var (list '())  '((list a b c k)))))
  (pprint (multiple-value-list (desgen/wror '(a b c &optional (l 24)) 'var (list '())  '((list a b c l)))))
  (pprint (multiple-value-list (desgen/wror '(a b c &optional k (l 24)) 'var (list '())  '((list a b c k l)))))
  (pprint (multiple-value-list (desgen/wror '(a b c &optional k (l 24) (m 42 mp)) 'var (list '())  '((list a b c k l m mp)))))


  (pprint (multiple-value-list (desgen/wror '(&whole w a b c &optional k) 'var (list '())  '((list a b c k)))))
  (pprint (multiple-value-list (desgen/wror '(&whole w a b c &optional (l 24)) 'var (list '())  '((list a b c l)))))
  (pprint (multiple-value-list (desgen/wror '(&whole w a b c &optional k (l 24)) 'var (list '())  '((list a b c k l)))))
  (pprint (multiple-value-list (desgen/wror '(&whole w a b c &optional k (l 24) (m 42 mp)) 'var (list '())  '((list a b c k l m mp)))))
  ;; (pprint (multiple-value-list (desgen/wror '(&whole w a b c &optional k &optional(l 24) (m 42 mp)) 'var)))

  ;; (pprint (generate-destructuring-list '(&whole w a b c &optional k &optional(l 24) (m 42 mp)) 'var '(((list w a b c k l m mp)))))

  (pprint (multiple-value-list (desgen/ror '(a b c &rest r) 'var (list '()) '((list a b c r)))))

  )


(progn

  (pprint (generate-destructuring-list '() 'var '() '((print 'hi))))
  (pprint (generate-destructuring-list '() 'var '((declare (special var))) '((print 'hi))))
  (pprint (generate-destructuring-list '() 'var '((declare (notinline print))) '((print 'hi))))
  (pprint (generate-destructuring-list '() 'var '((declare (special var)) (declare (notinline print))) '((print 'hi))))

  (pprint (generate-destructuring-list '(a b c) 'var
                                       '((declare (special a b))
                                         (declare (notinline print)))
                                       '((print 'hi))))
  
  (pprint (generate-destructuring-list '(a b c &optional k (l) (m 24) (n 42 np)) 'var
                                       (list '())
                                       '((print 'hi))))

  (pprint (generate-destructuring-list '() 'var
                                       '()
                                       '((print 'hi))))

  (pprint (generate-destructuring-list '(a b c) 'var
                                       '()
                                       '((print 'hi))))

  (pprint (generate-destructuring-list '(a b c &optional k (l) (m 24) (n 42 np)
                                         &rest r
                                         &key u (v 42) (w 33 wp)  ((:k1 x) 42) ((k2 y) 33 yp))
                                       'var
                                       '((special a b w wp x var)
                                         (notinline print))
                                       '((print 'hi))))

  (pprint (generate-destructuring-list '(&optional k (l) (m 24) (n 42 np)
                                         &rest r
                                         &key u (v 42) (w 33 wp)  ((:k1 x) 42) ((k2 y) 33 yp))
                                       'var
                                       '((special a b w wp x var)
                                         (notinline print))
                                       '((print 'hi))))

  (pprint (generate-destructuring-list '(&rest r
                                         &key u (v 42) (w 33 wp)  ((:k1 x) 42) ((k2 y) 33 yp))
                                       'var
                                       '((special a b w wp x var)
                                         (notinline print))
                                       '((print 'hi))))
  (pprint (generate-destructuring-list '(&key u (v 42) (w 33 wp)  ((:k1 x) 42) ((k2 y) 33 yp))
                                       'var
                                       '((special a b w wp x var)
                                         (notinline print))
                                       '((print 'hi))))
  (pprint (generate-destructuring-list '(&key u (v 42) (w 33 wp)  ((:k1 x) 42) ((k2 y) 33 yp)
                                         &aux (foo 1) (bar (* foo 2)) (quux (+ x v w bar)))
                                       'var
                                       '((special a b w wp x var)
                                         (notinline print))
                                       '((print 'hi)))))

