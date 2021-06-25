;;;; -*- mode:lisp;coding:utf-8 -*-

(asdf:defsystem "com.informatimago.bocl"
  :description "Bootstrap Common Lisp."
  :author "Pascal J. Bourguignon"
  :version "1.0.4"
  :license "AGPL3"
  :depends-on ("com.informatimago.common-lisp")
  #+asdf-unicode :encoding #+asdf-unicode :utf-8
  :components (
               (:file "bocl-packages"         :depends-on ())
               (:file "bocl"                  :depends-on ("bocl-packages"))

               (:file "kernel"                :depends-on ())
               (:file "kernel-sim"            :depends-on ("kernel"))

               (:file "environment"           :depends-on ())
               (:file "ct-register"           :depends-on ("kernel"
                                                           "environment"))

               (:file "bocl-compiler"         :depends-on ("environment"
                                                           "kernel"
                                                           "kernel-sim"))


               (:file "minimal-compiler"      :depends-on ("bocl-packages"
                                                           "bocl-compiler"
                                                           "environment"
                                                           "ct-register"))

               (:file "destructuring-bind"    :depends-on ("bocl-compiler"))

               (:file "macros"                :depends-on ("bocl-packages"
                                                           "bocl-compiler"
                                                           "ct-register"
                                                           "minimal-compiler"
                                                           "destructuring-bind"))

               (:file "mc-test"               :depends-on ("macros"
                                                           "minimal-compiler"))

               (:file "structures"            :depends-on ("macros"))
               (:file "bignum"                :depends-on ("macros"))
               (:file "variables"             :depends-on ("macros"))


               ;; (:file "eval"              :depends-on ("bocl-packages"))


               ;; (:file "cl-packages"       :depends-on ())
               ;; (:file "kernel"            :depends-on ())
               ;; (:file "metacircular"      :depends-on ())
               ))

;;;; THE END ;;;;
