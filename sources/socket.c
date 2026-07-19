/* socket.c — BSD-socket kernel primitives for bocl.
 *
 * This file exposes the POSIX/BSD sockets API to Lisp as a set of thin
 * BOCL::%... primitives.  The Lisp side (bootstrap/socket.lisp) builds a
 * usocket-compatible BOCL-SOCKET package on top of these.
 *
 * Design notes
 * ------------
 * - Raw byte payloads (sockaddr structures, socket-option values, datagram
 *   buffers) cross the C/Lisp boundary as bocl STRINGS, which are octet arrays
 *   that can hold arbitrary bytes 0..255.  bytes_to_string()/string_octets()
 *   convert.  Stream byte I/O does NOT go through here: a connected socket is
 *   wrapped in an ordinary FILE-STREAM (via fdopen) so READ-BYTE / WRITE-BYTE /
 *   READ-SEQUENCE / READ-CHAR all work through the existing stream machinery.
 *
 * - Errors are NOT signalled from C.  Every primitive that can fail returns a
 *   sentinel (-1 for fd/byte-count results, NIL for object results) and stashes
 *   errno where (BOCL::%SOCKET-ERRNO) can read it; the Lisp layer maps that to
 *   the appropriate usocket condition.  getaddrinfo()'s EAI_* code is kept
 *   separately and read via (BOCL::%GAI-ERRNO).  This keeps a clean,
 *   single-threaded, race-free errno contract and lets Lisp own the condition
 *   hierarchy.
 *
 * - sockaddr structures are opaque octet strings as far as Lisp is concerned.
 *   %GETADDRINFO produces them (covering names and numeric INET/INET6 forms);
 *   %MAKE-SOCKADDR-UN builds an AF_UNIX one; %SOCKADDR-FAMILY / %GETNAMEINFO
 *   parse them back.  Exotic address families work too: any octet string of the
 *   right layout can be handed to %BIND / %CONNECT / %SENDTO.
 */

/* NI_MAXHOST / NI_MAXSERV, EHOSTDOWN, MSG_DONTWAIT and the BSD sockaddr fields
   live outside strict ISO/POSIX, so they are exposed only when a platform
   feature-test macro selects the default/GNU (glibc) or Darwin/BSD namespace.
   These must precede every system header (pulled in transitively via bocl.h).
   Compiled under -std=c11, __STRICT_ANSI__ is set, so on Darwin/BSD the
   extensions stay hidden unless _DARWIN_C_SOURCE/_BSD_SOURCE are defined here. */
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#  define _DARWIN_C_SOURCE 1
#  define _BSD_SOURCE 1
   /* FreeBSD: ANY strict feature macro (the build passes -D_POSIX_C_SOURCE
      globally) sets __BSD_VISIBLE to 0, hiding MSG_DONTWAIT, EHOSTDOWN and
      even EAI_OVERFLOW (grouped with the BSD-only EAI_BADHINTS/EAI_PROTOCOL
      in <netdb.h>) — _BSD_SOURCE is a GLIBC name and does nothing here.
      Undo the command-line macro for this translation unit only. */
#  undef _POSIX_C_SOURCE
#  undef _XOPEN_SOURCE
#else
#  define _DEFAULT_SOURCE 1
#  define _GNU_SOURCE 1
#endif

#include "bocl.h"
#include "kernel.h"
#include "kernel_types.h"
#include "kernel_objects.h"
#include "packages.h"
#include "kernel_functions.h"
#include "cons.h"
#include "memory.h"
#include "macros.h"
#include "boerror.h"
#include "stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>

/* --- saved error state ----------------------------------------------------- */

static int bocl_sock_errno = 0;   /* errno of the last failed syscall          */
static int bocl_gai_error  = 0;   /* EAI_* code of the last getaddrinfo()      */

static void save_errno(void){ bocl_sock_errno = errno; }

/* --- small Ref helpers ----------------------------------------------------- */

/* Build a bocl STRING holding LEN raw bytes from P (single allocation: safe). */
static Ref bytes_to_string(const void* p, size_t len){
    Ref s = string_new((halfword)len);
    if(len){ memcpy(string_octets(s), p, len); }
    return s;}

/* Read an integer argument as a C int. */
static int arg_int(Ref x){ return (int)integer_value(x); }

/* Read a STRING argument's bytes; returns pointer + length (no copy, no NUL). */
static const octet* arg_bytes(Ref x, size_t* len){
    if(!typep(x,STRING_Class)){ TYPE_ERROR(x,STRING_Class); }
    *len = (size_t)string_length(x);
    return string_octets(x);}

/* Return one value. */
static void ret1(MultipleValues* results, Ref v){
    results->count=1; results->values[0]=v;}

/* --- socket(2) ------------------------------------------------------------- */

/* (%SOCKET domain type protocol) -> fd (int) | -1 on error. */
static void kernel_SOCKET(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int domain   = arg_int(pop_mandatory_argument(function,&current));
    int type     = arg_int(pop_mandatory_argument(function,&current));
    int protocol = arg_int(pop_mandatory_argument(function,&current));
    check_no_more_arguments(function,current);
    int fd = socket(domain,type,protocol);
    if(fd<0){ save_errno(); }
    ret1(results, I((word)fd));
    close_live_pool(pool);}

/* (%BIND fd sockaddr-string) -> 0 | -1. */
static void kernel_BIND(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int fd = arg_int(pop_mandatory_argument(function,&current));
    Ref addr = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    size_t len; const octet* p = arg_bytes(addr,&len);
    int rc = bind(fd,(const struct sockaddr*)p,(socklen_t)len);
    if(rc<0){ save_errno(); }
    ret1(results, I((word)rc));
    close_live_pool(pool);}

/* (%CONNECT fd sockaddr-string) -> 0 | -1. */
static void kernel_CONNECT(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int fd = arg_int(pop_mandatory_argument(function,&current));
    Ref addr = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    size_t len; const octet* p = arg_bytes(addr,&len);
    int rc = connect(fd,(const struct sockaddr*)p,(socklen_t)len);
    if(rc<0){ save_errno(); }
    ret1(results, I((word)rc));
    close_live_pool(pool);}

/* (%LISTEN fd backlog) -> 0 | -1. */
static void kernel_LISTEN(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int fd = arg_int(pop_mandatory_argument(function,&current));
    int backlog = arg_int(pop_mandatory_argument(function,&current));
    check_no_more_arguments(function,current);
    int rc = listen(fd,backlog);
    if(rc<0){ save_errno(); }
    ret1(results, I((word)rc));
    close_live_pool(pool);}

/* (%ACCEPT fd) -> (values newfd peer-sockaddr-string) | (values -1 NIL). */
static void kernel_ACCEPT(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int fd = arg_int(pop_mandatory_argument(function,&current));
    check_no_more_arguments(function,current);
    struct sockaddr_storage ss; socklen_t len=sizeof(ss);
    int nfd = accept(fd,(struct sockaddr*)&ss,&len);
    results->count=2;
    if(nfd<0){
        save_errno();
        results->values[0]=I((word)-1);
        results->values[1]=NIL_Symbol;
    } else {
        results->values[0]=I((word)nfd);
        results->values[1]=bytes_to_string(&ss,len);}
    close_live_pool(pool);}

/* (%CLOSE-FD fd) -> 0 | -1.  close(2) on a raw fd (not a stream). */
static void kernel_CLOSE_FD(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int fd = arg_int(pop_mandatory_argument(function,&current));
    check_no_more_arguments(function,current);
    int rc = close(fd);
    if(rc<0){ save_errno(); }
    ret1(results, I((word)rc));
    close_live_pool(pool);}

/* (%SHUTDOWN fd how) -> 0 | -1. how: 0=SHUT_RD 1=SHUT_WR 2=SHUT_RDWR. */
static void kernel_SHUTDOWN(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int fd = arg_int(pop_mandatory_argument(function,&current));
    int how = arg_int(pop_mandatory_argument(function,&current));
    check_no_more_arguments(function,current);
    int rc = shutdown(fd,how);
    if(rc<0){ save_errno(); }
    ret1(results, I((word)rc));
    close_live_pool(pool);}

/* (%GETSOCKNAME fd) -> sockaddr-string | NIL. */
static void kernel_GETSOCKNAME(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int fd = arg_int(pop_mandatory_argument(function,&current));
    check_no_more_arguments(function,current);
    struct sockaddr_storage ss; socklen_t len=sizeof(ss);
    int rc = getsockname(fd,(struct sockaddr*)&ss,&len);
    if(rc<0){ save_errno(); ret1(results,NIL_Symbol); }
    else    { ret1(results,bytes_to_string(&ss,len)); }
    close_live_pool(pool);}

/* (%GETPEERNAME fd) -> sockaddr-string | NIL. */
static void kernel_GETPEERNAME(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int fd = arg_int(pop_mandatory_argument(function,&current));
    check_no_more_arguments(function,current);
    struct sockaddr_storage ss; socklen_t len=sizeof(ss);
    int rc = getpeername(fd,(struct sockaddr*)&ss,&len);
    if(rc<0){ save_errno(); ret1(results,NIL_Symbol); }
    else    { ret1(results,bytes_to_string(&ss,len)); }
    close_live_pool(pool);}

/* (%SETSOCKOPT-INT fd level optname intval) -> 0 | -1. */
static void kernel_SETSOCKOPT_INT(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int fd      = arg_int(pop_mandatory_argument(function,&current));
    int level   = arg_int(pop_mandatory_argument(function,&current));
    int optname = arg_int(pop_mandatory_argument(function,&current));
    int value   = arg_int(pop_mandatory_argument(function,&current));
    check_no_more_arguments(function,current);
    int rc = setsockopt(fd,level,optname,&value,sizeof(value));
    if(rc<0){ save_errno(); }
    ret1(results, I((word)rc));
    close_live_pool(pool);}

/* (%GETSOCKOPT-INT fd level optname) -> intval | NIL. */
static void kernel_GETSOCKOPT_INT(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int fd      = arg_int(pop_mandatory_argument(function,&current));
    int level   = arg_int(pop_mandatory_argument(function,&current));
    int optname = arg_int(pop_mandatory_argument(function,&current));
    check_no_more_arguments(function,current);
    int value=0; socklen_t len=sizeof(value);
    int rc = getsockopt(fd,level,optname,&value,&len);
    if(rc<0){ save_errno(); ret1(results,NIL_Symbol); }
    else    { ret1(results,I((word)value)); }
    close_live_pool(pool);}

/* (%SETSOCKOPT fd level optname value-string) -> 0 | -1.  Raw byte option. */
static void kernel_SETSOCKOPT(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int fd      = arg_int(pop_mandatory_argument(function,&current));
    int level   = arg_int(pop_mandatory_argument(function,&current));
    int optname = arg_int(pop_mandatory_argument(function,&current));
    Ref val     = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    size_t len; const octet* p = arg_bytes(val,&len);
    int rc = setsockopt(fd,level,optname,p,(socklen_t)len);
    if(rc<0){ save_errno(); }
    ret1(results, I((word)rc));
    close_live_pool(pool);}

/* (%GETSOCKOPT fd level optname length) -> value-string | NIL.  Raw byte option. */
static void kernel_GETSOCKOPT(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int fd      = arg_int(pop_mandatory_argument(function,&current));
    int level   = arg_int(pop_mandatory_argument(function,&current));
    int optname = arg_int(pop_mandatory_argument(function,&current));
    int wantlen = arg_int(pop_mandatory_argument(function,&current));
    check_no_more_arguments(function,current);
    octet buf[256]; if(wantlen<0) wantlen=0; if(wantlen>(int)sizeof(buf)) wantlen=sizeof(buf);
    socklen_t len=(socklen_t)wantlen;
    int rc = getsockopt(fd,level,optname,buf,&len);
    if(rc<0){ save_errno(); ret1(results,NIL_Symbol); }
    else    { ret1(results,bytes_to_string(buf,len)); }
    close_live_pool(pool);}

/* (%SEND fd data-string flags) -> nbytes | -1. */
static void kernel_SEND(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int fd   = arg_int(pop_mandatory_argument(function,&current));
    Ref data = pop_mandatory_argument(function,&current);
    int flags= arg_int(pop_mandatory_argument(function,&current));
    check_no_more_arguments(function,current);
    size_t len; const octet* p = arg_bytes(data,&len);
    ssize_t n = send(fd,p,len,flags);
    if(n<0){ save_errno(); }
    ret1(results, I((word)n));
    close_live_pool(pool);}

/* (%RECV fd length flags) -> data-string | :EOF (peer closed) | NIL (error). */
static void kernel_RECV(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int fd    = arg_int(pop_mandatory_argument(function,&current));
    int want  = arg_int(pop_mandatory_argument(function,&current));
    int flags = arg_int(pop_mandatory_argument(function,&current));
    check_no_more_arguments(function,current);
    if(want<0) want=0;
    octet* buf = (octet*)malloc((size_t)(want?want:1));
    ssize_t n = recv(fd,buf,(size_t)want,flags);
    if(n<0){ save_errno(); ret1(results,NIL_Symbol); }
    else if(n==0){ ret1(results,K(EOF)); }
    else { ret1(results,bytes_to_string(buf,(size_t)n)); }
    free(buf);
    close_live_pool(pool);}

/* (%SENDTO fd data-string flags sockaddr-string) -> nbytes | -1. */
static void kernel_SENDTO(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int fd   = arg_int(pop_mandatory_argument(function,&current));
    Ref data = pop_mandatory_argument(function,&current);
    int flags= arg_int(pop_mandatory_argument(function,&current));
    Ref addr = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    size_t dlen; const octet* dp = arg_bytes(data,&dlen);
    const struct sockaddr* sa=NULL; socklen_t salen=0;
    if(addr!=NIL_Symbol){ size_t al; sa=(const struct sockaddr*)arg_bytes(addr,&al); salen=(socklen_t)al; }
    ssize_t n = sendto(fd,dp,dlen,flags,sa,salen);
    if(n<0){ save_errno(); }
    ret1(results, I((word)n));
    close_live_pool(pool);}

/* (%RECVFROM fd length flags)
   -> (values data-string sockaddr-string) | (values :EOF NIL) | (values NIL NIL). */
static void kernel_RECVFROM(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int fd    = arg_int(pop_mandatory_argument(function,&current));
    int want  = arg_int(pop_mandatory_argument(function,&current));
    int flags = arg_int(pop_mandatory_argument(function,&current));
    check_no_more_arguments(function,current);
    if(want<0) want=0;
    octet* buf = (octet*)malloc((size_t)(want?want:1));
    struct sockaddr_storage ss; socklen_t slen=sizeof(ss);
    ssize_t n = recvfrom(fd,buf,(size_t)want,flags,(struct sockaddr*)&ss,&slen);
    results->count=2;
    if(n<0){ save_errno(); results->values[0]=NIL_Symbol; results->values[1]=NIL_Symbol; }
    else if(n==0){ results->values[0]=K(EOF); results->values[1]=NIL_Symbol; }
    else {
        Ref d=bytes_to_string(buf,(size_t)n); add_to_live_pool(d);
        results->values[0]=d;
        results->values[1]=bytes_to_string(&ss,slen);}
    free(buf);
    close_live_pool(pool);}

/* (%SOCKETPAIR domain type protocol) -> (values fd1 fd2) | (values -1 -1). */
static void kernel_SOCKETPAIR(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int domain   = arg_int(pop_mandatory_argument(function,&current));
    int type     = arg_int(pop_mandatory_argument(function,&current));
    int protocol = arg_int(pop_mandatory_argument(function,&current));
    check_no_more_arguments(function,current);
    int sv[2]={-1,-1};
    int rc = socketpair(domain,type,protocol,sv);
    if(rc<0){ save_errno(); }
    results->count=2;
    results->values[0]=I((word)sv[0]);
    results->values[1]=I((word)sv[1]);
    close_live_pool(pool);}

/* --- name resolution ------------------------------------------------------- */

/* (%GETADDRINFO node service family socktype protocol flags)
   NODE and SERVICE are STRINGs or NIL.  Returns a list of entries, each
   (family socktype protocol sockaddr-string canonname-or-nil); NIL on error
   (read (%GAI-ERRNO) / (%GAI-STRERROR n)). */
static void kernel_GETADDRINFO(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref nodeR    = pop_mandatory_argument(function,&current);
    Ref servR    = pop_mandatory_argument(function,&current);
    int family   = arg_int(pop_mandatory_argument(function,&current));
    int socktype = arg_int(pop_mandatory_argument(function,&current));
    int protocol = arg_int(pop_mandatory_argument(function,&current));
    int flags    = arg_int(pop_mandatory_argument(function,&current));
    check_no_more_arguments(function,current);
    char* node = (nodeR!=NIL_Symbol) ? string_cstring(nodeR) : NULL;
    char* serv = (servR!=NIL_Symbol) ? string_cstring(servR) : NULL;
    struct addrinfo hints; memset(&hints,0,sizeof(hints));
    hints.ai_family=family; hints.ai_socktype=socktype;
    hints.ai_protocol=protocol; hints.ai_flags=flags;
    struct addrinfo* res=NULL;
    int rc = getaddrinfo(node,serv,&hints,&res);
    if(node) free(node);
    if(serv) free(serv);
    if(rc!=0){
        bocl_gai_error = rc;
        ret1(results,NIL_Symbol);
        close_live_pool(pool);
        return;}
    /* Build the list preserving getaddrinfo order (reverse-collect then it is
       already reversed by push, so walk res in reverse via recursion-free trick:
       collect into a vector-free reversed list, then reverse).  Simpler: collect
       forward into OUT by pushing, then reverse with a second pass. */
    Ref out=NIL_Symbol;
    for(struct addrinfo* ai=res; ai; ai=ai->ai_next){
        Ref addr = bytes_to_string(ai->ai_addr, ai->ai_addrlen); add_to_live_pool(addr);
        Ref canon = ai->ai_canonname ? string_new_cstring(ai->ai_canonname) : NIL_Symbol;
        add_to_live_pool(canon);
        Ref entry = list(I((word)ai->ai_family),
                         I((word)ai->ai_socktype),
                         I((word)ai->ai_protocol),
                         addr, canon, NULL);
        add_to_live_pool(entry);
        push(entry,&out); add_to_live_pool(out);}
    freeaddrinfo(res);
    /* OUT is in reverse order; reverse it back to getaddrinfo's preference order. */
    Ref rev=NIL_Symbol;
    for(Ref p=out; p!=NIL_Symbol; p=cons_cdr(p)){ push(cons_car(p),&rev); add_to_live_pool(rev); }
    ret1(results, rev);
    close_live_pool(pool);}

/* (%GETNAMEINFO sockaddr-string flags) -> (values host-string service-string) | (values NIL NIL). */
static void kernel_GETNAMEINFO(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref addr  = pop_mandatory_argument(function,&current);
    int flags = arg_int(pop_mandatory_argument(function,&current));
    check_no_more_arguments(function,current);
    size_t len; const octet* p = arg_bytes(addr,&len);
    char host[NI_MAXHOST], serv[NI_MAXSERV];
    int rc = getnameinfo((const struct sockaddr*)p,(socklen_t)len,
                         host,sizeof(host),serv,sizeof(serv),flags);
    results->count=2;
    if(rc!=0){ bocl_gai_error=rc; results->values[0]=NIL_Symbol; results->values[1]=NIL_Symbol; }
    else {
        Ref h=string_new_cstring(host); add_to_live_pool(h);
        results->values[0]=h;
        results->values[1]=string_new_cstring(serv);}
    close_live_pool(pool);}

/* (%SOCKADDR-FAMILY sockaddr-string) -> integer address family (ss_family). */
static void kernel_SOCKADDR_FAMILY(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref addr = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    size_t len; const octet* p = arg_bytes(addr,&len);
    int fam=-1;
    if(len>=sizeof(sa_family_t)){
        struct sockaddr sa; memcpy(&sa,p, len<sizeof(sa)?len:sizeof(sa));
        fam = sa.sa_family;}
    ret1(results, I((word)fam));
    close_live_pool(pool);}

/* (%MAKE-SOCKADDR-UN path-string) -> sockaddr-string (AF_UNIX). */
static void kernel_MAKE_SOCKADDR_UN(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref pathR = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    char* path = string_cstring(pathR);
    struct sockaddr_un un; memset(&un,0,sizeof(un));
    un.sun_family=AF_UNIX;
    strncpy(un.sun_path,path,sizeof(un.sun_path)-1);
    size_t total = offsetof(struct sockaddr_un,sun_path)+strlen(un.sun_path)+1;
    free(path);
    ret1(results, bytes_to_string(&un,total));
    close_live_pool(pool);}

/* (%INET-PTON family text) -> packed-address-string | NIL. */
static void kernel_INET_PTON(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int family = arg_int(pop_mandatory_argument(function,&current));
    Ref textR  = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    char* text = string_cstring(textR);
    octet buf[16]; size_t blen = (family==AF_INET6)?16:4;
    int rc = inet_pton(family,text,buf);
    free(text);
    if(rc==1){ ret1(results,bytes_to_string(buf,blen)); }
    else     { if(rc<0) save_errno(); ret1(results,NIL_Symbol); }
    close_live_pool(pool);}

/* (%INET-NTOP family packed-string) -> text-string | NIL. */
static void kernel_INET_NTOP(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int family = arg_int(pop_mandatory_argument(function,&current));
    Ref packed = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    size_t len; const octet* p = arg_bytes(packed,&len);
    char text[INET6_ADDRSTRLEN];
    const char* r = inet_ntop(family,p,text,sizeof(text));
    if(r){ ret1(results,string_new_cstring(text)); }
    else { save_errno(); ret1(results,NIL_Symbol); }
    close_live_pool(pool);}

/* --- nonblocking / select -------------------------------------------------- */

/* (%SET-NONBLOCKING fd flag) -> 0 | -1.  FLAG non-NIL sets O_NONBLOCK. */
static void kernel_SET_NONBLOCKING(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int fd   = arg_int(pop_mandatory_argument(function,&current));
    Ref flag = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    int fl = fcntl(fd,F_GETFL,0);
    if(fl<0){ save_errno(); ret1(results,I((word)-1)); close_live_pool(pool); return; }
    if(flag!=NIL_Symbol){ fl |= O_NONBLOCK; } else { fl &= ~O_NONBLOCK; }
    int rc = fcntl(fd,F_SETFL,fl);
    if(rc<0){ save_errno(); }
    ret1(results, I((word)rc));
    close_live_pool(pool);}

/* Collect fds from a Lisp list into FDSET; return the maximum fd seen. */
static int fill_fdset(Ref list_of_fds, fd_set* set){
    int maxfd=-1;
    for(Ref p=list_of_fds; p!=NIL_Symbol; p=cons_cdr(p)){
        int fd=arg_int(cons_car(p));
        if(fd>=0 && fd<FD_SETSIZE){ FD_SET(fd,set); if(fd>maxfd) maxfd=fd; }}
    return maxfd;}

/* Reverse-rebuild the subset of FDS that are set in SET. */
static Ref ready_fds(Ref list_of_fds, fd_set* set){
    Ref out=NIL_Symbol;
    for(Ref p=list_of_fds; p!=NIL_Symbol; p=cons_cdr(p)){
        int fd=arg_int(cons_car(p));
        if(fd>=0 && fd<FD_SETSIZE && FD_ISSET(fd,set)){ push(cons_car(p),&out); }}
    return out;}

/* (%SELECT read-fds write-fds except-fds timeout-ms)
   TIMEOUT-MS: NIL = block; integer milliseconds (0 = poll).
   -> (values ready-read ready-write ready-except) | (values NIL NIL NIL) on error. */
static void kernel_SELECT(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref rfds = pop_mandatory_argument(function,&current);
    Ref wfds = pop_mandatory_argument(function,&current);
    Ref efds = pop_mandatory_argument(function,&current);
    Ref toR  = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    fd_set rs,ws,es; FD_ZERO(&rs); FD_ZERO(&ws); FD_ZERO(&es);
    int m1=fill_fdset(rfds,&rs);
    int m2=fill_fdset(wfds,&ws);
    int m3=fill_fdset(efds,&es);
    int maxfd=m1; if(m2>maxfd)maxfd=m2; if(m3>maxfd)maxfd=m3;
    struct timeval tv, *tvp=NULL;
    if(toR!=NIL_Symbol){
        long ms=(long)integer_value(toR);
        tv.tv_sec=ms/1000; tv.tv_usec=(ms%1000)*1000; tvp=&tv;}
    int rc = select(maxfd+1,&rs,&ws,&es,tvp);
    results->count=3;
    if(rc<0){ save_errno(); results->values[0]=NIL_Symbol; results->values[1]=NIL_Symbol; results->values[2]=NIL_Symbol; }
    else {
        Ref rr=ready_fds(rfds,&rs); add_to_live_pool(rr);
        Ref rw=ready_fds(wfds,&ws); add_to_live_pool(rw);
        Ref re=ready_fds(efds,&es); add_to_live_pool(re);
        results->values[0]=rr; results->values[1]=rw; results->values[2]=re;}
    close_live_pool(pool);}

/* --- socket stream --------------------------------------------------------- */

/* (%MAKE-SOCKET-STREAM fd direction element-type) -> FILE-STREAM.
   DIRECTION is :INPUT / :OUTPUT / :IO.  The fd is wrapped with fdopen(...,"r+")
   in UNBUFFERED mode so reads and writes may freely interleave on the one fd
   (stdio's read/write-switching restriction only applies to buffered streams).
   Closing the stream (CL:CLOSE) closes the underlying fd. */
static void kernel_MAKE_SOCKET_STREAM(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int fd        = arg_int(pop_mandatory_argument(function,&current));
    Ref dirR      = pop_mandatory_argument(function,&current);
    Ref etype     = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    FILE* f = fdopen(fd,"r+");
    if(!f){ save_errno(); ret1(results,NIL_Symbol); close_live_pool(pool); return; }
    setvbuf(f,NULL,_IONBF,0);
    Ref pn = physical_pathname_new_cstring("socket:");
    add_to_live_pool(pn);
    Ref ef = list(K(ISO-8859-1),K(LF),S(NIL));
    add_to_live_pool(ef);
    Ref stream = file_stream_new(pn,etype,ef);
    direction_t dir = direction_io;
    if(dirR==K(INPUT)) dir=direction_input;
    else if(dirR==K(OUTPUT)) dir=direction_output;
    stream->slots->file_stream.direction = (halfword)dir;
    stream->slots->file_stream.is_open = 1;
    stream->slots->file_stream.stream = f;
    ret1(results, stream);
    close_live_pool(pool);}

/* --- errno / strerror ------------------------------------------------------ */

/* (%SOCKET-ERRNO) -> integer errno of the last failed socket syscall. */
static void kernel_SOCKET_ERRNO(Ref function, Ref arguments, MultipleValues* results){
    (void)function; (void)arguments;
    ret1(results, I((word)bocl_sock_errno));}

/* (%GAI-ERRNO) -> integer EAI_* code of the last failed getaddrinfo/getnameinfo. */
static void kernel_GAI_ERRNO(Ref function, Ref arguments, MultipleValues* results){
    (void)function; (void)arguments;
    ret1(results, I((word)bocl_gai_error));}

/* (%STRERROR n) -> the strerror(3) text for errno N. */
static void kernel_STRERROR(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int n = arg_int(pop_mandatory_argument(function,&current));
    check_no_more_arguments(function,current);
    ret1(results, string_new_cstring(strerror(n)));
    close_live_pool(pool);}

/* (%GAI-STRERROR n) -> the gai_strerror(3) text for EAI_* code N. */
static void kernel_GAI_STRERROR(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int n = arg_int(pop_mandatory_argument(function,&current));
    check_no_more_arguments(function,current);
    ret1(results, string_new_cstring(gai_strerror(n)));
    close_live_pool(pool);}

/* (%ERRNO-KEYWORD n) -> a keyword naming errno N (e.g. :ECONNREFUSED), or
   :EUNKNOWN.  Lets the Lisp layer map errno -> usocket condition portably,
   without hard-coding platform-specific numeric values. */
static void kernel_ERRNO_KEYWORD(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    int n = arg_int(pop_mandatory_argument(function,&current));
    check_no_more_arguments(function,current);
    const char* name="EUNKNOWN";
    switch(n){
#define E(sym) case sym: name=#sym; break;
        E(EACCES) E(EADDRINUSE) E(EADDRNOTAVAIL) E(EAFNOSUPPORT) E(EAGAIN)
        E(EALREADY) E(EBADF) E(ECONNABORTED) E(ECONNREFUSED) E(ECONNRESET)
        E(EDESTADDRREQ)
#ifdef EHOSTDOWN
        E(EHOSTDOWN)
#endif
        E(EHOSTUNREACH) E(EINPROGRESS) E(EINTR)
        E(EINVAL) E(EISCONN) E(EMFILE) E(EMSGSIZE) E(ENETDOWN) E(ENETRESET)
        E(ENETUNREACH) E(ENFILE) E(ENOBUFS) E(ENOPROTOOPT) E(ENOTCONN)
        E(ENOTSOCK) E(EOPNOTSUPP) E(EPERM) E(EPIPE) E(EPROTONOSUPPORT)
        E(EPROTOTYPE) E(ETIMEDOUT)
#undef E
        default: name="EUNKNOWN"; break;}
    ret1(results, keyword_intern_cstring(name));
    close_live_pool(pool);}

/* --- constants ------------------------------------------------------------- */

/* (%SOCKET-CONSTANTS) -> an alist (keyword . integer) of every socket constant
   the Lisp layer needs.  Values come straight from the platform headers, so the
   port is correct on any Unix without hard-coding numbers in Lisp. */
static void kernel_SOCKET_CONSTANTS(Ref function, Ref arguments, MultipleValues* results){
    (void)function; (void)arguments;
    uword pool=open_live_pool();
    Ref out=NIL_Symbol;
#define SC(sym) do{ Ref cell=cons(keyword_intern_cstring(#sym), I((word)(sym))); \
                   add_to_live_pool(cell); push(cell,&out); add_to_live_pool(out); }while(0)
    /* address families */
    SC(AF_UNSPEC); SC(AF_INET); SC(AF_INET6); SC(AF_UNIX);
#ifdef AF_PACKET
    SC(AF_PACKET);
#endif
#ifdef AF_NETLINK
    SC(AF_NETLINK);
#endif
    /* socket types */
    SC(SOCK_STREAM); SC(SOCK_DGRAM); SC(SOCK_RAW); SC(SOCK_SEQPACKET);
#ifdef SOCK_RDM
    SC(SOCK_RDM);
#endif
    /* protocols */
    SC(IPPROTO_IP); SC(IPPROTO_TCP); SC(IPPROTO_UDP); SC(IPPROTO_ICMP);
    SC(IPPROTO_RAW); SC(IPPROTO_IPV6);
#ifdef IPPROTO_SCTP
    SC(IPPROTO_SCTP);
#endif
    /* option levels */
    SC(SOL_SOCKET);
    /* socket options */
    SC(SO_REUSEADDR); SC(SO_KEEPALIVE); SC(SO_BROADCAST); SC(SO_LINGER);
    SC(SO_RCVBUF); SC(SO_SNDBUF); SC(SO_RCVTIMEO); SC(SO_SNDTIMEO);
    SC(SO_ERROR); SC(SO_TYPE); SC(SO_OOBINLINE); SC(SO_DONTROUTE);
#ifdef SO_REUSEPORT
    SC(SO_REUSEPORT);
#endif
    /* TCP options */
    SC(TCP_NODELAY);
    /* message flags */
    SC(MSG_OOB); SC(MSG_PEEK); SC(MSG_DONTROUTE); SC(MSG_WAITALL);
#ifdef MSG_DONTWAIT
    SC(MSG_DONTWAIT);
#endif
#ifdef MSG_NOSIGNAL
    SC(MSG_NOSIGNAL);
#endif
    /* shutdown how */
    SC(SHUT_RD); SC(SHUT_WR); SC(SHUT_RDWR);
    /* getaddrinfo flags */
    SC(AI_PASSIVE); SC(AI_CANONNAME); SC(AI_NUMERICHOST); SC(AI_NUMERICSERV);
    SC(AI_V4MAPPED); SC(AI_ALL); SC(AI_ADDRCONFIG);
    /* getnameinfo flags */
    SC(NI_NUMERICHOST); SC(NI_NUMERICSERV); SC(NI_NAMEREQD); SC(NI_DGRAM);
    /* getaddrinfo/getnameinfo error codes (EAI_*, negative) — let the Lisp layer
       map a (%GAI-ERRNO) value to the right name-resolution condition. */
    SC(EAI_BADFLAGS); SC(EAI_NONAME); SC(EAI_AGAIN); SC(EAI_FAIL);
    SC(EAI_FAMILY); SC(EAI_SOCKTYPE); SC(EAI_SERVICE); SC(EAI_MEMORY);
    SC(EAI_SYSTEM); SC(EAI_OVERFLOW);
#ifdef EAI_NODATA
    SC(EAI_NODATA);
#endif
#ifdef EAI_ADDRFAMILY
    SC(EAI_ADDRFAMILY);
#endif
#undef SC
    ret1(results, out);
    close_live_pool(pool);}

/* --- registration ---------------------------------------------------------- */

/* Intern a symbol NAME in the BOCL package and bind it to a built-in. */
static void def(const char* name, Ref lambda_list, KernelFunctionPtr fn){
    Ref s = package_intern_cstring_in(name, BOCL_Package);
    set_symbol_function(s, built_in_function_new(s, lambda_list, fn));}

static Ref sym(const char* n){ return symbol_intern_cstring(n); }

void bocl_register_socket(void){
    Ref FD     = sym("FD");
    Ref DOMAIN = sym("DOMAIN");
    Ref TYPE   = sym("TYPE");
    Ref PROTO  = sym("PROTOCOL");
    Ref ADDR   = sym("SOCKADDR");
    Ref BACKLG = sym("BACKLOG");
    Ref HOW    = sym("HOW");
    Ref LEVEL  = sym("LEVEL");
    Ref OPT    = sym("OPTNAME");
    Ref VAL    = sym("VALUE");
    Ref LEN    = sym("LENGTH");
    Ref DATA   = sym("DATA");
    Ref FLAGS  = sym("FLAGS");
    Ref DIR    = sym("DIRECTION");
    Ref ETYPE  = sym("ELEMENT-TYPE");
    Ref N      = sym("N");
    Ref FAMILY = sym("FAMILY");
    Ref STYPE  = sym("SOCKTYPE");
    Ref NODE   = sym("NODE");
    Ref SERV   = sym("SERVICE");
    Ref FLAG   = sym("FLAG");
    Ref PATH   = sym("PATH");
    Ref TEXT   = sym("TEXT");
    Ref RFDS   = sym("READ-FDS");
    Ref WFDS   = sym("WRITE-FDS");
    Ref EFDS   = sym("EXCEPT-FDS");
    Ref TOUT   = sym("TIMEOUT-MS");

    def("%SOCKET",         list(DOMAIN,TYPE,PROTO,NULL),            kernel_SOCKET);
    def("%BIND",           list(FD,ADDR,NULL),                      kernel_BIND);
    def("%CONNECT",        list(FD,ADDR,NULL),                      kernel_CONNECT);
    def("%LISTEN",         list(FD,BACKLG,NULL),                    kernel_LISTEN);
    def("%ACCEPT",         list(FD,NULL),                           kernel_ACCEPT);
    def("%CLOSE-FD",       list(FD,NULL),                           kernel_CLOSE_FD);
    def("%SHUTDOWN",       list(FD,HOW,NULL),                       kernel_SHUTDOWN);
    def("%GETSOCKNAME",    list(FD,NULL),                           kernel_GETSOCKNAME);
    def("%GETPEERNAME",    list(FD,NULL),                           kernel_GETPEERNAME);
    def("%SETSOCKOPT-INT", list(FD,LEVEL,OPT,VAL,NULL),             kernel_SETSOCKOPT_INT);
    def("%GETSOCKOPT-INT", list(FD,LEVEL,OPT,NULL),                 kernel_GETSOCKOPT_INT);
    def("%SETSOCKOPT",     list(FD,LEVEL,OPT,VAL,NULL),             kernel_SETSOCKOPT);
    def("%GETSOCKOPT",     list(FD,LEVEL,OPT,LEN,NULL),             kernel_GETSOCKOPT);
    def("%SEND",           list(FD,DATA,FLAGS,NULL),                kernel_SEND);
    def("%RECV",           list(FD,LEN,FLAGS,NULL),                 kernel_RECV);
    def("%SENDTO",         list(FD,DATA,FLAGS,ADDR,NULL),           kernel_SENDTO);
    def("%RECVFROM",       list(FD,LEN,FLAGS,NULL),                 kernel_RECVFROM);
    def("%SOCKETPAIR",     list(DOMAIN,TYPE,PROTO,NULL),            kernel_SOCKETPAIR);
    def("%GETADDRINFO",    list(NODE,SERV,FAMILY,STYPE,PROTO,FLAGS,NULL), kernel_GETADDRINFO);
    def("%GETNAMEINFO",    list(ADDR,FLAGS,NULL),                   kernel_GETNAMEINFO);
    def("%SOCKADDR-FAMILY",list(ADDR,NULL),                         kernel_SOCKADDR_FAMILY);
    def("%MAKE-SOCKADDR-UN",list(PATH,NULL),                        kernel_MAKE_SOCKADDR_UN);
    def("%INET-PTON",      list(FAMILY,TEXT,NULL),                  kernel_INET_PTON);
    def("%INET-NTOP",      list(FAMILY,DATA,NULL),                  kernel_INET_NTOP);
    def("%SET-NONBLOCKING",list(FD,FLAG,NULL),                      kernel_SET_NONBLOCKING);
    def("%SELECT",         list(RFDS,WFDS,EFDS,TOUT,NULL),          kernel_SELECT);
    def("%MAKE-SOCKET-STREAM",list(FD,DIR,ETYPE,NULL),              kernel_MAKE_SOCKET_STREAM);
    def("%SOCKET-ERRNO",   NIL_Symbol,                              kernel_SOCKET_ERRNO);
    def("%GAI-ERRNO",      NIL_Symbol,                              kernel_GAI_ERRNO);
    def("%STRERROR",       list(N,NULL),                            kernel_STRERROR);
    def("%GAI-STRERROR",   list(N,NULL),                            kernel_GAI_STRERROR);
    def("%ERRNO-KEYWORD",  list(N,NULL),                            kernel_ERRNO_KEYWORD);
    def("%SOCKET-CONSTANTS",NIL_Symbol,                             kernel_SOCKET_CONSTANTS);}

/**** THE END ****/
