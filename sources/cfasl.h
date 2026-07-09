#ifndef cfasl_h
#define cfasl_h
/* cfasl.h — compact BINARY fast-load file format (issue plan-1-compact-fasl).

   This header IS the format spec (cfasl-1).  The writer (cfasl.c, cfasl-2) and the
   loader (cfasl.c, cfasl-3) implement exactly what is written here.

   WHY: the text fasl (readable macroexpanded s-exprs, fully qualified symbols) is
   bigger than source and re-parsed + re-interned + re-macroexpanded at load.  This
   binary format stores the already-macroexpanded top-level form graph compactly so
   LOAD is deserialize-and-eval: no reader, no re-macroexpand, each symbol interned
   once.

   CONTAINER
     bytes 0..7 : magic  "BOCLFASL"  (ASCII, distinct from the text ";; BOCL <v>
                  FASL" header so LOAD discriminates by peeking the first bytes).
     byte  8    : u8 format version (CFASL_VERSION).  LOAD refuses / falls back to
                  source on a version it does not understand (like the text fasl's
                  %CHECK-FASL-VERSION).
     byte  9    : u8 flags (reserved, 0).
     then       : the FORM STREAM — the serialized top-level forms in order, each an
                  object graph, terminated by CFASL_TAG_EOF.

   OBJECT MODEL — streaming, with an incremental SHARED-OBJECT table (this subsumes
   the "intern table" and "constant pool" of the cfasl-1 sketch and additionally
   handles shared/circular structure).  Each *composite/interned* object (cons,
   string, symbol, keyword, uninterned symbol, simple-vector, structure, bignum,
   ratio, package) is assigned the NEXT integer id the first time it is emitted and
   remembered; a later occurrence emits CFASL_TAG_BACKREF + varint(id).  The reader
   mirrors this with an id->object array, assigning the id (and, for containers,
   ALLOCATING the object) BEFORE reading sub-components, so cycles reconstruct.
   Immediates (nil, t, fixnum, character, floats) are value-encoded and get no id.

   ENCODING PRIMITIVES (endianness-independent):
     - unsigned varint  : LEB128 (7 bits/byte, high bit = continue).
     - signed varint    : zig-zag then LEB128.
     - floats           : IEEE-754 bit pattern, LITTLE-ENDIAN on disk (byte-swapped
                          on big-endian hosts), so the file is portable.

   TAG PAYLOADS (a byte tag, then):
     NIL / T / EOF          : (nothing)
     BACKREF                : uvarint id
     FIXNUM                 : svarint value
     BIGNUM        [shared] : u8 sign(0=+,1=-), uvarint nbytes, nbytes magnitude LE
     RATIO         [shared] : <object numerator> <object denominator>
     SINGLE_FLOAT           : 4 bytes LE
     DOUBLE_FLOAT           : 8 bytes LE
     CHARACTER              : uvarint code
     STRING        [shared] : uvarint len, len octets (bocl strings are 1 octet/char)
     SYMBOL        [shared] : <object package-name-string> <object symbol-name-string>
     KEYWORD       [shared] : <object symbol-name-string>
     UNINTERNED    [shared] : <object symbol-name-string>
     CONS          [shared] : <object car> <object cdr>   (id assigned before car)
     SIMPLE_VECTOR [shared] : uvarint len, len <object elt>
     STRUCTURE     [shared] : <object class-name-symbol> uvarint nslots, nslots <object>
     PACKAGE       [shared] : <object package-name-string>   (designates the package)
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#define CFASL_MAGIC     "BOCLFASL"   /* 8 bytes, no trailing NUL written */
#define CFASL_MAGIC_LEN 8
#define CFASL_VERSION   1

typedef enum {
    CFASL_TAG_EOF = 0,
    CFASL_TAG_NIL,
    CFASL_TAG_T,
    CFASL_TAG_BACKREF,
    CFASL_TAG_FIXNUM,
    CFASL_TAG_BIGNUM,
    CFASL_TAG_RATIO,
    CFASL_TAG_SINGLE_FLOAT,
    CFASL_TAG_DOUBLE_FLOAT,
    CFASL_TAG_CHARACTER,
    CFASL_TAG_STRING,
    CFASL_TAG_SYMBOL,
    CFASL_TAG_KEYWORD,
    CFASL_TAG_UNINTERNED,
    CFASL_TAG_CONS,
    CFASL_TAG_SIMPLE_VECTOR,
    CFASL_TAG_STRUCTURE,
    CFASL_TAG_PACKAGE,
    CFASL_TAG_N
} cfasl_tag_t;

/* ---- Growable output buffer (writer) ------------------------------------ */
typedef struct {
    uint8_t* data;
    size_t   len;
    size_t   cap;
    int      error;      /* set on OOM */
} cfasl_out;

void cfasl_out_init(cfasl_out* b);
void cfasl_out_free(cfasl_out* b);
void cfasl_put_u8(cfasl_out* b, uint8_t v);
void cfasl_put_bytes(cfasl_out* b, const void* p, size_t n);
void cfasl_put_uvarint(cfasl_out* b, uint64_t v);
void cfasl_put_svarint(cfasl_out* b, int64_t v);
void cfasl_put_f32(cfasl_out* b, float v);    /* little-endian on disk */
void cfasl_put_f64(cfasl_out* b, double v);   /* little-endian on disk */

/* ---- Input cursor (reader) ---------------------------------------------- */
typedef struct {
    const uint8_t* data;
    size_t         len;
    size_t         pos;
    int            error;   /* set on truncation / malformed input */
} cfasl_in;

void     cfasl_in_init(cfasl_in* c, const uint8_t* data, size_t len);
int      cfasl_at_end(cfasl_in* c);
uint8_t  cfasl_get_u8(cfasl_in* c);
void     cfasl_get_bytes(cfasl_in* c, void* dst, size_t n);
uint64_t cfasl_get_uvarint(cfasl_in* c);
int64_t  cfasl_get_svarint(cfasl_in* c);
float    cfasl_get_f32(cfasl_in* c);
double   cfasl_get_f64(cfasl_in* c);

/* ---- Object-graph writer (cfasl-2) -------------------------------------- */
/* `Ref` is the bocl object handle (canonical typedef in kernel_types.h; the
   identical redefinition here is legal in C11+ and keeps this header usable by
   the object API without dragging in the whole kernel).                     */
typedef struct Object* Ref;

/* A writer context bundles the growable output with the streaming SHARED-OBJECT
   table: an object-pointer -> id map used to emit CFASL_TAG_BACKREF for shared
   or circular structure (the reader mirrors it with an id->object array).  The
   context is opaque; the table is a private open-addressing pointer hash.  GC
   is non-moving and the writer allocates no bocl objects, so raw Ref pointers
   are stable keys for the whole serialization.

   Supported object types: nil, t, fixnum, character, double-float, string,
   symbol / keyword / uninterned-symbol, cons, ratio (with fixnum components),
   and simple-vector.  cfasl_wctx_error() becomes true if an unsupported object
   is met (bignum, structure, bit-vector, non-simple array, stream, ...; see
   cfasl-6).  The caller then discards the buffer and falls back to the text
   fasl, so an unserializable literal degrades to a correct-but-slower load. */
typedef struct cfasl_wctx cfasl_wctx;

cfasl_wctx* cfasl_wctx_new(cfasl_out* out);
void        cfasl_wctx_free(cfasl_wctx* w);
int         cfasl_wctx_error(cfasl_wctx* w);   /* out-of-memory or unsupported type */

/* Serialize one object graph into the context's output buffer. */
void        cfasl_write_object(cfasl_wctx* w, Ref obj);

/* ---- Object-graph reader (cfasl-3) -------------------------------------- */
/* Rebuilds an object graph written by cfasl_write_object, mirroring the shared-
   object table with an id->object array so BACKREFs resolve and cycles / shared
   structure reconstruct with the original EQ identity.  The reader ALLOCATES
   bocl objects: the CALLER must have a live pool open before each read (see
   bocl_fasl_load) — allocate() auto-roots new objects there.  The shared-object
   scope is ONE top-level form: bocl_fasl_load uses a fresh context per form, so
   ids restart at 0 each form (bounded retention, mirroring the text loader's
   per-form pool discipline; the writer resets per form to match). */
typedef struct cfasl_rctx cfasl_rctx;

cfasl_rctx* cfasl_rctx_new(cfasl_in* in);
void        cfasl_rctx_free(cfasl_rctx* r);
int         cfasl_rctx_error(cfasl_rctx* r);   /* truncated / malformed / bad ref */
Ref         cfasl_read_object(cfasl_rctx* r);

/* Load a binary fasl: verify magic + version, then deserialize and evaluate
   each top-level form (like bocl_load_cstring for text).  Returns T_Symbol on a
   successful load, or NIL_Symbol when PATH is not a cfasl / carries a version
   this build cannot read (the caller then falls back to the source or text
   fasl, mirroring %CHECK-FASL-VERSION).  Defined in bocl.c. */
Ref         bocl_fasl_load(const char* path);

#endif
