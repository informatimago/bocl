
(defpackage "COM.INFORMATIMAGO.BOCL.KERNEL"
  (:nicknames "KERNEL")
  (:use #|Nothing|#)
  (:export))

(defpackage "COM.INFORMATIMAGO.BOCL.ENVIRONMENT"
  (:nicknames "BOENV")
  (:use "COMMON-LISP")
  (:export "GLOBAL-ENVIRONMENT"))

(defpackage "COM.INFORMATIMAGO.BOCL.IMPLEMENTATION"
  (:nicknames "BOIMP")
  (:use "COMMON-LISP")
  (:shadow "EVAL")
  (:export))

(defpackage "COM.INFORMATIMAGO.BOCL"
  (:nicknames "BOCL")
  (:use "COMMON-LISP")
  (:shadow "EVAL"
           "TAGBODY" "GO")
  (:export))

