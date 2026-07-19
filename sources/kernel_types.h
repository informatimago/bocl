#ifndef kernel_types_h
#define kernel_types_h
#include <unistd.h> // import _POSIX_V6_LP64_OFF64
#include <stdio.h>
#include <setjmp.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include "bolimits.h"

/* `word` is the fixnum integer type. It is int64 on EVERY target, including
   32-bit-pointer ones (armv7, i386): it is not tied to pointer width (there are
   no word<->pointer casts, and TAG_MASK/TAGBODY_SHIFT below are unused), so a
   64-bit word costs nothing in correctness and keeps the full fixnum range.
   The previous gate on _POSIX_V6_LP64_OFF64 was wrong: that POSIX macro is
   defined on Debian armv7 but NOT on i386, so armv7 got int64 (correct) while
   i386 silently got int32 — shrinking its fixnum range and breaking integer
   arithmetic. armv7 is the existence proof that int64 `word` is correct on a
   32-bit target. (See issues/.../i386-32bit-arithmetic-broken.issue.) */
typedef int64_t           word;
typedef uint64_t          uword;
typedef uint32_t          halfword;
#define WORD_MIN          INT64_MIN
#define WORD_MAX          INT64_MAX
#define UWORD_MAX         UINT64_MAX
#define HALFWORD_MAX      UINT32_MAX
#define WORD_FORMAT       PRId64
#define UWORD_FORMAT      PRIu64
#define HALFWORD_FORMAT   PRIu32
#define WORD_X_FORMAT     PRIx64
#define UWORD_X_FORMAT    PRIx64
#define HALFWORD_X_FORMAT PRIx32

#define TAG_MASK          0x00000000FFFFFFFF
#define TAGBODY_SHIFT     32

typedef uint8_t  octet;
typedef double   floating;

typedef struct Object Object;
typedef union  Slots  Slots;

/* Slots */

typedef struct Environment              Environment;
typedef struct VectorOfObject           VectorOfObject;
typedef struct VectorOfUWord            VectorOfUWord;
typedef struct VectorOfOctet            VectorOfOctet;
typedef struct Character                Character;
typedef struct Integer                  Integer;
typedef struct Bignum                   Bignum;
typedef struct Float                    Float;
typedef struct Array                    Array;
typedef struct OctetArray               OctetArray;
typedef struct Cons                     Cons;
typedef struct Symbol                   Symbol;
typedef struct FileStream               FileStream;
typedef struct Class                    Class;
typedef struct StandardClass            StandardClass;
typedef struct BuiltInClass             BuiltInClass;
typedef struct StructureClass           StructureClass;
typedef struct FuncallableStandardClass FuncallableStandardClass;

typedef struct FunctionClass            FunctionClass;
typedef struct CompiledFunctionClass    CompiledFunctionClass;
typedef struct BuiltInFunctionClass     BuiltInFunctionClass;
typedef struct FuncallableFunctionClass FuncallableFunctionClass;



#ifdef _POSIX_V6_LP64_OFF64
#define BLOCK_HEADER   halfword size; halfword objcount
#define BH_FORMAT      HALFWORD_FORMAT
#define BH_X_FORMAT    HALFWORD_X_FORMAT
#define FREE_BLOCK     HALFWORD_MAX
#else
#define BLOCK_HEADER   uword size;    uword objcount
#define BH_FORMAT      UWORD_FORMAT
#define BH_X_FORMAT    UWORD_X_FORMAT
#define FREE_BLOCK     UWORD_MAX
#endif

/* Variable-length trailing arrays: ALWAYS the C89-portable one-element form.
   This used to pick the GNU zero-length form (field[0]) when the glibc
   feature macros (__STDC_ISO_10646__/__USE_ISOC11) were absent — i.e. on
   FreeBSD — which shrank sizeof(Block) & friends by one slot relative to
   every Linux-tested layout and caused layout-dependent heap corruption
   there (mark bit stamped into a neighbouring block's SIZE field via an
   off-by-one-slot reference; see freebsd-varray-heap-corruption.issue).
   One uniform layout on every platform; the phantom slot costs 8 bytes per
   varray STRUCT TYPE, not per object. */
#define VARRAY(field)      field[1]
#define VARRAY_MIN(count)  (((count)<=0)?1:(count))

typedef struct Block{
    BLOCK_HEADER;
    struct Block* VARRAY(blocks);
} Block;

typedef struct Object* Ref;
typedef struct Object {
    BLOCK_HEADER;
    Ref class;
    Slots* slots;
} Object;

/* Compact cons: car/cdr are stored INLINE in the object itself rather than in a
   separate Cons slots block.  A cons is therefore a single 32-byte block
   {header; class=CONS_Class; car; cdr} with objcount=3 — one allocation, one
   dereference for car/cdr, half the GC objects.  It shares the first two fields
   (header, class) with Object, so class_of()/consp() are unchanged; only the
   cons accessors (cons_car/cons_cdr/cons_rplaca/cons_rplacd) read car/cdr from
   here instead of through ->slots.  (See issues/open/deferred-cons-paged-heap
   for the 16-byte next step.) */
typedef struct ConsCell {
    BLOCK_HEADER;
    Ref class;   /* always CONS_Class */
    Ref car;
    Ref cdr;
} ConsCell;




typedef struct MultipleValues {
    uword count;
    Ref values[MULTIPLE_VALUES_LIMIT];
} MultipleValues;

typedef enum {
    frame_protect=1,
    frame_dynamic_binding,
    frame_block,
    frame_catch,
    frame_tagbody,
    frame_handler,
    frame_handler_bind,          /* HANDLER-BIND: call handler in signal's dynamic context */
    frame_handler_bind_inactive, /* same frame, temporarily deactivated while handler runs */
    frame_restart,
    frame_vm_call,               /* byte-code VM activation marker: name+function+args for the
                                    backtrace ONLY.  Carries NO setjmp (unlike every other frame),
                                    so the unwind cascade must SKIP it; never a RETURN-FROM/THROW
                                    target (the VM handles BLOCK/TAGBODY lexically). */
} frame_type;

typedef struct Frame {
    struct Frame* next;      /* link on the global dynamic frame stack (g_frame_stack) */
    struct Frame* env_next;  /* link on the establishing environment's lexical frame list */
    struct Frame* target;
    VectorOfObject* frame; /* the local variables and local functions */
    frame_type type;
    union {
        struct {
            Ref name;
            /* For a function-body block (pushed by APPLY), FUNCTION is the function
               object and ENVIRONMENT its parameter environment, so the debugger
               backtrace can report a :CALL frame with parameter names and values.
               For an explicit BLOCK form both are NIL_Symbol. */
            Ref function;
            Ref environment;
        } block;
        struct {
            Ref object;
        } catch;
        struct {
            Ref tags; /* a list of sublist of the tagbody body, each starting with a different tag */
            Ref target_tag; /* set by GO, the tag we must jump to */
        } tagbody;
        struct {
            /* List of (type-class var-symbol body-form) clauses in source
               order. SIGNAL fills the next two fields with the firing
               clause and its condition before longjmp. */
            Ref clauses;
            Ref matched_clause;   /* (type-class var-sym body) — set by signaller */
            Ref condition;        /* the condition object that was signalled */
        } handler;
        struct {
            /* List of (type-class . handler-fn) pairs.
               handler-fn is a Lisp callable (closure / function).
               Unlike frame_handler, signaling calls handler-fn directly
               and then continues; no longjmp unless the handler itself
               does a non-local exit. */
            Ref bindings;
        } handler_bind;
        struct {
            /* RESTART-CASE: list of (name varlist body-form) triples in
               source order.  INVOKE-RESTART sets matched_name + matched_args
               before longjmping to this frame (same protocol as HANDLER-CASE
               sets matched_clause + condition). */
            Ref restarts;
            Ref matched_name;
            Ref matched_args;
        } restart;
    } parameters;
    MultipleValues* results;
    jmp_buf non_local_exit;
} Frame;

typedef struct Environment {
    BLOCK_HEADER;               /* objcount=6 */
    Ref variables;
    Ref functions;
    Ref blocks;         /* used while minimal-compiling */
    Ref catchs;         /* used while minimal-compiling */
    Ref declarations;   /* used while minimal-compiling */
    Ref next_environment;
    Frame* frames;
    bool is_toplevel;
} Environment;


typedef struct VectorOfObject {
    BLOCK_HEADER;
    Ref VARRAY(elements); /* length is objcount in the block_header */
} VectorOfObject;

typedef struct VectorOfUWord {
    BLOCK_HEADER;
    uword VARRAY(elements);  /* length is (size/8-1) the block_header */
} VectorOfUWord;

typedef struct VectorOfOctet {
    BLOCK_HEADER;
    uword length;       /* used length<=(size/8-2) */
    octet VARRAY(elements);  /* allocated length is (size/8-2) the block_header */
} VectorOfOctet;

typedef struct Character {
    BLOCK_HEADER;
    Ref name;
    Ref code;
} Character;

typedef struct Integer {
    BLOCK_HEADER;
    word value;
} Integer;

/* A BIGNUM boxes an arbitrary-precision integer that does not fit in a word.
   VALUE is a CONS (SIGN . DIGITS) in the i%-format of bootstrap/bignum.lisp:
   SIGN is the symbol + or -, DIGITS a low-order-first list of base-10^9 fixnum
   digits.  The single Ref is GC-traced, so the digit list stays live with no
   finalizer (this is why the Lisp engine is preferred over a malloc'd GMP). */
typedef struct Bignum {
    BLOCK_HEADER;       /* objcount = 1 */
    Ref value;
} Bignum;

typedef struct Float {
    BLOCK_HEADER;
    floating value;
} Float;

/* fill_pointer is an UNTRACED inline word (objcount stays 2, so the collector
   marks only element_type + elements and ignores it).  Encoding: 0 means "no
   fill pointer"; an active length of N is stored as N+1.  Because allocate()
   zeroes every block body, a freshly built vector/string/bit-vector carries no
   fill pointer automatically — no constructor change, and no missed allocation
   site can masquerade as fill pointer 0.  See array_fill_pointer() in kernel.c. */
typedef struct Array {
    BLOCK_HEADER;
    Ref element_type;
    VectorOfObject* elements;
    uword fill_pointer;        /* 0 = none; else active-length + 1 (see above) */
    uword VARRAY(dimensions);
} Array;

typedef struct OctetArray {
    BLOCK_HEADER;
    Ref element_type;
    VectorOfOctet* elements;
    uword fill_pointer;        /* 0 = none; else active-length + 1 (see above) */
    uword VARRAY(dimensions);
} OctetArray;




typedef struct Cons {
    BLOCK_HEADER;
    Ref car;
    Ref cdr;
} Cons;

typedef struct Symbol {
    BLOCK_HEADER;
    Ref name;
    Ref package;
    Ref value;
    Ref function;
    Ref plist;
} Symbol;


typedef enum { direction_input, direction_output, direction_io, direction_probe } direction_t;
typedef enum { type_character, type_octet, type_bit } type_t;
typedef enum { encoding_us_ascii, encoding_iso_8859_1 } encoding_t;
typedef enum { new_line_crlf, new_line_cr, new_line_lf } new_line_t;
typedef enum { on_error_error=0xf001, on_error_ignore=0xf002 } on_error_t;

typedef struct FileStream {
    BLOCK_HEADER;
    Ref pathname;         /* a PHYSICAL-PATHNAME */
    Ref element_type;     /* CHARACTER or (UNSIGNED-BYTE 8) or BIT */
    Ref external_format;  /* Normalized external-format: (list (member :US-ASCII :ISO-8859-1) (member :crlf :cr :lf) (or character (member :error nil))) */
    Ref string;           /* for STRING-STREAMS */
    halfword is_open;
    halfword direction;
    halfword type;        /* derived from element_type */
    halfword encoding;    /* derived from external_format */
    halfword new_line;    /* derived from external_format */
    halfword on_error;    /* derived from external_format */
    FILE* stream;
    octet* buffer;        /* buffer for memstream; in string. */
    size_t buffer_size;   /* size of buffer for memstream; in string. */
    // Note: whatever the element-type, we always write the same octets.
    // the above layer will convert possible character or integer into octets,
    // but the usual buffer types will already use VectorOfOctet storage.
    // Encoding is only used to signal an error upon 128<=octet
    // and new_line is only used for character streams.
    /* TODO: add a file-stream lock? Or rely on the FILE lock? */
} FileStream;


#define CLASS_HEADER \
    BLOCK_HEADER; \
    Ref name; \
    Ref direct_superclasses

typedef struct Class {
    CLASS_HEADER;
} Class;

typedef struct StandardClass {
    CLASS_HEADER;
} StandardClass;

typedef struct BuiltInClass {
    CLASS_HEADER;
} BuiltInClass;

typedef struct StructureClass {
    CLASS_HEADER;
    Ref offset; /* An integer, number of additionnal slots. */
    Ref slot_names; /* A list of symbols naming the slots. */
    Ref cached_slots; /* memoized (slot-name . index) a-list for ALL slots, or
                         NULL until first computed; reset to NULL on redefinition
                         (a redefinition makes a fresh StructureClass object).
                         MUST stay among the GC-marked Ref slots (objcount). */
    uword nslots; /* in instances of this instance of structure-class */
    /* Instances of this  instance of structure-class use VectorOfObject to store the slots. */
} StructureClass;

typedef struct FuncallableStandardClass {
    CLASS_HEADER;
} FuncallableStandardClass;


/* Function Objects */
typedef void (*KernelFunctionPtr)(Ref function,Ref arguments,MultipleValues* results);

typedef struct Closure            {
    BLOCK_HEADER;
    Ref function;
    Ref environment;
} Closure;

typedef struct Function            { /* both FunctionClass and CompiledFunctionClass objets */
    BLOCK_HEADER;
    Ref lisp_name;
    Ref lambda_list;
    Ref body;
} Function;

/* Byte-code VM (plan-2, vm-1).  A CompiledFunction is an immutable TEMPLATE: the
   bytecode + constant pool + arity/frame descriptor; a CompiledClosure pairs one
   with its captured upvalues.  All slots are Refs (objcount counts them), so the
   GC traces them generically.  See bytecode.h for the ISA and the arity packing. */
typedef struct CompiledFunction    {
    BLOCK_HEADER;                 /* objcount = 8 */
    Ref lisp_name;                /* naming symbol, or NIL */
    Ref lambda_list;              /* source lambda list (arglist / FLE) */
    Ref code;                     /* (simple-array (unsigned-byte 8) (*)) — the bytecode */
    Ref constants;                /* simple-vector — the constant pool */
    Ref arity;                    /* fixnum: BC_ARITY_PACK(...) */
    Ref max_stack;                /* fixnum: operand-stack depth needed */
    Ref n_locals;                 /* fixnum: number of local slots */
    Ref source;                   /* source lambda expression (FLE/disassemble), or NIL */
} CompiledFunction;

typedef struct CompiledClosure     {
    BLOCK_HEADER;                 /* objcount = 2 */
    Ref function;                 /* the CompiledFunction template */
    Ref upvalues;                 /* simple-vector of captured cells (the display) */
} CompiledClosure;

typedef struct BuiltInFunction     {
    BLOCK_HEADER;
    Ref           lisp_name;       /* the Lisp Symbol naming the kernel function. */
    Ref           lambda_list;     /* the lambda list of the kernel function. */
    KernelFunctionPtr kernel_function; /* the entry point of to the kernel function. */
} BuiltInFunction;

typedef struct FuncallableStandardObject {
    BLOCK_HEADER;
    /* TODO: FuncallableStandardObject */
    Ref           object; /* The object data */
    KernelFunctionPtr apply;  /* The entry point of to the Funcallable Standard Object. */
} FuncallableStandardObject;

/* CL Package. Phase 3 minimum: every symbol lives in KERNEL_obarray
   (one global pool), and each package keeps a list of the symbols that
   are "in" it. Real per-package symbol isolation will come later. */
typedef struct Package {
    BLOCK_HEADER;            /* objcount = 7 */
    Ref name;        /* STRING — the package's primary name */
    Ref nicknames;   /* list of STRING */
    Ref present;     /* obarray (cons sentinel) — all symbols present (interned) here */
    Ref external;    /* obarray (cons sentinel) — the external subset of `present` */
    Ref shadowing;   /* list of symbol — the shadowing symbols of this package */
    Ref used;        /* list of Package — packages this one uses (use-list) */
    Ref used_by;     /* list of Package — packages that use this one */
} Package;

/* CL Hash Table. Phase 4 minimum: alist-backed (per design notes —
   "alists for simplicity"). test is one of the symbols EQ, EQL, EQUAL,
   EQUALP and selects the comparator. */
typedef struct HashTable {
    BLOCK_HEADER;            /* objcount = 6 */
    Ref test;             /* SYMBOL — EQ / EQL / EQUAL / EQUALP */
    Ref entries;          /* alist: ((key . value) ...) */
    Ref count;            /* INTEGER — number of entries */
    /* Stored verbatim for the CL accessors; bocl's alist table doesn't rehash,
       so these are cosmetic (returned by HASH-TABLE-SIZE / -REHASH-SIZE /
       -REHASH-THRESHOLD and accepted back by MAKE-HASH-TABLE). */
    Ref nominal_size;     /* INTEGER — nominal size (the BLOCK_HEADER `size` is unrelated) */
    Ref rehash_size;      /* REAL */
    Ref rehash_threshold; /* REAL in (0,1] */
} HashTable;

/* CL Condition object. Minimal: the class slot already lives in Object;
   we add a format-control STRING and format-arguments list so SIMPLE-
   ERROR / SIMPLE-WARNING etc. can carry their data. */
typedef struct Condition {
    BLOCK_HEADER;
    Ref format_control;       /* STRING or NIL */
    Ref format_arguments;     /* list */
} Condition;


typedef struct Ratio {
    BLOCK_HEADER;
    Ref numerator;      /* INTEGER (always positive after normalisation) */
    Ref denominator;    /* INTEGER (always > 0 after normalisation) */
} Ratio;

typedef struct Complex {
    BLOCK_HEADER;
    Ref realpart;       /* any real number */
    Ref imagpart;       /* any real number */
} Complex;

/* CL Readtable.  syntax/macros/dispatch are SIMPLE-VECTORs indexed by character
   code (0..127; codes >= 128 are constituents with no macro).  syntax[c] is a
   fixnum syntax type (see +cs-*+ in reader.c); macros[c] is the reader-macro
   function (or NIL); dispatch[c] is NIL or a 128-element SIMPLE-VECTOR mapping a
   sub-character to its dispatch function. */
typedef struct Readtable {
    BLOCK_HEADER;       /* objcount = 4 */
    Ref syntax;         /* SIMPLE-VECTOR[128] of fixnum syntax types */
    Ref macros;         /* SIMPLE-VECTOR[128] of macro function or NIL */
    Ref dispatch;       /* SIMPLE-VECTOR[128] of dispatch sub-table or NIL */
    Ref readcase;       /* keyword :UPCASE / :DOWNCASE / :PRESERVE / :INVERT */
} Readtable;

/* Slots */

typedef union Slots {
    /* Internal Object structures: */
    Environment                 environment;
    VectorOfObject              vector_of_object;
    VectorOfUWord               vector_of_uword;
    VectorOfOctet               vector_of_octet;
    /* Objects: */
    Character                   character;
    Integer                     integer;
    Bignum                      bignum;
    Float                       floater;
    Array                       array;
    OctetArray                  octet_array;
    Cons                        cons;
    Symbol                      symbol;
    FileStream                  file_stream;
    Closure                     closure;
    Function                    function;
    CompiledFunction            compiled_function;
    CompiledClosure             compiled_closure;
    BuiltInFunction             built_in_function;
    FuncallableStandardObject	funcallable_function;
    Package                     package;
    HashTable                   hash_table;
    Condition                   condition;
    Ratio                       ratio;
    Complex                     complex;
    Readtable                   readtable;
    /*                          Classes: */
    Class                       class;
    StandardClass               standard_class;
    StructureClass              structure_class;
    BuiltInClass                built_in_class;
    FuncallableStandardClass    funcallable_standard_class;
} Slots;


Ref make_boot_Object(Slots* slots);
Class* make_boot_Class(void);
StandardClass* make_boot_StandardClass(void);
BuiltInClass* make_boot_BuiltInClass(void);
StructureClass* make_boot_StructureClass(void);
FuncallableStandardClass* make_boot_FuncallableStandardClass(void);



typedef struct {
    word variable;
    word literal;

    word compiler_macro;
    word global_function;
    word local_function;
    word global_macro;
    word local_macro;
    word lambda_function;

    word QUOTE;
    word FUNCTION;
    word SETQ;
    word IF;
    word BLOCK;
    word RETURN_FROM;
    word CATCH;
    word THROW;
    word UNWIND_PROTECT;
    word TAGBODY;
    word GO;
    word LET;
    word LET_;
    word FLET;
    word LABELS;
    word SYMBOL_MACROLET;
    word MACROLET;
    word MULTIPLE_VALUE_CALL;
    word MULTIPLE_VALUE_PROG1;
    word PROGN;
    word PROGV;
    word LOCALLY;
    word THE;
    word EVAL_WHEN;
    word LOAD_TIME_VALUE;
} eval_counters_t;

#endif
