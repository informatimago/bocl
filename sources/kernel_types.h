#ifndef kernel_types_h
#define kernel_types_h
#include <unistd.h> // import _POSIX_V6_LP64_OFF64
#include <stdio.h>
#include <setjmp.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include "bolimits.h"

#ifdef _POSIX_V6_LP64_OFF64
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

#else
typedef int32_t           word;
typedef uint32_t          uword;
typedef uint32_t          halfword;
#define WORD_MIN          INT32_MIN
#define WORD_MAX          INT32_MAX
#define UWORD_MAX         UINT32_MAX
#define HALFWORD_MAX      UINT32_MAX
#define WORD_FORMAT       PRId32
#define UWORD_FORMAT      PRIu32
#define HALFWORD_FORMAT   PRIu32
#define WORD_X_FORMAT     PRIx32
#define UWORD_X_FORMAT    PRIx32
#define HALFWORD_X_FORMAT PRIx32

#define TAG_MASK          0x0000FFFF
#define TAGBODY_SHIFT     16

#endif

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

#if defined(__STDC_ISO_10646__) || defined(__USE_ISOC11) || defined(strict_iso_c)
#define VARRAY(field)      field[1]
#define VARRAY_MIN(count)  (((count)<=0)?1:(count))
#else
#define VARRAY(field)      field[0]
#define VARRAY_MIN(count)  (count)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-length-array"
#endif

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
} frame_type;

typedef struct Frame {
    struct Frame* next;
    struct Frame* target;
    VectorOfObject* frame; /* the local variables and local functions */
    frame_type type;
    union {
        struct {
            Ref name;
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

typedef struct Float {
    BLOCK_HEADER;
    floating value;
} Float;

typedef struct Array {
    BLOCK_HEADER;
    Ref element_type;
    VectorOfObject* elements;
    uword VARRAY(dimensions);
} Array;

typedef struct OctetArray {
    BLOCK_HEADER;
    Ref element_type;
    VectorOfOctet* elements;
    uword VARRAY(dimensions);
} OctetArray;

#if defined(__STDC_ISO_10646__) || defined(__USE_ISOC11) || defined(strict_iso_c)
/* --- */
#else
#pragma GCC diagnostic pop
#endif


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
    BLOCK_HEADER;
    Ref name;        /* STRING — the package's primary name */
    Ref nicknames;   /* list of STRING */
    Ref symbols;     /* list of symbol — interned in this package */
    Ref exported;    /* list of symbol — visible via pkg:sym syntax */
    Ref used;        /* list of Package — used-package list */
} Package;

/* CL Hash Table. Phase 4 minimum: alist-backed (per design notes —
   "alists for simplicity"). test is one of the symbols EQ, EQL, EQUAL,
   EQUALP and selects the comparator. */
typedef struct HashTable {
    BLOCK_HEADER;
    Ref test;        /* SYMBOL — EQ / EQL / EQUAL / EQUALP */
    Ref entries;     /* alist: ((key . value) ...) */
    Ref count;       /* INTEGER — number of entries */
} HashTable;

/* CL Condition object. Minimal: the class slot already lives in Object;
   we add a format-control STRING and format-arguments list so SIMPLE-
   ERROR / SIMPLE-WARNING etc. can carry their data. */
typedef struct Condition {
    BLOCK_HEADER;
    Ref format_control;       /* STRING or NIL */
    Ref format_arguments;     /* list */
} Condition;


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
    Float                       floater;
    Array                       array;
    OctetArray                  octet_array;
    Cons                        cons;
    Symbol                      symbol;
    FileStream                  file_stream;
    Closure                     closure;
    Function                    function;
    BuiltInFunction             built_in_function;
    FuncallableStandardObject	funcallable_function;
    Package                     package;
    HashTable                   hash_table;
    Condition                   condition;
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
