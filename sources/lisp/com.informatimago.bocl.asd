;;;; -*- mode:lisp;coding:utf-8 -*-

(asdf:defsystem "com.informatimago.bocl"
  :description "Bootstrap Common Lisp."
  :author "Pascal J. Bourguignon"
  :version "1.0.0"
  :license "AGPL3"
  :depends-on ("com.informatimago.common-lisp")
  #+asdf-unicode :encoding #+asdf-unicode :utf-8
  :components (
               (:file "bocl-packages"     :depends-on ())
               (:file "bocl"              :depends-on ())

               (:file "minimal-compiler"  :depends-on ("environment"))
               (:file "mc-macros"         :depends-on ("minimal-compiler"))
               (:file "mc-test"           :depends-on ("minimal-compiler"))

               (:file "environment"       :depends-on ("bocl-packages"))

               (:file "special-operators" :depends-on ("bocl-packages"))
               (:file "structures"        :depends-on ("bocl-packages"))
               (:file "bignum"            :depends-on ("bocl-packages"))

               
               (:file "eval"              :depends-on ("bocl-packages"))


               ;; (:file "cl-packages"       :depends-on ())
               ;; (:file "kernel"            :depends-on ())
               ;; (:file "metacircular"      :depends-on ())
               ))

;;;; THE END ;;;;
