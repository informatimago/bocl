#ifndef kernel_types_h
#define kernel_types_h
#define _POSIX_SOURCE
#include <unistd.h> // import _POSIX_V6_LP64_OFF64
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "bolimits.h"

#ifdef _POSIX_V6_LP64_OFF64
typedef int64_t  word;
typedef uint64_t uword;
typedef uint32_t halfword;
#define WORD_MIN INT64_MIN
#define WORD_MAX INT64_MAX
#define UWORD_MAX UINT64_MAX
#define HALFWORD_MAX UINT32_MAX
#define WORD_FORMAT "lld"
#define UWORD_FORMAT "llu"
#define HALFWORD_FORMAT "u"
#define WORD_X_FORMAT "llx"
#define UWORD_X_FORMAT "llx"
#define HALFWORD_X_FORMAT "x"
#else
typedef int32_t  word;
typedef uint32_t uword;
typedef uint32_t halfword;
#define WORD_MIN INT32_MIN
#define WORD_MAX INT32_MAX
#define UWORD_MAX UINT32_MAX
#define HALFWORD_MAX UINT32_MAX
#define WORD_FORMAT "d"
#define UWORD_FORMAT "u"
#define HALFWORD_FORMAT "u"
#define WORD_X_FORMAT "x"
#define UWORD_X_FORMAT "x"
#define HALFWORD_X_FORMAT "x"
#endif
typedef uint8_t  octet;
typedef double   floating;

typedef struct Object Object;
typedef union  Slots  Slots;

/* Slots */

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
} StructureClass;

typedef struct FuncallableStandardClass {
    CLASS_HEADER;
} FuncallableStandardClass;


/* Function Objects */

typedef struct MultipleValues {
    uword count;
    Ref values[MULTIPLE_VALUES_LIMIT];
} MultipleValues;

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


/* Slots */

typedef union Slots {
    /* Internal Object structures: */
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
FuncallableStandardClass* make_boot_FuncallableStandardClass(void);


#endif
