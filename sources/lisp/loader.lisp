(cl:in-package "COMMON-LISP-USER")

(eval-when (:compile-toplevel :load-toplevel)
  (error "This file should be loaded as source, not compiled"))

;;;----------------------------------------
(defmacro with-directory (directory-expression &body body)
  (let ((vsaved (gensym)))
    `(let ((,vsaved (uiop:getcwd)))
       (unwind-protect (progn
                         (uiop:chdir ,directory-expression)
                         (locally ,@body))
         (uiop:chdir (namestring ,vsaved))))))


(defvar *source-directory*
  (make-pathname :defaults *load-truename*
                 :name nil :type nil :version nil)
  "This source directory.")

(defvar *base-directory*
  (merge-pathnames (make-pathname :directory '(:relative :up "dependencies"))
                   *source-directory*
                   nil)

  "Base directory, where this source directory is, and where we store
the dependencies.")

;;;----------------------------------------

(defun clone-repository (repository test-file)
  (unless (probe-file (merge-pathnames test-file *base-directory* nil))
    (with-directory (namestring *base-directory*)
      (uiop:run-program (list "git" "clone" repository)))))

(defun setup-dependency (repository test-file)
  (clone-repository repository test-file)
  (setf asdf:*central-registry*
        (append (delete-duplicates
                 (mapcar (lambda (path)
                           (truename (make-pathname :name nil :type nil :version nil
                                                    :defaults path)))
                         (directory (merge-pathnames
                                     (make-pathname :directory '(:relative :wild-inferiors)
                                                    :name :wild :type "asd" :version nil)
                                     test-file)))
                 :test (function equalp))
                asdf:*central-registry*)))

;; (setup-dependency "git@framagit.org:patchwork/CoreMIDI.git"                  "../dependencies/CoreMIDI/coremidi.lisp")
;; (setup-dependency "git@framagit.org:abnotation/midi.git"                     "../dependencies/midi/midi.lisp")
;; (setup-dependency "git@framagit.org:com-informatimago/com-informatimago.git" "../dependencies/com-informatimago/com.informatimago.asd")
;; (push #P"./" asdf:*central-registry*)


;;;----------------------------------------
(pushnew *source-directory* asdf:*central-registry* :test (function equal))

(defun lls ()
  (dolist (source
           '("bocl-packages" "bocl" "kernel" "kernel-sim"
             "environment" "ct-register" "bocl-compiler"
             "minimal-compiler"
             "destructuring-bind"
             "eval"
             "macros"
             ;; "variables" "structures" "bignum"

             ;; "mc-test"
             ))
    (load (merge-pathnames ".lisp" source nil) :verbose t :print t)))

;; (ql:quickload "com.informatimago.bocl" :verbose t :explain t)
(lls)

;; (block abort
;;   (handler-bind
;;       ((condition
;;          #1=(lambda (condition)
;;               (let ((backtrace (ccl:backtrace-as-list)))
;;                 (format *error-output* "~S (~{~%  ~S~}~%  …~{~%  ~S~})~%"
;;                         (class-name (class-of condition))
;;                         (subseq backtrace 0 30)
;;                         ;; (mapcar (function first) (subseq backtrace 0 30))
;;                         (let ((len (length backtrace)))
;;                           (mapcar (function first) (subseq backtrace (- len 30)))))
;;                 (let ((signal (assoc 'signal backtrace)))
;;                   (format *error-output* "~S~%" signal)
;;                   ;; (inspect (second signal))
;;                   ))
;;               (return-from abort)))
;;        (ccl::stack-overflow-condition #1#))
;;     (load "macros.lisp" :verbose t :print t)))

