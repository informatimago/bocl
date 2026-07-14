/* callback_stream.c — output streams whose writes call back into Lisp.

   This is the kernel enabling primitive for CL-level compound streams
   (BROADCAST / TWO-WAY / ECHO / SYNONYM) and DRIBBLE.  An output stream
   created here is backed by a custom stdio stream (fopencookie on
   glibc/Linux, funopen on BSD/macOS) whose write callback builds a string
   from the emitted bytes and funcalls a stored Lisp WRITE-FUNCTION.  Because
   the entire printer obtains its FILE* through stream_output_cstream(),
   printing to such a stream redirects every PRINC / WRITE-CHAR / FORMAT byte
   through WRITE-FUNCTION with no changes to printer.c.

   The non-POSIX cookie APIs (fopencookie / funopen) are not declared under
   -std=iso9899:2018 with only _POSIX_C_SOURCE, so the relevant feature-test
   macro must precede every include in THIS translation unit. */

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#  define _DARWIN_C_SOURCE 1
#  define _BSD_SOURCE 1
   /* FreeBSD: ANY strict feature macro (the build passes -D_POSIX_C_SOURCE
      globally) sets __BSD_VISIBLE to 0 and hides funopen — _BSD_SOURCE is a
      GLIBC name and does nothing here.  Undo the command-line macro for this
      translation unit only (macOS re-exposes via _DARWIN_C_SOURCE instead). */
#  undef _POSIX_C_SOURCE
#  undef _XOPEN_SOURCE
#  define BOCL_CALLBACK_STREAM_FUNOPEN 1
#else
#  define _GNU_SOURCE 1
#  define BOCL_CALLBACK_STREAM_FOPENCOOKIE 1
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>   /* ssize_t for the fopencookie write callback */
#include "kernel.h"
#include "kernel_types.h"
#include "kernel_functions.h"
#include "memory.h"
#include "boerror.h"
#include "cons.h"
#include "stream.h"

/* Shared body: forward SIZE bytes of BUF to the stream's WRITE-FUNCTION.

   We are called from inside stdio (a flush of the cookie FILE*), so the
   evaluator re-enters here with GC live-pools active.  GC is precise, so the
   freshly built string and the write-function must both be rooted before
   kernel_apply (which may GC).  The stream object roots WRITE-FUNCTION via its
   `string` slot (objcount=4 traces it); the stream itself is kept live by the
   caller (it is bound, e.g. *STANDARD-OUTPUT*, while open). */
static void callback_stream_write(Ref stream, const char* buf, size_t size){
    if(size == 0){ return; }
    Ref write_function = stream->slots->file_stream.string;
    if(write_function == NIL_Symbol || nullp(write_function)){ return; }
    uword pool = open_live_pool();
    add_to_live_pool(stream);
    add_to_live_pool(write_function);
    /* string_new takes a halfword length; the stdio buffer this drains is far
       smaller than HALFWORD_MAX, but clamp defensively rather than truncate. */
    halfword chunk = (halfword)((size > (size_t)HALFWORD_MAX) ? (size_t)HALFWORD_MAX : size);
    Ref string = string_new(chunk);
    add_to_live_pool(string);
    memcpy(string_octets(string), buf, (size_t)chunk);
    MultipleValues results;
    results.count = 0;
    /* We are inside a libc stdio flush: a non-local exit escaping WRITE-FUNCTION
       would longjmp through libc and corrupt it (verified to crash).  Use the
       boundary-protected apply, which absorbs any such exit. */
    kernel_apply_at_boundary(write_function, list(string, NULL), &results);
    close_live_pool(pool);}

#if defined(BOCL_CALLBACK_STREAM_FOPENCOOKIE)
static ssize_t cookie_write(void* cookie, const char* buf, size_t size){
    callback_stream_write((Ref)cookie, buf, size);
    return (ssize_t)size;}
#elif defined(BOCL_CALLBACK_STREAM_FUNOPEN)
static int bsd_write(void* cookie, const char* buf, int size){
    if(size > 0){ callback_stream_write((Ref)cookie, buf, (size_t)size); }
    return size;}
#endif

Ref make_output_callback_stream(Ref write_function){
    uword pool = open_live_pool();
    add_to_live_pool(write_function);
    /* A genuine FILE-STREAM so stream_cstream()/typep work everywhere.  The
       `string` slot (unused for non-string FILE-STREAMs) holds WRITE-FUNCTION. */
    Ref stream = file_stream_new(physical_pathname_new_cstring("/dev/callback-stream"),
                                 S(CHARACTER), K(DEFAULT));
    add_to_live_pool(stream);
    stream->slots->file_stream.direction = direction_output;
    stream->slots->file_stream.string = write_function;
    /* The cookie is the stream Ref itself.  bocl's GC is non-moving, so the
       pointer stays valid for the stream's lifetime; the stream must remain
       reachable while the FILE* is open (the CL layer guarantees this). */
#if defined(BOCL_CALLBACK_STREAM_FOPENCOOKIE)
    cookie_io_functions_t io = { NULL, cookie_write, NULL, NULL };
    FILE* file = fopencookie((void*)stream, "w", io);
#elif defined(BOCL_CALLBACK_STREAM_FUNOPEN)
    FILE* file = funopen((const void*)stream, NULL, bsd_write, NULL, NULL);
#else
#  error "No custom-stream API (fopencookie/funopen) for this platform"
#endif
    if(!file){
        ERROR("Cannot create a callback output stream%s", "");}
    /* Unbuffered: each WRITE-CHAR / WRITE-STRING fires the callback immediately,
       so writes tee synchronously and in order (the CL line-prefix logic copes
       with whatever chunk boundaries arrive).  Callers that want batching can
       still rely on stdio's own line discipline via FORCE-OUTPUT. */
    setvbuf(file, NULL, _IONBF, 0);
    stream->slots->file_stream.stream = file;
    stream->slots->file_stream.is_open = true;
    close_live_pool(pool);
    return stream;}

void kernel_MAKE_OUTPUT_CALLBACK_STREAM(Ref function,Ref arguments,MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current = arguments;
    Ref par_write_function = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count = 1;
    results->values[0] = make_output_callback_stream(par_write_function);
    close_live_pool(pool);}

/**** THE END ****/
