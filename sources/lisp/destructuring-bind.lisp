(in-package "COM.INFORMATIMAGO.BOCL.COMPILER")

;; reqvars::= var* 
;; 
;; optvars::= [&optional {var | (var [init-form [supplied-p-parameter]])}*] 
;; 
;; restvar::= [{&rest | &body} var] 
;; 
;; keyvars::= [&key {var | ({var | (keyword-name var)} [init-form [supplied-p-parameter]])}* 
;;             [&allow-other-keys]] 
;; 
;; auxvars::= [&aux {var | (var [init-form])}*] 
;; 
;; wholevar::= [&whole var] 
;; 
;; lambda-list::= (wholevar reqvars optvars restvar keyvars auxvars) | 
;;                (wholevar reqvars optvars . var) 



;; A DECLARATION declaration must suppress warnings about unrecognized
;; declarations of the kind that it declares. If an implementation does
;; not produce warnings about unrecognized declarations, it may safely
;; ignore this declaration. 
;; 
;; A NOTINLINE declaration must be recognized by any implementation that
;; supports inline functions or compiler macros in order to disable those
;; facilities. An implementation that does not use inline functions or
;; compiler macros may safely ignore this declaration. 
;; 
;; A SAFETY declaration that increases the current safety level must
;; always be recognized. An implementation that always processes code as
;; if safety were high may safely ignore this declaration. 
;; 
;; A SPECIAL declaration must be processed by all implementations. 

;; DECLARATION     IGNORE     SPECIAL  
;; DYNAMIC-EXTENT  INLINE     TYPE     
;; FTYPE           NOTINLINE           
;; IGNORABLE       OPTIMIZE            


(defun extract-declarations-for (variable decl)
  "Return a list of declarations concerning the VARIABLE extracted from
the list in DECL (which is mutated).
The declarations that are extracted are:
DYNAMIC-EXTEND, IGNORE, IGNORABLE, TYPE, SPECIAL
A type specifier can be used as a declaration identifier. (type-specifier var*) is taken as shorthand for (type type-specifier var*). 
"
  (let ((extracted '())
        (decls     '()))
    (dolist (declaration (car decl))
      (cond
        ((or (eql (car declaration) 'dynamic-extend)
             (eql (car declaration) 'ignore)
             (eql (car declaration) 'ignorable)
             (eql (car declaration) 'special))
         (if (find variable (cdr declaration))
             (let ((rest (remove variable (cdr declaration))))
               (push (list (car declaration) variable) extracted)
               (if rest
                   (push (cons (car declaration) rest) decls)))
             (push declaration decls)))
        ((eql (car declaration) 'type)
         (if (find variable (cddr declaration))
             (let ((rest (remove variable (cddr declaration))))
               (push (list (car declaration) (cadr declaration) variable) extracted)
               (if rest
                   (push (list (car declaration) (cadr declaration) rest) decls)))
             (push declaration decls)))
        ;; ((env:find-type (car declaration)))
        (t
         (push declaration decls))))
    (setf (car decl) decls)
    extracted))


(defvar *current-destructuring-lambda-list* nil)

(defun error-ll-expect-list (ll)
  (error "While parsing the destructuring-lambda-list ~S~
        ~%Invalid syntax, expected a list, got: ~S"
         *current-destructuring-lambda-list*
         ll))

(defun error-ll-missing-variable (ll key)
  (cl:declare (cl:ignore ll))
  (error "While parsing the destructuring-lambda-list ~S~
        ~%Invalid syntax, missing a variable name after ~A"
         *current-destructuring-lambda-list*
         key))

(defun error-ll-expect-symbol (ll token follows)
  (cl:declare (cl:ignore ll))
  (error "While parsing the destructuring-lambda-list ~S~
        ~%Invalid token ~S, expected a symbol~@[, or one of ~{~A~^, ~}~]."
         *current-destructuring-lambda-list*
         token follows))

(defun error-ll-duplicate-key (ll key)
  (cl:declare (cl:ignore ll))
  (error "While parsing the destructuring-lambda-list ~S~
        ~%Duplicate key ~A"
         *current-destructuring-lambda-list*
         key))

(defun error-ll-key-order (ll key after)
  (cl:declare (cl:ignore ll))
  (error "While parsing the destructuring-lambda-list ~S~
        ~%Invalid key order, ~A found after ~{~A~^ or~}."
         *current-destructuring-lambda-list*
         key after))

(defun error-too-many-elements (ll key)
  (error "While parsing the destructuring-lambda-list ~S~
        ~%Too many elements in a ~A clause: ~S"
         *current-destructuring-lambda-list*
         key ll))

(defun error-ll-dotted-list (ll)
  (error "While parsing the destructuring-lambda-list ~S~
        ~%Syntax error, dotted-list after &rest: ~A"
         *current-destructuring-lambda-list*
         ll))

(defun every-other (predicate list)
  (let ((result nil))
    (tagbody
     :loop
       (setq result (cond
                      ((null list)                    t)
                      ((atom list)                    nil)
                      ((null (cdr list))              nil)
                      ((funcall predicate (car list))
                       (setq list (cddr list))
                       (go :loop))
                      (t                              nil))))
    result))

(defun extract-keyword-argument (arguments key default)
  (let ((result default)
        (foundp nil)
        (new-args '())
        (current arguments))
    (tagbody
     :loop
       (cond
         ((null current))
         ((atom current)       (error "argument list is a dotted-list ~S" arguments))
         ((null (cdr current)) (error "odd number of arguments in ~S" arguments))
         ((eql key (car current))
          (setq foundp t
                result  (cadr current)
                current (cddr current)))
         (t
          (push (pop current) new-args)
          (push (pop current) new-args)
          (go :loop))))
    (values result foundp (nreverse new-args))))

(defun desgen/null (ll var decl body &optional allow-other-keys)
  (cl:declare (cl:ignore ll))
  (let ((body-form (if (car decl)
                       `(locally (declare ,@(car decl))
                          ,@body)
                       `(progn
                          ,@body)))
        (error-form `(error "~S can't be destructured against the lambda list ()~@[, because it contains more elements than expected~]."
                            ,var (listp ,var))))
    (if allow-other-keys
        `(if (not (every-other (function symbolp) ,var))
             ,error-form
             ,body-form)
        `(if (not (null ,var))
             ,error-form
             ,body-form))))

(defun desgen/rest-symbol (restvar var decl body)
  (let ((restdecl (extract-declarations-for restvar decl)))
    `(let ((,restvar ,var))
       ,@(when restdecl `((declare ,@restdecl)))
       ,@body)))

(defun desgen/a (ll var decl body)
  (cond
    ((null ll)                         (desgen/null     ll var decl body))
    ((atom ll)                         (error-ll-dotted-list ll))
    ((eql (car ll) '&optional)         (error-ll-key-order ll '&optional '(&aux)))
    ((eql (car ll) '&rest)             (error-ll-key-order ll '&rest     '(&aux)))
    ((eql (car ll) '&body)             (error-ll-key-order ll '&body     '(&aux)))
    ((eql (car ll) '&key)              (error-ll-key-order ll '&key      '(&aux)))
    ((eql (car ll) '&aux)              (error-ll-duplicate-key ll '&aux))
    ((eql (car ll) '&allow-other-keys) (error-ll-key-order ll '&allow-other-keys '(&aux)))
    ((consp (car ll))
     (let ((aux     (car ll))
           (auxvar  (caar ll))
           (auxdef  (cadar ll)))
       (cond
         ((not (symbolp auxvar))
          (error-ll-expect-symbol ll auxvar '()))
         ((null (cdr aux)))
         ((null (cddr aux))
          (setq auxdef (cadr aux)))
         (t
          (error-too-many-elements ll '&aux)))
       (let ((auxdecl  (extract-declarations-for auxvar decl)))
         `(let ((,auxvar  ,auxdef))
            ,@(when auxdecl
                `((declare ,@auxdecl)))
            ,(desgen/a (cdr ll) var decl body)))))
    ((symbolp (car ll))
     (let ((auxvar (car ll)))
       (let ((auxdecl (extract-declarations-for auxvar decl)))
         `(let ((,auxvar ,auxdecl))
            ,@(when auxdecl
                `((declare ,@auxdecl)))
            ,(desgen/a (cdr ll) var decl body)))))
    (t
     (error-ll-expect-symbol ll ll '(&aux)))))

(defun desgen/ka (ll var decl body &optional allow-other-keys)
  ;; parses a &key (which has been popped) and the optional following &aux.
  ;; and returns a destructuring-bind form expansion.
  (cond
    ((null ll)                         (desgen/null     ll var decl body allow-other-keys))
    ((atom ll)                         (error-ll-dotted-list ll))
    ((eql (car ll) '&optional)         (error-ll-key-order ll '&optional '(&key)))
    ((eql (car ll) '&rest)             (error-ll-key-order ll '&rest     '(&key)))
    ((eql (car ll) '&body)             (error-ll-key-order ll '&body     '(&key)))
    ((eql (car ll) '&key)              (error-ll-duplicate-key ll '&key))
    ((eql (car ll) '&aux)              (desgen/a   (cdr ll) var decl body))
    ((eql (car ll) '&allow-other-keys) (desgen/ka  (cdr ll) var decl body t))
    ((consp (car ll))
     (let ((key     (car ll))
           (keykey  (if (symbolp (caar ll))
                        (intern (string (caar ll)) "KEYWORD")
                        (first (caar ll))))
           (keyvar  (if (symbolp (caar ll))
                        (caar ll)
                        (second (caar ll))))
           (keydef  nil)
           (keyvarp nil))
       (cond
         ((not (symbolp keyvar))
          (error-ll-expect-symbol ll keyvar '()))
         ((null (cdr key)))
         ((null (cddr key))
          (setq keydef (cadr key)))
         ((not (symbolp (caddr key)))
          (error-ll-expect-symbol ll (caddr key) '()))
         ((null (cdddr key))
          (setq keydef  (cadr key))
          (setq keyvarp (caddr key)))
         (t
          (error-too-many-elements ll '&keyional)))
       (let ((keydecl  (extract-declarations-for keyvar decl))
             (keydeclp (when keyvarp (extract-declarations-for keyvarp decl)))
             (temp     (gensym)))
         `(let ((,temp (extract-keyword-argument ,var ',keykey ,keydef)))
            (let ((,keyvar  (car ,temp))
                  ,@(when keyvarp `((,keyvarp (cadr ,temp))))
                  (,var  (caddr ,temp)))
              ,@(when (or keydecl keydeclp)
                  `((declare ,@keydeclp ,@keydecl)))
              ,(desgen/ka (cdr ll) var decl body))))))
    ((symbolp (car ll))
     (let ((keykey  (intern (string (car ll)) "KEYWORD"))
           (keyvar  (car ll))
           (keydecl (extract-declarations-for (car ll) decl))
           (temp    (gensym)))
       `(let ((,temp (extract-keyword-argument ,var ',keykey nil)))
          (let ((,keyvar  (car ,temp))
                (,var  (caddr ,temp)))
            ,@(when keydecl
                `((declare ,@keydecl)))
            ,(desgen/ka (cdr ll) var decl body)))))
    (t
     (error-ll-expect-symbol ll ll '(&aux)))))

(defun desgen/opt-ka (ll var decl body)
  (cond
    ((null ll)                 (desgen/null     ll var decl body))
    ((atom ll)                 (error-ll-dotted-list ll))
    ((eql (car ll) '&optional) (error-ll-key-order ll '&optional '(&rest &body)))
    ((eql (car ll) '&rest)     (error-ll-duplicate-key ll '&rest))
    ((eql (car ll) '&body)     (error-ll-duplicate-key ll '&body))
    ((eql (car ll) '&key)      (desgen/ka  (cdr ll) var decl body))
    ((eql (car ll) '&aux)      (desgen/a   (cdr ll) var decl body))
    ((eql (car ll) '&allow-other-keys) (error-ll-key-order ll '&allow-other-keys '()))
    (t (error-too-many-elements ll '&rest))))

(defun desgen/rka (ll var decl body)
  ;; parses a &rest (which has been popped) and the optional following &key and &aux.
  ;; and returns a destructuring-bind form expansion.
  (cond
    ((null ll)                 (desgen/null     ll var decl body))
    ((atom ll)                 (error-ll-dotted-list ll))
    ((eql (car ll) '&optional) (error-ll-key-order ll '&optional '(&rest &body)))
    ((eql (car ll) '&rest)     (error-ll-duplicate-key ll '&rest))
    ((eql (car ll) '&body)     (error-ll-duplicate-key ll '&body))
    ((eql (car ll) '&key)      (desgen/ka  (cdr ll) var decl body))
    ((eql (car ll) '&aux)      (desgen/a   (cdr ll) var decl body))
    ((eql (car ll) '&allow-other-keys) (error-ll-key-order ll '&allow-other-keys '()))
    ((consp (car ll))
     (let ((placeholder (gensym)))
       ;; TODO: we'd have to substitute the declarations in front of the body placeholder…
       (let ((form (desgen/opt-wror (car ll) var decl placeholder)))
         (subst (desgen/opt-ka (cdr ll) var decl body)
                placeholder form))))
    ((symbolp (car ll))
     (let ((restvar (car ll)))
       (let ((restdecl (extract-declarations-for restvar decl)))
         `(let ((,restvar ,var))
            ,@(when restdecl `((declare ,@restdecl)))
            ,(desgen/opt-ka (cdr ll) var decl body)))))
    (t
     (error-ll-expect-symbol ll ll '(&key &aux)))))

(defun desgen/or (ll var decl body)
  ;; parses an &optional (which has been popped) and the optional following &rest or &body, &key and &aux.
  ;; and returns a destructuring-bind form expansion.
  (cond
    ((null ll)                 (desgen/null        ll var decl body))
    ((symbolp ll)              (desgen/rest-symbol ll var decl body)) 
    ((atom ll)                 (error-ll-expect-list ll))
    ((eql (car ll) '&optional) (error-ll-duplicate-key ll '&optional))
    ((eql (car ll) '&rest)     (desgen/rka (cdr ll) var decl body))
    ((eql (car ll) '&body)     (desgen/rka (cdr ll) var decl body))
    ((eql (car ll) '&key)      (desgen/ka  (cdr ll) var decl body))
    ((eql (car ll) '&aux)      (desgen/a   (cdr ll) var decl body))
    ((eql (car ll) '&allow-other-keys) (error-ll-key-order ll '&allow-other-keys '()))
    ((consp (car ll))
     (let ((opt     (car ll))
           (optvar  (caar ll))
           (optdef  nil)
           (optvarp nil))
       (cond
         ((not (symbolp optvar))
          (error-ll-expect-symbol ll optvar '()))
         ((null (cdr opt)))
         ((null (cddr opt))
          (setq optdef (cadr opt)))
         ((not (symbolp (caddr opt)))
          (error-ll-expect-symbol ll (caddr opt) '()))
         ((null (cdddr opt))
          (setq optdef  (cadr opt))
          (setq optvarp (caddr opt)))
         (t
          (error-too-many-elements ll '&optional)))
       (let ((optdecl  (extract-declarations-for optvar decl))
             (optdeclp (when optvarp
                         (extract-declarations-for optvarp decl))))
         `(let (,@(when optvarp
                    `((,optvarp (if ,var t nil))))
                (,optvar (if ,var (pop ,var) ,optdef)))
            ,@(when (or optdecl optdeclp)
                `((declare ,@optdeclp ,@optdecl)))
            ,(desgen/or (cdr ll) var decl body)))))
    ((symbolp (car ll))
     (let ((optvar (car ll)))
       (let ((optdecl (extract-declarations-for optvar decl)))
         `(let ((,optvar (pop ,var)))
            ,@(when optdecl
                `((declare ,@optdecl)))
            ,(desgen/or (cdr ll) var decl body)))))
    (t
     (error-ll-expect-symbol ll ll '(&rest &body &key &aux)))))

(defun desgen/ror (ll var decl body)
  ;; parses required vars and the optional following &optional,  &rest or &body, &key and &aux.
  ;; and returns a destructuring-bind form expansion.
  (cond
    ((null ll)                 (desgen/null        ll var decl body))
    ((symbolp ll)              (desgen/rest-symbol ll var decl body)) 
    ((atom ll)                 (error-ll-expect-list ll))
    ((eql (car ll) '&optional) (desgen/or    (cdr ll) var decl body))
    ((eql (car ll) '&rest)     (desgen/rka   (cdr ll) var decl body))
    ((eql (car ll) '&body)     (desgen/rka   (cdr ll) var decl body))
    ((eql (car ll) '&key)      (desgen/ka    (cdr ll) var decl body))
    ((eql (car ll) '&aux)      (desgen/a     (cdr ll) var decl body))
    ((eql (car ll) '&allow-other-keys) (error-ll-key-order ll '&allow-other-keys '()))
    ((consp (car ll))
     (let ((placeholder (gensym)))
       ;; TODO: we'd have to substitute the declarations in front of the body placeholder…
       (let ((form (desgen/opt-wror (car ll) var decl placeholder)))
         (subst (desgen/ror (cdr ll) var decl body)
                placeholder form))))
    ((symbolp (car ll))
     (let ((mandavar (car ll)))
       (let ((mandadecl (extract-declarations-for mandavar decl)))
         `(let ((,mandavar (if ,var
                               (pop ,var)
                               (error "Missing elements to fill the mandatory parameter ~S" ',(car ll)))))
            ,@(when mandadecl `((declare ,@mandadecl)))
            ,(desgen/ror (cdr ll) var decl body)))))
    (t
     (error-ll-expect-symbol ll ll '(&optional &rest &body &key &aux)))))

(defun desgen/wror (ll var decl body)
  ;; parses an optional &whole (popped) and a following lambda-list.
  ;; and returns a destructuring-bind form expansion.
  (cond
    ((null ll)     (desgen/null ll var decl body))
    ((atom ll)     (error-ll-dotted-list ll))
    ((symbolp (car ll))
     (let ((wholevar (car ll)))
       (let ((wholedecl (extract-declarations-for wholevar decl)))
         `(let ((,wholevar ,var))
            ,@(when wholedecl `((declare ,@wholedecl)))
            ,(desgen/ror (cdr ll) var decl body)))))
    (t
     (error-ll-missing-variable ll '&whole))))

(defun desgen/opt-wror (ll var decl body)
  (cond
    ((null ll)              (desgen/null ll       var decl body))
    ((atom ll)              (error-ll-dotted-list ll))
    ((eql (car ll) '&whole) (desgen/wror (cdr ll) var decl body))
    (t                      (desgen/ror  ll       var decl body))))

(defun generate-destructuring-list (ll expression declarations body)
  ;; DECLARATIONS is a list of declarations ((special foo) (type integer bar) …)
  (let ((*current-destructuring-lambda-list* ll)
        (var  (gensym))
        (decl (list declarations)))
    (cond
      ((null ll)
       `(let ((,var ,expression))
          ,(desgen/null ll var decl body)))
      ((atom ll)
       (error-ll-expect-list ll))
      (t
       `(let ((,var ,expression))
          ,(desgen/opt-wror ll var decl body))))))

