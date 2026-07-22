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
#include "kernel_objects.h"   /* FILE_STREAM_Class, SYMBOL_Class */
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
    add_to_live_pool(stream);   /* re-root: fresh stream, trimmed by the close */
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

/* ------------------------------------------------------------------------
   Composite (compound) streams: TWO-WAY / ECHO / SYNONYM / CONCATENATED.

   Each is a genuine FILE-STREAM backed by a cookie FILE* whose read/write
   callbacks forward, IN PURE C, to the component streams.  The component data
   lives in the stream's own `string` slot (a cons (INPUT . OUTPUT), the
   synonym SYMBOL, or the CONCATENATED list of streams) and the kind lives in
   `composite_kind`.  Because the callbacks only resolve component FILE*s and
   do fgetc/fputc — no allocation, no evaluator re-entry, no non-local exit —
   they are safe to run inside a libc stdio fill/flush and cannot trigger GC.
   That is the whole point of doing this in C rather than via a Lisp callback
   (unlike the output callback above): the kernel reader calls fgetc() with
   live, unrooted C locals on the stack, so a GC there would corrupt the parse.

   The Lisp layer (prelude.lisp) additionally records (KIND . COMPONENTS) in
   the compound-stream registry for the predicates and accessors; this C copy
   is the callbacks' private, allocation-free view of the same components. */

/* Resolve a component stream to its input/output FILE*, tolerating the NIL / T
   designators, and WITHOUT ever signalling: a stray non-stream simply yields
   NULL (→ EOF / discard), because ERROR here would longjmp through libc. */
static FILE* composite_input_file(Ref component){
    component = stream_designator_input(component);
    if(!typep(component,FILE_STREAM_Class)){ return NULL; }
    return component->slots->file_stream.stream;}

static FILE* composite_output_file(Ref component){
    component = stream_designator_output(component);
    if(!typep(component,FILE_STREAM_Class)){ return NULL; }
    return component->slots->file_stream.stream;}

/* The synonym target: the current value of the stored symbol, or NIL if the
   symbol is unbound (→ NULL FILE* → EOF / discard). */
static Ref composite_synonym_target(Ref symbol){
    if(!typep(symbol,SYMBOL_Class) || !symbol_boundp(symbol)){ return NIL_Symbol; }
    return symbol_value(symbol);}

/* Produce the next input byte for a composite input stream, or EOF. */
static int composite_read_byte(Ref stream){
    Ref data = stream->slots->file_stream.string;
    switch((composite_t)stream->slots->file_stream.composite_kind){
    case composite_synonym: {
        FILE* in = composite_input_file(composite_synonym_target(data));
        return in ? fgetc(in) : EOF; }
    case composite_two_way: {
        FILE* in = composite_input_file(cons_car(data));   /* data = (in . out) */
        return in ? fgetc(in) : EOF; }
    case composite_echo: {
        FILE* in = composite_input_file(cons_car(data));   /* data = (in . out) */
        int c = in ? fgetc(in) : EOF;
        if(c != EOF){
            FILE* out = composite_output_file(cons_cdr(data));
            if(out){ fputc(c,out); }}   /* echo the read char to the output side */
        return c; }
    case composite_concatenated: {
        /* data = list of component streams; skip any already at end-of-file. */
        for(Ref l=data; consp(l); l=cons_cdr(l)){
            FILE* in = composite_input_file(cons_car(l));
            if(!in || feof(in)){ continue; }
            int c = fgetc(in);
            if(c != EOF){ return c; }
            /* this component just reached EOF (feof(in) now set) — try the next. */
        }
        return EOF; }
    case composite_none:
    default:
        return EOF; }}

/* Forward SIZE bytes written to a composite stream to its output side. */
static void composite_write_bytes(Ref stream, const char* buf, size_t size){
    if(size == 0){ return; }
    Ref data = stream->slots->file_stream.string;
    FILE* out = NULL;
    switch((composite_t)stream->slots->file_stream.composite_kind){
    case composite_two_way:
    case composite_echo:
        out = composite_output_file(cons_cdr(data)); break;   /* data = (in . out) */
    case composite_synonym:
        out = composite_output_file(composite_synonym_target(data)); break;
    case composite_concatenated:   /* input-only: nothing to write */
    case composite_none:
    default:
        return; }
    if(out){ fwrite(buf,1,size,out); }}

#if defined(BOCL_CALLBACK_STREAM_FOPENCOOKIE)
static ssize_t composite_cookie_read(void* cookie, char* buf, size_t size){
    if(size == 0){ return 0; }
    int c = composite_read_byte((Ref)cookie);
    if(c == EOF){ return 0; }               /* 0 => end of file for fopencookie */
    buf[0] = (char)c;
    return 1;}
static ssize_t composite_cookie_write(void* cookie, const char* buf, size_t size){
    composite_write_bytes((Ref)cookie, buf, size);
    return (ssize_t)size;}
#elif defined(BOCL_CALLBACK_STREAM_FUNOPEN)
static int composite_bsd_read(void* cookie, char* buf, int size){
    if(size <= 0){ return 0; }
    int c = composite_read_byte((Ref)cookie);
    if(c == EOF){ return 0; }
    buf[0] = (char)c;
    return 1;}
static int composite_bsd_write(void* cookie, const char* buf, int size){
    if(size > 0){ composite_write_bytes((Ref)cookie, buf, (size_t)size); }
    return size;}
#endif

/* KIND is a composite_t (see kernel_types.h).  DATA is the component data:
   (INPUT . OUTPUT) for two-way/echo, the SYMBOL for synonym, the list of
   streams for concatenated.  Returns a fresh FILE-STREAM. */
Ref make_composite_stream(int kind, Ref data){
    uword pool = open_live_pool();
    add_to_live_pool(data);
    Ref stream = file_stream_new(physical_pathname_new_cstring("/dev/composite-stream"),
                                 S(CHARACTER), K(DEFAULT));
    add_to_live_pool(stream);
    stream->slots->file_stream.string = data;   /* GC roots the components (objcount=4) */
    stream->slots->file_stream.composite_kind = (halfword)kind;
    bool has_input  = (kind==composite_two_way || kind==composite_echo ||
                       kind==composite_synonym || kind==composite_concatenated);
    bool has_output = (kind==composite_two_way || kind==composite_echo ||
                       kind==composite_synonym);
    stream->slots->file_stream.direction =
        (has_input && has_output) ? direction_io
        : (has_output ? direction_output : direction_input);
    const char* mode = (has_input && has_output) ? "r+" : (has_output ? "w" : "r");
#if defined(BOCL_CALLBACK_STREAM_FOPENCOOKIE)
    cookie_io_functions_t io = { has_input  ? composite_cookie_read  : NULL,
                                 has_output ? composite_cookie_write : NULL,
                                 NULL, NULL };
    FILE* file = fopencookie((void*)stream, mode, io);
#elif defined(BOCL_CALLBACK_STREAM_FUNOPEN)
    FILE* file = funopen((const void*)stream,
                         has_input  ? composite_bsd_read  : NULL,
                         has_output ? composite_bsd_write : NULL,
                         NULL, NULL);
#else
#  error "No custom-stream API (fopencookie/funopen) for this platform"
#endif
    if(!file){
        ERROR("Cannot create a composite stream%s", "");}
    /* Unbuffered: exactly one component byte per underlying read (so an echo
       stream echoes each char once, and a concatenated stream never over-reads
       past a component boundary) and each write teed immediately. */
    setvbuf(file, NULL, _IONBF, 0);
    stream->slots->file_stream.stream = file;
    stream->slots->file_stream.is_open = true;
    close_live_pool(pool);
    add_to_live_pool(stream);   /* re-root: fresh stream, trimmed by the close */
    return stream;}

void kernel_MAKE_COMPOSITE_STREAM(Ref function,Ref arguments,MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(arguments);
    Ref current = arguments;
    Ref par_kind = pop_mandatory_argument(function,&current);
    Ref par_data = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count = 1;
    results->values[0] = make_composite_stream((int)integer_value(par_kind), par_data);
    close_live_pool(pool);}

/**** THE END ****/
