;;****************************************************************************
;;FILE:               cl-12-numbers.lisp
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
    

;; -----------------
;; - 12 -  Numbers -
;; -----------------

;; Primitives: Most of them.


;; System Class NUMBER
;; 
;; System Class COMPLEX
;; 
;; System Class REAL
;; 
;; System Class FLOAT
;; 
;; Type SHORT-FLOAT
;; Type SINGLE-FLOAT
;; Type DOUBLE-FLOAT
;; Type LONG-FLOAT
;; 
;; System Class RATIONAL
;; 
;; System Class RATIO
;; 
;; System Class INTEGER
;; 
;; Type SIGNED-BYTE
;; 
;; Type UNSIGNED-BYTE
;; 
;; Type Specifier MOD
;; 
;; Type BIT
;; 
;; Type FIXNUM
;; 
;; Type BIGNUM
;; 
;; 
;; Function =
;; Function /=
;; Function <
;; Function >
;; Function <=
;; Function >=
;; 
;; 
;; Function MAX
;; Function MIN
;; 
;; 
;; Function MINUSP
;; Function PLUSP
;; 
;; 
;; Function ZEROP
;; 
;; 
;; Function FLOOR
;; Function FFLOOR
;; Function CEILING
;; Function FCEILING
;; Function TRUNCATE
;; Function FTRUNCATE
;; Function ROUND
;; Function FROUND
;; 
;; 
;; Function SIN
;; Function COS
;; Function TAN
;; 
;; 
;; Function ASIN
;; Function ACOS
;; Function ATAN
;; 
;; 
;; Constant Variable PI
;; 
;; 
;; Function SINH
;; Function COSH
;; Function TANH
;; Function ASINH
;; Function ACOSH
;; Function ATANH
;; 
;; 
;; Function *
;; 
;; 
;; Function +
;; 
;; 
;; Function -
;; 
;; 
;; Function /
;; 
;; 
;; Function 1+
;; Function 1-
;; 
;; 
;; Function ABS
;; 
;; 
;; Function EVENP
;; Function ODDP
;; 
;; 
;; Function EXP
;; Function EXPT
;; 
;; 
;; Function GCD
;; 
;; 
;; Macro INCF
;; Macro DECF
;; 
;; 
;; Function LCM
;; 
;; 
;; Function LOG
;; 
;; 
;; Function MOD
;; Function REM
;; 
;; 
;; Function SIGNUM
;; 
;; 
;; Function SQRT
;; Function ISQRT
;; 
;; 
;; System Class RANDOM-STATE
;; 
;; 
;; Function MAKE-RANDOM-STATE
;; 
;; 
;; Function RANDOM
;; 
;; 
;; Function RANDOM-STATE-P
;; 
;; 
;; Variable *RANDOM-STATE*
;; 
;; 
;; Function NUMBERP
;; 
;; 
;; Function CIS
;; 
;; 
;; Function COMPLEX
;; 
;; 
;; Function COMPLEXP
;; 
;; 
;; Function CONJUGATE
;; 
;; 
;; Function PHASE
;; 
;; 
;; Function REALPART
;; Function IMAGPART
;; 
;; 
;; Function UPGRADED-COMPLEX-PART-TYPE
;; 
;; 
;; Function REALP
;; 
;; 
;; Function NUMERATOR
;; Function DENOMINATOR
;; 
;; 
;; Function RATIONAL
;; Function RATIONALIZE
;; 
;; 
;; Function RATIONALP
;; 
;; 
;; Function ASH
;; 
;; 
;; Function INTEGER-LENGTH                                    
;; 
;; 
;; Function INTEGERP
;; 
;; 
;; Function PARSE-INTEGER
;; 
;; 
;; Function BOOLE
;; 
;; 
;; Constant Variable BOOLE-1
;; Constant Variable BOOLE-2
;; Constant Variable BOOLE-AND
;; Constant Variable BOOLE-ANDC1
;; Constant Variable BOOLE-ANDC2
;; Constant Variable BOOLE-C1
;; Constant Variable BOOLE-C2
;; Constant Variable BOOLE-CLR
;; Constant Variable BOOLE-EQV
;; Constant Variable BOOLE-IOR
;; Constant Variable BOOLE-NAND
;; Constant Variable BOOLE-NOR
;; Constant Variable BOOLE-ORC1
;; Constant Variable BOOLE-ORC2
;; Constant Variable BOOLE-SET
;; Constant Variable BOOLE-XOR
;; 
;; 
;; Function LOGAND
;; Function LOGANDC1
;; Function LOGANDC2
;; Function LOGEQV
;; Function LOGIOR
;; Function LOGNAND
;; Function LOGNOR
;; Function LOGNOT
;; Function LOGORC1
;; Function LOGORC2
;; Function LOGXOR
;; 
;; 
;; Function LOGBITP
;; 
;; 
;; Function LOGCOUNT
;; 
;; 
;; Function LOGTEST
;; 
;; 
;; Function BYTE
;; Function BYTE-SIZE
;; Function BYTE-POSITION
;; 
;; 
;; Function DEPOSIT-FIELD
;; 
;; 
;; Function DPB
;; 
;; 
;; Accessor LDB
;; 
;; 
;; Function LDB-TEST
;; 
;; 
;; Accessor MASK-FIELD
;; 
;; 
;; Constant Variable MOST-POSITIVE-FIXNUM                      
;; Constant Variable MOST-NEGATIVE-FIXNUM                      
;; 
;; 
;; Function DECODE-FLOAT
;; Function SCALE-FLOAT
;; Function FLOAT-RADIX
;; Function FLOAT-SIGN
;; Function FLOAT-DIGITS
;; Function FLOAT-PRECISION
;; Function INTEGER-DECODE-FLOAT
;; 
;; 
;; Function FLOAT
;; 
;; 
;; Function FLOATP
;; 
;; 
;; Constant Variable MOST-POSITIVE-SHORT-FLOAT
;; Constant Variable LEAST-POSITIVE-SHORT-FLOAT
;; Constant Variable LEAST-POSITIVE-NORMALIZED-SHORT-FLOAT
;; Constant Variable MOST-POSITIVE-DOUBLE-FLOAT
;; Constant Variable LEAST-POSITIVE-DOUBLE-FLOAT
;; Constant Variable LEAST-POSITIVE-NORMALIZED-DOUBLE-FLOAT
;; Constant Variable MOST-POSITIVE-LONG-FLOAT
;; Constant Variable LEAST-POSITIVE-LONG-FLOAT
;; Constant Variable LEAST-POSITIVE-NORMALIZED-LONG-FLOAT
;; Constant Variable MOST-POSITIVE-SINGLE-FLOAT
;; Constant Variable LEAST-POSITIVE-SINGLE-FLOAT
;; Constant Variable LEAST-POSITIVE-NORMALIZED-SINGLE-FLOAT
;; Constant Variable MOST-NEGATIVE-SHORT-FLOAT
;; Constant Variable LEAST-NEGATIVE-SHORT-FLOAT
;; Constant Variable LEAST-NEGATIVE-NORMALIZED-SHORT-FLOAT
;; Constant Variable MOST-NEGATIVE-SINGLE-FLOAT
;; Constant Variable LEAST-NEGATIVE-SINGLE-FLOAT
;; Constant Variable LEAST-NEGATIVE-NORMALIZED-SINGLE-FLOAT
;; Constant Variable MOST-NEGATIVE-DOUBLE-FLOAT
;; Constant Variable LEAST-NEGATIVE-DOUBLE-FLOAT
;; Constant Variable LEAST-NEGATIVE-NORMALIZED-DOUBLE-FLOAT
;; Constant Variable MOST-NEGATIVE-LONG-FLOAT
;; Constant Variable LEAST-NEGATIVE-LONG-FLOAT
;; Constant Variable LEAST-NEGATIVE-NORMALIZED-LONG-FLOAT
;; 
;; 
;; Constant Variable SHORT-FLOAT-EPSILON
;; Constant Variable SHORT-FLOAT-NEGATIVE-EPSILON
;; Constant Variable SINGLE-FLOAT-EPSILON
;; Constant Variable SINGLE-FLOAT-NEGATIVE-EPSILON
;; Constant Variable DOUBLE-FLOAT-EPSILON
;; Constant Variable DOUBLE-FLOAT-NEGATIVE-EPSILON
;; Constant Variable LONG-FLOAT-EPSILON
;; Constant Variable LONG-FLOAT-NEGATIVE-EPSILON
;; 
;; 
;; Condition Type ARITHMETIC-ERROR
;; 
;; Function ARITHMETIC-ERROR-OPERANDS
;; Function ARITHMETIC-ERROR-OPERATION
;; 
;; 
;; Condition Type DIVISION-BY-ZERO
;; 
;; Condition Type FLOATING-POINT-INVALID-OPERATION
;; 
;; Condition Type FLOATING-POINT-INEXACT
;; 
;; Condition Type FLOATING-POINT-OVERFLOW
;; 
;; Condition Type FLOATING-POINT-UNDERFLOW

;;;; cl-12-numbers.lisp               -- 2004-01-26 14:59:06 -- pascal   ;;;;
