;;;; -*- mode:lisp; coding:utf-8 -*-
;;;;**************************************************************************
;;;;FILE:               socket.lisp
;;;;LANGUAGE:           Common-Lisp
;;;;
;;;;DESCRIPTION
;;;;
;;;;    BOCL-SOCKET — a usocket-compatible BSD-socket library for bocl.
;;;;
;;;;    This package implements the public API of usocket
;;;;    (https://github.com/usocket/usocket) on top of the BOCL::%... kernel
;;;;    primitives defined in sources/socket.c.  It is nicknamed USOCKET so that
;;;;    code written against usocket loads unchanged.
;;;;
;;;;    Layering
;;;;    --------
;;;;    - The kernel primitives are thin wrappers over socket(2), bind(2),
;;;;      connect(2), accept(2), send(2)/recv(2), getaddrinfo(3), select(2), …
;;;;      They never signal: a failure returns a sentinel and stashes errno
;;;;      where (BOCL::%SOCKET-ERRNO) can read it.
;;;;    - This file owns the condition hierarchy: every failed primitive is
;;;;      turned into the appropriate USOCKET condition by SIGNAL-SOCKET-ERROR /
;;;;      SIGNAL-NS-ERROR, mapping errno (resp. the getaddrinfo EAI_* code) to a
;;;;      portable condition class via (BOCL::%ERRNO-KEYWORD n).
;;;;    - Raw payloads (sockaddr structures, datagram buffers, socket options)
;;;;      cross the C boundary as bocl STRINGs, which are octet arrays.  A
;;;;      connected stream socket is wrapped in an ordinary FILE-STREAM by
;;;;      (BOCL::%MAKE-SOCKET-STREAM …), so READ-BYTE / WRITE-BYTE / READ-CHAR /
;;;;      READ-SEQUENCE all work through the existing stream machinery.
;;;;
;;;;    All socket domains, types and protocols the platform supports are
;;;;    reachable: SOCKET-CONNECT / SOCKET-LISTEN cover the common AF_INET /
;;;;    AF_INET6 / AF_UNIX stream and datagram cases, and the underlying
;;;;    primitives accept arbitrary (DOMAIN TYPE PROTOCOL) triples and raw
;;;;    sockaddr octet strings for everything else.
;;;;**************************************************************************

(defpackage "BOCL-SOCKET"
  (:nicknames "USOCKET")
  (:use "COMMON-LISP")
  (:export
   ;; classes & accessors
   "USOCKET" "STREAM-USOCKET" "STREAM-SERVER-USOCKET" "DATAGRAM-USOCKET"
   "SOCKET" "SOCKET-STREAM" "SOCKET-STATE" "ELEMENT-TYPE" "CONNECTED-P"
   ;; constructors / operators
   "SOCKET-CONNECT" "SOCKET-LISTEN" "SOCKET-ACCEPT" "SOCKET-CLOSE"
   "SOCKET-SHUTDOWN" "SOCKET-SEND" "SOCKET-RECEIVE"
   "GET-LOCAL-ADDRESS" "GET-PEER-ADDRESS" "GET-LOCAL-PORT" "GET-PEER-PORT"
   "GET-LOCAL-NAME" "GET-PEER-NAME"
   ;; waiting / multiplexing
   "WAIT-FOR-INPUT" "MAKE-WAIT-LIST" "ADD-WAITER" "REMOVE-WAITER"
   "REMOVE-ALL-WAITERS" "WAIT-LIST" "WAIT-LIST-WAITERS"
   ;; conditions — generic
   "SOCKET-CONDITION" "SOCKET-ERROR" "SOCKET-WARNING" "UNKNOWN-CONDITION"
   "UNKNOWN-ERROR" "UNSUPPORTED" "USOCKET-SOCKET"
   ;; conditions — specific
   "ADDRESS-IN-USE-ERROR" "ADDRESS-NOT-AVAILABLE-ERROR"
   "BAD-FILE-DESCRIPTOR-ERROR" "CONNECTION-REFUSED-ERROR"
   "CONNECTION-ABORTED-ERROR" "CONNECTION-RESET-ERROR"
   "INVALID-ARGUMENT-ERROR" "NO-BUFFERS-ERROR"
   "OPERATION-NOT-PERMITTED-ERROR" "OPERATION-NOT-SUPPORTED-ERROR"
   "PROTOCOL-NOT-SUPPORTED-ERROR" "SOCKET-TYPE-NOT-SUPPORTED-ERROR"
   "NETWORK-UNREACHABLE-ERROR" "NETWORK-DOWN-ERROR" "NETWORK-RESET-ERROR"
   "HOST-DOWN-ERROR" "HOST-UNREACHABLE-ERROR" "SHUTDOWN-ERROR"
   "TIMEOUT-ERROR" "DEADLINE-TIMEOUT-ERROR"
   ;; conditions — name resolution
   "NS-CONDITION" "NS-ERROR" "NS-TRY-AGAIN-CONDITION" "NS-TRY-AGAIN-ERROR"
   "NS-HOST-NOT-FOUND-ERROR" "NS-NO-RECOVERY-ERROR" "NS-UNKNOWN-ERROR"
   "WITH-MAPPED-CONDITIONS"
   ;; macros
   "WITH-CONNECTED-SOCKET" "WITH-CLIENT-SOCKET"
   "WITH-SERVER-SOCKET" "WITH-SOCKET-LISTENER"
   ;; address utilities
   "HOST-BYTE-ORDER" "HOST-TO-HBO" "HBO-TO-DOTTED-QUAD" "HBO-TO-VECTOR-QUAD"
   "DOTTED-QUAD-TO-VECTOR-QUAD" "VECTOR-QUAD-TO-DOTTED-QUAD"
   "VECTOR-TO-IPV6-HOST" "IPV6-HOST-TO-VECTOR"
   "IP=" "IP/=" "IP-FROM-OCTET-BUFFER"
   ;; misc
   "*REMOTE-HOST*" "*REMOTE-PORT*" "*VERSION*"
   "*WILDCARD-HOST*" "+MAX-DATAGRAM-PACKET-SIZE+"))

(in-package "BOCL-SOCKET")

(defparameter *version* "bocl-socket 1.0 (usocket-compatible)")

;;; ======================================================================
;;; Constants pulled from the platform headers via (BOCL::%SOCKET-CONSTANTS)
;;; ======================================================================

(defparameter *socket-constants* (bocl::%socket-constants)
  "Alist (keyword . integer) of every socket constant exported by the kernel.")

(defun socket-constant (keyword)
  "The platform integer value of the named socket constant, or NIL if the
platform does not define it."
  (cdr (assoc keyword *socket-constants*)))

(defmacro define-socket-constants (&rest names)
  "Bind a lexical-looking +NAME+ parameter for each socket constant NAME."
  `(progn
     ,@(mapcar (lambda (name)
                 (let* ((kw   (intern (symbol-name name) :keyword))
                        (var  (intern (format nil "+~A+" (symbol-name name)))))
                   `(defparameter ,var (socket-constant ,kw))))
               names)))

(define-socket-constants
  af_unspec af_inet af_inet6 af_unix
  sock_stream sock_dgram sock_raw sock_seqpacket
  ipproto_ip ipproto_tcp ipproto_udp ipproto_ipv6
  sol_socket so_reuseaddr so_keepalive so_broadcast so_error
  so_rcvbuf so_sndbuf so_rcvtimeo so_sndtimeo
  tcp_nodelay
  msg_oob msg_peek msg_dontroute msg_waitall
  shut_rd shut_wr shut_rdwr
  ai_passive ai_canonname ai_numerichost ai_numericserv ai_addrconfig
  ni_numerichost ni_numericserv ni_namereqd ni_dgram)

;; MSG_NOSIGNAL is Linux-specific; fall back to 0 where absent so SEND never
;; raises SIGPIPE-by-omission errors on platforms that lack it.
(defparameter +msg-nosignal+ (or (socket-constant :msg_nosignal) 0))

(defparameter +max-datagram-packet-size+ 65507
  "The maximum size of a UDP datagram payload (IPv4).")

(defparameter *wildcard-host* #(0 0 0 0))
(defparameter *remote-host* nil)
(defparameter *remote-port* nil)

;;; ======================================================================
;;; Condition hierarchy (mirrors usocket/condition.lisp)
;;; ======================================================================

(define-condition socket-condition (condition)
  ((socket :initarg :socket :initform nil :accessor usocket-socket)))

(define-condition socket-error (socket-condition error)
  ((errno   :initarg :errno   :initform nil :reader socket-error-errno)
   (syscall :initarg :syscall :initform nil :reader socket-error-syscall))
  (:report (lambda (c stream)
             (format stream "Socket error~@[ in ~A~]~@[: ~A~]"
                     (socket-error-syscall c)
                     (let ((e (socket-error-errno c)))
                       (and e (bocl::%strerror e)))))))

(define-condition socket-warning (socket-condition warning) ())

(macrolet ((deferr (name)
             `(define-condition ,name (socket-error) ())))
  (deferr address-in-use-error)
  (deferr address-not-available-error)
  (deferr bad-file-descriptor-error)
  (deferr connection-refused-error)
  (deferr connection-aborted-error)
  (deferr connection-reset-error)
  (deferr invalid-argument-error)
  (deferr no-buffers-error)
  (deferr operation-not-permitted-error)
  (deferr operation-not-supported-error)
  (deferr protocol-not-supported-error)
  (deferr socket-type-not-supported-error)
  (deferr network-unreachable-error)
  (deferr network-down-error)
  (deferr network-reset-error)
  (deferr host-down-error)
  (deferr host-unreachable-error)
  (deferr shutdown-error)
  (deferr timeout-error)
  (deferr deadline-timeout-error)
  (deferr unknown-error))

(define-condition unknown-condition (socket-condition) ())

(define-condition unsupported (error)
  ((feature :initarg :feature :reader unsupported-feature)
   (context :initarg :context :reader unsupported-context))
  (:report (lambda (c stream)
             (format stream "Unsupported usocket feature: ~A~@[ in ~A~]"
                     (unsupported-feature c) (unsupported-context c)))))

;;; --- name-resolution conditions ---------------------------------------

(define-condition ns-condition (condition)
  ((host-or-ip :initarg :host-or-ip :initform nil :reader ns-host-or-ip)))

(define-condition ns-error (ns-condition error)
  ((errno :initarg :errno :initform nil :reader ns-error-errno))
  (:report (lambda (c stream)
             (format stream "Name-resolution error~@[ for ~A~]~@[: ~A~]"
                     (ns-host-or-ip c)
                     (let ((e (ns-error-errno c)))
                       (and e (bocl::%gai-strerror e)))))))

(define-condition ns-host-not-found-error (ns-error) ())
(define-condition ns-no-recovery-error (ns-error) ())
(define-condition ns-unknown-error (ns-error) ())
(define-condition ns-try-again-condition (ns-condition) ())
(define-condition ns-try-again-error (ns-error) ())

;;; --- errno -> condition mapping ---------------------------------------

(defparameter *errno->condition*
  '((:EADDRINUSE      . address-in-use-error)
    (:EADDRNOTAVAIL   . address-not-available-error)
    (:EBADF           . bad-file-descriptor-error)
    (:ECONNREFUSED    . connection-refused-error)
    (:ECONNABORTED    . connection-aborted-error)
    (:ECONNRESET      . connection-reset-error)
    (:EINVAL          . invalid-argument-error)
    (:ENOBUFS         . no-buffers-error)
    (:EPERM           . operation-not-permitted-error)
    (:EACCES          . operation-not-permitted-error)
    (:EOPNOTSUPP      . operation-not-supported-error)
    (:EPROTONOSUPPORT . protocol-not-supported-error)
    (:EPROTOTYPE      . socket-type-not-supported-error)
    (:ENETUNREACH     . network-unreachable-error)
    (:ENETDOWN        . network-down-error)
    (:ENETRESET       . network-reset-error)
    (:EHOSTDOWN       . host-down-error)
    (:EHOSTUNREACH    . host-unreachable-error)
    (:ESHUTDOWN       . shutdown-error)
    (:EPIPE           . shutdown-error)
    (:ETIMEDOUT       . timeout-error))
  "Maps an errno keyword (from BOCL::%ERRNO-KEYWORD) to a usocket condition.")

(defun errno-condition-class (errno)
  "The usocket condition class name for the integer ERRNO."
  (or (cdr (assoc (bocl::%errno-keyword errno) *errno->condition*))
      'unknown-error))

(defun signal-socket-error (syscall &optional socket (errno (bocl::%socket-errno)))
  "Signal the usocket condition matching the current (or given) ERRNO."
  (error (errno-condition-class errno)
         :socket socket :errno errno :syscall syscall))

;;; --- getaddrinfo EAI_* -> ns-condition mapping ------------------------

(defparameter *gai->condition*
  ;; Built from the platform's EAI_* values so it is correct on any libc.
  (remove nil
          (list (let ((v (socket-constant :eai_noname)))
                  (and v (cons v 'ns-host-not-found-error)))
                (let ((v (socket-constant :eai_nodata)))
                  (and v (cons v 'ns-host-not-found-error)))
                (let ((v (socket-constant :eai_again)))
                  (and v (cons v 'ns-try-again-error)))
                (let ((v (socket-constant :eai_fail)))
                  (and v (cons v 'ns-no-recovery-error))))))

(defun signal-ns-error (host-or-ip &optional (code (bocl::%gai-errno)))
  "Signal the name-resolution condition matching the getaddrinfo CODE."
  (error (or (cdr (assoc code *gai->condition*)) 'ns-unknown-error)
         :host-or-ip host-or-ip :errno code))

;;; usocket's WITH-MAPPED-CONDITIONS maps implementation-specific errors to the
;;; portable hierarchy.  Here the primitives already raise portable conditions,
;;; so the macro is an identity wrapper kept for source compatibility.
(defmacro with-mapped-conditions ((&optional socket host) &body body)
  (declare (ignore socket host))
  `(progn ,@body))

;;; ======================================================================
;;; The usocket classes
;;; ======================================================================

(defclass usocket ()
  ((socket    :initarg :socket    :accessor socket
              :documentation "The underlying integer file descriptor.")
   (wait-list :initarg :wait-list :initform nil :accessor wait-list)
   (state     :initform nil :accessor socket-state
              :documentation "NIL, :READ, :WRITE or :READ-WRITE after WAIT-FOR-INPUT."))
  (:documentation "The common superclass of all usocket objects."))

(defclass stream-usocket (usocket)
  ((stream :initarg :stream :accessor socket-stream))
  (:documentation "A connected stream (e.g. TCP) socket and its bidirectional stream."))

(defclass stream-server-usocket (usocket)
  ((element-type :initarg :element-type :initform 'character :reader element-type))
  (:documentation "A passive (listening) stream socket; SOCKET-ACCEPT yields clients."))

(defclass datagram-usocket (usocket)
  ((connected-p :initarg :connected-p :initform nil :accessor connected-p))
  (:documentation "A datagram (e.g. UDP) socket."))

;;; ======================================================================
;;; Address parsing & formatting utilities
;;; ======================================================================

(defun dotted-quad-to-vector-quad (string)
  "\"127.0.0.1\" -> #(127 0 0 1)."
  (let ((parts (loop with start = 0
                     for dot = (position #\. string :start start)
                     collect (parse-integer string :start start :end (or dot (length string)))
                     while dot do (setf start (1+ dot)))))
    (coerce parts 'vector)))

(defun vector-quad-to-dotted-quad (vector)
  "#(127 0 0 1) -> \"127.0.0.1\"."
  (format nil "~D.~D.~D.~D"
          (aref vector 0) (aref vector 1) (aref vector 2) (aref vector 3)))

(defun host-byte-order (address)
  "ADDRESS (a dotted-quad string, a vector-quad, or an integer) -> a 32-bit
integer in host byte order."
  (etypecase address
    (integer address)
    (string  (host-byte-order (dotted-quad-to-vector-quad address)))
    (vector  (logior (ash (aref address 0) 24) (ash (aref address 1) 16)
                     (ash (aref address 2)  8) (aref address 3)))))

(setf (symbol-function 'host-to-hbo) #'host-byte-order)

(defun hbo-to-dotted-quad (integer)
  "A host-byte-order INTEGER -> \"a.b.c.d\"."
  (format nil "~D.~D.~D.~D"
          (ldb (byte 8 24) integer) (ldb (byte 8 16) integer)
          (ldb (byte 8  8) integer) (ldb (byte 8 0) integer)))

(defun hbo-to-vector-quad (integer)
  "A host-byte-order INTEGER -> #(a b c d)."
  (vector (ldb (byte 8 24) integer) (ldb (byte 8 16) integer)
          (ldb (byte 8  8) integer) (ldb (byte 8 0) integer)))

(defun ipv6-host-to-vector (string)
  "An IPv6 textual address -> a 16-element octet vector."
  (let ((packed (bocl::%inet-pton +af_inet6+ string)))
    (unless packed (signal-ns-error string))
    (let ((v (make-array 16)))
      (dotimes (i 16) (setf (aref v i) (char-code (char packed i))))
      v)))

(defun vector-to-ipv6-host (vector)
  "A 16-element octet vector -> a canonical IPv6 textual address."
  (bocl::%inet-ntop +af_inet6+ (octets->string vector 16 0)))

(defun ip= (a b)
  "True if the two host designators denote the same address."
  (cond ((and (vectorp a) (vectorp b))
         (and (= (length a) (length b)) (every #'= a b)))
        ((and (stringp a) (stringp b)) (string= a b))
        (t (equal a b))))

(defun ip/= (a b) (not (ip= a b)))

;;; ----------------------------------------------------------------------
;;; Octet-buffer <-> bocl-string transport helpers.  bocl STRINGs are octet
;;; arrays (one char = one byte 0..255), so CODE-CHAR / CHAR-CODE move bytes.
;;; ----------------------------------------------------------------------

(defun octets->string (buffer length offset)
  "Build a LENGTH-byte bocl string from BUFFER (a sequence of octets) starting
at OFFSET, for handing to the byte-moving primitives."
  (let ((s (make-string length)))
    (dotimes (i length s)
      (setf (char s i) (code-char (logand #xff (elt buffer (+ offset i))))))))

(defun string->octets (string buffer length offset)
  "Copy the bytes of the octet STRING into BUFFER at OFFSET; returns BUFFER (or
a freshly-consed simple-vector when BUFFER is NIL)."
  (let ((buf (or buffer (make-array length))))
    (dotimes (i length buf)
      (setf (elt buf (+ offset i)) (char-code (char string i))))))

;;; ----------------------------------------------------------------------
;;; sockaddr <-> (host port)
;;; ----------------------------------------------------------------------

(defun sockaddr-host+port (sockaddr)
  "Parse the opaque SOCKADDR octet string into (VALUES host port), host being a
vector-quad for AF_INET, a 16-octet vector for AF_INET6, or a namestring for
other families."
  (when (null sockaddr) (return-from sockaddr-host+port (values nil nil)))
  (let ((family (bocl::%sockaddr-family sockaddr)))
    (cond
      ((eql family +af_inet+)
       (multiple-value-bind (host serv)
           (bocl::%getnameinfo sockaddr (logior +ni_numerichost+ +ni_numericserv+))
         (values (and host (dotted-quad-to-vector-quad host))
                 (and serv (parse-integer serv)))))
      ((eql family +af_inet6+)
       (multiple-value-bind (host serv)
           (bocl::%getnameinfo sockaddr (logior +ni_numerichost+ +ni_numericserv+))
         (values (and host (ignore-errors (ipv6-host-to-vector host)))
                 (and serv (parse-integer serv)))))
      (t
       (multiple-value-bind (host serv)
           (bocl::%getnameinfo sockaddr (logior +ni_numerichost+ +ni_numericserv+))
         (values host (and serv (ignore-errors (parse-integer serv)))))))))

(defun host-designator-string (host)
  "Render a usocket HOST designator (string / vector-quad / ipv6-vector /
host-byte-order integer) as a string suitable for getaddrinfo."
  (etypecase host
    (string host)
    (integer (hbo-to-dotted-quad host))
    (vector (case (length host)
              (4  (vector-quad-to-dotted-quad host))
              (16 (vector-to-ipv6-host host))
              (t  (error 'invalid-argument-error :syscall "host"))))))

(defun resolve (host port &key (family +af_unspec+) (socktype +sock_stream+)
                               (flags 0) passive)
  "Resolve HOST/PORT to a list of getaddrinfo entries
\(family socktype protocol sockaddr canonname\), signalling an NS-ERROR on
failure.  HOST or PORT may be NIL."
  (let* ((node (and host (host-designator-string host)))
         (serv (and port (princ-to-string port)))
         (entries (bocl::%getaddrinfo node serv family socktype 0
                                      (logior flags (if passive +ai_passive+ 0)))))
    (or entries (signal-ns-error host))))

;;; ======================================================================
;;; SOCKET-CONNECT
;;; ======================================================================

(defun %set-nodelay (fd)
  (bocl::%setsockopt-int fd +ipproto_tcp+ +tcp_nodelay+ 1))

(defun %connect-with-timeout (fd sockaddr timeout)
  "connect(2) FD to SOCKADDR, honouring TIMEOUT seconds (a real, NIL = block).
Signals the appropriate condition on failure."
  (cond
    ((null timeout)
     (when (minusp (bocl::%connect fd sockaddr)) (signal-socket-error "connect")))
    (t
     (bocl::%set-nonblocking fd t)
     (let ((rc (bocl::%connect fd sockaddr)))
       (if (zerop rc)
           (bocl::%set-nonblocking fd nil)
           (let ((kw (bocl::%errno-keyword (bocl::%socket-errno))))
             (if (member kw '(:einprogress :ewouldblock :eagain))
                 (multiple-value-bind (r w e)
                     (bocl::%select nil (list fd) nil (round (* timeout 1000)))
                   (declare (ignore r e))
                   (if (member fd w)
                       (let ((so-error (bocl::%getsockopt-int fd +sol_socket+ +so_error+)))
                         (bocl::%set-nonblocking fd nil)
                         (when (and so-error (plusp so-error))
                           (signal-socket-error "connect" nil so-error)))
                       (progn (bocl::%close-fd fd)
                              (error 'timeout-error :syscall "connect"))))
                 (signal-socket-error "connect"))))))))

(defun socket-connect (host port &key (protocol :stream) (element-type 'character)
                                      timeout deadline (nodelay t)
                                      local-host local-port)
  "Connect to HOST:PORT.  PROTOCOL is :STREAM (TCP, returns a STREAM-USOCKET) or
:DATAGRAM (UDP, returns a DATAGRAM-USOCKET).  For :DATAGRAM a NIL HOST leaves the
socket unconnected.  Supported keys: ELEMENT-TYPE, TIMEOUT (seconds), NODELAY,
LOCAL-HOST, LOCAL-PORT."
  (declare (ignore deadline))
  (ecase protocol
    (:stream
     (let ((entries (resolve host port :family +af_unspec+ :socktype +sock_stream+))
           (last-error nil))
       (dolist (e entries (if last-error (error last-error)
                              (signal-socket-error "connect")))
         (destructuring-bind (family socktype proto sockaddr canon) e
           (declare (ignore canon))
           (let ((fd (bocl::%socket family socktype proto)))
             (when (minusp fd) (setf last-error (make-condition 'unknown-error
                                                  :errno (bocl::%socket-errno)
                                                  :syscall "socket")))
             (unless (minusp fd)
               (when (or local-host local-port)
                 (let ((la (fourth (first (resolve (or local-host "0.0.0.0")
                                                   (or local-port 0)
                                                   :family family
                                                   :socktype socktype :passive t)))))
                   (bocl::%bind fd la)))
               (handler-case
                   (progn
                     (%connect-with-timeout fd sockaddr timeout)
                     (when (and nodelay (eql proto +ipproto_tcp+)) (%set-nodelay fd))
                     (let ((stream (bocl::%make-socket-stream fd :io element-type)))
                       (return
                         (make-instance 'stream-usocket :socket fd :stream stream))))
                 (socket-error (c)
                   (bocl::%close-fd fd)
                   (setf last-error c)))))))))
    (:datagram
     (if host
         (let ((e (first (resolve host port :family +af_unspec+ :socktype +sock_dgram+))))
           (destructuring-bind (family socktype proto sockaddr canon) e
             (declare (ignore canon))
             (let ((fd (bocl::%socket family socktype proto)))
               (when (minusp fd) (signal-socket-error "socket"))
               (when (or local-host local-port)
                 (let ((la (fourth (first (resolve (or local-host "0.0.0.0")
                                                   (or local-port 0)
                                                   :family family
                                                   :socktype socktype :passive t)))))
                   (bocl::%bind fd la)))
               (when (minusp (bocl::%connect fd sockaddr))
                 (bocl::%close-fd fd) (signal-socket-error "connect"))
               (make-instance 'datagram-usocket :socket fd :connected-p t))))
         ;; unconnected datagram socket, optionally bound to a local address
         (let* ((family (if (and local-host (vectorp local-host)
                                 (= (length local-host) 16))
                            +af_inet6+ +af_inet+))
                (fd (bocl::%socket family +sock_dgram+ 0)))
           (when (minusp fd) (signal-socket-error "socket"))
           (when (or local-host local-port)
             (let ((la (fourth (first (resolve (or local-host "0.0.0.0")
                                               (or local-port 0)
                                               :family family
                                               :socktype +sock_dgram+ :passive t)))))
               (bocl::%bind fd la)))
           (make-instance 'datagram-usocket :socket fd :connected-p nil))))))

;;; ======================================================================
;;; SOCKET-LISTEN / SOCKET-ACCEPT
;;; ======================================================================

(defun socket-listen (host port &key reuseaddress (reuse-address nil reuse-supplied)
                                      (backlog 5) (element-type 'character))
  "Bind a passive stream socket to HOST:PORT and listen.  HOST may be a
hostname, dotted-quad/vector address, or *WILDCARD-HOST*.  Returns a
STREAM-SERVER-USOCKET."
  (let* ((reuse (if reuse-supplied reuse-address reuseaddress))
         (entries (resolve host port :family +af_unspec+ :socktype +sock_stream+
                                     :passive t))
         (last-error nil))
    (dolist (e entries (if last-error (error last-error) (signal-socket-error "bind")))
      (destructuring-bind (family socktype proto sockaddr canon) e
        (declare (ignore canon))
        (let ((fd (bocl::%socket family socktype proto)))
          (unless (minusp fd)
            (when reuse (bocl::%setsockopt-int fd +sol_socket+ +so_reuseaddr+ 1))
            (cond
              ((minusp (bocl::%bind fd sockaddr))
               (setf last-error (make-condition (errno-condition-class (bocl::%socket-errno))
                                  :errno (bocl::%socket-errno) :syscall "bind"))
               (bocl::%close-fd fd))
              ((minusp (bocl::%listen fd backlog))
               (setf last-error (make-condition (errno-condition-class (bocl::%socket-errno))
                                  :errno (bocl::%socket-errno) :syscall "listen"))
               (bocl::%close-fd fd))
              (t
               (return (make-instance 'stream-server-usocket
                                      :socket fd :element-type element-type))))))))))

(defun socket-accept (socket &key element-type)
  "Accept a pending connection on the server SOCKET, returning a STREAM-USOCKET."
  (multiple-value-bind (fd peer) (bocl::%accept (socket socket))
    (declare (ignore peer))
    (when (minusp fd) (signal-socket-error "accept" socket))
    (let* ((etype (or element-type
                      (and (typep socket 'stream-server-usocket) (element-type socket))
                      'character))
           (stream (bocl::%make-socket-stream fd :io etype)))
      (make-instance 'stream-usocket :socket fd :stream stream))))

;;; ======================================================================
;;; SOCKET-CLOSE / SOCKET-SHUTDOWN
;;; ======================================================================

(defgeneric socket-close (socket)
  (:documentation "Close SOCKET, releasing its file descriptor."))

(defmethod socket-close ((socket stream-usocket))
  (when (socket-stream socket)
    (ignore-errors (close (socket-stream socket))))  ; closes the underlying fd
  (setf (socket socket) nil)
  t)

(defmethod socket-close ((socket usocket))
  (when (socket socket)
    (bocl::%close-fd (socket socket))
    (setf (socket socket) nil))
  t)

(defmethod socket-close ((socket stream))   ; tolerate a bare stream
  (close socket))

(defun socket-shutdown (socket direction)
  "Shut down part of a duplex SOCKET.  DIRECTION is :INPUT, :OUTPUT or :IO."
  (let ((how (ecase direction
               (:input  +shut_rd+)
               (:output +shut_wr+)
               ((:io :input-output) +shut_rdwr+))))
    (when (minusp (bocl::%shutdown (socket socket) how))
      (signal-socket-error "shutdown" socket))
    t))

;;; ======================================================================
;;; Datagram I/O: SOCKET-SEND / SOCKET-RECEIVE
;;; ======================================================================

(defun socket-send (socket buffer length &key host port (offset 0))
  "Send LENGTH octets of BUFFER (from OFFSET) on the datagram SOCKET.  When HOST
and PORT are given, send to that address (sendto); otherwise the socket must be
connected.  Returns the number of octets actually sent."
  (let* ((len (or length (- (length buffer) offset)))
         (data (octets->string buffer len offset))
         (n (if (and host port)
                (let ((sa (fourth (first (resolve host port :family +af_unspec+
                                                            :socktype +sock_dgram+)))))
                  (bocl::%sendto (socket socket) data +msg-nosignal+ sa))
                (bocl::%send (socket socket) data +msg-nosignal+))))
    (when (minusp n) (signal-socket-error "send" socket))
    n))

(defun socket-receive (socket buffer length &key)
  "Receive a datagram on SOCKET into BUFFER (allocated if NIL), at most LENGTH
octets.  Returns (VALUES buffer size remote-host remote-port)."
  (let ((want (or length +max-datagram-packet-size+)))
    (multiple-value-bind (data from) (bocl::%recvfrom (socket socket) want +msg-nosignal+)
      (when (null data) (signal-socket-error "recvfrom" socket))
      (if (eq data :eof)
          (values (or buffer (make-array 0)) 0 nil nil)
          (let* ((size (length data))
                 (buf (string->octets data buffer size 0)))
            (multiple-value-bind (host port) (sockaddr-host+port from)
              (values buf size host port)))))))

;;; ======================================================================
;;; Local / peer name accessors
;;; ======================================================================

(defun get-local-name (socket)
  "(VALUES address port) of the local end of SOCKET."
  (sockaddr-host+port (bocl::%getsockname (socket socket))))

(defun get-peer-name (socket)
  "(VALUES address port) of the remote end of SOCKET."
  (sockaddr-host+port (bocl::%getpeername (socket socket))))

(defun get-local-address (socket) (values (get-local-name socket)))
(defun get-local-port (socket) (nth-value 1 (get-local-name socket)))
(defun get-peer-address (socket) (values (get-peer-name socket)))
(defun get-peer-port (socket) (nth-value 1 (get-peer-name socket)))

;;; ======================================================================
;;; WAIT-FOR-INPUT and the wait-list
;;; ======================================================================

(defstruct (wait-list (:constructor make-wait-list ()))
  (waiters nil))

(defun add-waiter (wait-list input)
  "Add the usocket INPUT to WAIT-LIST."
  (pushnew input (wait-list-waiters wait-list))
  (setf (wait-list input) wait-list)
  input)

(defun remove-waiter (wait-list input)
  "Remove the usocket INPUT from WAIT-LIST."
  (setf (wait-list-waiters wait-list) (remove input (wait-list-waiters wait-list)))
  (when (eq (wait-list input) wait-list) (setf (wait-list input) nil))
  input)

(defun remove-all-waiters (wait-list)
  "Empty WAIT-LIST."
  (dolist (w (wait-list-waiters wait-list)) (when (eq (wait-list w) wait-list)
                                              (setf (wait-list w) nil)))
  (setf (wait-list-waiters wait-list) nil)
  wait-list)

(defun %socket-fd-of (object)
  (etypecase object
    (usocket (socket object))
    (integer object)))

(defun wait-for-input (socket-or-sockets &key timeout ready-only)
  "Block until input is available on one of SOCKET-OR-SOCKETS (a single usocket,
a list, or a WAIT-LIST).  TIMEOUT is in seconds (NIL = block forever).  Marks
each ready socket's STATE :READ.  Returns the ready sockets (or, with READY-ONLY,
only those, as a fresh list)."
  (let* ((wl       (when (wait-list-p socket-or-sockets) socket-or-sockets))
         (sockets  (cond (wl (wait-list-waiters wl))
                         ((listp socket-or-sockets) socket-or-sockets)
                         (t (list socket-or-sockets))))
         (fds      (mapcar #'%socket-fd-of sockets))
         (ms       (and timeout (round (* timeout 1000)))))
    ;; clear previous state
    (dolist (s sockets) (when (typep s 'usocket) (setf (socket-state s) nil)))
    (let ((ready-fds (bocl::%select fds nil nil ms)))
      (let ((ready (remove-if-not
                    (lambda (s) (member (%socket-fd-of s) ready-fds)) sockets)))
        (dolist (s ready) (when (typep s 'usocket) (setf (socket-state s) :read)))
        (if ready-only
            ready
            (if wl (wait-list-waiters wl) sockets))))))

;;; ======================================================================
;;; Convenience macros
;;; ======================================================================

(defmacro with-connected-socket ((var socket) &body body)
  "Bind VAR to SOCKET for the dynamic extent of BODY, closing it on exit."
  `(let ((,var ,socket))
     (unwind-protect (progn ,@body)
       (when ,var (socket-close ,var)))))

(defmacro with-client-socket ((socket-var stream-var &rest socket-connect-args)
                              &body body)
  "Connect with SOCKET-CONNECT-ARGS, binding SOCKET-VAR and its STREAM-VAR for
BODY, closing the socket on exit."
  `(with-connected-socket (,socket-var (socket-connect ,@socket-connect-args))
     (let ((,stream-var (and ,socket-var (socket-stream ,socket-var))))
       (declare (ignorable ,stream-var))
       ,@body)))

(defmacro with-server-socket ((var server) &body body)
  "Bind VAR to the server usocket SERVER for BODY, closing it on exit."
  `(with-connected-socket (,var ,server) ,@body))

(defmacro with-socket-listener ((socket-var &rest socket-listen-args) &body body)
  "Listen with SOCKET-LISTEN-ARGS, binding SOCKET-VAR for BODY, closing on exit."
  `(with-server-socket (,socket-var (socket-listen ,@socket-listen-args))
     ,@body))

;;; ======================================================================
(provide "BOCL-SOCKET")
(provide "USOCKET")
;;;; THE END ;;;;
