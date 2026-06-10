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
