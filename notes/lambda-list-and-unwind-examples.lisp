(ql:quickload :trucler :trucler-native)

(defmacro var (name &environment env)
  (let ((client  (make-instance 'trucler-native:client)))
    (trucler:describe-variable client env name)))

(let ((x 42) (y 33)) (declare (fixnum x)) (var x))

(let ((client  (make-instance 'trucler-native:client)))
  (trucler:describe-variable client (trucler:global-environment client nil) 'pi))

;;======================================================================



(terpri)
    (write-line "    /* STANDARD-CLASS is its own metaclass so we need to create it,    */")
    (write-line "    /* and BUILT-IN-CLASS which is the metaclass for the classes of    */")
    (write-line "    /* the instances required to initialize A STANDARD-CLASS (SYMBOL   */")
    (write-line "    /* and STRING for the class-name, CONS for the direct-superclasses */")
    (write-line "    /* list).                                                          */")
    (terpri)

    (format t "    ~40A -> slots->class.direct_superclasses = cons(~:*~A,NIL_Symbol);~%"
            "STANDARD_CLASS_Class")
    (format t "    ~40A -> slots->class.direct_superclasses = cons(~:*~A,NIL_Symbol);~%"
            "BUILT-IN-CLASS_Class" "STANDARD_CLASS_Class")




(let ((object (list 1 2 3 (list 4 5 6) 7 8 9)))

  (when (consp object)
    (let ((current object))
      (princ "(")
      (prin1 (car current))
      (setf current (cdr current))
      (loop while (consp current) do
        (princ " ")
        (prin1 (car current))
        (setf current (cdr current)))
      (when (not (null current))
        (princ " . ")
        (prin1 current))
      (princ ")")
      object)))
(1 2 3 (4 5 6) 7 8 9)

(defun f (exit)
  (funcall exit 42))

(block garbonzo
  (f (lambda (r) (return-from garbonzo r))))
--> 42


(defun f (exit)
  (unwind-protect
       (funcall exit 42)
    (print '(exiting from f))))

(block garbonzo
  (unwind-protect
       (f (lambda (r) (return-from garbonzo r)))
    (print '(exiting from garbonzo))))

(exiting from f) 
(exiting from garbonzo)
--> 42


(defmacro moo (&whole w x) (prin1 w) x)
(defun foo (&optional (x (moo 42))) x)
(moo 42)foo
(foo)42

(defparameter *f* (let ((x 33))
                    (lambda (&optional (y (moo x)) (z (moo (+ y x))))
                      (list x y z))))
(moo x)(moo (+ y x))*f*
(funcall *f*)
(33 33 66)
(funcall *f* 1)
(33 1 34)
(funcall *f* 1 2)
(33 1 2)

(defparameter *g* (let ((w 30))
                    (lambda (&key (x (moo w)) (y (moo (+ w x))) (z (moo (list w x y))))
                      (list w x y z))))

(funcall *g*)
(30 30 60 (30 30 60))
(funcall *g* :y 2)
(30 30 2 (30 30 2))
(funcall *g* :z 3)
(30 30 60 3)
(funcall *g* :y 20 :x 1)
(30 1 20 (30 1 20))
(funcall *g* :x 1 :y 20 :x 101 :y 120)
(30 1 20 (30 1 20))

(defparameter *h* (let ((x 1)
                        (y 2)
                        (z 3))
                    (lambda (&key (x (moo (list x y z))) (y (moo (list x y z))) (z (moo (list x y z))))
                      (list x y z))))
(moo (list x y z)) (moo (list x y z)) (moo (list x y z)) *h*
(funcall *h*)
((1 2 3) ((1 2 3) 2 3) ((1 2 3) ((1 2 3) 2 3) 3))
(funcall *h* :z 33)
((1 2 3) ((1 2 3) 2 3) 33)
(funcall *h* :y 22)
((1 2 3) 22 ((1 2 3) 22 3))
(funcall *h* :x 11)
(11 (11 2 3) (11 (11 2 3) 3))
(funcall *h* :x 11 :y 22 :z 33)
(11 22 33)

 (lambda (a b)
   (block foo
     (fun a)
     (let ((x (+ a b)))
       (foo x b))))

(foo 1 2)
