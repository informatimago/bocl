(defun cl-hyperspec-for-symbol (symbol-name)
  (interactive 
   (list (let ((completion-ignore-case t)
               (symbol-at-point (thing-at-point 'symbol)))
           (completing-read
            "Look up symbol in Common Lisp HyperSpec: "
            common-lisp-hyperspec-symbols #'boundp
            t symbol-at-point
            'common-lisp-hyperspec-history))))
  (mapcar
   (lambda (entry)
     (let ((browse-url-browser-function common-lisp-hyperspec-browser))
       (concat common-lisp-hyperspec-root "Body/"  entry)))
   (let ((symbol (intern-soft (downcase symbol-name)
                              common-lisp-hyperspec-symbols)))
     (if (and symbol (boundp symbol))
       (symbol-value symbol)
       (error "The symbol `%s' is not defined in Common Lisp"
              symbol-name))))
  );;cl-hyperspec-for-symbol



(defun pcd (start end)
  (interactive "r")
  (let ((mend (make-marker)))
    (set-marker mend end)
    (goto-char start)
    (while (re-search-forward "^\\([A-Z][ a-z]*\\) " (marker-position mend) t)
      (let ((qual (match-string 1)))
        (message "%s" qual)
        (beginning-of-line)
        (insert ";; ")
        (message "%s" (buffer-substring-no-properties
                       (progn (beginning-of-line) (point))
                       (progn (end-of-line) (point))))
        (while (re-search-forward "," (prog1 (progn (end-of-line) (point))
                                        (beginning-of-line)) t)
          (delete-region (match-beginning 0) (match-end 0))
          (insert (format "\n;; %s" qual))))
      (when (re-search-forward "^$"  (marker-position mend) t)
        (insert ";; ")))
    (set-marker mend nil))
  );;pcd



(show (delete-duplicates (cl-hyperspec-for-symbol "cddar") :test (function string=)))

==> ("http://thalassa.informatimago.com/local/lisp/HyperSpec/Body/f_car_c.htm")

(show (shell-command-to-string
 (format "lynx -dump %s" "http://thalassa.informatimago.com/local/lisp/HyperSpec/Body/f_car_c.htm")))



(dolist (dst '(
cl-01-introduction.lisp
cl-02-syntax.lisp
cl-03-evaluation-and-compilation.lisp
cl-04-types-and-classes.lisp
cl-05-data-and-control-flow.lisp
cl-06-iteration.lisp
cl-07-objects.lisp
cl-08-structures.lisp
cl-09-conditions.lisp
cl-10-symbols.lisp
cl-11-packages.lisp
cl-12-numbers.lisp
cl-13-characters.lisp
cl-15-arrays.lisp
cl-14-conses.lisp
cl-16-strings.lisp
cl-17-sequences.lisp
cl-18-hash-tables.lisp
cl-19-filenames.lisp
cl-20-files.lisp
cl-21-streams.lisp
cl-22-printer.lisp
cl-23-reader.lisp
cl-24-system-construction.lisp
cl-25-environment.lisp
))
  (shell-command (format "cp pdacl.lisp %s" dst)))


#||

Small foot print, but full implementation of Common-Lisp.

16 MB RAM,    64 MB RAM
64 MB CARD,  256 MB CARD.


 8 MB image+heap
 
(show (integer-to-base (* 8 1024 1024) 2))
(length "100000000000000000000000")

32 bit / 24 bit address : 16 Mword


symbol
number fixnum bignum  floating-point rational complex
character
cons
array
hash
stream


object
structure (object, list, vector)

type            immediate           memory
--------------  ------------------  ---------------------------------
fixnum          29+|000| #          array: 32bit
char            29+|001| #          array: 8bit (ISO-8859-1) or 16bit (UTF-16)
cons            29+|100| (cdr)      car;cdr
symbol          29+|101| (value)    name;value;function;package;plist
array           29+|110|            
hash            29+|111|


  CHAR-CODE
  CHAR-INT

  CAR
  CDR


  SYMBOL-NAME
  SYMBOL-PACKAGE
  SYMBOL-VALUE
  SYMBOL-FUNCTION  
  SYMBOL-PLIST  

fillptr:1;is-adjustable:1;is-displaced;1;filler:10;element-size:3;rank:16;
elements        ^data
dim0            fixnum
...
dimN            fixnum
fillptr         fixnum
displaced-to    array
disp-offset     fixnum
element-type    (or symbol cons)

element-size: 1 (bit)
              2 (integer 0 3)
              4 (integer 0 15)
              8 (integer 0 255)    base-char ISO-8859
             16 (integer 0 65535)  character UTF-16
             32 (integer 0 (1- (expt 2 32))) fixnum *
            
  ADJUSTABLE-ARRAY-P
  ARRAY-DIMENSIONS
  ARRAY-ELEMENT-TYPE
  ARRAY-HAS-FILL-POINTER-P
  FILL-POINTER 
  ARRAY-DISPLACEMENT
  ARRAY-ROW-MAJOR-INDEX




test              function
count             fixnum
size              fixnum
rehash-size       (or (integer 1 *) (float (1.0) *))
rehash-threshold  (real 0 1)
buckets           (array cons *)

          []->(key1 value1 key2 value2 key3 value3) all keys with same sxhash.


  HASH-TABLE-COUNT
  HASH-TABLE-REHASH-SIZE
  HASH-TABLE-REHASH-THRESHOLD
  HASH-TABLE-SIZE
  HASH-TABLE-TEST
  GETHASH
  MAPHASH


(let ((h (make-hash-table :test (function eq))))
  (setf (gethash '(a . b) h) 1)
  (setf (gethash (cons 'a  'b) h) 2)
  (setf (gethash (cons :a  :b) h) 3)
  (maphash (lambda (k v) (show k v)) h))

(show (sxhash '( b . a))(sxhash '( a . b)) (sxhash '( :a . :b)))
==> (10982616 4494651 4494651)
NIL


object
structure


fixnum 29   000
char   29   001
cons        010
symbol      011
array       100
hash        101
            110
            111


|........|.....................___|
|........|........................|



|--------.....................---|--------.....................---|



||#


#||

03-evaluation-and-compilation

  COMPILER-MACRO-FUNCTION MACRO-FUNCTION

  eval-when load-time-value quote symbol-macrolet locally the flet
  labels macrolet function let let* progv setq block catch go
  return-from tagbody throw unwind-protect if multiple-value-call
  multiple-value-prog1 progn


04-types-and-classes

  None  


05-data-and-control-flow

  NIL T APPLY FUNCTION-LAMBDA-EXPRESSION EQ EQL VALUES 

  flet labels macrolet function let let* progv setq block catch go
  return-from tagbody throw unwind-protect if multiple-value-call
  multiple-value-prog1 progn


06-iteration

  None


07-objects

  None

10-symbols

  SYMBOLP MAKE-SYMBOL  SYMBOL-FUNCTION  SYMBOL-NAME  SYMBOL-PACKAGE
  SYMBOL-PLIST  SYMBOL-VALUE MAKUNBOUND BOUNDP MAKFUNBOUND FBOUNDP


11-packages

  None


12-numbers

  Most of them.


13-characters

  CHARACTERP CHAR-CODE CHAR-INT CODE-CHAR


14-conses

  CONSP CONS RPLACA RPLACD CAR CDR


15-arrays

  MAKE-ARRAY ARRAYP ADJUST-ARRAY ADJUSTABLE-ARRAY-P ARRAY-DIMENSIONS
  ARRAY-ELEMENT-TYPE ARRAY-HAS-FILL-POINTER-P ARRAY-DISPLACEMENT
  FILL-POINTER ROW-MAJOR-AREF ARRAY-ROW-MAJOR-INDEX


16-strings

  None


17-sequences

  None


18-hash-tables

  MAKE-HASH-TABLE HASH-TABLE-P  HASH-TABLE-COUNT
  HASH-TABLE-REHASH-SIZE  HASH-TABLE-REHASH-THRESHOLD  HASH-TABLE-SIZE
  HASH-TABLE-TEST  GETHASH  REMHASH  MAPHASH SXHASH


19-filenames

  None


20-files

  DIRECTORY PROBE-FILE ENSURE-DIRECTORIES-EXIST TRUENAME FILE-AUTHOR
  FILE-WRITE-DATE RENAME-FILE DELETE-FILE


21-streams

  Not determined yet. Cf. underlying I/O?


22-printer

  None


23-reader

  None


24-system-construction

  None


25-environment

  SLEEP GET-UNIVERSAL-TIME ROOM LISP-IMPLEMENTATION-TYPE
  LISP-IMPLEMENTATION-VERSION SHORT-SITE-NAME LONG-SITE-NAME
  MACHINE-INSTANCE MACHINE-TYPE MACHINE-VERSION SOFTWARE-TYPE
  SOFTWARE-VERSION USER-HOMEDIR-PATHNAME



||#



#||

(let ((h (make-hash-table :test (function eql)))
      (m 0))
  (do-external-symbols (s "COMMON-LISP")
    (let ((l (length (string s))))
      (when (< m l) (setf m l))
      (incf (gethash l h 0))))
  (let ((a (make-array (list (1+ m)) :initial-element 0)))
    (maphash (lambda (k v) (setf (aref a k) v)) h)
    a))

(do ((a '(0 8 15 35 56 81 91 65 52 55 46 52 49 49 47 41 36 26 19 24 17 17 6 9 13 10 12 13 5 5 1 1  1 1 1 0 2 2 4) (cdr a))
     (i 0 (1+ i))
     (s 0))
    ((null a) s)
  (incf s (* i (car a))))

;;11034


||#

;;;; pdacl
                       -- 2003-12-28 20:50:45 -- pascal   ;;;;

;;;; pdacl.lisp                       -- 2003-12-29 05:01:00 -- pascal   ;;;;
