#ifndef kernel_types_h
#define kernel_types_h
#include <stdio.h>
#include <stdint.h>

#ifdef _POSIX_V6_LP64_OFF64
typedef int64_t  word;
typedef uint64_t uword;
typedef uint32_t halfword;
#define WORD_MIN INT64_MIN
#define WORD_MAX INT64_MAX
#define UWORD_MAX UINT64_MAX
#define HALFWORD_MAX UINT32_MAX
#else
typedef int32_t  word;
typedef uint32_t uword;
typedef uint16_t halfword;
#define WORD_MIN INT32_MIN
#define WORD_MAX INT32_MAX
#define UWORD_MAX UINT32_MAX
#define HALFWORD_MAX UINT16_MAX
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



#define BLOCK_HEADER halfword size;halfword objcount

typedef struct Block{
    BLOCK_HEADER;
    struct Block* blocks[0];
} Block;

typedef struct Object {
    BLOCK_HEADER;
    Object* class;
    Slots* slots;
} Object;

typedef struct VectorOfObject {
    BLOCK_HEADER;
    Object* elements[0];
} VectorOfObject;

typedef struct VectorOfUWord {
    BLOCK_HEADER;
    uword elements[0];
} VectorOfUWord;

typedef struct VectorOfOctet {
    BLOCK_HEADER;
    uword length;
    octet elements[0];
} VectorOfOctet;

typedef struct Character {
    BLOCK_HEADER;
    Object* name;
    Object* code;
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
    Object* element_type;
    VectorOfObject* elements;
    uword dimensions[0];
} Array;

typedef struct OctetArray {
    BLOCK_HEADER;
    Object* element_type;
    VectorOfOctet* elements;
    uword dimensions[0];
} OctetArray;

typedef struct Cons {
    BLOCK_HEADER;
    Object* car;
    Object* cdr;
} Cons;

typedef struct Symbol {
    BLOCK_HEADER;
    Object* name;
    Object* package;
    Object* value;
    Object* function;
    Object* plist;
} Symbol;

typedef struct FileStream {
    BLOCK_HEADER;
    Object* pathname;     // a PHYSICAL-PATHNAME
    Object* element_type; // CHARACTER or (UNSIGNED-BYTE 8) or BIT
    Object* external_format; // :DEFAULT or encoding or (encoding . line-termination)
    // with encoding in (:US-ASCII :ISO-8859-1)
    // with line-termination in (:CR :LF :CRLF)
    halfword is_open;
    halfword direction;
    halfword encoding; // derived from external_format
    halfword line_termination; // derived from external_format
    FILE* stream;
    // Note: whatever the element-type, we always write the same octets.
    // the above layer will convert possible character or integer into octets,
    // but the usual buffer types will already use VectorOfOctet storage.
    // Encoding is only used to signal an error upon 128<=octet
    // and line_termination is only used for character streams.
} FileStream;


#define CLASS_HEADER \
    BLOCK_HEADER; \
    Object* name; \
    Object* direct_superclasses

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

typedef union Slots {
    VectorOfObject vector_of_object;
    VectorOfUWord  vector_of_uword;
    VectorOfOctet  vector_of_octet;
    Character      character;
    Integer        integer;
    Float          floate;
    Array          array;
    OctetArray     octet_array;
    Cons           cons;
    Symbol         symbol;
    FileStream     file_stream;
    Class          class;
    StandardClass  standard_class;
    StructureClass structure_class;
    BuiltInClass   built_in_class;
    FuncallableStandardClass funcallable_standard_class;
} Slots;


Object* make_boot_Object(Slots* slots);
Class* make_boot_Class(void);
StandardClass* make_boot_StandardClass(void);
BuiltInClass* make_boot_BuiltInClass(void);
FuncallableStandardClass* make_boot_FuncallableStandardClass(void);


#endif
