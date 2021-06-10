;;;; -*- mode:lisp;coding:utf-8 -*-
;;;;**************************************************************************
;;;;FILE:               bignum.lisp
;;;;LANGUAGE:           Common-Lisp
;;;;SYSTEM:             Common-Lisp
;;;;USER-INTERFACE:     NONE
;;;;DESCRIPTION
;;;;    
;;;;    Implements bignum arithmetic in lisp using lists of fixnums.
;;;;    
;;;;AUTHORS
;;;;    <PJB> Pascal J. Bourguignon <pjb@informatimago.com>
;;;;MODIFICATIONS
;;;;    2008-08-16 <PJB> Created.
;;;;BUGS
;;;;LEGAL
;;;;    GPL
;;;;    
;;;;    Copyright Pascal J. Bourguignon 2009 - 2009
;;;;    
;;;;    This program is free software; you can redistribute it and/or
;;;;    modify it under the terms of the GNU General Public License
;;;;    as published by the Free Software Foundation; either version
;;;;    2 of the License, or (at your option) any later version.
;;;;    
;;;;    This program is distributed in the hope that it will be
;;;;    useful, but WITHOUT ANY WARRANTY; without even the implied
;;;;    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
;;;;    PURPOSE.  See the GNU General Public License for more details.
;;;;    
;;;;    You should have received a copy of the GNU General Public
;;;;    License along with this program; if not, write to the Free
;;;;    Software Foundation, Inc., 59 Temple Place, Suite 330,
;;;;    Boston, MA 02111-1307 USA
;;;;**************************************************************************

(defpackage "COM.INFORMATIMAGO.COMMON-LISP.LISP.BIGNUM"
  (:use "COMMON-LISP")
  (:import-from "COM.INFORMATIMAGO.COMMON-LISP.CESARUM.UTILITY"
                "NSUBSEQ"))
(in-package "COM.INFORMATIMAGO.COMMON-LISP.LISP.BIGNUM")

;; (defun remaining-safak ()
;; "Return how many days left."
;;   (- (encode-universal-time 0 0 0 16 9 2008)
;;      (get-universal-time)))
;; 
;; 
;; (defparameter *time-bases*       '(60       60        24    30     12))
;; (defparameter *time-bases-names* '("second" "minute" "hour" "day" "month" "year"))
;; 
;; (defmacro define-base-decomposer (name bases)
;;   "Note: BASES is evaluated."
;;   `(defun ,name (value)
;;      (labels ((decompose (value bases result)
;;                 (if (null bases)
;;                     (reverse (cons (unless (zerop value) value) result))
;;                     (multiple-value-bind (q r) (truncate value (car bases))
;;                       (decompose q (cdr bases)
;;                                  (cons (unless (zerop r) r) result))))))
;;        (decompose value ,bases '()))))
;; 
;; (define-base-decomposer time-interval-to-components *time-bases*)
;; 
;; 
;; (defun format-decomposed-value (decomposed-values bases)
;;   (substitute #\. #\,
;;    (format nil "~:{~:[~*~;~:*~A ~A~2:*~P~*, ~]~}"
;;            (mapcar (function list)
;;                    (reverse decomposed-values)
;;                    (reverse bases)))
;;    :from-end t :count 1))
;; 
;; (format-decomposed-value (time-interval-to-components (remaining-safak))
;;                         *time-bases-names*)





;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; utilities



(defun sequence-left-trim (bag-of-elements sequence
                           &key (key (function identity)) (test (function eql)))
  (etypecase sequence
    (string
     (if (and (eql key  (function identity))
              (eql test (function eql)))
         (string-left-trim bag-of-elements sequence)
         (loop
            :for i :from 0 :below (length sequence)
            :while (position (funcall key (aref sequence i))
                             bag-of-elements :test test)
            :finally (return (subseq sequence i)))))
    (list
     (loop
        :for s :on sequence
        :while (position (funcall key (car s)) bag-of-elements :test test)
        :finally (return s)))
    (vector
     (loop
        :for i :from 0 :below (length sequence)
        :while (position (funcall key (aref sequence i))
                         bag-of-elements :test test)
        :finally (return (subseq sequence i))))))


(defun test/sequence-left-trim ()
  (dolist (test '(( ((#\space #\!) "   !!!abc")   "abc" )
                  ( ((#\space #\!) "abc")         "abc" )
                  ( (()            "abc")         "abc" )
                  ( ((#\space #\!) "")            "" )
                  ( (()            "")            "" )
                  ( (#(#\space #\!) "   !!!abc")  "abc" )
                  ( (#(#\space #\!) "abc")        "abc" )
                  ( (#()            "abc")        "abc" )
                  ( (#(#\space #\!) "")           "" )
                  ( (#()            "")           "" )
                  ( (" !"           "   !!!abc")  "abc")
                  ( (" !"           "abc")        "abc")
                  ( (""             "abc")        "abc")
                  ( ("abc"          "abcabc")     "")
                  ( ("ABC" "abcABCxyz" :test #.(function char-equal))  "xyz" )
                  ( (""    "abcABCxyz" :test #.(function char-equal))  "abcABCxyz" ))
           :success)
    (destructuring-bind ((bag sequence &rest rest) expected-result) test
      (assert (equal (apply (function sequence-left-trim) bag
                            sequence rest)
                     expected-result))
      (assert (equal (apply (function sequence-left-trim) bag
                            (coerce sequence 'list) rest)
                     (coerce expected-result 'list)))
      (assert (equal (apply (function sequence-left-trim) bag
                            (coerce sequence 'vector) rest)
                     (coerce expected-result 'vector))))))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;
;;; Arithmetic in arbitrary base.
;;; Could be used to implement an inefficient bignum package.
;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;
;;; First, functions (u%xxx) to manipulate unsigned big numbers
;;; represented as lists of digits, such as (<= 0 digit (1- base))
;;; 0 is either () or (0 ...).
;;; The low order digit comes first.
;;;

(defun u%compose-base (base a)
  "Return the lisp bignum encoded in A"
  (let ((n 0))
    (dolist (digit (reverse a) n)
      (setf n  (+ (* base n) digit)))))


(defun u%decompose-base (base n)
  "Return list-of-digit representation in base BASE of the native bignum N."
  (if (zerop n)
      '()
      (labels ((decompose (n result)
                 (if (< n base)
                     (nreverse (cons n result))
                     (multiple-value-bind (quotient remainder) (truncate n base)
                       (decompose quotient (cons remainder result))))))
        (decompose n '()))))


(defun u%plus (base a b)
  "Return (+ a b)"
  (loop
     :with cc = 0
     :for aa = a :then (rest aa)
     :for bb = b :then (rest bb)
     :collect (let* ((s (+ (or (first aa) 0) (or (first bb) 0) cc)))
                (if (< s base)
                    (progn (setf cc 0) s)
                    (progn (setf cc 1) (- s base))))
     :while (or aa bb)))


(defun u%minus (base a b)
  "Return (- a b)
PRE: (>= a b)"
  (loop
     :with cc = 0
     :for aa = a :then (rest aa)
     :for bb = b :then (rest bb)
     :while (or aa bb)
     :collect (let* ((s (- (or (first aa) 0) (or (first bb) 0) cc)))
                (if (< s 0)
                    (progn (setf cc 1) (+ s base))
                    (progn (setf cc 0) s)))))


(defun u%times-digit (base digit a)
  "Return the product of a * digit."
  (loop
     :with cc = 0
     :with p = '()
     :for aa = a then (rest aa)
     :do (let ((pp (+ (* digit (or (first aa) 0)) cc)))
           (if (< pp base)
               (progn
                 (setf cc 0)
                 (push pp p))
               (multiple-value-bind (quotient remainder) (truncate pp base)
                 (setf cc quotient)
                 (push remainder p))))
     :while aa
     :finally (return (nreverse p))))


(defvar *u%zero* '())

(defun u%zerop (a)
  "Predicate: (= a 0)"
  (or (null a) (every (function zerop) a)))


(defun u%times (base a b)
  "Return (* a b)"
  (if (or (u%zerop a) (u%zerop b))
       *u%zero*
      (u%plus base
              (u%times-digit base (first a) b)
              (cons 0 (u%times base (rest a) b)))))


(defun u%normalize (a)
  "Return A normalized. 0 is (), non-zero have high order zeroes removed."
  (if (u%zerop a)
      *u%zero*
      (nreverse (sequence-left-trim '(0) (reverse a)))))


(defun u%normalize%reverse (a)
  "Return A normalized, but reversed.
0 is (), non-zero have high order zeroes removed."
  (if (u%zerop a)
      *u%zero*
      (sequence-left-trim '(0) (reverse a))))


(defun u%< (a b)
  "Return (< a b)"
  (let* ((a (u%normalize%reverse a))
         (b (u%normalize%reverse b))
         (la (length a))
         (lb (length b)))
    (cond
      ((< la lb) t)
      ((> la lb) nil)
      (t (labels ((compare (a b)
                    (cond
                      ((null a)            nil) ; (= a b)
                      ((< (car a) (car b)) t)
                      ((> (car a) (car b)) nil)
                      (t (compare (cdr a) (cdr b))))))
           (compare a b))))))


(defun u%%< (a b) ; is that name scary enough?
  (let* ((la (length a))
         (lb (length b)))
    (cond
      ((< la lb) t)
      ((> la lb) nil)
      (t (labels ((compare (a b)
                    (cond
                      ((null a)            nil) ; (= a b)
                      ((< (car a) (car b)) t)
                      ((> (car a) (car b)) nil)
                      (t (compare (cdr a) (cdr b))))))
           (compare a b))))))


(defun u%>  (a b) (u%< b a))
(defun u%>= (a b) (not (u%< a b)))
(defun u%<= (a b) (not (u%< b a)))
(defun u%=  (a b) (and (not (u%< a b)) (not (u%< b a))))
(defun u%/= (a b) (not (u%= a b)))


(defun guess-quotient-digit (base rn rd d)
  (declare (ignorable rd))
  (let ((min 0
          ;; (if (<= (first rn) (first rd))
          ;;    (truncate (+ base (first rn))  (1+ (first rd)))
          ;;    (truncate         (first rn)   (1+ (first rd))))
          )
        (max base
          ;;  (if (<= (first rn) (first rd))
          ;; (truncate (+ base (first rn) 1)    (first rd))
          ;; (truncate     (1+ (first rn))      (first rd)))
          )
        (n (reverse rn)))
    ;; (print (list min max rn rd n d))
    (loop
       :until (>= (1+ min) max)
       ;; :do (print `( (and (u%<= ,(u%times-digit base min d) ,n)
       ;;                    ,(if (= max base)
       ;;                         `(u%<  ,n ,(u%times base '(0 1) d))
       ;;                         `(u%<  ,n ,(u%times-digit base max d))))
       ;;               --> ,(and (u%<= (u%times-digit base min d) n)
       ;;                         (if (= max base)
       ;;                             (u%<  n (u%times base '(0 1) d))
       ;;                             (u%<  n (u%times-digit base max d))))))
       :do (assert (and (u%<= (u%times-digit base min d) n)
                        (if (= max base)
                            (u%<  n (u%times base '(0 1) d))
                            (u%<  n (u%times-digit base max d)))))
       :do (let* ((cur (truncate (+ min max) 2)))
             ;; (print (list min max rn rd n d  (u%times-digit base cur d)))
             (if (u%< n (u%times-digit base cur d))
                 (setf max cur)
                 (setf min cur)))
       :finally (assert (and (or (= min      max)
                                 (= (1+ min) max))
                             (u%<= (u%times-digit base min d) n)
                             (if (= (1+ min) base)
                                 (u%<  n (u%times base '(0 1) d))
                                 (u%<  n (u%times-digit base (1+ min) d)))))
                (return min))))

;; (setf *print-circle* t)
;; (guess-quotient-digit 10 '(9 2 3) '(9 8) '(8 9))

#-(and) (pushnew :u%divide/explain *features*)
#-(and) (setf *features* (delete :u%divide/explain *features*))

(defun u%divide (base a b)
  "Return quotient ; remainder"
  (when (u%zerop b) (error 'division-by-zero))
  (let* ((rn (u%normalize%reverse a))
         (rd (u%normalize%reverse b))
         (n  (reverse rn))
         (d  (reverse rd))
         (ln (length n))
         (ld (length d)))
    (if (< ln ld)
        (progn
          #+:u%divide/explain (format t "~2&~{~A~^.~} divided by ~{~A~^.~}~2%  The numerator is smaller than the denominator.~%  --> The quotient is 0, the remainder is ~2:* ~{~A~^.~}~%" rn rd)
          (values *u%zero* n))
        (macrolet ((take-one-more-digit (dst src)
                     `(setf ,dst (nconc ,dst (list (first ,src)))
                            ,src (rest ,src)))
                   (remains-digits-p (place) place))
          #+:u%divide/explain (format t "~2&~{~A~^.~} divided by ~{~A~^.~}~2%  The numerator is greater than the denominator.~%" rn rd)
          (loop
             :with q   = *u%zero*
             :with cur = rn
             :with rhd = '()
             :while (remains-digits-p cur)
             :do (progn
                   (take-one-more-digit rhd cur)
                   #+:u%divide/explain (format t "  Take one more digit: ~{~A~^.~}, from the numerator.~%" (or rhd '(0))))
                   (if (u%%< rhd rd)
                       (progn
                         (push 0 q)
                         #+:u%divide/explain (format t "  They're smaller than the divisor, so let's take one more,~%  and put down one 0 on the quotient: ~{~A~^.~}.~%" (reverse q)))
                       (let* ((qd (guess-quotient-digit base rhd rd d))
                              (p  (u%times-digit base qd d)))
                         #+:u%divide/explain (format t "  They're greater than the divisor, so let's divide them.~%")
                         #+:u%divide/explain (format t "  ~{~A~^.~} time ~A ...~%" rd qd)
                         (push qd q) ; (+ (* q base) qd)
                         #+:u%divide/explain (format t "  ~{~A~^.~} = ~{~A~^.~} * ~A, so we put down ~:*~A on the quotient: ~{~A~^.~} and~%" (u%normalize%reverse p) rd qd (u%normalize%reverse q))
                         (setf rhd (u%normalize%reverse (u%minus base (nreverse rhd) p)))
                         #+:u%divide/explain (format t "  ~{~A~^.~} remains~%" (or rhd '(0)))
                         #+:u%divide/explain (format t "~%")))
             :finally (progn #+:u%divide/explain (format t "  There remains no more digits from the numerator.~%")
                             #+:u%divide/explain (format t "~%  The quotient is ~{~A~^.~} and the remainder is ~{~A~^.~}.~2%" (u%normalize%reverse q) (or (reverse rhd) '(0)))
                             (return (values q (reverse rhd)))))))))


(defun u%parse-integer (base string)
  (loop
     :with ten = (u%decompose-base base 10.)
     :with n = *u%zero*
     :with digits = (coerce (loop
                               :for digit :from 0 :to 9
                               :collect (u%decompose-base base digit))
                            'vector)
     :for digit-char :across string
     :do (setf n
               (u%normalize (u%plus base (u%times base ten n)
                                    (aref digits (digit-char-p digit-char)))))
     :finally (return n)))


(defun u%princ-to-string (base n)
  (if (u%zerop n)
      "0"
      (if (< base 10.)
          (loop
             :with result = '()
             :with ten = (u%decompose-base base 10.)
             :with digits = (let ((h (make-hash-table :test (function equal))))
                              (loop
                                 :for digit from 0 to 9
                                 :do (setf (gethash (u%decompose-base base digit) h) digit))
                              h)
             :until (u%zerop n)
             :do (multiple-value-bind (q r) (u%divide base n ten)
                   (push (gethash r digits) result)
                   (setf n q))
             :finally (return (format nil "~{~A~}" result)))
          (loop
             :with result = '()
             :with ten = (u%decompose-base base 10.)
             :until (u%zerop n)
             :do (multiple-value-bind (q r) (u%divide base n ten)
                   (push (first r) result)
                   (setf n q))
             :finally (return (format nil "~{~A~}" result))))))






;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;
;;; u%xxx Tests
;;;

(defvar *test-base* 10.)


(defun test/u%decompose-base ()
  (assert (eql '() (u%decompose-base 10 0)))
  :success)


(defun test/operation (clop u%op
                       &key (min (constantly 0)) (max (constantly (expt *test-base* 2))))
  (loop for a from 0 to (expt *test-base* 2)
     do (loop for b from (funcall min a) to (funcall max a)
           do (assert (= (u%compose-base *test-base* (funcall u%op *test-base*
                                                    (u%decompose-base *test-base* a)
                                                    (u%decompose-base *test-base* b)))
                         (funcall clop a b)))))
  :success)


(defun test/u%times-digit ()
  (loop for a from 0 to (expt *test-base* 2)
     do (loop for digit from 0 to 9
           do (assert (= (* digit a) (u%compose-base *test-base* (u%times-digit
                                                         *test-base*
                                                         digit
                                                         (u%decompose-base *test-base* a)))))))
  :success)


(defun test/u%< ()
  (flet ((<=> (a b) (or (and a b) (and (not a) (not b)))))
    (loop for a from 0 to (expt *test-base* 2)
       do (loop for b from 0 to (expt *test-base* 2)
             do (let ((da (u%decompose-base *test-base* a))
                      (db (u%decompose-base *test-base* b)))
                  (assert (<=> (< a b) (u%< da db)))
                  (assert (<=> (> a b) (u%> da db)))
                  (assert (<=> (= a b) (u%= da db)))
                  (assert (<=> (/= a b) (u%/= da db)))
                  (assert (<=> (<= a b) (u%<= da db)))
                  (assert (<=> (>= a b) (u%>= da db)))))))
  :success)


(defun test/u%zero ()
  (assert (u%zerop *u%zero*))
  (assert (u%zerop '(0)))
  (assert (u%zerop '(0 0 0)))
  (assert (not (u%zerop '(1))))
  (assert (not (u%zerop '(0 0 1))))
  (assert (not (u%zerop '(1 0 0))))
  :success)


(defun test/u%normalize ()
  (assert (equal '() (u%normalize '())))
  (assert (equal '() (u%normalize '(0))))
  (assert (equal '() (u%normalize '(0 0 0 0))))
  (assert (equal '(1 2 3) (u%normalize '(1 2 3))))
  (assert (equal '(1 2 3) (u%normalize '(1 2 3 0))))
  (assert (equal '(1 2 3) (u%normalize '(1 2 3 0 0 0 0))))
  (assert (equal '(0 0 1 2 3) (u%normalize '(0 0 1 2 3))))
  (assert (equal '(0 0 1 2 3) (u%normalize '(0 0 1 2 3 0))))
  (assert (equal '(0 0 1 2 3) (u%normalize '(0 0 1 2 3 0 0 0 0))))
  :success)

(defun test/u%normalize%reverse ()
  (assert (equal '() (u%normalize%reverse '())))
  (assert (equal '() (u%normalize%reverse '(0))))
  (assert (equal '() (u%normalize%reverse '(0 0 0 0))))
  (assert (equal '(3 2 1 0 0) (u%normalize%reverse '(0 0 1 2 3))))
  (assert (equal '(3 2 1 0 0) (u%normalize%reverse '(0 0 1 2 3 0))))
  (assert (equal '(3 2 1 0 0) (u%normalize%reverse '(0 0 1 2 3 0 0 0 0))))
  :success)



(defun test/u%divide ()
  (loop for a from 0 to (expt *test-base* 2)
     do (loop for b from 1 to (expt *test-base* 2)
           do (assert (equal (mapcar (lambda (n) (u%compose-base *test-base* n))
                                     (multiple-value-list
                                      (u%divide *test-base*
                                               (u%decompose-base *test-base* a)
                                               (u%decompose-base *test-base* b))))
                         (multiple-value-list (truncate a b))))))
  (assert (let ((n  '(2 3 4 9 9 8 7 6 5 4 3 2 1 0 0 0 1 2 3 4 5))
                   (d '(9 9 4 7 7)))
               (multiple-value-bind (q r) (u%divide 10 n d)
                 (u%= n  (u%plus 10 r (u%times 10 q d))))))
  :success)



(defun factorial (n) (if (<= n 1)  1 (* n (factorial (- n 1)))))

(defun test/u%parse-integer ()
  (assert
   (let ((base  (expt 2 32)))
     (u%= (u%divide base (u%parse-integer base (princ-to-string (factorial 1000)))
                    (u%decompose-base base 1000))
          (u%parse-integer base (princ-to-string (factorial 999))))))

  :success)


(defun test/u%princ-to-string ()
  (assert (string= "543" (U%princ-to-string 2 (u%decompose-base 2 543))))
  (assert (string= "543" (U%princ-to-string 10 (u%decompose-base 10 543))))
  (assert (string= "543" (U%princ-to-string 1000 (u%decompose-base 1000 543))))
  :success)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;
;;; Signed big integer arithmetic
;;;


(defun i%parse-integer (base string)
  (let ((digits (u%parse-integer base
                                 (if (or (char= #\+ (aref string 0))
                                         (char= #\- (aref string 0)))
                                     (nsubseq string 1)
                                     string))))
    (cons (if (and digits (char= #\- (aref string 0)))
              '-
              '+)
          digits)))


(defun i%plus (base a b)
  (cond
    ((eql (car a) (car b))
     (cons (car a) (u%plus base (cdr a) (cdr b))))
    ((u%< (cdr a) (cdr b))
     (cons (car b) (u%minus base (cdr b) (cdr a))))
    (t
     (cons (car a) (u%minus base (cdr a) (cdr b))))))


(defun i%minus (base a b)
  (if (eql (car a) (car b))
      (if (u%< (cdr a) (cdr b))
          (cons (if (eql (car a) '+) '- '+) (u%minus base (cdr b) (cdr a)))
          (cons (if (eql (car a) '+) '+ '-) (u%minus base (cdr a) (cdr b))))
      (cons (car a) (u%plus base (cdr a) (cdr b)))))


(defun i%times (base a b)
  (cons (if (eql (car a) '+)
            (car b)
            (if (eql (car b) '+)
                '-
                '+))
        (u%times base (cdr a) (cdr b))))



;; C/USER[490]> (I%PARSE-INTEGER 10 "-5321")
;; (- 1 2 3 5)
;; C/USER[491]> (I%PARSE-INTEGER 10 "+5321")
;; (+ 1 2 3 5)
;; C/USER[492]> (I%PARSE-INTEGER 10 "5321")
;; (+ 1 2 3 5)
;; C/USER[493]> (I%PARSE-INTEGER 10 "005321")
;; (+ 1 2 3 5)
;; C/USER[494]> (I%PARSE-INTEGER 10 "0")
;; (+)
;; C/USER[495]> (I%PARSE-INTEGER 10 "-0")
;; (-)
;; C/USER[496]> I%PARSE-INTEGER
;; C/USER[497]> (I%PARSE-INTEGER 10 "-0")
;; (+)
;; C/USER[498]> (I%PARSE-INTEGER 10 "+0")
;; (+)
;; C/USER[499]> (I%PARSE-INTEGER 10 "0")
;; (+)
;; C/USER[500]> (I%PARSE-INTEGER 10 "0")



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;
;;; TEST/ALL


(defun test/all ()
  (dolist (test
            '(
              (test/sequence-left-trim)
              (test/u%zero)
              (test/u%decompose-base)
              (test/operation (function +) (function u%plus))
              (test/operation (function -) (function u%minus) :max (function identity))
              (test/u%times-digit)
              (test/operation (function *) (function u%times))
              (TEST/U%NORMALIZE)
              (TEST/U%NORMALIZE%reverse)
              (test/u%<)
              (test/u%divide)
              (test/u%parse-integer)
              (test/u%princ-to-string)))
    (format t "~&~60A" test)
    (format t "~A~%" (or (ignore-errors
                           (block :test
                             (with-output-to-string (out)
                               (let ((*standard-output* out)
                                     (*trace-output* out)
                                     (*error-output* out))
                                 (eval test)
                                 (return-from :test "[32m SUCCESS [0m")))))
                         "[31m FAILURE [0m"))))


(print '(time (test/all)))


;; (time (test/all))


;; C/USER[400]> (load (compile-file "base.lisp"))
;; ;; Compiling file /home/pjb/src/lisp/encours/scratch/base.lisp ...
;; ;; Wrote file /home/pjb/src/lisp/encours/scratch/base.fas
;; 0 errors, 1 warning
;; ;; Loading file /home/pjb/src/lisp/encours/scratch/base.fas ...
;; (TEST/SEQUENCE-LEFT-TRIM)                                    SUCCESS 
;; (TEST/OPERATION #'+ #'U%PLUS)                                SUCCESS 
;; (TEST/OPERATION #'- #'U%MINUS MAX #'IDENTITY)                SUCCESS 
;; (TEST/OPERATION #'* #'U%TIMES)                               SUCCESS 
;; (TEST/U%TIMES-DIGIT)                                         SUCCESS 
;; (TEST/U%ZERO)                                                SUCCESS 
;; (TEST/U%NORMALIZE)                                           SUCCESS 
;; (TEST/U%NORMALIZE%REVERSE)                                   SUCCESS 
;; (TEST/U%<)                                                   SUCCESS 
;; (TEST/U%DIVIDE)                                              SUCCESS 
;; 
;; Run time: 2.000125 sec.
;; Space: 17210864 Bytes
;; GC: 10, GC time: 0.328022 sec.
;; ;; Loaded file /home/pjb/src/lisp/encours/scratch/base.fas
;; T
;; 
;; 
;; C/USER[401]> (load "base.lisp")
;; ;; Loading file base.lisp ...
;; (TEST/SEQUENCE-LEFT-TRIM)                                    SUCCESS 
;; (TEST/OPERATION #'+ #'U%PLUS)                                SUCCESS 
;; (TEST/OPERATION #'- #'U%MINUS MAX #'IDENTITY)                SUCCESS 
;; (TEST/OPERATION #'* #'U%TIMES)                               SUCCESS 
;; (TEST/U%TIMES-DIGIT)                                         SUCCESS 
;; (TEST/U%ZERO)                                                SUCCESS 
;; (TEST/U%NORMALIZE)                                           SUCCESS 
;; (TEST/U%NORMALIZE%REVERSE)                                   SUCCESS 
;; (TEST/U%<)                                                   SUCCESS 
;; (TEST/U%DIVIDE)                                              SUCCESS 
;; 
;; Run time: 13.340833 sec.
;; Space: 57696024 Bytes
;; GC: 32, GC time: 0.876053 sec.
;; 
;; ;; Loaded file base.lisp
;; T
;; C/USER[402]>




;; (time (test/U%DIVIDE))
;;
;; Interpreted:
;; Run time: 7.132446 sec.
;; Space: 22631560 Bytes
;; GC: 13, GC time: 0.344019 sec
;;
;; Compiled:
;; Run time: 6.532409 sec.
;; Space: 22631560 Bytes
;; GC: 12, GC time: 0.320021 sec.

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;; THE END ;;;;
