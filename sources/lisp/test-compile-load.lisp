(print '(compile-file #P"test-compile.lisp"))
(print (multiple-value-list (compile-file #P"test-compile.lisp")))

(print '(compile-file #P"test-load.lisp"))
(print (multiple-value-list (compile-file #P"test-load.lisp")))

(print '(load #P"test-compile.dx64fsl"))
(print (multiple-value-list (load #P"test-compile.dx64fsl")))

(print '(load #P"test-compile.lisp"))
(print (multiple-value-list (load #P"test-compile.lisp")))

(print '(load #P"test-load.dx64fsl"))
(print (multiple-value-list(load #P"test-load.dx64fsl")))

(print '(load #P"test-load.lisp"))
(print (multiple-value-list (load #P"test-load.lisp")))


#|

cl-user> (load "test-compile-load.lisp")

(compile-file #P"test-compile.lisp")
(in test-compile :compile-toplevel)
(*compile-file-pathname* #P"home:src;public;bocl;sources;lisp;test-compile.lisp.newest")
(*load-pathname* #P"/Users/pjb/src/public/bocl/sources/lisp/test-compile-load.lisp")
(in test-load :execute)
(*compile-file-pathname* #P"home:src;public;bocl;sources;lisp;test-compile.lisp.newest")
(*load-pathname* #P"/Users/pjb/src/public/bocl/sources/lisp/test-load.lisp")
(#P"/Users/pjb/src/public/bocl/sources/lisp/test-compile.dx64fsl" nil nil)
(compile-file #P"test-load.lisp")
(in test-load :compile-toplevel)
(*compile-file-pathname* #P"home:src;public;bocl;sources;lisp;test-load.lisp.newest")
(*load-pathname* #P"/Users/pjb/src/public/bocl/sources/lisp/test-compile-load.lisp")
(#P"/Users/pjb/src/public/bocl/sources/lisp/test-load.dx64fsl" nil nil)
(load #P"test-compile.dx64fsl")
(in test-compile :load-toplevel)
(*compile-file-pathname* nil)
(*load-pathname* #P"/Users/pjb/src/public/bocl/sources/lisp/test-compile.dx64fsl")
(in test-load :load-toplevel)
(*compile-file-pathname* nil)
(*load-pathname* #P"/Users/pjb/src/public/bocl/sources/lisp/test-load.dx64fsl")
(#P"/Users/pjb/src/public/bocl/sources/lisp/test-compile.dx64fsl")
(load #P"test-compile.lisp")
(in test-compile :execute)
(*compile-file-pathname* nil)
(*load-pathname* #P"/Users/pjb/src/public/bocl/sources/lisp/test-compile.lisp")
(in test-load :execute)
(*compile-file-pathname* nil)
(*load-pathname* #P"/Users/pjb/src/public/bocl/sources/lisp/test-load.lisp")
(#P"/Users/pjb/src/public/bocl/sources/lisp/test-compile.lisp")
(load #P"test-load.dx64fsl")
(in test-load :load-toplevel)
(*compile-file-pathname* nil)
(*load-pathname* #P"/Users/pjb/src/public/bocl/sources/lisp/test-load.dx64fsl")
(#P"/Users/pjb/src/public/bocl/sources/lisp/test-load.dx64fsl")
(load #P"test-load.lisp")
(in test-load :execute)
(*compile-file-pathname* nil)
(*load-pathname* #P"/Users/pjb/src/public/bocl/sources/lisp/test-load.lisp")
(#P"/Users/pjb/src/public/bocl/sources/lisp/test-load.lisp")
#P"/Users/pjb/src/public/bocl/sources/lisp/test-compile-load.lisp"
cl-user>
|#
