;;;; -*- mode:lisp;coding:utf-8 -*-
;;;;**************************************************************************
;;;;FILE:               bignum-integration.lisp
;;;;LANGUAGE:           Common-Lisp
;;;;DESCRIPTION
;;;;
;;;;    Wires the Lisp bignum engine (bootstrap/bignum.lisp) into bocl's
;;;;    kernel arithmetic.  Loaded by the prelude right after bignum.lisp.
;;;;
;;;;    A BIGNUM is a kernel-boxed object whose value is a (SIGN . DIGITS)
;;;;    i%-format cons (base +BIGNUM-BASE+).  The kernel provides three C
;;;;    primitives used here:
;;;;       (%BIGNUMP x)                  -> T iff X is a boxed bignum
;;;;       (%BIGNUM-VALUE bignum)        -> its (SIGN . DIGITS) cons
;;;;       (%MAKE-INTEGER-FROM-BIGNUM sb)-> demote SB to a fixnum if it fits,
;;;;                                        else box it as a BIGNUM
;;;;    num_combine() (arithmetic.c) calls %BIGNUM-PLUS/MINUS/TIMES/DIVIDE on
;;;;    overflow or a bignum operand, %BIGNUM-COMPARE for =/</>, the reader
;;;;    calls %PARSE-BIGNUM on integer-literal overflow, and the printer calls
;;;;    %BIGNUM-TO-STRING.
;;;;**************************************************************************

;;; Digit base of the Lisp representation.  MUST equal BIGNUM_BASE in kernel.c.
(defparameter +bignum-base+ 1000000000)   ; 10^9

(defun %integer->idigits (x)
  "Return the i%-format (SIGN . DIGITS) of integer X (a fixnum or a BIGNUM)."
  (if (%bignump x)
      (%bignum-value x)
      (i%decompose +bignum-base+ x)))     ; X is a fixnum

;;; --- binary operators called from num_combine() ---------------------------

(defun %bignum-plus (a b)
  (%make-integer-from-bignum
   (i%plus +bignum-base+ (%integer->idigits a) (%integer->idigits b))))

(defun %bignum-minus (a b)
  (%make-integer-from-bignum
   (i%minus +bignum-base+ (%integer->idigits a) (%integer->idigits b))))

(defun %bignum-times (a b)
  (%make-integer-from-bignum
   (i%times +bignum-base+ (%integer->idigits a) (%integer->idigits b))))

(defun %bignum-divide (a b)
  "The / operator when a bignum is involved.  Exact => integer; a ratio with a
bignum numerator or denominator is not yet supported."
  (multiple-value-bind (q r)
      (i%divide +bignum-base+ (%integer->idigits a) (%integer->idigits b))
    ;; i%divide signals DIVISION-BY-ZERO when the divisor is zero.
    (if (i%zerop r)
        (%make-integer-from-bignum q)
        (error "bocl: a ratio with a bignum part is not yet supported: (/ ~A ~A)"
               a b))))

(defun %bignum-compare (a b)
  "Return -1, 0, or 1 comparing integers A and B (either may be a bignum)."
  (let ((ia (%integer->idigits a))
        (ib (%integer->idigits b)))
    (cond ((i%< ia ib) -1)
          ((i%< ib ia)  1)
          (t            0))))

;;; --- reader / printer hooks -----------------------------------------------

(defun %parse-bignum (string radix)
  "Parse STRING (optional leading sign, optional trailing #\\.) in RADIX, used by
the reader when an integer literal overflows a word.  Returns a fixnum when it
fits, else a bignum."
  (let* ((base  +bignum-base+)
         (len   (length string))
         (neg   (and (> len 0) (char= (char string 0) #\-)))
         (start (if (and (> len 0)
                         (or (char= (char string 0) #\+)
                             (char= (char string 0) #\-)))
                    1 0))
         (end   (if (and (> len start) (char= (char string (1- len)) #\.))
                    (1- len) len))
         (rd    (i%decompose base radix))
         (n     (i%decompose base 0)))      ; canonical zero (+)
    (do ((i start (1+ i)))
        ((>= i end))
      (let ((d (digit-char-p (char string i) radix)))
        (setq n (i%plus base (i%times base rd n) (i%decompose base d)))))
    (%make-integer-from-bignum
     (i%normalize (if neg (cons '- (cdr n)) n)))))

(defun %bignum-to-string (bignum)
  "Decimal string for a boxed BIGNUM, used by the printer."
  (i%princ-to-string +bignum-base+ (%bignum-value bignum)))

;;; --- Bit operations on bignums (issue deferred-bignum-ops) -----------------
;;; The kernel ASH / LOGAND / LOGIOR / LOGXOR / LOGNOT / INTEGER-LENGTH /
;;; LOGCOUNT / ISQRT primitives signal a TYPE-ERROR (and ASH silently returned 0)
;;; on a BIGNUM operand.  We implement them in Lisp on top of the already-wired
;;; bignum arithmetic (* / FLOOR / EXPT / MOD), with two's-complement semantics
;;; for the logical ops, and keep a fixnum fast-path through the saved kernel
;;; primitives so all-fixnum calls are unaffected.

(defparameter *%kernel-ash*            (symbol-function 'ash))
(defparameter *%kernel-logand*         (symbol-function 'logand))
(defparameter *%kernel-logior*         (symbol-function 'logior))
(defparameter *%kernel-logxor*         (symbol-function 'logxor))
(defparameter *%kernel-lognot*         (symbol-function 'lognot))
(defparameter *%kernel-integer-length* (symbol-function 'integer-length))
(defparameter *%kernel-logcount*       (symbol-function 'logcount))
(defparameter *%kernel-isqrt*          (symbol-function 'isqrt))

(defun ash (integer count)
  "Arithmetic shift, bignum-safe.  Left shift = multiply by 2^count (auto-promotes
   on overflow, which the kernel ASH did not — it returned 0); right shift = floor
   by 2^|count|."
  (cond
    ((zerop count) integer)
    ((plusp count)
     (if (%bignump integer)
         (* integer (expt 2 count))
         ;; fixnum left shift: trust the kernel only if it does not overflow,
         ;; detected by shifting the result back; otherwise promote via multiply.
         (let ((r (funcall *%kernel-ash* integer count)))
           (if (eql (funcall *%kernel-ash* r (- count)) integer)
               r
               (* integer (expt 2 count))))))
    (t                                   ; right shift (count < 0)
     (if (%bignump integer)
         (values (floor integer (expt 2 (- count))))
         (funcall *%kernel-ash* integer count)))))

;; Two's-complement bitwise combiner: walk the bits LSB->MSB until both operands
;; reach their sign-extension steady state (0 for non-negative, -1 for negative).
(defun %and-bit (p q) (* p q))
(defun %ior-bit (p q) (- 1 (* (- 1 p) (- 1 q))))
(defun %xor-bit (p q) (mod (+ p q) 2))

(defun %logop (combiner a b)
  (let ((result 0) (weight 1) (x a) (y b))
    (loop
      (when (and (or (zerop x) (eql x -1)) (or (zerop y) (eql y -1))) (return))
      (when (= 1 (funcall combiner (mod x 2) (mod y 2)))
        (setq result (+ result weight)))
      (setq weight (* weight 2) x (ash x -1) y (ash y -1)))
    ;; The infinite sign tail: bits above are combiner(sign-a, sign-b) forever.
    (if (= 1 (funcall combiner (if (eql x -1) 1 0) (if (eql y -1) 1 0)))
        (- result weight)
        result)))

(defun logand (&rest integers)
  (if (some #'%bignump integers)
      (reduce (lambda (a b) (%logop #'%and-bit a b)) integers :initial-value -1)
      (apply *%kernel-logand* integers)))

(defun logior (&rest integers)
  (if (some #'%bignump integers)
      (reduce (lambda (a b) (%logop #'%ior-bit a b)) integers :initial-value 0)
      (apply *%kernel-logior* integers)))

(defun logxor (&rest integers)
  (if (some #'%bignump integers)
      (reduce (lambda (a b) (%logop #'%xor-bit a b)) integers :initial-value 0)
      (apply *%kernel-logxor* integers)))

(defun lognot (integer)
  (if (%bignump integer)
      (- -1 integer)                     ; ~x = -x-1 = -1 - x
      (funcall *%kernel-lognot* integer)))

(defun integer-length (integer)
  (if (%bignump integer)
      (let ((n (if (minusp integer) (lognot integer) integer)) (len 0))
        (loop (when (zerop n) (return len))
              (setq n (ash n -1))
              (incf len)))
      (funcall *%kernel-integer-length* integer)))

(defun logcount (integer)
  (if (%bignump integer)
      (let ((n (if (minusp integer) (lognot integer) integer)) (count 0))
        (loop (when (zerop n) (return count))
              (when (= 1 (mod n 2)) (incf count))
              (setq n (ash n -1))))
      (funcall *%kernel-logcount* integer)))

(defun isqrt (n)
  "Greatest integer whose square is <= N (CLHS).  Newton's method for bignums."
  (cond
    ((minusp n) (error "ISQRT: argument must be non-negative: ~S" n))
    ((not (%bignump n)) (funcall *%kernel-isqrt* n))
    ((< n 4) 1)                          ; n is a bignum >= 4 below
    (t (let ((x (ash 1 (ceiling (integer-length n) 2))))   ; x >= sqrt(n)
         (loop
           (let ((y (ash (+ x (floor n x)) -1)))           ; (x + n/x)/2
             (if (< y x) (setq x y) (return x))))))))
