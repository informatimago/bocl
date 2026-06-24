#define _POSIX_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include "backtrace_compat.h"   /* execinfo backtrace() or no-op stubs (musl) */
#include "kernel.h"
#include "kernel_types.h"
#include "overflow.h"   /* portable bocl_u{add,mul}_overflow for the bignum magnitude code */
#include "kernel_objects.h"
#include "kernel_private.h"
#include "macros.h"
#include "memory.h"
#include "boerror.h"
#include "cons.h"
#include "data.h"
#include "structure.h"
#include "variable.h"
#include "stream.h"
#include "reader.h"
#include "printer.h"
#include "structure_functions.h"
#include "arithmetic.h"
#include "version.h"
#include "bocl.h"
#include "packages.h"
#include "hash_table.h"
#include "conditions.h"
#include "sequences.h"

/* Internal Object Structure */

Ref                              Object_new                         (Ref  class,        Slots*   slots);

Array*                           Array_slots                        (Ref  element_type, halfword rank, halfword* dimensions);
static BuiltInClass*             BuiltInClass_slots                 (Ref  name,         Ref  superclasses);
static Character*                Character_slots                    (Ref  name,         Ref  code);
static Class*                    Class_slots                        (Ref  name,         Ref  superclasses);
static Float*                    Float_slots                        (floating value);
static Closure*                  Closure_slots                      (Ref  function,     Ref environment);
static Function*                 Function_slots                     (Ref  lisp_name,    Ref lambda_list, Ref body);
static FuncallableStandardClass* FuncallableStandardClass_slots     (Ref  name ,        Ref superclasses) unused;
static Integer*                  Integer_slots                      (word value);
static OctetArray*               OctetArray_slots                   (Ref  element_type, halfword rank, halfword* dimensions);
static OctetArray*               OctetArray_slots_cstring           (Ref  element_type, const char* initial_value);
static OctetArray*               OctetArray_slots_vector_of_octets  (Ref  element_type, VectorOfOctet* octets);
static StandardClass*            StandardClass_slots                (Ref  name,         Ref  superclasses) unused;
static StructureClass*           StructureClass_slots               (Ref  class_name,   Ref  direct_superclass, Ref offset, Ref slot_names);
static Symbol*                   Symbol_slots                       (Ref  name,         Ref  package);
static VectorOfObject*           VectorOfObject_slots               (halfword size);
static VectorOfOctet*            VectorOfOctet_slots                (halfword length);
static VectorOfOctet*            VectorOfOctet_slots_cstring        (const char* value);
static VectorOfUWord*            VectorOfUWord_slots                (halfword size) unused;
static Environment*              Environment_slots                  (Ref next_environment,bool is_toplevel);




/****************************************/
/*** Object and Slots *******************/
/****************************************/

/* Object */

Ref Object_new(Ref class,Slots* slots){
    /* Root class + slots across the allocation: a caller typically writes
       Object_new(CLASS, X_slots(...)) with the fresh slots held only in a C
       temporary, so a GC inside allocate() would reclaim it.  Balanced
       open/close => no live-pool growth (rooted only for the allocation). */
    uword pool=open_live_pool();
    add_to_live_pool(class);
    add_to_live_pool((Ref)slots);
    Ref that=allocate(sizeof(*that));
    close_live_pool(pool);
    that->objcount=2;
    that->class=class;
    that->slots=slots;
    return that;}

Ref make_boot_Object(Slots* slots){
    Ref that=allocate(sizeof(*that));
    that->objcount=2;
    that->class=&uninitialized_object;
    that->slots=slots;
    return that;}

/* VectorOfObject */

VectorOfObject* VectorOfObject_slots(halfword size){
    VectorOfObject* that=allocate(offsetof(VectorOfObject,elements)+sizeof(that->elements[0])*VARRAY_MIN(size));
    that->objcount=size;
    for(halfword i=0;i<size;i++){
        that->elements[i]=NIL_Symbol;}
    return that;}

/* VectorOfUWord */

VectorOfUWord* VectorOfUWord_slots(halfword size){
    VectorOfUWord* that=allocate(offsetof(VectorOfUWord,elements)+sizeof(that->elements[0])*VARRAY_MIN(size));
    /* Count must be SIZE elements, not one: a SIZE==0 vector has no element
       storage, so writing one element here overruns into the next block's
       header (see VectorOfOctet_slots).  No-op for SIZE==0. */
    memset(that->elements,0,(size_t)size*sizeof(that->elements[0]));
    return that;}


/* VectorOfOctet */

VectorOfOctet* VectorOfOctet_slots(halfword length){
    VectorOfOctet* that=allocate(offsetof(VectorOfOctet,elements)+sizeof(that->elements[0])*VARRAY_MIN(length));
    that->length=length;
    /* Zero exactly the LENGTH allocated element bytes.  The count must be LENGTH,
       not sizeof(element): with the zero-length-array layout a LENGTH==0 vector
       has no element storage, and writing even one byte at that->elements lands
       in the next block's header (clobbering its size field -> size-0 block ->
       next_block() self-loop -> heap-walk hang).  allocate() already zeroed the
       body, so this is belt-and-braces for LENGTH>0 and a no-op for LENGTH==0. */
    memset(that->elements,0,(size_t)length);
    return that;}

VectorOfOctet* VectorOfOctet_slots_cstring(const char* value){
    size_t len=strlen(value);
    if(HALFWORD_MAX<len){
        ERROR("string too long %ld",len);}
    halfword length=(halfword)len;
    VectorOfOctet* that=allocate(offsetof(VectorOfOctet,elements)+sizeof(that->elements[0])*VARRAY_MIN(length));
    that->length=length;
    for(halfword i=0;i<length;i++){
        that->elements[i]=(octet)value[i];}
    return that;}

/* ARRAY */
/* Used both for ARRAY and VECTOR classes, */
/* as long as element-type is not octet, bit or character. */
Array* Array_slots(Ref element_type,halfword rank,halfword* dimensions)  {
    halfword total_array_size=1;
    for(halfword i=0;i<rank;i++){
        total_array_size=(halfword)(total_array_size*dimensions[i]);}
    VectorOfObject* elements=VectorOfObject_slots(total_array_size);
    Array* that=allocate(offsetof(Array,dimensions)+sizeof(that->dimensions[0])*VARRAY_MIN(rank));
    that->objcount=2;
    that->element_type=element_type;
    that->elements=elements;
    for(halfword i=0;i<rank;i++){
        that->dimensions[i]=dimensions[i];}
    return that;}


/* OCTET-ARRAY */
/* Used both for ARRAY and VECTOR classes when */
/* element-type is octet, bit or character. */

OctetArray* OctetArray_slots(Ref element_type,halfword rank,halfword* dimensions){
    halfword total_array_size=1;
    for(halfword i=0;i<rank;i++){
        total_array_size=(halfword)(total_array_size*dimensions[i]);}
    VectorOfOctet* elements=VectorOfOctet_slots(total_array_size);
    OctetArray* that=allocate(offsetof(OctetArray,dimensions)+sizeof(that->dimensions[0])*VARRAY_MIN(rank));
    that->objcount=2;
    that->element_type=element_type;
    that->elements=elements;
    for(halfword i=0;i<rank;i++){
        that->dimensions[i]=dimensions[i];}
    return that;}

OctetArray* OctetArray_slots_cstring(Ref element_type,const char* initial_value){
    VectorOfOctet* elements=VectorOfOctet_slots_cstring(initial_value);
    halfword rank=1;
    OctetArray* that=allocate(offsetof(OctetArray,dimensions)+sizeof(that->dimensions[0])*VARRAY_MIN(rank));
    that->objcount=2;
    that->element_type=element_type;
    that->elements=elements;
    that->dimensions[0]=elements->length;
    return that;}

static OctetArray* OctetArray_slots_vector_of_octets(Ref element_type, VectorOfOctet* octets){
    halfword rank=1;
    OctetArray* that=allocate(offsetof(OctetArray,dimensions)+sizeof(that->dimensions[0])*VARRAY_MIN(rank));
    that->objcount=2;
    that->element_type=element_type;
    that->elements=octets;
    that->dimensions[0]=octets->length;
    return that;}


/* CHARACTER */

static Character* Character_slots(Ref name,Ref code){
    Character* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=name;
    that->code=code;
    return that;}

static Ref Character_new(word code,const char* name){
    return Object_new(CHARACTER_Class,
                      (Slots*)Character_slots(string_new_cstring(name),
                                              integer_from_word(code)));}

bool characterp(Ref object){
    return class_of(object)==CHARACTER_Class;}

Ref character_code(Ref object){
    check_class(object,CHARACTER_Class);
    return object->slots->character.code;}

Ref character_name(Ref object){
    check_class(object,CHARACTER_Class);
    return object->slots->character.name;}


static Ref character_table[CHAR_CODE_LIMIT];
static Ref characterset;

Ref code_character(Ref code){
    check_class(code,INTEGER_Class);
    word icode=integer_value(code);
    if((icode<0)||(CHAR_CODE_LIMIT<=icode)){
        return NIL_Symbol;}
    else{
        return character_table[icode];}}

Ref octet_character(octet code){
    return character_table[code];}

Ref name_character(Ref name){
    if(characterp(name)){
        return name;}
    else if(symbolp(name)){
        return name_character(symbol_name(name));}
    else if(stringp(name)){
        if(string_length(name)==1){
            return octet_character(name->slots->octet_array.elements->elements[0]);}
        for(int i=0;i<CHAR_CODE_LIMIT;i++){
            if(string_equal(name,character_name(character_table[i]))){
                return character_table[i];}}
        return NIL_Symbol;}
    else{
        /* TODO: STRING-DESIGNATOR type! */
        TYPE_ERROR(name,STRING_Class);}}

static void kernel_initialize_characters(void){

    character_table[  0] = Character_new(0x00, "NULL");
    character_table[  1] = Character_new(0x01, "SOH");
    character_table[  2] = Character_new(0x02, "STX");
    character_table[  3] = Character_new(0x03, "ETX");
    character_table[  4] = Character_new(0x04, "EOT");
    character_table[  5] = Character_new(0x05, "ENQ");
    character_table[  6] = Character_new(0x06, "ACK");
    character_table[  7] = Character_new(0x07, "BELL");
    character_table[  8] = Character_new(0x08, "Backspace");
    character_table[  9] = Character_new(0x09, "Tab");
    character_table[ 10] = Character_new(0x0A, "Linefeed");
    character_table[ 11] = Character_new(0x0B, "VT");
    character_table[ 12] = Character_new(0x0C, "Page");
    character_table[ 13] = Character_new(0x0D, "Return");
    character_table[ 14] = Character_new(0x0E, "SO");
    character_table[ 15] = Character_new(0x0F, "SI");
    character_table[ 16] = Character_new(0x10, "DLE");
    character_table[ 17] = Character_new(0x11, "DC1");
    character_table[ 18] = Character_new(0x12, "DC2");
    character_table[ 19] = Character_new(0x13, "DC3");
    character_table[ 20] = Character_new(0x14, "DC4");
    character_table[ 21] = Character_new(0x15, "NAK");
    character_table[ 22] = Character_new(0x16, "SYN");
    character_table[ 23] = Character_new(0x17, "ETB");
    character_table[ 24] = Character_new(0x18, "CAN");
    character_table[ 25] = Character_new(0x19, "EM");
    character_table[ 26] = Character_new(0x1A, "SUB");
    character_table[ 27] = Character_new(0x1B, "ESCAPE");
    character_table[ 28] = Character_new(0x1C, "FS");
    character_table[ 29] = Character_new(0x1D, "GS");
    character_table[ 30] = Character_new(0x1E, "RS");
    character_table[ 31] = Character_new(0x1F, "US");
    character_table[ 32] = Character_new(0x20, "Space");
    character_table[ 33] = Character_new(0x21, "EXCLAMATION_MARK");
    character_table[ 34] = Character_new(0x22, "QUOTATION_MARK");
    character_table[ 35] = Character_new(0x23, "NUMBER_SIGN");
    character_table[ 36] = Character_new(0x24, "DOLLAR_SIGN");
    character_table[ 37] = Character_new(0x25, "PERCENT_SIGN");
    character_table[ 38] = Character_new(0x26, "AMPERSAND");
    character_table[ 39] = Character_new(0x27, "APOSTROPHE");
    character_table[ 40] = Character_new(0x28, "LEFT_PARENTHESIS");
    character_table[ 41] = Character_new(0x29, "RIGHT_PARENTHESIS");
    character_table[ 42] = Character_new(0x2A, "ASTERISK");
    character_table[ 43] = Character_new(0x2B, "PLUS_SIGN");
    character_table[ 44] = Character_new(0x2C, "COMMA");
    character_table[ 45] = Character_new(0x2D, "HYPHEN-MINUS");
    character_table[ 46] = Character_new(0x2E, "FULL_STOP");
    character_table[ 47] = Character_new(0x2F, "SOLIDUS");
    character_table[ 48] = Character_new(0x30, "DIGIT_ZERO");
    character_table[ 49] = Character_new(0x31, "DIGIT_ONE");
    character_table[ 50] = Character_new(0x32, "DIGIT_TWO");
    character_table[ 51] = Character_new(0x33, "DIGIT_THREE");
    character_table[ 52] = Character_new(0x34, "DIGIT_FOUR");
    character_table[ 53] = Character_new(0x35, "DIGIT_FIVE");
    character_table[ 54] = Character_new(0x36, "DIGIT_SIX");
    character_table[ 55] = Character_new(0x37, "DIGIT_SEVEN");
    character_table[ 56] = Character_new(0x38, "DIGIT_EIGHT");
    character_table[ 57] = Character_new(0x39, "DIGIT_NINE");
    character_table[ 58] = Character_new(0x3A, "COLON");
    character_table[ 59] = Character_new(0x3B, "SEMICOLON");
    character_table[ 60] = Character_new(0x3C, "LESS-THAN_SIGN");
    character_table[ 61] = Character_new(0x3D, "EQUALS_SIGN");
    character_table[ 62] = Character_new(0x3E, "GREATER-THAN_SIGN");
    character_table[ 63] = Character_new(0x3F, "QUESTION_MARK");
    character_table[ 64] = Character_new(0x40, "COMMERCIAL_AT");
    character_table[ 65] = Character_new(0x41, "LATIN_CAPITAL_LETTER_A");
    character_table[ 66] = Character_new(0x42, "LATIN_CAPITAL_LETTER_B");
    character_table[ 67] = Character_new(0x43, "LATIN_CAPITAL_LETTER_C");
    character_table[ 68] = Character_new(0x44, "LATIN_CAPITAL_LETTER_D");
    character_table[ 69] = Character_new(0x45, "LATIN_CAPITAL_LETTER_E");
    character_table[ 70] = Character_new(0x46, "LATIN_CAPITAL_LETTER_F");
    character_table[ 71] = Character_new(0x47, "LATIN_CAPITAL_LETTER_G");
    character_table[ 72] = Character_new(0x48, "LATIN_CAPITAL_LETTER_H");
    character_table[ 73] = Character_new(0x49, "LATIN_CAPITAL_LETTER_I");
    character_table[ 74] = Character_new(0x4A, "LATIN_CAPITAL_LETTER_J");
    character_table[ 75] = Character_new(0x4B, "LATIN_CAPITAL_LETTER_K");
    character_table[ 76] = Character_new(0x4C, "LATIN_CAPITAL_LETTER_L");
    character_table[ 77] = Character_new(0x4D, "LATIN_CAPITAL_LETTER_M");
    character_table[ 78] = Character_new(0x4E, "LATIN_CAPITAL_LETTER_N");
    character_table[ 79] = Character_new(0x4F, "LATIN_CAPITAL_LETTER_O");
    character_table[ 80] = Character_new(0x50, "LATIN_CAPITAL_LETTER_P");
    character_table[ 81] = Character_new(0x51, "LATIN_CAPITAL_LETTER_Q");
    character_table[ 82] = Character_new(0x52, "LATIN_CAPITAL_LETTER_R");
    character_table[ 83] = Character_new(0x53, "LATIN_CAPITAL_LETTER_S");
    character_table[ 84] = Character_new(0x54, "LATIN_CAPITAL_LETTER_T");
    character_table[ 85] = Character_new(0x55, "LATIN_CAPITAL_LETTER_U");
    character_table[ 86] = Character_new(0x56, "LATIN_CAPITAL_LETTER_V");
    character_table[ 87] = Character_new(0x57, "LATIN_CAPITAL_LETTER_W");
    character_table[ 88] = Character_new(0x58, "LATIN_CAPITAL_LETTER_X");
    character_table[ 89] = Character_new(0x59, "LATIN_CAPITAL_LETTER_Y");
    character_table[ 90] = Character_new(0x5A, "LATIN_CAPITAL_LETTER_Z");
    character_table[ 91] = Character_new(0x5B, "LEFT_SQUARE_BRACKET");
    character_table[ 92] = Character_new(0x5C, "REVERSE_SOLIDUS");
    character_table[ 93] = Character_new(0x5D, "RIGHT_SQUARE_BRACKET");
    character_table[ 94] = Character_new(0x5E, "CIRCUMFLEX_ACCENT");
    character_table[ 95] = Character_new(0x5F, "LOW_LINE");
    character_table[ 96] = Character_new(0x60, "GRAVE_ACCENT");
    character_table[ 97] = Character_new(0x61, "LATIN_SMALL_LETTER_A");
    character_table[ 98] = Character_new(0x62, "LATIN_SMALL_LETTER_B");
    character_table[ 99] = Character_new(0x63, "LATIN_SMALL_LETTER_C");
    character_table[100] = Character_new(0x64, "LATIN_SMALL_LETTER_D");
    character_table[101] = Character_new(0x65, "LATIN_SMALL_LETTER_E");
    character_table[102] = Character_new(0x66, "LATIN_SMALL_LETTER_F");
    character_table[103] = Character_new(0x67, "LATIN_SMALL_LETTER_G");
    character_table[104] = Character_new(0x68, "LATIN_SMALL_LETTER_H");
    character_table[105] = Character_new(0x69, "LATIN_SMALL_LETTER_I");
    character_table[106] = Character_new(0x6A, "LATIN_SMALL_LETTER_J");
    character_table[107] = Character_new(0x6B, "LATIN_SMALL_LETTER_K");
    character_table[108] = Character_new(0x6C, "LATIN_SMALL_LETTER_L");
    character_table[109] = Character_new(0x6D, "LATIN_SMALL_LETTER_M");
    character_table[110] = Character_new(0x6E, "LATIN_SMALL_LETTER_N");
    character_table[111] = Character_new(0x6F, "LATIN_SMALL_LETTER_O");
    character_table[112] = Character_new(0x70, "LATIN_SMALL_LETTER_P");
    character_table[113] = Character_new(0x71, "LATIN_SMALL_LETTER_Q");
    character_table[114] = Character_new(0x72, "LATIN_SMALL_LETTER_R");
    character_table[115] = Character_new(0x73, "LATIN_SMALL_LETTER_S");
    character_table[116] = Character_new(0x74, "LATIN_SMALL_LETTER_T");
    character_table[117] = Character_new(0x75, "LATIN_SMALL_LETTER_U");
    character_table[118] = Character_new(0x76, "LATIN_SMALL_LETTER_V");
    character_table[119] = Character_new(0x77, "LATIN_SMALL_LETTER_W");
    character_table[120] = Character_new(0x78, "LATIN_SMALL_LETTER_X");
    character_table[121] = Character_new(0x79, "LATIN_SMALL_LETTER_Y");
    character_table[122] = Character_new(0x7A, "LATIN_SMALL_LETTER_Z");
    character_table[123] = Character_new(0x7B, "LEFT_CURLY_BRACKET");
    character_table[124] = Character_new(0x7C, "VERTICAL_LINE");
    character_table[125] = Character_new(0x7D, "RIGHT_CURLY_BRACKET");
    character_table[126] = Character_new(0x7E, "TILDE");
    character_table[127] = Character_new(0x7F, "Rubout");
    character_table[128] = Character_new(0x80, "PAD");
    character_table[129] = Character_new(0x81, "HOP");
    character_table[130] = Character_new(0x82, "BPH");
    character_table[131] = Character_new(0x83, "NBH");
    character_table[132] = Character_new(0x84, "IND");
    character_table[133] = Character_new(0x85, "Newline");
    character_table[134] = Character_new(0x86, "SSA");
    character_table[135] = Character_new(0x87, "ESA");
    character_table[136] = Character_new(0x88, "HTS");
    character_table[137] = Character_new(0x89, "HTJ");
    character_table[138] = Character_new(0x8A, "VTS");
    character_table[139] = Character_new(0x8B, "PLD");
    character_table[140] = Character_new(0x8C, "PLU");
    character_table[141] = Character_new(0x8D, "RI");
    character_table[142] = Character_new(0x8E, "SS2");
    character_table[143] = Character_new(0x8F, "SS3");
    character_table[144] = Character_new(0x90, "DCS");
    character_table[145] = Character_new(0x91, "PU1");
    character_table[146] = Character_new(0x92, "PU2");
    character_table[147] = Character_new(0x93, "STS");
    character_table[148] = Character_new(0x94, "CCH");
    character_table[149] = Character_new(0x95, "MW");
    character_table[150] = Character_new(0x96, "SPA");
    character_table[151] = Character_new(0x97, "EPA");
    character_table[152] = Character_new(0x98, "SOS");
    character_table[153] = Character_new(0x99, "SGCI");
    character_table[154] = Character_new(0x9A, "SCI");
    character_table[155] = Character_new(0x9B, "CSI");
    character_table[156] = Character_new(0x9C, "ST");
    character_table[157] = Character_new(0x9D, "OSC");
    character_table[158] = Character_new(0x9E, "PM");
    character_table[159] = Character_new(0x9F, "APC");
    character_table[160] = Character_new(0xA0, "NO-BREAK_SPACE");
    character_table[161] = Character_new(0xA1, "INVERTED_EXCLAMATION_MARK");
    character_table[162] = Character_new(0xA2, "CENT_SIGN");
    character_table[163] = Character_new(0xA3, "POUND_SIGN");
    character_table[164] = Character_new(0xA4, "CURRENCY_SIGN");
    character_table[165] = Character_new(0xA5, "YEN_SIGN");
    character_table[166] = Character_new(0xA6, "BROKEN_BAR");
    character_table[167] = Character_new(0xA7, "SECTION_SIGN");
    character_table[168] = Character_new(0xA8, "DIAERESIS");
    character_table[169] = Character_new(0xA9, "COPYRIGHT_SIGN");
    character_table[170] = Character_new(0xAA, "FEMININE_ORDINAL_INDICATOR");
    character_table[171] = Character_new(0xAB, "LEFT-POINTING_DOUBLE_ANGLE_QUOTATION_MARK");
    character_table[172] = Character_new(0xAC, "NOT_SIGN");
    character_table[173] = Character_new(0xAD, "SOFT_HYPHEN");
    character_table[174] = Character_new(0xAE, "REGISTERED_SIGN");
    character_table[175] = Character_new(0xAF, "MACRON");
    character_table[176] = Character_new(0xB0, "DEGREE_SIGN");
    character_table[177] = Character_new(0xB1, "PLUS-MINUS_SIGN");
    character_table[178] = Character_new(0xB2, "SUPERSCRIPT_TWO");
    character_table[179] = Character_new(0xB3, "SUPERSCRIPT_THREE");
    character_table[180] = Character_new(0xB4, "ACUTE_ACCENT");
    character_table[181] = Character_new(0xB5, "MICRO_SIGN");
    character_table[182] = Character_new(0xB6, "PILCROW_SIGN");
    character_table[183] = Character_new(0xB7, "MIDDLE_DOT");
    character_table[184] = Character_new(0xB8, "CEDILLA");
    character_table[185] = Character_new(0xB9, "SUPERSCRIPT_ONE");
    character_table[186] = Character_new(0xBA, "MASCULINE_ORDINAL_INDICATOR");
    character_table[187] = Character_new(0xBB, "RIGHT-POINTING_DOUBLE_ANGLE_QUOTATION_MARK");
    character_table[188] = Character_new(0xBC, "VULGAR_FRACTION_ONE_QUARTER");
    character_table[189] = Character_new(0xBD, "VULGAR_FRACTION_ONE_HALF");
    character_table[190] = Character_new(0xBE, "VULGAR_FRACTION_THREE_QUARTERS");
    character_table[191] = Character_new(0xBF, "INVERTED_QUESTION_MARK");
    character_table[192] = Character_new(0xC0, "LATIN_CAPITAL_LETTER_A_WITH_GRAVE");
    character_table[193] = Character_new(0xC1, "LATIN_CAPITAL_LETTER_A_WITH_ACUTE");
    character_table[194] = Character_new(0xC2, "LATIN_CAPITAL_LETTER_A_WITH_CIRCUMFLEX");
    character_table[195] = Character_new(0xC3, "LATIN_CAPITAL_LETTER_A_WITH_TILDE");
    character_table[196] = Character_new(0xC4, "LATIN_CAPITAL_LETTER_A_WITH_DIAERESIS");
    character_table[197] = Character_new(0xC5, "LATIN_CAPITAL_LETTER_A_WITH_RING_ABOVE");
    character_table[198] = Character_new(0xC6, "LATIN_CAPITAL_LETTER_AE");
    character_table[199] = Character_new(0xC7, "LATIN_CAPITAL_LETTER_C_WITH_CEDILLA");
    character_table[200] = Character_new(0xC8, "LATIN_CAPITAL_LETTER_E_WITH_GRAVE");
    character_table[201] = Character_new(0xC9, "LATIN_CAPITAL_LETTER_E_WITH_ACUTE");
    character_table[202] = Character_new(0xCA, "LATIN_CAPITAL_LETTER_E_WITH_CIRCUMFLEX");
    character_table[203] = Character_new(0xCB, "LATIN_CAPITAL_LETTER_E_WITH_DIAERESIS");
    character_table[204] = Character_new(0xCC, "LATIN_CAPITAL_LETTER_I_WITH_GRAVE");
    character_table[205] = Character_new(0xCD, "LATIN_CAPITAL_LETTER_I_WITH_ACUTE");
    character_table[206] = Character_new(0xCE, "LATIN_CAPITAL_LETTER_I_WITH_CIRCUMFLEX");
    character_table[207] = Character_new(0xCF, "LATIN_CAPITAL_LETTER_I_WITH_DIAERESIS");
    character_table[208] = Character_new(0xD0, "LATIN_CAPITAL_LETTER_ETH");
    character_table[209] = Character_new(0xD1, "LATIN_CAPITAL_LETTER_N_WITH_TILDE");
    character_table[210] = Character_new(0xD2, "LATIN_CAPITAL_LETTER_O_WITH_GRAVE");
    character_table[211] = Character_new(0xD3, "LATIN_CAPITAL_LETTER_O_WITH_ACUTE");
    character_table[212] = Character_new(0xD4, "LATIN_CAPITAL_LETTER_O_WITH_CIRCUMFLEX");
    character_table[213] = Character_new(0xD5, "LATIN_CAPITAL_LETTER_O_WITH_TILDE");
    character_table[214] = Character_new(0xD6, "LATIN_CAPITAL_LETTER_O_WITH_DIAERESIS");
    character_table[215] = Character_new(0xD7, "MULTIPLICATION_SIGN");
    character_table[216] = Character_new(0xD8, "LATIN_CAPITAL_LETTER_O_WITH_STROKE");
    character_table[217] = Character_new(0xD9, "LATIN_CAPITAL_LETTER_U_WITH_GRAVE");
    character_table[218] = Character_new(0xDA, "LATIN_CAPITAL_LETTER_U_WITH_ACUTE");
    character_table[219] = Character_new(0xDB, "LATIN_CAPITAL_LETTER_U_WITH_CIRCUMFLEX");
    character_table[220] = Character_new(0xDC, "LATIN_CAPITAL_LETTER_U_WITH_DIAERESIS");
    character_table[221] = Character_new(0xDD, "LATIN_CAPITAL_LETTER_Y_WITH_ACUTE");
    character_table[222] = Character_new(0xDE, "LATIN_CAPITAL_LETTER_THORN");
    character_table[223] = Character_new(0xDF, "LATIN_SMALL_LETTER_SHARP_S");
    character_table[224] = Character_new(0xE0, "LATIN_SMALL_LETTER_A_WITH_GRAVE");
    character_table[225] = Character_new(0xE1, "LATIN_SMALL_LETTER_A_WITH_ACUTE");
    character_table[226] = Character_new(0xE2, "LATIN_SMALL_LETTER_A_WITH_CIRCUMFLEX");
    character_table[227] = Character_new(0xE3, "LATIN_SMALL_LETTER_A_WITH_TILDE");
    character_table[228] = Character_new(0xE4, "LATIN_SMALL_LETTER_A_WITH_DIAERESIS");
    character_table[229] = Character_new(0xE5, "LATIN_SMALL_LETTER_A_WITH_RING_ABOVE");
    character_table[230] = Character_new(0xE6, "LATIN_SMALL_LETTER_AE");
    character_table[231] = Character_new(0xE7, "LATIN_SMALL_LETTER_C_WITH_CEDILLA");
    character_table[232] = Character_new(0xE8, "LATIN_SMALL_LETTER_E_WITH_GRAVE");
    character_table[233] = Character_new(0xE9, "LATIN_SMALL_LETTER_E_WITH_ACUTE");
    character_table[234] = Character_new(0xEA, "LATIN_SMALL_LETTER_E_WITH_CIRCUMFLEX");
    character_table[235] = Character_new(0xEB, "LATIN_SMALL_LETTER_E_WITH_DIAERESIS");
    character_table[236] = Character_new(0xEC, "LATIN_SMALL_LETTER_I_WITH_GRAVE");
    character_table[237] = Character_new(0xED, "LATIN_SMALL_LETTER_I_WITH_ACUTE");
    character_table[238] = Character_new(0xEE, "LATIN_SMALL_LETTER_I_WITH_CIRCUMFLEX");
    character_table[239] = Character_new(0xEF, "LATIN_SMALL_LETTER_I_WITH_DIAERESIS");
    character_table[240] = Character_new(0xF0, "LATIN_SMALL_LETTER_ETH");
    character_table[241] = Character_new(0xF1, "LATIN_SMALL_LETTER_N_WITH_TILDE");
    character_table[242] = Character_new(0xF2, "LATIN_SMALL_LETTER_O_WITH_GRAVE");
    character_table[243] = Character_new(0xF3, "LATIN_SMALL_LETTER_O_WITH_ACUTE");
    character_table[244] = Character_new(0xF4, "LATIN_SMALL_LETTER_O_WITH_CIRCUMFLEX");
    character_table[245] = Character_new(0xF5, "LATIN_SMALL_LETTER_O_WITH_TILDE");
    character_table[246] = Character_new(0xF6, "LATIN_SMALL_LETTER_O_WITH_DIAERESIS");
    character_table[247] = Character_new(0xF7, "DIVISION_SIGN");
    character_table[248] = Character_new(0xF8, "LATIN_SMALL_LETTER_O_WITH_STROKE");
    character_table[249] = Character_new(0xF9, "LATIN_SMALL_LETTER_U_WITH_GRAVE");
    character_table[250] = Character_new(0xFA, "LATIN_SMALL_LETTER_U_WITH_ACUTE");
    character_table[251] = Character_new(0xFB, "LATIN_SMALL_LETTER_U_WITH_CIRCUMFLEX");
    character_table[252] = Character_new(0xFC, "LATIN_SMALL_LETTER_U_WITH_DIAERESIS");
    character_table[253] = Character_new(0xFD, "LATIN_SMALL_LETTER_Y_WITH_ACUTE");
    character_table[254] = Character_new(0xFE, "LATIN_SMALL_LETTER_THORN");
    character_table[255] = Character_new(0xFF, "LATIN_SMALL_LETTER_Y_WITH_DIAERESIS");

    characterset=NIL_Symbol;
    for(int i=0;i<CHAR_CODE_LIMIT;i++){
        push(character_table[i],&characterset);}}


/* INTEGER */
/* Before we use a bigint library, let's just use 64-bit integers: */

static Ref integer_table[INTEGER_INTERN_MAX - INTEGER_INTERN_MIN];
static Ref integerset;

Integer* Integer_slots(word value){
    Integer* that=allocate(sizeof(*that));
    that->value=value;
    return that;}

Ref integer_from_word(word value){
    if((INTEGER_INTERN_MIN<=value) && (value<INTEGER_INTERN_MAX)){
        return integer_table[value-INTEGER_INTERN_MIN];}
    return Object_new(INTEGER_Class,(Slots*)Integer_slots(value));}

Ref integer_from_uword(uword value){
    if(value<INTEGER_INTERN_MAX){
        return integer_table[(word)value-INTEGER_INTERN_MIN];}
    return Object_new(INTEGER_Class,(Slots*)Integer_slots(CHECK_UWORD_TO_WORD(value)));}

bool integerp(Ref object){
    return class_of(object)==INTEGER_Class;}

word integer_value(Ref object){
    check_class(object,INTEGER_Class);
    return object->slots->integer.value;}

static void kernel_initialize_integers(void){
    integerset=NIL_Symbol;
    for(word i=INTEGER_INTERN_MIN;i<INTEGER_INTERN_MAX;i++){
        integer_table[i-INTEGER_INTERN_MIN] = Object_new(INTEGER_Class,(Slots*)Integer_slots(i));
        push(integer_table[i-INTEGER_INTERN_MIN],&integerset);}}


/* -------------------------------------------------------------------- *
 *  BIGNUM — arbitrary-precision integers backed by the Lisp engine       *
 *  (bootstrap/bignum.lisp + bootstrap/bignum-integration.lisp).          *
 *                                                                        *
 *  A Bignum boxes one Ref: the (SIGN . DIGITS) i%-format cons.  All       *
 *  bignum arithmetic lives in Lisp; num_combine() (arithmetic.c) promotes *
 *  on overflow and dispatches bignum operands to the glue.  This file     *
 *  provides only the type, the demote-on-fit constructor, and the three   *
 *  kernel primitives the glue calls.                                      *
 * -------------------------------------------------------------------- */

Ref BIGNUM_Class = &uninitialized_object;

bool bignump(Ref object){
    return class_of(object)==BIGNUM_Class;}

Ref bignum_new(Ref value){
    Bignum* slots=allocate(sizeof(Bignum));
    slots->value=value;
    slots->objcount=1;
    return Object_new(BIGNUM_Class,(Slots*)slots);}

Ref bignum_value(Ref object){
    check_class(object,BIGNUM_Class);
    return object->slots->bignum.value;}

/* Base of the Lisp digit representation; MUST match +bignum-base+ in the glue.
   (10^9)^2 = 10^18 < WORD_MAX (9.22e18), so the engine never overflows a word. */
#define BIGNUM_BASE ((uword)1000000000)

/* Horner reconstruction of the magnitude of a low-order-first digit list.
   Sets *overflow when the value exceeds a word.  Recurses to depth = #digits;
   callers cap that at 3 (a >=4-digit base-10^9 number cannot fit a word). */
static uword bignum_magnitude(Ref digits, bool* overflow){
    if(!consp(digits)){ return 0; }
    uword rest=bignum_magnitude(cons_cdr(digits),overflow);
    if(*overflow){ return 0; }
    uword r;
    if(bocl_umul_overflow(rest,BIGNUM_BASE,&r)){ *overflow=true; return 0; }
    if(bocl_uadd_overflow(r,(uword)integer_value(cons_car(digits)),&r)){
        *overflow=true; return 0; }
    return r;}

/* Given a canonical (SIGN . DIGITS) cons (SIGN is + or -, DIGITS normalized
   low-order-first), return a fixnum when the value fits in a word, else box it. */
Ref make_integer_from_bignum(Ref signed_bignum){
    Ref sign=cons_car(signed_bignum);
    Ref digits=cons_cdr(signed_bignum);
    bool neg=(sign==S(-));
    word n=0;
    for(Ref d=digits; consp(d) && n<4; d=cons_cdr(d)){ n++; }
    if(n==0){ return I(0); }
    if(n<=3){
        bool overflow=false;
        uword mag=bignum_magnitude(digits,&overflow);
        if(!overflow){
            if(!neg){
                if(mag<=(uword)WORD_MAX){ return integer_from_word((word)mag); }
            } else {
                if(mag<=(uword)WORD_MAX){ return integer_from_word(-(word)mag); }
                if(mag==(uword)WORD_MAX+1u){ return integer_from_word(WORD_MIN); }
            }
        }
    }
    return bignum_new(signed_bignum);}

/* ==================================================================== *
 *  C bignum engine — schoolbook arithmetic over base-10^9 uword limbs.   *
 *                                                                        *
 *  Operands/results use the same (SIGN . DIGITS) i%-format cons that the *
 *  Lisp reader/printer build, so I/O interoperates unchanged; only the   *
 *  inner loops move to C (replacing the interpreted Lisp arithmetic).    *
 *  A "magnitude" is a uword[] of base-10^9 limbs, low-order first,        *
 *  normalized (no high-order zero limbs).  These helpers are pure C and  *
 *  allocate no Refs, so no GC can run inside them.                       *
 * ==================================================================== */

static size_t mag_norm(const uword* a, size_t n){
    while(n>0 && a[n-1]==0) n--;
    return n; }

static int mag_cmp(const uword* a, size_t na, const uword* b, size_t nb){
    na=mag_norm(a,na); nb=mag_norm(b,nb);
    if(na!=nb){ return na<nb ? -1 : 1; }
    for(size_t i=na; i-->0;){ if(a[i]!=b[i]){ return a[i]<b[i] ? -1 : 1; } }
    return 0; }

/* out = a+b ; out needs max(na,nb)+1 limbs ; returns length. */
static size_t mag_add(const uword* a, size_t na, const uword* b, size_t nb, uword* out){
    size_t n = na>nb ? na : nb;
    uword carry=0;
    for(size_t i=0;i<n;i++){
        uword s = carry + (i<na?a[i]:0) + (i<nb?b[i]:0);
        out[i] = s % BIGNUM_BASE;
        carry  = s / BIGNUM_BASE; }
    if(carry){ out[n++]=carry; }
    return n; }

/* out = a-b ; requires a>=b ; out needs na limbs ; returns normalized length. */
static size_t mag_sub(const uword* a, size_t na, const uword* b, size_t nb, uword* out){
    word borrow=0;
    for(size_t i=0;i<na;i++){
        word d = (word)a[i] - borrow - (word)(i<nb?b[i]:0);
        if(d<0){ d += (word)BIGNUM_BASE; borrow=1; } else { borrow=0; }
        out[i]=(uword)d; }
    return mag_norm(out,na); }

/* out = a*m (m<BIGNUM_BASE) ; out needs na+1 limbs ; returns length. */
static size_t mag_mul_limb(const uword* a, size_t na, uword m, uword* out){
    uword carry=0;
    for(size_t i=0;i<na;i++){
        uword cur = a[i]*m + carry;
        out[i] = cur % BIGNUM_BASE;
        carry  = cur / BIGNUM_BASE; }
    size_t n=na;
    if(carry){ out[n++]=carry; }
    return mag_norm(out,n); }

/* out = a*b ; out needs na+nb limbs (will be written in full) ; returns length. */
static size_t mag_mul(const uword* a, size_t na, const uword* b, size_t nb, uword* out){
    size_t no=na+nb;
    for(size_t i=0;i<no;i++){ out[i]=0; }
    for(size_t i=0;i<na;i++){
        uword carry=0;
        for(size_t j=0;j<nb;j++){
            uword cur = out[i+j] + a[i]*b[j] + carry;   /* < BIGNUM_BASE^2, fits uword */
            out[i+j] = cur % BIGNUM_BASE;
            carry    = cur / BIGNUM_BASE; }
        size_t k=i+nb;
        while(carry){ uword cur=out[k]+carry; out[k]=cur%BIGNUM_BASE; carry=cur/BIGNUM_BASE; k++; } }
    return mag_norm(out,no); }

/* U/V (V nonzero).  Q needs nu limbs, R needs nv+1 limbs.  Long division with a
   binary search for each base-10^9 quotient digit.  scratch needs nv+2 limbs. */
static void mag_divmod(const uword* U, size_t nu, const uword* V, size_t nv,
                       uword* Q, size_t* nq, uword* R, size_t* nr, uword* scratch){
    nv=mag_norm(V,nv);
    for(size_t i=0;i<nu;i++){ Q[i]=0; }
    if(mag_cmp(U,nu,V,nv)<0){
        *nq=0;
        for(size_t i=0;i<nu;i++){ R[i]=U[i]; }
        *nr=mag_norm(R,nu);
        return; }
    size_t rn=0;                                  /* current remainder length in R */
    for(size_t qi=nu; qi-->0;){
        for(size_t i=rn; i-->0;){ R[i+1]=R[i]; }   /* R = R*BASE + U[qi] */
        R[0]=U[qi];
        rn=mag_norm(R,rn+1);
        uword lo=0, hi=BIGNUM_BASE-1, q=0;          /* find max q with q*V <= R */
        while(lo<=hi){
            uword mid = lo + (hi-lo)/2;
            size_t tn = mag_mul_limb(V,nv,mid,scratch);
            if(mag_cmp(scratch,tn,R,rn)<=0){ q=mid; lo=mid+1; }
            else { hi=mid-1; } }
        Q[qi]=q;
        size_t tn=mag_mul_limb(V,nv,q,scratch);
        rn=mag_sub(R,rn,scratch,tn,R); }
    *nq=mag_norm(Q,nu);
    *nr=mag_norm(R,rn); }

/* Read a fixnum-or-bignum into a freshly malloc'd magnitude + sign. */
static uword* bn_decompose(Ref x, size_t* n_out, bool* neg_out){
    if(bignump(x)){
        Ref sb=bignum_value(x);                 /* (SIGN . DIGITS) */
        *neg_out = (cons_car(sb)==S(-));
        Ref d=cons_cdr(sb);
        size_t n=0;
        for(Ref p=d; consp(p); p=cons_cdr(p)){ n++; }
        uword* mag=malloc((n?n:1)*sizeof(uword));
        size_t i=0;
        for(Ref p=d; consp(p); p=cons_cdr(p)){ mag[i++]=(uword)integer_value(cons_car(p)); }
        *n_out=mag_norm(mag,n);
        return mag; }
    word v=integer_value(x);
    bool neg = v<0;
    uword u = neg ? (uword)(-(v+1))+1u : (uword)v;   /* |v|, safe at WORD_MIN */
    uword* mag=malloc(3*sizeof(uword));
    size_t n=0;
    while(u){ mag[n++]=u%BIGNUM_BASE; u/=BIGNUM_BASE; }
    *neg_out=neg;
    *n_out=n;
    return mag; }

/* Build an INTEGER (demoted fixnum, or boxed BIGNUM) from a magnitude+sign.
   GC is disabled across the bounded result-list construction so the partial
   (SIGN . DIGITS) list never needs live-pool rooting. */
static Ref make_integer_from_limbs(bool neg, uword* mag, size_t n){
    n=mag_norm(mag,n);
    if(n==0){ return integer_from_word(0); }
    if(n<=3){
        uword v=0; bool of=false;
        for(size_t i=n; i-->0;){
            if(bocl_umul_overflow(v,BIGNUM_BASE,&v)){ of=true; break; }
            if(bocl_uadd_overflow(v,mag[i],&v)){ of=true; break; } }
        if(!of){
            if(!neg){ if(v<=(uword)WORD_MAX){ return integer_from_word((word)v); } }
            else { if(v<=(uword)WORD_MAX){ return integer_from_word(-(word)v); }
                   if(v==(uword)WORD_MAX+1u){ return integer_from_word(WORD_MIN); } } } }
    bool saved=garbage_collection_is_enabled();
    set_garbage_collection_enabled(false);
    Ref digits=NIL_Symbol;
    for(size_t i=n; i-->0;){ digits=cons(integer_from_word((word)mag[i]),digits); }
    Ref sb=cons(neg?S(-):S(+),digits);
    Ref result=bignum_new(sb);
    set_garbage_collection_enabled(saved);
    return result; }

static Ref bn_signed_add(uword* ma, size_t na, bool sa,
                         uword* mb, size_t nb, bool sb){
    if(sa==sb){
        size_t cap=(na>nb?na:nb)+1;
        uword* out=malloc(cap*sizeof(uword));
        size_t no=mag_add(ma,na,mb,nb,out);
        Ref r=make_integer_from_limbs(sa,out,no);
        free(out);
        return r; }
    int c=mag_cmp(ma,na,mb,nb);
    if(c==0){ return integer_from_word(0); }
    uword* out=malloc((c>0?na:nb)*sizeof(uword));
    size_t no = c>0 ? mag_sub(ma,na,mb,nb,out) : mag_sub(mb,nb,ma,na,out);
    Ref r=make_integer_from_limbs(c>0?sa:sb,out,no);
    free(out);
    return r; }

Ref c_bignum_add(Ref a, Ref b){
    size_t na,nb; bool sa,sb;
    uword* ma=bn_decompose(a,&na,&sa);
    uword* mb=bn_decompose(b,&nb,&sb);
    Ref r=bn_signed_add(ma,na,sa,mb,nb,sb);
    free(ma); free(mb);
    return r; }

Ref c_bignum_sub(Ref a, Ref b){
    size_t na,nb; bool sa,sb;
    uword* ma=bn_decompose(a,&na,&sa);
    uword* mb=bn_decompose(b,&nb,&sb);
    Ref r=bn_signed_add(ma,na,sa,mb,nb,!sb);   /* a-b = a+(-b) */
    free(ma); free(mb);
    return r; }

Ref c_bignum_mul(Ref a, Ref b){
    size_t na,nb; bool sa,sb;
    uword* ma=bn_decompose(a,&na,&sa);
    uword* mb=bn_decompose(b,&nb,&sb);
    Ref r;
    if(na==0||nb==0){ r=integer_from_word(0); }
    else {
        uword* out=malloc((na+nb)*sizeof(uword));
        size_t no=mag_mul(ma,na,mb,nb,out);
        r=make_integer_from_limbs(sa!=sb,out,no);
        free(out); }
    free(ma); free(mb);
    return r; }

/* CL '/' on integers: exact quotient, else a (reduced) ratio. */
Ref c_bignum_divide(Ref a, Ref b){
    size_t na,nb; bool sa,sb;
    uword* ma=bn_decompose(a,&na,&sa);
    uword* mb=bn_decompose(b,&nb,&sb);
    if(nb==0){ free(ma); free(mb); ARITHMETIC_ERROR("Division by zero%s",""); }
    Ref r;
    if(na==0){ r=integer_from_word(0); }
    else {
        uword* Q=malloc(na*sizeof(uword));
        uword* R=malloc((nb+1)*sizeof(uword));
        uword* sc=malloc((nb+2)*sizeof(uword));
        size_t nq,nr;
        mag_divmod(ma,na,mb,nb,Q,&nq,R,&nr,sc);
        if(nr!=0){
            free(Q); free(R); free(sc); free(ma); free(mb);
            return ratio_new(a,b); }   /* non-exact: a/b is a reduced ratio */
        r=make_integer_from_limbs(sa!=sb,Q,nq);
        free(Q); free(R); free(sc); }
    free(ma); free(mb);
    return r; }

int c_bignum_cmp(Ref a, Ref b){
    size_t na,nb; bool sa,sb;
    uword* ma=bn_decompose(a,&na,&sa);
    uword* mb=bn_decompose(b,&nb,&sb);
    if(na==0){ sa=false; }                 /* zero is non-negative */
    if(nb==0){ sb=false; }
    int r;
    if(sa!=sb){ r = sa ? -1 : 1; }
    else { int m=mag_cmp(ma,na,mb,nb); r = sa ? -m : m; }
    free(ma); free(mb);
    return r; }

/* Signed division with CL rounding MODE: 't' truncate (toward 0), 'f' floor
   (toward -inf), 'c' ceiling (toward +inf), 'r' round (half to even).  Writes the
   quotient to *q_out and remainder to *r_out (n = q*d + r).  Either operand may be
   a bignum.  GC is disabled across the bounded Ref construction so the
   intermediates need no live-pool rooting. */
void c_bignum_divmod(Ref a, Ref b, char mode, Ref* q_out, Ref* r_out){
    size_t na,nb; bool sa,sb;
    uword* ma=bn_decompose(a,&na,&sa);
    uword* mb=bn_decompose(b,&nb,&sb);
    if(nb==0){ free(ma); free(mb); ARITHMETIC_ERROR("Division by zero%s",""); }
    uword* Q=malloc((na?na:1)*sizeof(uword));
    uword* R=malloc((nb+1)*sizeof(uword));
    uword* sc=malloc((nb+2)*sizeof(uword));
    size_t nq,nr;
    mag_divmod(ma,na,mb,nb,Q,&nq,R,&nr,sc);
    bool rzero=(nr==0);
    bool qodd =(nq>0)&&(Q[0]&1u);                  /* parity of truncate quotient */
    int  rcmp=0;                                   /* round: 2*|R| vs |b| */
    if(mode=='r'){
        uword* tr=malloc((nr+1)*sizeof(uword));
        size_t ntr=mag_mul_limb(R,nr,2,tr);
        rcmp=mag_cmp(tr,ntr,mb,nb);
        free(tr); }
    bool saved=garbage_collection_is_enabled();
    set_garbage_collection_enabled(false);
    Ref qt=make_integer_from_limbs(sa!=sb,Q,nq);   /* truncate quotient        */
    Ref rt=make_integer_from_limbs(sa,R,nr);        /* truncate remainder (sa)  */
    Ref q=qt, r=rt;
    if(!rzero){
        bool away=false;
        switch(mode){
          case 't': break;
          case 'f': away=(sa!=sb); break;           /* negative quotient -> down */
          case 'c': away=(sa==sb); break;           /* positive quotient -> up   */
          case 'r': away=(rcmp>0)||(rcmp==0&&qodd); break;
          default:  break; }
        if(away){
            if(sa!=sb){ q=c_bignum_sub(qt,integer_from_word(1)); r=c_bignum_add(rt,b); }
            else      { q=c_bignum_add(qt,integer_from_word(1)); r=c_bignum_sub(rt,b); } } }
    set_garbage_collection_enabled(saved);
    free(ma); free(mb); free(Q); free(R); free(sc);
    *q_out=q; *r_out=r; }

/* Euclid's algorithm on magnitudes; result is the non-negative GCD. */
Ref c_bignum_gcd(Ref a, Ref b){
    size_t na,nb; bool sa,sb;
    uword* x=bn_decompose(a,&na,&sa);
    uword* y=bn_decompose(b,&nb,&sb);
    size_t cap=(na>nb?na:nb)+2;
    uword* X=malloc(cap*sizeof(uword)); uword* Y=malloc(cap*sizeof(uword));
    uword* Q=malloc(cap*sizeof(uword)); uword* R=malloc(cap*sizeof(uword));
    uword* sc=malloc(cap*sizeof(uword));
    for(size_t i=0;i<na;i++){ X[i]=x[i]; } for(size_t i=0;i<nb;i++){ Y[i]=y[i]; }
    free(x); free(y);
    size_t nx=na, ny=nb;
    while(ny>0){
        size_t nq,nr;
        mag_divmod(X,nx,Y,ny,Q,&nq,R,&nr,sc);       /* R = X mod Y */
        for(size_t i=0;i<ny;i++){ X[i]=Y[i]; } nx=ny;
        for(size_t i=0;i<nr;i++){ Y[i]=R[i]; } ny=nr; }
    Ref r=make_integer_from_limbs(false,X,nx);
    free(X); free(Y); free(Q); free(R); free(sc);
    return r; }

/* LCM = |a|/gcd(a,b) * |b| (divide first to avoid an extra-wide intermediate). */
Ref c_bignum_lcm(Ref a, Ref b){
    Ref zero=integer_from_word(0);
    if(c_bignum_cmp(a,zero)==0 || c_bignum_cmp(b,zero)==0){ return zero; }
    bool saved=garbage_collection_is_enabled();
    set_garbage_collection_enabled(false);
    Ref g =c_bignum_gcd(a,b);
    Ref aa=(c_bignum_cmp(a,zero)<0)?c_bignum_sub(zero,a):a;
    Ref bb=(c_bignum_cmp(b,zero)<0)?c_bignum_sub(zero,b):b;
    Ref q =c_bignum_divide(aa,g);
    Ref result=c_bignum_mul(q,bb);
    set_garbage_collection_enabled(saved);
    return result; }

/* Write a BIGNUM's decimal representation straight to OUT.  Each non-top base-10^9
   limb is exactly 9 decimal digits, so no division is needed.  No Ref allocation,
   hence no GC — replaces the interpreted Lisp printer (and its rooting gap). */
void print_bignum(Ref object, FILE* out){
    Ref sb=bignum_value(object);
    bool neg=(cons_car(sb)==S(-));
    Ref d=cons_cdr(sb);
    size_t n=0;
    for(Ref p=d; consp(p); p=cons_cdr(p)){ n++; }
    if(n==0){ fputc('0',out); return; }
    uword* mag=malloc(n*sizeof(uword));
    size_t i=0;
    for(Ref p=d; consp(p); p=cons_cdr(p)){ mag[i++]=(uword)integer_value(cons_car(p)); }
    n=mag_norm(mag,n);
    if(n==0){ free(mag); fputc('0',out); return; }
    if(neg){ fputc('-',out); }
    fprintf(out,"%llu",(unsigned long long)mag[n-1]);
    for(size_t k=n-1; k-->0;){ fprintf(out,"%09llu",(unsigned long long)mag[k]); }
    free(mag); }

/* Radix-aware BIGNUM printer (BASE 2..36) for *PRINT-BASE* / *PRINT-RADIX*.
   Converts the base-10^9 magnitude limbs to BASE by repeated short division of
   the whole magnitude by BASE, collecting remainders (least-significant first),
   then emits sign + uppercase digits.  BASE==10 still goes through print_bignum
   (faster); this handles the general case.  remainder*BIGNUM_BASE+limb fits a
   uword since BASE<=36 and BIGNUM_BASE=10^9. */
void print_bignum_radix(Ref object, FILE* out, int base){
    if(base==10){ print_bignum(object,out); return; }
    if(base<2){ base=2; } if(base>36){ base=36; }
    Ref sb=bignum_value(object);
    bool neg=(cons_car(sb)==S(-));
    Ref d=cons_cdr(sb);
    size_t n=0;
    for(Ref p=d; consp(p); p=cons_cdr(p)){ n++; }
    if(n==0){ fputc('0',out); return; }
    uword* mag=malloc(n*sizeof(uword));
    size_t i=0;
    for(Ref p=d; consp(p); p=cons_cdr(p)){ mag[i++]=(uword)integer_value(cons_car(p)); }
    n=mag_norm(mag,n);
    if(n==0){ free(mag); fputc('0',out); return; }
    /* Collect digits least-significant first. */
    size_t cap=n*10+8, dlen=0;       /* 10^9 limb ~ up to ~30 base-2 digits */
    char* digits=malloc(cap);
    size_t live=n;
    while(live>0){
        uword rem=0;
        for(size_t k=live; k-->0;){
            uword cur=rem*(uword)BIGNUM_BASE + mag[k];
            mag[k]=cur/(uword)base;
            rem=cur%(uword)base; }
        if(dlen>=cap){ cap*=2; digits=realloc(digits,cap); }
        digits[dlen++]=(char)(rem<10 ? '0'+(int)rem : 'A'+(int)rem-10);
        while(live>0 && mag[live-1]==0){ live--; } }
    if(neg){ fputc('-',out); }
    for(size_t k=dlen; k-->0;){ fputc((unsigned char)digits[k],out); }
    free(digits); free(mag); }

static int bn_digit(unsigned char c, int base){
    int d;
    if(c>='0'&&c<='9'){ d=c-'0'; }
    else if(c>='a'&&c<='z'){ d=c-'a'+10; }
    else if(c>='A'&&c<='Z'){ d=c-'A'+10; }
    else { return -1; }
    return d<base ? d : -1; }

/* Parse a (possibly signed, possibly trailing-'.') integer literal in BASE that
   overflowed a word, into a BIGNUM (demoted if it somehow fits).  Horner over
   base-10^9 limbs.  Returns NIL on a malformed token. */
Ref parse_bignum_cstring(const char* token, int base){
    bool neg=false;
    const char* p=token;
    if(*p=='-'){ neg=true; p++; } else if(*p=='+'){ p++; }
    size_t len=0; while(p[len]){ len++; }
    size_t cap=len+2;
    uword* mag=calloc(cap,sizeof(uword));
    uword* tmp=calloc(cap,sizeof(uword));
    size_t n=0;
    for(; *p; p++){
        if(*p=='.' && p[1]=='\0'){ break; }            /* trailing decimal point */
        int dd=bn_digit((unsigned char)*p, base);
        if(dd<0){ free(mag); free(tmp); return NIL_Symbol; }
        size_t tn=mag_mul_limb(mag,n,(uword)base,tmp);  /* tmp = mag*base */
        uword carry=(uword)dd;                           /* tmp += dd */
        size_t k=0;
        for(; k<tn && carry; k++){ uword s=tmp[k]+carry; tmp[k]=s%BIGNUM_BASE; carry=s/BIGNUM_BASE; }
        if(carry){ tmp[tn++]=carry; }
        uword* t=mag; mag=tmp; tmp=t;                    /* swap */
        n=tn; }
    Ref r=make_integer_from_limbs(neg,mag,n);
    free(mag); free(tmp);
    return r; }

void kernel_BIGNUMP(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref x=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0]=as_boolean(bignump(x));
    close_live_pool(pool);}

void kernel_BIGNUM_VALUE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref x=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0]=bignum_value(x);
    close_live_pool(pool);}

void kernel_MAKE_INTEGER_FROM_BIGNUM(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref sb=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1;
    results->values[0]=make_integer_from_bignum(sb);
    close_live_pool(pool);}

/* (%GC-STRESS n) — diagnostic: set the GC-stress period at runtime (0 = off). */
void kernel_GC_STRESS(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref n=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    set_gc_stress((long)integer_value(n));
    results->count=1;
    results->values[0]=n;
    close_live_pool(pool);}

/* (%GC-ENABLE x) — diagnostic: enable (x non-nil) or disable (nil) the GC. */
void kernel_GC_ENABLE(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref x=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    set_garbage_collection_enabled(!nullp(x));
    results->count=1;
    results->values[0]=x;
    close_live_pool(pool);}

/* (%POISON x) — diagnostic: enable (x non-nil) / disable (nil) the use-after-free
   finder at runtime, so it can target a post-prelude workload. */
void kernel_POISON(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref x=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    set_gc_poison(!nullp(x));
    results->count=1;
    results->values[0]=x;
    close_live_pool(pool);}

void kernel_initialize_bignum(void){
    /* BIGNUM is a built-in subclass of INTEGER, so TYPEP/INTEGERP/RATIONALP/
       REALP/NUMBERP all treat a bignum as an integer.  Registered into
       kernel_classes so FIND-CLASS and the kernel TYPEP resolve the name. */
    BIGNUM_Class=built_in_class_new(symbol_intern_cstring("BIGNUM"));
    boot_class_add_superclass(BIGNUM_Class,INTEGER_Class);
    push(BIGNUM_Class,&kernel_classes);
    /* Primitives used by bootstrap/bignum-integration.lisp. */
    Ref ll_x=list(S(X),NULL);
    set_symbol_function(S(%BIGNUMP),
        built_in_function_new(S(%BIGNUMP),ll_x,kernel_BIGNUMP));
    set_symbol_function(S(%BIGNUM-VALUE),
        built_in_function_new(S(%BIGNUM-VALUE),ll_x,kernel_BIGNUM_VALUE));
    set_symbol_function(S(%MAKE-INTEGER-FROM-BIGNUM),
        built_in_function_new(S(%MAKE-INTEGER-FROM-BIGNUM),
                              list(S(SIGNED-BIGNUM),NULL),kernel_MAKE_INTEGER_FROM_BIGNUM));
    set_symbol_function(S(%GC-STRESS),
        built_in_function_new(S(%GC-STRESS),ll_x,kernel_GC_STRESS));
    set_symbol_function(S(%GC-ENABLE),
        built_in_function_new(S(%GC-ENABLE),ll_x,kernel_GC_ENABLE));
    set_symbol_function(S(%POISON),
        built_in_function_new(S(%POISON),ll_x,kernel_POISON));}


/* FLOAT */

Float* Float_slots(floating value){
    Float* that=allocate(sizeof(*that));
    that->value=value;
    return that;}

Ref float_from_floating(floating value){
    return Object_new(FLOAT_Class,(Slots*)Float_slots(value));}

bool floatp(Ref object){
    return class_of(object)==FLOAT_Class;}

floating float_value(Ref object){
    check_class(object,FLOAT_Class);
    return object->slots->floater.value;}


/* RATIO */

static word gcd_word(word a, word b){
    a = (a < 0) ? -a : a;
    b = (b < 0) ? -b : b;
    while(b){ word t=b; b=a%b; a=t; }
    return a;}

/* Allocate a normalised (numerator,denominator) pair into a RATIO object.  Both
   args must already be reduced, with a positive denominator > 1.  They are
   rooted before the allocation so a GC triggered by allocate() cannot follow the
   half-built Ratio's uninitialised slots. */
static Ref ratio_box(Ref num_i, Ref den_i){
    add_to_live_pool(num_i);
    add_to_live_pool(den_i);
    Ratio* slots = allocate(sizeof(Ratio));
    slots->numerator   = num_i;
    slots->denominator = den_i;
    slots->objcount    = 2;
    return Object_new(RATIO_Class, (Slots*)slots);}

Ref ratio_new(Ref num, Ref den){
    /* Normalise: denominator always > 0; reduce by GCD; integer if den==1. */
    if(integerp(num) && integerp(den)){
        /* Fast path: both operands are fixnums, use word arithmetic. */
        word n = integer_value(num);
        word d = integer_value(den);
        if(d == 0){ ERROR("Division by zero in ratio%s",""); }
        if(d < 0){ n = -n; d = -d; }
        word g = gcd_word(n < 0 ? -n : n, d);
        if(g > 1){ n /= g; d /= g; }
        if(d == 1){ return integer_from_word(n); }
        return ratio_box(integer_from_word(n), integer_from_word(d));}
    /* General path: at least one operand is a BIGNUM.  Reduce with the C bignum
       engine.  GC is disabled across the bounded Ref construction so the
       intermediates (and the operands themselves) need no live-pool rooting;
       ratio_box() re-roots its two survivors before the final allocate. */
    Ref zero = integer_from_word(0);
    if(c_bignum_cmp(den, zero) == 0){ ERROR("Division by zero in ratio%s",""); }
    bool saved = garbage_collection_is_enabled();
    set_garbage_collection_enabled(false);
    if(c_bignum_cmp(den, zero) < 0){          /* force denominator positive */
        num = c_bignum_sub(zero, num);
        den = c_bignum_sub(zero, den); }
    Ref g = c_bignum_gcd(num, den);           /* non-negative, >= 1 (den != 0) */
    if(!(integerp(g) && integer_value(g) == 1)){
        num = c_bignum_divide(num, g);        /* exact: g divides both          */
        den = c_bignum_divide(den, g); }
    Ref result;
    if(integerp(den) && integer_value(den) == 1){ result = num; }
    else                                        { result = ratio_box(num, den); }
    set_garbage_collection_enabled(saved);
    return result;}

bool ratiop(Ref object){
    return class_of(object) == RATIO_Class;}

Ref ratio_numerator(Ref r){
    if(integerp(r) || bignump(r)) return r;   /* a bare integer is its own numerator */
    check_class(r, RATIO_Class);
    return r->slots->ratio.numerator;}

Ref ratio_denominator(Ref r){
    if(integerp(r) || bignump(r)) return integer_from_word(1);
    check_class(r, RATIO_Class);
    return r->slots->ratio.denominator;}

/* Rational → floating */
floating ratio_to_float(Ref r){
    if(integerp(r)) return (floating)integer_value(r);
    return (floating)integer_value(r->slots->ratio.numerator)
         / (floating)integer_value(r->slots->ratio.denominator);}

/* COMPLEX */

Ref complex_new(Ref re, Ref im){
    /* (complex r 0) collapses to the real R only when BOTH parts are rational
       and the imaginary part is the integer 0 (CLHS 12.1.5.3): a float part
       forces a complex result even when the imaginary part is 0.0, so
       #C(1.0 0.0) stays complex. */
    if(integerp(im) && integer_value(im) == 0 && !floatp(re)) return re;
    /* re/im are already-rooted Refs; set slots before objcount so a GC during
       Object_new() never sees a half-built Complex. */
    Complex* slots = allocate(sizeof(Complex));
    slots->realpart = re;
    slots->imagpart = im;
    slots->objcount = 2;
    return Object_new(COMPLEX_Class, (Slots*)slots);}

bool complexp(Ref object){
    return class_of(object) == COMPLEX_Class;}

Ref complex_realpart(Ref c){
    if(!complexp(c)) return c;     /* any non-complex number is its own realpart */
    return c->slots->complex.realpart;}

Ref complex_imagpart(Ref c){
    if(!complexp(c)) return integer_from_word(0);
    return c->slots->complex.imagpart;}

/* CONS */

Ref cons(Ref car,Ref cdr){
    /* Single-block compact cons (see ConsCell in kernel_types.h): one
       allocation, car/cdr inline.  Fill slots before setting objcount so the
       collector never sees uninitialised reference slots.  car/cdr must already
       be rooted by the caller (same contract as before — cons is far too hot to
       root them here; the unrooted-arg gaps are fixed at the specific call
       sites: tagbody_collect_tags, environment_add_*, etc.). */
    ConsCell* that=allocate(sizeof(ConsCell));
    that->class=CONS_Class;
    that->car=car;
    that->cdr=cdr;
    that->objcount=3;
    return (Ref)that;}

bool consp(Ref object){
    return class_of(object)==CONS_Class;}

Ref cons_car(Ref cell){
    /* Poison finder: catch a raw car/cdr of a freed+quarantined cons — the case
       class_of misses (a rooting-gap cycle is traversed via these, never through
       class_of).  gc_poison_mode is false in normal runs (predictable branch). */
    if(gc_poison_mode && (uintptr_t)cell->class==0x015015015015015eULL){ gc_poison_report(cell); }
    return ((ConsCell*)cell)->car;}

Ref cons_cdr(Ref cell){
    if(gc_poison_mode && (uintptr_t)cell->class==0x015015015015015eULL){ gc_poison_report(cell); }
    return ((ConsCell*)cell)->cdr;}

Ref cons_rplaca(Ref cell,Ref newcar){
    /* Poison finder: a WRITE to a freed+quarantined cons is how rooting-gap cycles
       get created (rplacd into a reused cell).  Catch it here too. */
    if(gc_poison_mode && (uintptr_t)cell->class==0x015015015015015eULL){ gc_poison_report(cell); }
    ((ConsCell*)cell)->car=newcar;
    return cell;}

Ref cons_rplacd(Ref cell,Ref newcdr){
    if(gc_poison_mode && (uintptr_t)cell->class==0x015015015015015eULL){ gc_poison_report(cell); }
    ((ConsCell*)cell)->cdr=newcdr;
    return cell;}



/* STRING */

Ref string_new(halfword length){
    return Object_new(STRING_Class,
                      (Slots*)OctetArray_slots(CHARACTER_Symbol,1,&length));}

Ref string_new_vector_of_octets(VectorOfOctet* octets){
    return Object_new(STRING_Class,
                      (Slots*)OctetArray_slots_vector_of_octets(CHARACTER_Symbol,octets));}

Ref string_new_cstring(const char* string){
    return Object_new(STRING_Class,
                      (Slots*)OctetArray_slots_cstring(CHARACTER_Symbol,string));}

bool stringp(Ref object){
    return class_of(object)==STRING_Class;}

uword string_length(Ref string){
    check_class(string,STRING_Class);
    return string->slots->octet_array.dimensions[0];}

octet* string_octets(Ref string){
    check_class(string,STRING_Class);
    return string->slots->octet_array.elements->elements;}

char* string_cstring(Ref string){
    check_class(string,STRING_Class);
    size_t length=string->slots->octet_array.dimensions[0];
    char* result=CHECK_POINTER(malloc(1+length));
    strncpy(result,(const char*)string->slots->octet_array.elements->elements,length);
    result[length]='\0';
    return result;}

bool string_eq(Ref a,Ref b){
    if(a==b){
        return true;}
    uword length=string_length(a);
    if(length!=string_length(b)){
        return false;}
    OctetArray* aa=&(a->slots->octet_array);
    OctetArray* bb=&(b->slots->octet_array);
    for(uword i=0;i<length;i++){
        if(aa->elements->elements[i]!=bb->elements->elements[i]){
            return false;}}
    return true;}


static bool vector_of_octet_equalp(VectorOfOctet* a,VectorOfOctet* b){
    if(a->length!=b->length){
        return false;}
    octet* ap=a->elements;
    octet* bp=b->elements;
    for(uword i=0;i<a->length;i++){
        int al=islower(ap[i])?toupper(ap[i]):ap[i];
        int bl=islower(bp[i])?toupper(bp[i]):bp[i];
        if(al!=bl){
            return false;}}
    return true;}

bool string_equal(Ref a,Ref b){
    if(a==b){
        return true;}
    uword length=string_length(a);
    if(length!=string_length(b)){
        return false;}
    OctetArray* aa=&(a->slots->octet_array);
    OctetArray* bb=&(b->slots->octet_array);
    return vector_of_octet_equalp(aa->elements,bb->elements);}



/* SYMBOL */

Symbol* Symbol_slots(Ref name,Ref package){
    Symbol* that=allocate(sizeof(*that));
    that->objcount=5;
    that->name=name;
    that->package=package;
    that->value=UNBOUND_Object;
    that->function=UNBOUND_Object;
    that->plist=NIL_Symbol;
    return that;}

/* ----------------------------------------------------------------------
 * Symbol-table index (obarray hash).
 *
 * find_symbol used to linear-scan the obarray (a cons list), comparing names
 * with string_eq — O(n) per intern, O(n^2) to read a large program.  (The ANSI
 * suite spent ~30% of load time in string_eq/find_symbol.)  The cons list stays
 * authoritative — it roots the symbols and supports package iteration — and we
 * add a C-side open-addressing hash, name -> symbol, purely as an index:
 *   - the GC is non-moving, so cached symbol pointers stay valid;
 *   - every symbol in the index is also on the obarray list (hence rooted), so
 *     the index needs no GC roots and is never scanned by the collector;
 *   - the obarray symbol list is mutated in exactly one place
 *     (symbol_intern_obarray), which updates the index, so they never diverge.
 * The index is built lazily from the current list the first time an obarray is
 * seen, capturing any symbols installed during bootstrap.  All storage is the
 * C heap (malloc/calloc), so none of this triggers a Lisp GC.
 * -------------------------------------------------------------------- */
typedef struct SymHash {
    Ref             obarray;
    Ref*            buckets;   /* symbol Refs; NULL == empty slot */
    uword*          hashes;    /* cached name hash for each occupied slot */
    uword           capacity;  /* power of two */
    uword           count;
    struct SymHash* next;
} SymHash;

static SymHash* g_sym_hashes = NULL;

static uword sym_name_hash(Ref name){
    /* FNV-1a over the name's octets. */
    uword h = 1469598103934665603UL;
    uword length = string_length(name);
    const octet* p = name->slots->octet_array.elements->elements;
    for(uword i=0;i<length;i++){ h ^= (uword)p[i]; h *= 1099511628211UL; }
    return h;}

static void sym_hash_put(SymHash* H,Ref symbol,uword hash){
    uword mask = H->capacity - 1;
    uword i = hash & mask;
    while(H->buckets[i] != NULL){ i = (i+1) & mask; }
    H->buckets[i] = symbol;
    H->hashes[i]  = hash;
    H->count++;}

static void sym_hash_grow(SymHash* H){
    uword  oldcap = H->capacity;
    Ref*   oldb   = H->buckets;
    uword* oldh   = H->hashes;
    H->capacity = oldcap * 2;
    H->buckets  = (Ref*)  calloc(H->capacity, sizeof(Ref));
    H->hashes   = (uword*)calloc(H->capacity, sizeof(uword));
    H->count    = 0;
    for(uword i=0;i<oldcap;i++){ if(oldb[i]){ sym_hash_put(H,oldb[i],oldh[i]); } }
    free(oldb); free(oldh);}

static void sym_hash_add(SymHash* H,Ref symbol){
    if((H->count + 1) * 10 >= H->capacity * 7){ sym_hash_grow(H); }   /* load < 0.7 */
    sym_hash_put(H, symbol, sym_name_hash(symbol->slots->symbol.name));}

static SymHash* sym_hash_for(Ref obarray){
    for(SymHash* H=g_sym_hashes; H; H=H->next){
        if(H->obarray==obarray){ return H; }}
    SymHash* H = (SymHash*)malloc(sizeof(SymHash));
    H->obarray  = obarray;
    H->capacity = 2048;
    H->buckets  = (Ref*)  calloc(H->capacity, sizeof(Ref));
    H->hashes   = (uword*)calloc(H->capacity, sizeof(uword));
    H->count    = 0;
    H->next     = g_sym_hashes;
    g_sym_hashes = H;
    /* Build from the symbols already on the list. */
    Ref syms = cons_cdr(obarray);
    while(consp(syms)){ sym_hash_add(H, cons_car(syms)); syms = cons_cdr(syms); }
    return H;}

Ref find_symbol(Ref name,Ref obarray){
    SymHash* H = sym_hash_for(obarray);
    uword hash = sym_name_hash(name);
    uword mask = H->capacity - 1;
    uword i = hash & mask;
    while(H->buckets[i] != NULL){
        if(H->hashes[i]==hash
           && string_eq(H->buckets[i]->slots->symbol.name, name)){
            return H->buckets[i];}
        i = (i+1) & mask;}
    return NULL;}

Ref symbol_new(Ref name){
    /* make an uninterned symbol */
    return Object_new(SYMBOL_Class, (Slots*)Symbol_slots(name,NIL_Symbol));}

Ref symbol_new_cstring(const char* name){
    /* make an uninterned symbol */
    return symbol_new(string_new_cstring(name));}

static Ref symbol_intern_obarray(Ref name,Ref obarray,bool* internal){
    Ref symbol=find_symbol(name,obarray);   /* builds/uses the obarray hash index */
    if(symbol){
        (*internal)=true;
        return symbol;}
    (*internal)=false;
    /* Cold path only (interning a NEW symbol, rare after bootstrap): root
       name + obarray + the fresh symbol across the Symbol_slots / Object_new /
       cons / sym_hash_add allocations — a GC in any of them would otherwise free
       `name` (reachable only through this C local until stored in the symbol).
       The rooting is kept OUT of the hot found-path above so it does not perturb
       layout on every S(...) lookup. */
    uword pool=open_live_pool();
    add_to_live_pool(name);
    add_to_live_pool(obarray);
    symbol=Object_new(SYMBOL_Class, (Slots*)Symbol_slots(name,obarray));
    add_to_live_pool(symbol);
    /* Home package: once the package system exists, KERNEL_obarray IS the
       COMMON-LISP package and KEYWORD_obarray IS the KEYWORD package, so set the
       symbol's home package accordingly.  During very early bootstrap (before
       kernel_initialize_packages) the packages are NULL; those symbols get their
       home package fixed up at the end of kernel_initialize_packages. */
    {   extern Ref COMMON_LISP_Package, KEYWORD_Package;
        if(obarray==KERNEL_obarray && COMMON_LISP_Package){
            symbol->slots->symbol.package=COMMON_LISP_Package; }
        else if(obarray==KEYWORD_obarray && KEYWORD_Package){
            symbol->slots->symbol.package=KEYWORD_Package; }}
    cons_rplacd(obarray,cons(symbol,cons_cdr(obarray)));
    sym_hash_add(sym_hash_for(obarray),symbol);   /* keep the index in sync */
    close_live_pool(pool);
    return symbol;}

Ref symbol_intern(Ref name){
    bool internal;
    return symbol_intern_obarray(name,KERNEL_obarray,&internal);}

/* Package support (packages.c): an "obarray" is a cons sentinel whose CDR is the
   symbol list, indexed by a SymHash.  These let the package system reuse the
   fast name->symbol index for each package's present/external tables. */
Ref make_obarray(void){
    return cons(NIL_Symbol,NIL_Symbol);}

/* Rebuild an obarray's SymHash index from its current symbol list (used after a
   removal, which is rare). */
static void sym_hash_rebuild(Ref obarray){
    SymHash* H=sym_hash_for(obarray);
    for(uword i=0;i<H->capacity;i++){ H->buckets[i]=NULL; H->hashes[i]=0; }
    H->count=0;
    Ref syms=cons_cdr(obarray);
    while(consp(syms)){ sym_hash_add(H,cons_car(syms)); syms=cons_cdr(syms); }}

/* Add an EXISTING symbol to an obarray (no-op if a symbol of that name is
   already there).  Used to populate a package's present/external tables. */
void obarray_add_symbol(Ref obarray,Ref sym){
    if(find_symbol(symbol_name(sym),obarray)){ return; }
    /* Cold path only (adding a NEW symbol): root obarray + sym across the cons +
       sym_hash_add — a GC in either would free one of these C-local-only Refs.
       Same gap + fix as symbol_intern_obarray; kept out of the hot found-path. */
    uword pool=open_live_pool();
    add_to_live_pool(obarray);
    add_to_live_pool(sym);
    cons_rplacd(obarray,cons(sym,cons_cdr(obarray)));
    sym_hash_add(sym_hash_for(obarray),sym);
    close_live_pool(pool);}

/* Remove a symbol from an obarray (list + index).  Returns true if removed. */
bool obarray_remove_symbol(Ref obarray,Ref sym){
    Ref prev=obarray, it=cons_cdr(obarray);
    bool found=false;
    while(consp(it)){
        if(cons_car(it)==sym){ cons_rplacd(prev,cons_cdr(it)); found=true; break; }
        prev=it; it=cons_cdr(it);}
    if(found){
        /* Rebuild the index for this obarray (simplest correct approach). */
        sym_hash_rebuild(obarray);}
    return found;}

Ref symbol_intern_cstring(const char* name){
    return symbol_intern(string_new_cstring(name));}

bool symbolp(Ref object){
    return typep(object,SYMBOL_Class);}
    /* return class_of(object)==SYMBOL_Class;} */

Ref symbol_package(Ref symbol){
    check_type(symbol,SYMBOL_Class);
    return symbol->slots->symbol.package;}

Ref symbol_name(Ref symbol){
    check_type(symbol,SYMBOL_Class);
    return symbol->slots->symbol.name;}

bool symbol_boundp(Ref symbol){
    check_type(symbol,SYMBOL_Class);
    return UNBOUND_Object!=symbol->slots->symbol.value;}

bool symbol_fboundp(Ref symbol){
    check_type(symbol,SYMBOL_Class);
    return UNBOUND_Object!=symbol->slots->symbol.function;}

Ref symbol_value(Ref symbol){
    check_type(symbol,SYMBOL_Class);
    return symbol->slots->symbol.value;}

Ref symbol_function(Ref symbol){
    check_type(symbol,SYMBOL_Class);
    return symbol->slots->symbol.function;}

Ref symbol_plist(Ref symbol){
    check_type(symbol,SYMBOL_Class);
    return symbol->slots->symbol.plist;}

Ref symbol_make_unbound(Ref symbol){
    check_type(symbol,SYMBOL_Class);
    symbol->slots->symbol.value=UNBOUND_Object;
    return symbol;}

Ref symbol_make_funbound(Ref symbol){
    check_type(symbol,SYMBOL_Class);
    symbol->slots->symbol.function=UNBOUND_Object;
    return symbol;}

Ref set_symbol_package(Ref symbol,Ref new_package){
    check_type(symbol,SYMBOL_Class);
    return symbol->slots->symbol.package=new_package;}

Ref set_symbol_value(Ref symbol,Ref new_value){
    check_type(symbol,SYMBOL_Class);
    return symbol->slots->symbol.value=new_value;}

Ref set_symbol_function(Ref symbol,Ref new_function){
    check_type(symbol,SYMBOL_Class);
    /* Accept any callable: kernel function, user closure, or built-in. */
    if(!functionp(new_function) && !closurep(new_function) && !built_in_function_p(new_function)){
        TYPE_ERROR(new_function,FUNCTION_Class);}
    return symbol->slots->symbol.function=new_function;}

Ref set_symbol_plist(Ref symbol,Ref new_plist){
    check_type(symbol,SYMBOL_Class);
    if(consp(new_plist)||nullp(new_plist)){
        return symbol->slots->symbol.plist=new_plist;}
    else{
        TYPE_ERROR(new_plist,LIST_Class);}}

Ref obarray_all_symbols(Ref obarray){
    return copy_list(cdr(obarray));}

/* KEYWORD */
/* Note: KEYWORD is not a class, but a subtype of SYMBOL */

Ref keyword_intern(Ref name){
    bool internal;
    Ref keyword=symbol_intern_obarray(name,KEYWORD_obarray,&internal);
    if(!internal){
        define_constant(keyword,keyword);}
    return keyword;}

Ref keyword_intern_cstring(const char* name){
    return keyword_intern(string_new_cstring(name));}

bool keywordp(Ref object){
    return symbolp(object)
            && !nullp(memq(object,cdr(KEYWORD_obarray)));}

/* NULL */
/* NULL class has a SYMBOL superclass and has a NIL_Symbol instance. */

bool nullp(Ref object){
    return object==NIL_Symbol;}


/* BOOLEAN */

Ref as_boolean(bool value){
    return (value
            ?T_Symbol
            :NIL_Symbol);}

bool is_true(Ref object){
    return object!=NIL_Symbol;}


/* VECTOR */

Ref vector_new(Ref elements){
    uword length=list_length(elements);
    halfword dimensions[1];
    dimensions[0]=CHECK_UWORD_TO_HALFWORD(length);
    Array* array=Array_slots(T_Symbol,1,dimensions);
    for(int i=0;consp(elements);i++,elements=cdr(elements)){
        array->elements->elements[i]=car(elements);}
    return Object_new(VECTOR_Class,(Slots*)array);}


/* BIT-VECTOR */

Ref bit_vector_new_octets(uword length,octet* bits){
    halfword dimensions[1];
    dimensions[0]=CHECK_UWORD_TO_HALFWORD(length);
    /* Use OctetArray_slots so the VectorOfOctet layout matches what
       bit(), bit_set(), and bit_vector_equal() expect. */
    OctetArray* array=OctetArray_slots(BIT_Symbol,1,dimensions);
    memcpy(array->elements->elements,bits,length);
    return Object_new(BIT_VECTOR_Class,(Slots*)array);}

bool bit_vector_equal(Ref a,Ref b){
    if(a==b){
        return true;}
    uword length=bit_vector_length(a);
    if(length!=bit_vector_length(b)){
        return false;}
    OctetArray* aa=&(a->slots->octet_array);
    OctetArray* bb=&(b->slots->octet_array);
    octet* ap=aa->elements->elements;
    octet* bp=bb->elements->elements;
    for(uword i=0;i<length;i++){
        if(ap[i]!=bp[i]){
            return false;}}
    return true;}


/* FILL POINTERS ----------------------------------------------------------
   A fill pointer lives inline in the vector's slots block: the untraced
   `fill_pointer` word of Array (for VECTOR) / OctetArray (for STRING and
   BIT-VECTOR).  Encoding: 0 = no fill pointer, active length N stored as N+1
   (so a zeroed/fresh vector has none — see kernel_types.h).  Storing it in the
   object — rather than an external table — means a vector that becomes garbage
   is collected normally, with no pinning.  AREF/CHAR/SVREF and *_length keep
   reading dimensions[0] (the physical size), so they correctly ignore the fill
   pointer per CLHS; only LENGTH/ELT/STRING and the VECTOR-PUSH family use it. */
static uword* fill_pointer_loc(Ref v){
    Ref c=class_of(v);
    if(c==VECTOR_Class)     return &v->slots->array.fill_pointer;
    if(c==STRING_Class)     return &v->slots->octet_array.fill_pointer;
    if(c==BIT_VECTOR_Class) return &v->slots->octet_array.fill_pointer;
    return NULL;}   /* not a fill-pointer-capable object */

bool array_has_fill_pointer(Ref v){
    uword* loc=fill_pointer_loc(v);
    return loc && (*loc!=0);}

/* The fill pointer of V as an INTEGER, or NIL when V has none (or is not a
   vector at all — callers pass arbitrary objects, e.g. LENGTH). */
Ref array_fill_pointer(Ref v){
    uword* loc=fill_pointer_loc(v);
    return (loc && *loc!=0) ? integer_from_uword(*loc-1) : NIL_Symbol;}

/* Set V's fill pointer to FP (a non-negative integer), registering V as having
   one.  Errors if V cannot carry a fill pointer. */
Ref array_set_fill_pointer(Ref v,Ref fp){
    uword* loc=fill_pointer_loc(v);
    if(!loc){ TYPE_ERROR(v,VECTOR_Class); }
    word f=integer_value(fp);
    if(f<0){ ERROR("fill pointer must be non-negative, got %" WORD_FORMAT,f); }
    *loc=(uword)f+1;
    return fp;}


/* CLASS */

Class* Class_slots(Ref name,Ref superclasses){
    Class* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=name;
    that->direct_superclasses=superclasses;
    return that;}

Class* make_boot_Class(){
    Class* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=&uninitialized_object;
    that->direct_superclasses=&uninitialized_object;
    return that;}

Ref class_new(Ref name){
    return Object_new(CLASS_Class,(Slots*)Class_slots(name,NIL_Symbol));}

/* ---- CLOS type-bridge -------------------------------------------------------
 * When Slice 19 (CLOS) is loaded, these two variables are set by calling
 * (%clos-register obj-struct-class cls-struct-class) from prelude.lisp.
 * Once set, class_of() and subclassp() understand CLOS instances/classes.
 * --------------------------------------------------------------------------- */
static Ref clos_obj_sc  = NULL;  /* structure class for %obj (CLOS instances) */
static Ref clos_cls_sc  = NULL;  /* structure class for %cls (CLOS class objects) */
static Ref sym_cpl      = NULL;  /* the 'CPL slot of %cls */
static Ref sym_class_name = NULL;/* the 'NAME slot of %cls */
static uword clos_obj_class_slot_idx = 0; /* index of 'class slot in %obj storage */

void clos_register_bridge(Ref obj_sc, Ref cls_sc){
    clos_obj_sc = obj_sc;
    clos_cls_sc = cls_sc;
    /* Cache slot index: 'class is the FIRST user slot in %obj after BLOCK_HEADER. */
    clos_obj_class_slot_idx = 0;  /* %obj: (defstruct (%obj) class slots) — class=0 */
    sym_cpl       = symbol_intern_cstring("CPL");
    sym_class_name= symbol_intern_cstring("NAME");}

bool clos_instance_p(Ref obj){
    return clos_obj_sc != NULL && obj != NULL && obj->class == clos_obj_sc;}

bool clos_class_p(Ref cls){
    return clos_cls_sc != NULL && cls != NULL && cls->class == clos_cls_sc;}

Ref class_of(Ref object){
    /* Use-after-free finder: if this object was freed + quarantined (BOCL_POISON),
       its class is the poison sentinel — report and abort. */
    if(gc_poison_mode && object && (uintptr_t)object->class==0x015015015015015eULL){
        gc_poison_report(object);}
    /* For CLOS instances (%obj structs), return the CLOS class stored in
     * the 'class slot (index 0 in the structure's slot vector).
     * Direct array access avoids allocating via structure_get. */
    if(clos_obj_sc != NULL && object != NULL && object->class == clos_obj_sc){
        return object->slots->vector_of_object.elements[clos_obj_class_slot_idx];}
    return object->class;}

bool classp(Ref object){
    return class_of(object)==CLASS_Class;}

Ref class_name(Ref class){
    check_type(class,CLASS_Class);
    return class->slots->class.name;}

Ref class_direct_superclasses(Ref class){
    check_type(class,CLASS_Class);
    return class->slots->class.direct_superclasses;}

void class_add_superclass(Ref class, Ref superclass){
    check_type(class,CLASS_Class);
    check_type(superclass,CLASS_Class);
    if(class->slots->class.direct_superclasses){
        class->slots->class.direct_superclasses=nconc(class->slots->class.direct_superclasses,
                                                      cons(superclass,NIL_Symbol));}
    else{
        class->slots->class.direct_superclasses=cons(superclass,NIL_Symbol);}}

Ref find_class(Ref name){
    /* Search the kernel class list only (no Lisp call to avoid recursion). */
    {dolist(class,kernel_classes){
            if(eql(class->slots->class.name,name)){
                return class;}}}
    return NULL;}

/* Name of a class designator that may be either a kernel CLASS object or a CLOS
   %cls structure. */
static Ref class_designator_name(Ref class){
    if(class == NULL){ return NIL_Symbol; }
    if(clos_class_p(class)){
        return sym_class_name != NULL ? structure_get(class, sym_class_name) : NIL_Symbol;}
    /* Otherwise a kernel CLASS object (subclassp is only called with class
       designators, like the ->slots->class access below); class_same's symbolp
       guard rejects any non-symbol that might be read for a stray non-class. */
    return class->slots->class.name;}

/* Two class designators name the SAME class iff identical, or — bridging the two
   parallel condition hierarchies — they have the same non-NIL class name.  bocl
   has both a kernel built-in class (e.g. WARNING_Class) and a CLOS %cls for each
   standard condition type; a make-condition'd condition's class is the %cls while
   a handler type resolves to the kernel class, so identity alone would miss. */
static bool class_same(Ref a, Ref b){
    if(a==b){ return true; }
    Ref na = class_designator_name(a);
    return na != NIL_Symbol && symbolp(na) && na == class_designator_name(b);}

bool subclassp(Ref class,Ref superclass){
    if(class_same(class, superclass)){
        return true;}
    /* Handle CLOS class objects (%cls): walk the CPL list. */
    if(clos_class_p(class) && sym_cpl != NULL){
        Ref cpl = structure_get(class, sym_cpl);
        while(consp(cpl)){
            if(class_same(cons_car(cpl), superclass)){ return true; }
            cpl = cons_cdr(cpl);}
        return false;}
    Ref direct_superclasses=class->slots->class.direct_superclasses;
    while(consp(direct_superclasses)){
        Ref direct_superclass=cons_car(direct_superclasses);
        direct_superclasses=cons_cdr(direct_superclasses);
        if(subclassp(direct_superclass,superclass)){
            return true;}}
    return false;}

bool typep(Ref object,Ref class){
    Ref object_class=class_of(object);
#ifdef DEBUG
    fprintf(stderr,"Is object %p of type %p\n",
            (void*)object,
            (void*)class);
    if(object==NULL){
        fprintf(stderr,"NULL object!\n");
        abort();}
    if(class==NULL){
        fprintf(stderr,"NULL class!\n");
        abort();}
    if(object_class==class){
        return true;}
    fprintf(stderr,"Is object of class %p of type %p\n",
            (void*)object_class->slots->class.name->slots->symbol.name,
            (void*)       class->slots->class.name->slots->symbol.name);
    fprintf(stderr,"Is object of class %s of type %s\n",
            string_cstring(object_class->slots->class.name->slots->symbol.name),
            string_cstring(       class->slots->class.name->slots->symbol.name));
#endif
    /* check_type(class,CLASS_Class); */
    return(object_class==class)
            ?true
            :subclassp(object_class,class);}


void check_type(Ref object,Ref class){
    /* checks that the object is an instance of the class, direct or indirect */
    if(!typep(object,class)){
        TYPE_ERROR(object,class);}}


void check_class(Ref object,Ref class){
    /* checks that the object is a direct instance of the class */
    if(class_of(object)!=class){
        TYPE_ERROR(object,class);}}






Ref boot_last(Ref list){
    Ref next;
    if(consp(list)){
        while(consp(next=cons_cdr(list))){
            list=next;}
        return list;}
    else if(nullp(list)){
        return list;}
    else{
        TYPE_ERROR(list,LIST_Class);}}

Ref boot_nconc(Ref list,Ref tail){
    if(nullp(list)){
        return tail;}
    else if(consp(list)){
        Ref last_cell=boot_last(list);
        cons_rplacd(last_cell,tail);
        return list;}
    else{
        TYPE_ERROR(list,LIST_Class);}}

void boot_class_add_superclass(Ref class, Ref superclass){
    if(class->slots->class.direct_superclasses){
        class->slots->class.direct_superclasses=boot_nconc(class->slots->class.direct_superclasses,
                                                           cons(superclass,NIL_Symbol));}
    else{
        class->slots->class.direct_superclasses=cons(superclass,NIL_Symbol);}}



/* STANDARD-CLASS */

StandardClass* StandardClass_slots(Ref name,Ref superclasses){
    StandardClass* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=name;
    that->direct_superclasses=superclasses;
    return that;}

StandardClass* make_boot_StandardClass(void){
    StandardClass* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=&uninitialized_object;
    that->direct_superclasses=&uninitialized_object;
    return that;}


/* BUILT-IN-CLASS */

Ref built_in_class_new(Ref name){
    return Object_new(BUILT_IN_CLASS_Class,(Slots*)BuiltInClass_slots(name,NIL_Symbol));}

BuiltInClass* BuiltInClass_slots(Ref name,Ref superclasses){
    BuiltInClass* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=name;
    that->direct_superclasses=superclasses;
    return that;}

BuiltInClass* make_boot_BuiltInClass(void){
    BuiltInClass* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=&uninitialized_object;
    that->direct_superclasses=&uninitialized_object;
    return that;}


/* STRUCTURE-CLASS */


/* TODO: implement initial-offset
For this, we may have to keep an a-list of slot-names with their
indexes, to deal with the holes, and to record the initial-offset in
the structure-class.
*/


StructureClass* make_boot_StructureClass(void){
    StructureClass* that=allocate(sizeof(*that));
    that->objcount=3;
    that->name=&uninitialized_object;
    that->direct_superclasses=&uninitialized_object;
    that->slot_names=&uninitialized_object;
    return that;}

StructureClass* StructureClass_slots(Ref class_name,Ref direct_superclass,Ref offset, Ref slot_names){
    uword pool=open_live_pool();
    add_to_live_pool(class_name);
    add_to_live_pool(direct_superclass);
    add_to_live_pool(offset);
    add_to_live_pool(slot_names);
    StructureClass* that=allocate(sizeof(*that));
    that->objcount=5;   /* name, direct_superclasses, offset, slot_names, cached_slots */
    that->name=class_name;
    that->direct_superclasses=list(direct_superclass,NULL);
    that->offset=offset;
    that->slot_names=slot_names;
    that->cached_slots=NULL;   /* not yet computed (allocate() already zeroed it) */
    that->nslots=direct_superclass->slots->structure_class.nslots
            +(uword)integer_value(offset)
            +list_length(slot_names);
    close_live_pool(pool);
    add_to_live_pool((Ref)that);   /* re-root: caller's Object_new allocates (GC) before storing `that` */
    return that;}

Ref make_structure_class(Ref name,Ref direct_superclass,Ref offset,Ref slots){
    uword pool=open_live_pool();
    add_to_live_pool(name);
    add_to_live_pool(direct_superclass);
    add_to_live_pool(offset);
    add_to_live_pool(slots);
    /*
    Creates a class that has structure-class metaclass,
    direct_superclass or structure-object as superclass.

    OFFSET is an offset number of slots inserted before the SLOTS.

    SLOTS is a list of symbols naming the additionnal slots to the new
    structure class.

    (An instance will have the concatenation of all the slots lists of all
    the superclasses of its structure-class).
    */
    check_class(name,SYMBOL_Class);
    if(direct_superclass==NIL_Symbol){
        direct_superclass=STRUCTURE_OBJECT_Class;}
    else{
        check_type(direct_superclass,STRUCTURE_CLASS_Class);}
    check_type(slots,LIST_Class);
    /* TODO: check that all elements of slots is a symbol */
    Ref class=Object_new(STRUCTURE_CLASS_Class,
                         (Slots*)StructureClass_slots(name,direct_superclass,offset,slots));
    close_live_pool(pool);
    add_to_live_pool(class);   /* re-root: close_live_pool trimmed the fresh class */
    return class;}

Ref make_structure_instance(Ref structure_class){
    uword pool=open_live_pool();
    add_to_live_pool(structure_class);
    /*
    Creates an instance of the structure_class.
    All slots are initialized to NIL.
    */
    check_class(structure_class,STRUCTURE_CLASS_Class);
    VectorOfObject* slots=VectorOfObject_slots(CHECK_UWORD_TO_HALFWORD(structure_class->slots->structure_class.nslots));
    add_to_live_pool((Ref)slots);   /* root the fresh slots across Object_new's allocation */
    Ref instance=Object_new(structure_class,(Slots*)slots);
    close_live_pool(pool);
    add_to_live_pool(instance);   /* re-root: close_live_pool trimmed the fresh instance */
    return instance;}


/* FUNCALLABLE-STANDARD-CLASS */

FuncallableStandardClass* FuncallableStandardClass_slots(Ref name,Ref superclasses){
    FuncallableStandardClass* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=name;
    that->direct_superclasses=superclasses;
    return that;}

FuncallableStandardClass* make_boot_FuncallableStandardClass(void){
    FuncallableStandardClass* that=allocate(sizeof(*that));
    that->objcount=2;
    that->name=&uninitialized_object;
    that->direct_superclasses=&uninitialized_object;
    return that;}




/* PHYSICAL-PATHNAME */

Ref physical_pathname_new(VectorOfOctet* path){
    return Object_new(find_class(symbol_intern_cstring("PHYSICAL-PATHNAME")),
                      (Slots*)path);}

/* If PATH begins with ~ followed by '/' or end-of-string, return a malloc'd
   copy with the leading ~ replaced by $HOME (trailing slashes of $HOME trimmed),
   i.e. ~ abbreviates (USER-HOMEDIR-PATHNAME); otherwise a plain copy.  ~user is
   not handled.  Caller frees. */
static char* expand_home_namestring(const char* path){
    if(path[0]=='~' && (path[1]=='/' || path[1]=='\0')){
        const char* home=getenv("HOME");
        if(home && home[0]){
            size_t hlen=strlen(home);
            while(hlen>1 && home[hlen-1]=='/'){ hlen--; }
            size_t rlen=strlen(path+1);             /* part after '~' (incl. '/') */
            char* result=CHECK_POINTER(malloc(hlen+rlen+1));
            memcpy(result,home,hlen);
            memcpy(result+hlen,path+1,rlen+1);      /* +1 copies the NUL */
            return result;}}
    return strdup(path);}

Ref physical_pathname_new_cstring(const char* path){
    char* expanded=expand_home_namestring(path);
    Ref result=physical_pathname_new(VectorOfOctet_slots_cstring(expanded));
    free(expanded);
    return result;}

Ref physical_pathname(Ref namestring){
    check_class(namestring,STRING_Class);
    /* A leading ~ abbreviates the user's home directory; expand via the cstring
       path (which trims/joins $HOME). */
    {   OctetArray* oa0 = &namestring->slots->octet_array;
        if(oa0->dimensions[0] >= 1 && oa0->elements->elements[0] == '~'){
            return physical_pathname_new_cstring(string_cstring(namestring));}}
    /* Copy the string's bytes into a fresh VectorOfOctet so the pathname has
       its own backing store. We cannot share the string's slots directly:
       STRING is laid out as an OctetArray (element_type, elements*,
       dimensions[]) while PHYSICAL-PATHNAME is laid out as a VectorOfOctet
       (length, elements[]). Same union, different members — reinterpreting
       the address picks up garbage (e.g. element_type is read as length). */
    OctetArray* oa = &namestring->slots->octet_array;
    halfword len = (halfword)oa->dimensions[0];
    VectorOfOctet* vof = allocate(offsetof(VectorOfOctet,elements)
                                  + sizeof(vof->elements[0]) * VARRAY_MIN(len));
    vof->length = len;
    for(halfword i = 0; i < len; i++){
        vof->elements[i] = oa->elements->elements[i];}
    return physical_pathname_new(vof);}

bool physical_pathname_p(Ref object){
    return typep(object,PATHNAME_Class);}

bool physical_pathname_equal(Ref a,Ref b){
    if(!typep(a,PHYSICAL_PATHNAME_Class)){
        return false;}
    if(!typep(b,PHYSICAL_PATHNAME_Class)){
        return false;}
    if(a==b){
        return true;}
    if(a->slots->vector_of_octet.length
       !=b->slots->vector_of_octet.length){
        return false;}
    /* A PHYSICAL-PATHNAME's slots IS a VectorOfOctet (see physical_pathname_new),
       not an OctetArray.  Reading ->octet_array.elements reinterprets the inline
       namestring bytes as a VectorOfOctet* and dereferences garbage (crash whose
       fault address is the namestring's own characters).  Compare the two
       VectorOfOctet slots directly. */
    return vector_of_octet_equalp(&(a->slots->vector_of_octet),
                                  &(b->slots->vector_of_octet));}


/* EXTERNAL-FORMAT */

/*
External-formats can be:

- :DEFAULT in place of (:iso-8859-1 :lf :error)

- a keyword denoting an encoding in (:us-ascii :iso-8859-1);
  the default line-termination is :lf, and
  the default encoding error handling is :error (signal an error).

- a list containing:

  + a keyword denoting an encoding in (:us-ascii :iso-8859-1),

  + a line-termination keyword (:dos :crlf :mac :cr :unix :lf),
    the default if NIL or absent is :LF.

  + an object specifing the handling of encoding errors:

    * a character used as substitution character #\sub is suggested,
      or an integer used as substitution code 26 (SUB) is suggested.
      (code-char code) is used on input encoding error.
      (char-code char) is used on output decoding error.

    * the keyword :error asking for a continuable encoding-error to be
      signaled, with a substitute-code restart. This is the default if absent.

    * the symbol NIL asking to ignore and skip the unencodable
      character.

Encoding or decoding errors can only occur with a code>127 given with
:us-ascii, since we deal only with :us-ascii (7-bit) and :iso-8859-1
(8-bit) and internal coding is iso-8859-1.


Note: kernel:stream-external-format accept only (:crlf :cr :lf) as
line-termination; the mapping is done in the CL layer.

(list (member :US-ASCII :ISO-8859-1)
      (member :crlf :cr :lf)
      (or character (member :error nil)))
*/

#define DEFAULT_ENCODING      K(ISO-8859-1)
#define DEFAULT_ENCODING_ENUM encoding_iso_8859_1

Ref normalize_encoding(Ref encoding_o,encoding_t* encoding){
    if(encoding_o==K(DEFAULT)){
        (*encoding)=DEFAULT_ENCODING_ENUM;
        return DEFAULT_ENCODING;}
    else if(encoding_o==K(ISO-8859-1)){
        (*encoding)=encoding_iso_8859_1;
        return encoding_o;}
    else if(encoding_o==K(US-ASCII)){
        (*encoding)=encoding_us_ascii;
        return encoding_o;}
    else{
        error("Invalid encoding %s",
              string_cstring(prin1_to_string(encoding_o)));}}

Ref normalize_new_line(Ref new_line_o,new_line_t* new_line){
    if((new_line_o==K(LF))||(new_line_o==K(UNIX))
       ||(new_line_o==K(DEFAULT))||(new_line_o==NIL_Symbol)){
        (*new_line)=new_line_lf;
        return K(LF);}
    else if((new_line_o==K(CR))||(new_line_o==K(MAC))){
        (*new_line)=new_line_cr;
        return K(CR);}
    else if((new_line_o==K(CRLF))||(new_line_o==K(DOS))){
        (*new_line)=new_line_crlf;
        return K(CRLF);}
    else{
        error("Invalid new-line format %s",
              string_cstring(prin1_to_string(new_line_o)));}}

Ref normalize_on_error(Ref on_error_o,on_error_t* on_error){
    if(characterp(on_error_o)){
        word value=integer_value(character_code(on_error_o));
        (*on_error)=(on_error_t)value;
        return on_error_o;}
    else if(on_error_o==K(ERROR)){
        (*on_error)=on_error_error;
        return on_error_o;}
    else if(on_error_o==NIL_Symbol){
        (*on_error)=on_error_ignore;
        return on_error_o;}
    else{
        error("Invalid on-error format %s",
              string_cstring(prin1_to_string(on_error_o)));}}

Ref normalize_external_format_internal(Ref external_format,
                                  encoding_t* encoding,
                                  new_line_t* new_line,
                                  on_error_t* on_error){
    Ref encoding_o=DEFAULT_ENCODING;
    Ref new_line_o=K(LF);
    Ref on_error_o=K(ERROR);
    if(symbolp(external_format)){
        encoding_o=normalize_encoding(external_format,encoding);}
    else if(consp(external_format)){
        encoding_o=normalize_encoding(car(external_format),encoding);
        new_line_o=normalize_new_line(cadr(external_format),new_line);
        if(nullp(cddr(external_format))){
            on_error_o=K(ERROR);
            (*on_error)=on_error_error;}
        else{
            on_error_o=normalize_on_error(caddr(external_format),on_error);}}
    else{
        error("Invalid external-format %s",
              string_cstring(prin1_to_string(external_format)));}
    return cons(encoding_o,cons(new_line_o,cons(on_error_o,NIL_Symbol)));}

Ref normalize_external_format(Ref external_format){
    encoding_t encoding;
    new_line_t new_line;
    on_error_t on_error;
    return normalize_external_format_internal(external_format, &encoding,&new_line,&on_error);}


direction_t convert_direction(Ref direction){
    if(direction==K(INPUT)){
        return direction_input;}
    else if(direction==K(OUTPUT)){
        return direction_output;}
    else if(direction==K(IO)){
        return direction_io;}
    else if(direction==K(PROBE)){
        return direction_probe;}
    else {
        ERROR("Invalid direction %s",
              string_cstring(prin1_to_string(direction)));}}

Ref convert_from_direction(direction_t direction){
    switch(direction){
      case direction_input:   return K(INPUT);
      case direction_output:  return K(OUTPUT);
      case direction_io:      return K(IO);
      case direction_probe:   return K(PROBE);
      default:
          FATAL("Invalid direction value %d",direction);}}

type_t convert_element_type(Ref element_type){
    /* CHARACTER or (UNSIGNED-BYTE 8) or BIT */
    if(element_type==S(CHARACTER)){
        return type_character;}
    if(element_type==S(BIT)){
        return type_bit;}
    if((consp(element_type)
        &&(car(element_type)==S(UNSIGNED-BYTE))
        &&(consp(cdr(element_type)))
        &&(integerp(cadr(element_type)))
        &&(integer_value(cadr(element_type))==8)
        &&(nullp(cddr(element_type))))){
        return type_octet;}
    ERROR("Invalid element-type %s",
          string_cstring(prin1_to_string(element_type)));}



/* FILE-STREAM */

Ref file_stream_new(Ref pathname,     /* a PHYSICAL-PATHNAME */
                    Ref element_type, /* CHARACTER or (UNSIGNED-BYTE 8) or BIT */
                    Ref external_format){
    check_class(pathname,PHYSICAL_PATHNAME_Class);
    type_t type=convert_element_type(element_type);
    encoding_t encoding=encoding_iso_8859_1;
    new_line_t new_line=new_line_lf;
    on_error_t on_error=on_error_error;
    external_format=normalize_external_format_internal(external_format,&encoding,&new_line,&on_error);
    FileStream* slots=(FileStream*)allocate(sizeof(*slots));
    slots->objcount=4;
    slots->pathname=pathname;
    slots->element_type=element_type;
    slots->external_format=external_format;
    slots->string=NIL_Symbol;
    slots->is_open=0;
    slots->direction=direction_input;
    slots->type=type;
    slots->encoding=encoding;
    slots->new_line=new_line;
    slots->on_error=on_error;
    slots->stream=NULL;
    return Object_new(find_class(symbol_intern_cstring("FILE-STREAM")),
                      (Slots*)slots);}

Ref string_stream_new(FILE* memstream){
    Ref element_type=S(CHARACTER);
    Ref external_format=list(K(ISO-8859-1),K(LF),S(NIL));
    type_t type=type_character;
    encoding_t encoding=encoding_iso_8859_1;
    new_line_t new_line=new_line_lf;
    on_error_t on_error=on_error_ignore;
    FileStream* slots=(FileStream*)allocate(sizeof(*slots));
    slots->objcount=4;
    slots->pathname=NIL_Symbol;
    slots->element_type=element_type;
    slots->external_format=external_format;
    slots->string=NIL_Symbol;
    slots->is_open=1;
    slots->direction=direction_input;
    slots->type=type;
    slots->encoding=encoding;
    slots->new_line=new_line;
    slots->on_error=on_error;
    slots->stream=memstream;
    return Object_new(find_class(symbol_intern_cstring("STRING-STREAM")),
                      (Slots*)slots);}

Ref stream_input_string(Ref string,Ref start,Ref end){
    uword len   = string_length(string);
    word slen   = CHECK_UWORD_TO_WORD(len);
    word istart = (nullp(start)) ?0 : integer_value(start);
    word iend   = (nullp(end)) ? slen : integer_value(end);
    if((istart<0) || (slen<istart)){
        ERROR("START = %"WORD_FORMAT" is beyond length of string = %"WORD_FORMAT"",istart,len);}
    if(slen<iend){
        ERROR("END = %"WORD_FORMAT" is beyond length of string = %"WORD_FORMAT"",iend,slen);}
    if(iend<istart){
        ERROR("END = %"WORD_FORMAT" is before START = %"WORD_FORMAT"",iend,istart);}
    char* buffer = (char*)string_octets(string)+istart;
    size_t size = CHECK_WORD_TO_SIZE(iend-istart);
    /* fmemopen(_, 0, _) is undefined / fails on some platforms (e.g. macOS).
       Work around: open a 1-byte stream then immediately seek to EOF.        */
    static char empty_byte = '\0';
    FILE* memstream = (size > 0)
        ? fmemopen(buffer, size, "r")
        : fmemopen(&empty_byte, 1, "r");
    if(!memstream){
        ERROR("Cannot create the STRING-STREAM for a string of length %"WORD_FORMAT"",size);}
    if(size == 0){ fseek(memstream, 0, SEEK_END); }
    Ref stream=string_stream_new(memstream);
    stream->slots->file_stream.direction=direction_input;
    stream->slots->file_stream.string=string;
    stream->slots->file_stream.buffer=(octet*)buffer;
    stream->slots->file_stream.buffer_size=size;
    return stream;}

Ref stream_output_string(unused Ref element_type){
    /* Element type is not that useful (our BASE-CHAR = CHARACTER), but it's in the API. */
    Ref string=string_new(STRING_STREAM_BUFFER_SIZE);
    char* buffer = (char*)string_octets(string);
    size_t size = STRING_STREAM_BUFFER_SIZE;
    FILE* memstream=fmemopen(buffer,size,"w");
    if(!memstream){
        ERROR("Cannot create the STRING-STREAM for a string of length %"WORD_FORMAT"",size);}
    Ref stream=string_stream_new(memstream);
    stream->slots->file_stream.direction=direction_output;
    stream->slots->file_stream.string=string;
    stream->slots->file_stream.buffer=(octet*)buffer;
    string->slots->octet_array.dimensions[0]=CHECK_SIZE_TO_UWORD(size);
    return stream;}

Ref string_output_stream_get_string(Ref string_output_stream){
    FILE* cstream=string_output_stream->slots->file_stream.stream;
    /* Guard: detect double-consume or use-after-close. */
    if(!cstream){
        STREAM_ERROR(string_output_stream);}
    fflush(cstream);
    long pos=ftell(cstream);
    /* int err=errno; */
    if(pos<0){
        STREAM_ERROR(string_output_stream); /* ,"OS Stream Error %d",err); */}
    Ref string=string_output_stream->slots->file_stream.string;
    string->slots->octet_array.dimensions[0]=CHECK_LONG_TO_UWORD(pos);
    /* Close the fmemopen FILE* now — the stream is consumed; further writes
       would be undefined per CL semantics.  Nulling the pointer prevents a
       double-close if stream_close() is called later on this object. */
    fclose(cstream);
    string_output_stream->slots->file_stream.stream=NULL;
    string_output_stream->slots->file_stream.is_open=false;
    return string;}

Ref stream_pathname(Ref stream){
    check_type(stream,FILE_STREAM_Class);
    return stream->slots->file_stream.pathname;}

Ref stream_element_type(Ref stream){
    check_type(stream,FILE_STREAM_Class);
    return stream->slots->file_stream.element_type;}

Ref stream_external_format(Ref stream){
    check_type(stream,FILE_STREAM_Class);
    return stream->slots->file_stream.external_format;}

Ref stream_direction(Ref stream){
    check_type(stream,FILE_STREAM_Class);
    return convert_from_direction(stream->slots->file_stream.direction);}

bool stream_open_p(Ref stream){
    check_type(stream,FILE_STREAM_Class);
    return stream->slots->file_stream.is_open;}

FILE* stream_cstream(Ref stream){
    check_type(stream,FILE_STREAM_Class);
    return stream->slots->file_stream.stream;}

type_t stream_type(Ref stream){
    check_type(stream,FILE_STREAM_Class);
    return stream->slots->file_stream.type;}


Ref standard_input_stream(void){
    Ref stream=file_stream_new(physical_pathname_new_cstring("/dev/stdin"),
                                  S(CHARACTER),
                                  K(DEFAULT));
    stream->slots->file_stream.is_open=true;
    stream->slots->file_stream.direction=direction_input;
    stream->slots->file_stream.stream=stdin;
    return stream;}

Ref standard_output_stream(void){
    Ref stream=file_stream_new(physical_pathname_new_cstring("/dev/stdout"),
                                  S(CHARACTER),
                                  K(DEFAULT));
    stream->slots->file_stream.is_open=true;
    stream->slots->file_stream.direction=direction_output;
    stream->slots->file_stream.stream=stdout;
    return stream;}

Ref standard_error_stream(void){
    Ref stream=file_stream_new(physical_pathname_new_cstring("/dev/stderr"),
                                  S(CHARACTER),
                                  K(DEFAULT));
    stream->slots->file_stream.is_open=true;
    stream->slots->file_stream.direction=direction_output;
    stream->slots->file_stream.stream=stderr;
    return stream;}

/* ENVIRONMENT */

static Environment* Environment_slots(Ref next_environment,bool is_toplevel){
    Environment* that=allocate(sizeof(*that));
    that->objcount=6;
    that->variables        = NIL_Symbol;
    that->functions        = NIL_Symbol;
    that->blocks           = NIL_Symbol;
    that->catchs           = NIL_Symbol;
    that->declarations     = NIL_Symbol;
    that->next_environment = next_environment;
    that->frames=NULL;
    that->is_toplevel=is_toplevel;
    return that;}

Ref environment_new(Ref next_environment,Ref toplevelp){
    return Object_new(ENVIRONMENT_Class,(Slots*)Environment_slots(next_environment,!nullp(toplevelp)));}


/* FUNCTION */

Closure* Closure_slots(Ref function,Ref environment){
    check_type(function,FUNCTION_Class);
    check_type(environment,ENVIRONMENT_Class);
    /* Root the args across allocate(): a caller typically passes a FUNCTION that
       is itself a fresh Object_new() result held only in a C temporary (e.g.
       closure_new(function_new(...), env)), so a GC inside allocate() would
       reclaim it and leave that->function dangling. */
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(environment);
    Closure* that=allocate(sizeof(*that));
    close_live_pool(pool);
    that->objcount=2;
    that->function=function;
    that->environment=environment;
    return that;}

Ref closure_new(Ref function,Ref environment){
    return Object_new(CLOSURE_Class,(Slots*)Closure_slots(function,environment));}


Function* Function_slots(Ref lisp_name,Ref lambda_list,Ref body){
    check_type(lisp_name,SYMBOL_Class);
    check_type(lambda_list,LIST_Class);
    check_type(body,LIST_Class);
    uword pool=open_live_pool();           /* root args across allocate() */
    add_to_live_pool(lisp_name);
    add_to_live_pool(lambda_list);
    add_to_live_pool(body);
    Function* that=allocate(sizeof(*that));
    close_live_pool(pool);
    that->objcount=3;
    that->lisp_name=lisp_name;
    that->lambda_list=lambda_list;
    that->body=body;
    return that;}

Ref function_new(Ref lisp_name,Ref lambda_list,Ref body){
    return Object_new(FUNCTION_Class,(Slots*)Function_slots(lisp_name,lambda_list,body));}

BuiltInFunction* BuiltInFunction_slots(Ref lisp_name,Ref lambda_list,KernelFunctionPtr kernel_function){
    check_type(lisp_name,SYMBOL_Class);
    check_type(lambda_list,LIST_Class);
    BuiltInFunction* that=allocate(sizeof(*that));
    that->objcount=2;
    that->lisp_name=lisp_name;
    that->lambda_list=lambda_list;
    that->kernel_function=kernel_function;
    return that;}

Ref built_in_function_new(Ref lisp_name,Ref lambda_list,KernelFunctionPtr kernel_function){
    return Object_new(BUILT_IN_FUNCTION_Class,(Slots*)BuiltInFunction_slots(lisp_name,lambda_list,kernel_function));}

//
// typedef struct FuncallableStandardObject {
//     BLOCK_HEADER;
//     /* TODO: FuncallableStandardObject */
//     Ref           object; /* The object data */
//     KernelFunctionPtr apply;  /* The entry point of to the Funcallable Standard Object. */
// } FuncallableStandardObject;

bool closurep(Ref object){
    return class_of(object)==CLOSURE_Class;}

bool functionp(Ref object){
    return typep(object,FUNCTION_Class);}

bool built_in_function_p(Ref object){
    return class_of(object)==BUILT_IN_FUNCTION_Class;}

Ref function_name(Ref funclo){
    if(closurep(funclo)){
        return function_name(funclo->slots->closure.function);}
    check_type(funclo,FUNCTION_Class);
    return funclo->slots->function.lisp_name;}

void values(Ref arguments,MultipleValues* results){
    /* Pre-fix this loop never advanced `arguments` (missing
       arguments = cdr(arguments)), so (values 10 20) produced
       MULTIPLE_VALUES_LIMIT copies of 10 and blew up M-V-CALL. */
    results->count=0;
    while(consp(arguments) && (results->count<MULTIPLE_VALUES_LIMIT)){
        results->values[results->count++]=car(arguments);
        arguments=cdr(arguments);}}

void kernel_apply(Ref function_or_closure,Ref arguments,MultipleValues* results){
    /* CL function designator: a symbol names the function in its function cell. */
    if(symbolp(function_or_closure) && symbol_fboundp(function_or_closure)){
        function_or_closure=symbol_function(function_or_closure);}
    if(closurep(function_or_closure)){
        kernel_apply_closure(function_or_closure,&function_or_closure->slots->closure,arguments,results);}
    else if(built_in_function_p(function_or_closure)){
        kernel_apply_built_in(function_or_closure,&function_or_closure->slots->built_in_function,arguments,results);}
    else if(functionp(function_or_closure)){
        kernel_apply_function(function_or_closure,&function_or_closure->slots->function,arguments,results);}
    else{
        TYPE_ERROR(function_or_closure,FUNCTION_Class);}}

/* kernel_apply_closure and kernel_apply_function are implemented in eval.c,
   where the frame machinery (push_frame / UNWIND) lives. */

void kernel_apply_built_in(Ref function,BuiltInFunction* slots,Ref arguments,MultipleValues* results){
    slots->kernel_function(function,arguments,results);}


/* CONDITION */
/* STANDARD-OBJECT */


void check_no_more_arguments(Ref function,Ref arguments){
    if(nullp(arguments)){
        return;}
    /* function_name returns the SYMBOL naming the function; pass its name
       string into string_cstring, not the symbol itself. (Pre-fix, this
       crashed with "got SYMBOL, expected STRING".) */
    char* fname=string_cstring(symbol_name(function_name(function)));
    char* args=string_cstring(prin1_to_string(arguments));
    error(fname,"Too many arguments, unused: %s",args);
    /* error() is _Noreturn, so the frees below are unreachable. Kept as a
       note that the strdup'd buffers are leaked when we exit on error. */
    free(fname);free(args);}

Ref pop_mandatory_argument(Ref function,Ref* arguments){
    if(nullp(*arguments)){
        char* fname=string_cstring(symbol_name(function_name(function)));
        error(fname,"Missing mandatory argument");
        free(fname);}
    return pop(arguments);}

Ref pop_optional_argument(unused Ref function,Ref* arguments,Ref default_value){
    return nullp(*arguments)
            ? default_value
            : pop(arguments);}

Ref pop_key_argument(Ref function,Ref* arguments,Ref keyword,Ref default_value){
    Ref current=(*arguments);
    while(consp(current) && !eql(keyword,car(current))){
        if(consp(cdr(current))){
            current=cddr(current);}
        else{
            char* fname=string_cstring(function_name(function));
            char* args=string_cstring(prin1_to_string(*arguments));
            char* mkey=string_cstring(prin1_to_string(keyword));
            error(fname,"Odd number of &key arguments %s, missing value for %s",
                  args,mkey);
            /* TODO: clear up this mess, error doesn't return! */
            free(fname);free(args);free(mkey);}}
    return consp(current)
            ? cadr(current)
            : default_value;}


static Ref rootset;
static Ref kernel_initialize_rootset(void){
    extern Ref all_packages;   /* the package registry (sentinel header, packages.c) */
    rootset=NIL_Symbol;
    push(UNBOUND_Object,&rootset);
    push(KERNEL_obarray,&rootset);
    push(KEYWORD_obarray,&rootset);
    push(kernel_classes,&rootset);
    push(characterset,&rootset);
    push(all_packages,&rootset);
    return rootset;}

Ref kernel_rootset(void){
    return rootset;}

/* The GC marks memory.c's RootSet->kernel_rootset, a snapshot installed once by
   set_rootset() at init.  push/delete here mutate the `rootset` LIST HEAD, so
   the snapshot must be re-installed after every change — otherwise objects
   added post-init (notably the global environment, pushed on first eval) are
   invisible to the collector and get swept while still in use.  This manifested
   as heap-dependent corruption: the global env collected and its memory reused
   (e.g. environment_find_function dereferencing a string). */
void push_to_rootset(Ref object){
    rootset=cons(object,rootset);
    set_rootset((Block*)rootset);}

void delete_from_rootset(Ref object){
    rootset=delete(object,rootset);
    set_rootset((Block*)rootset);}


static void kernel_LISP_IMPLEMENTATION_TYPE(unused Ref function,Ref arguments,MultipleValues* results){
    check_no_more_arguments(function,arguments);
    results->count=1;
    results->values[0]=T("BOCL");}

static void kernel_LISP_IMPLEMENTATION_VERSION(unused Ref function,Ref arguments,MultipleValues* results){
    check_no_more_arguments(function,arguments);
    results->count=1;
    results->values[0]=T(BOCL_VERSION);}

static void kernel_initialize_features(void){
    Ref features=S(*FEATURES*);
    Ref list=NIL_Symbol;

    push(K(COMMON-LISP),&list);
    push(K(BOCL),&list);
    push(K(BOCL-1.0),&list);

#ifdef _POSIX_V6_LP64_OFF64
    push(K(64-BIT-TARGET),&list);
    push(K(64-BIT-HOST),&list);
#else
    push(K(32-BIT-TARGET),&list);
    push(K(32-BIT-HOST),&list);
#endif

    define_parameter(features,list);

    define_constant(S(ARRAY-TOTAL-SIZE-LIMIT),  I(ARRAY_TOTAL_SIZE_LIMIT));
    define_constant(S(ARRAY-DIMENSION-LIMIT),   I(ARRAY_DIMENSION_LIMIT));
    define_constant(S(ARRAY-RANK-LIMIT),        I(ARRAY_RANK_LIMIT));
    define_constant(S(CALL-ARGUMENTS-LIMIT),    I(CALL_ARGUMENTS_LIMIT));
    define_constant(S(CHAR-CODE-LIMIT),         I(CHAR_CODE_LIMIT));
    define_constant(S(LAMBDA-PARAMETERS-LIMIT), I(LAMBDA_PARAMETERS_LIMIT));
    define_constant(S(MULTIPLE-VALUES-LIMIT),   I(MULTIPLE_VALUES_LIMIT));

    /* The kernel has no fixnum/bignum distinction yet — every integer is a
       word. most-{positive,negative}-fixnum therefore reflect the full word
       range. Bignum support lives on the Lisp side ([bootstrap/bignum.lisp])
       and will narrow MOST-POSITIVE-FIXNUM once it is loaded. */
    define_constant(S(MOST-POSITIVE-FIXNUM),    I(WORD_MAX));
    define_constant(S(MOST-NEGATIVE-FIXNUM),    I(WORD_MIN));

    {   Ref fname=S(LISP-IMPLEMENTATION-TYPE);
        set_symbol_function(fname,built_in_function_new(fname,NIL_Symbol,
                                                        kernel_LISP_IMPLEMENTATION_TYPE)); }
    {   Ref fname=S(LISP-IMPLEMENTATION-VERSION);
        set_symbol_function(fname,built_in_function_new(fname,NIL_Symbol,
                                                        kernel_LISP_IMPLEMENTATION_VERSION)); }

    define_variable(S(*LOADING-FASL-FILE*),     S(NIL));

    /* LOAD honours *LOAD-VERBOSE* and *LOAD-PRINT*; initialise them so the
       kernel can call bocl_load_cstring without main.c having run first
       (e.g. from a test harness). */
    define_parameter(S(*LOAD-VERBOSE*),          S(NIL));
    define_parameter(S(*LOAD-PRINT*),            S(NIL));
    define_parameter(S(*COMPILE-VERBOSE*),       S(NIL));
    define_parameter(S(*COMPILE-PRINT*),         S(NIL));

    /* These are referenced by EVAL-WHEN dispatch in eval.c and must be
       bound before any EVAL-WHEN form is processed (including in user files
       loaded via COMPILE-FILE). The Lisp prelude re-defvar's them to the
       same NIL value, which is a no-op thanks to define_variable semantics. */
    define_variable(S(*LOAD-PATHNAME*),          NIL_Symbol);
    define_variable(S(*LOAD-TRUENAME*),          NIL_Symbol);
    define_variable(S(*COMPILE-FILE-PATHNAME*),  NIL_Symbol);
    define_variable(S(*COMPILE-FILE-TRUENAME*),  NIL_Symbol);
}

/* Debug aid: when BOCL_BACKTRACE is set, dump a native backtrace on a fatal
   signal (so heap-layout-dependent crashes can be located without a debugger,
   which perturbs the layout and hides them). */
/* BOCL:*ABORT-HOOK* — best effort, called from the crash handler with the GC
   disabled.  We only enter Lisp if a hook is actually registered (cheap symbol
   check, no allocation otherwise), to keep ordinary crashes unperturbed.  The
   Lisp runner wraps each hook in IGNORE-ERRORS. */
static void bocl_run_abort_hooks(int sig){
    extern Ref BOCL_Package;
    if(BOCL_Package==NULL || !typep(BOCL_Package,PACKAGE_Class)){ return; }
    Ref present=BOCL_Package->slots->package.present;
    Ref hooks_sym=find_symbol(string_new_cstring("*ABORT-HOOK*"),present);
    if(!hooks_sym || nullp(symbol_value(hooks_sym))){ return; }
    Ref fn_sym=find_symbol(string_new_cstring("%RUN-ABORT-HOOKS"),present);
    if(!fn_sym || !symbol_fboundp(fn_sym)){ return; }
    MultipleValues mv;
    kernel_apply(symbol_function(fn_sym),cons(integer_from_word(sig),NIL_Symbol),&mv);}

static void bocl_crash_handler(int sig, siginfo_t* info, void* ucontext){
    (void)ucontext;
    /* Re-entrancy guard: if running the hooks itself faults, don't loop —
       fall straight through to the default action on the second entry. */
    static volatile sig_atomic_t in_handler = 0;
    if(!in_handler){
        in_handler = 1;
        /* The system may be corrupt: stop the GC and do minimal work. */
        set_garbage_collection_enabled(false);
        /* Dump the native backtrace FIRST — before the Lisp abort-hooks, which
           allocate / walk the (possibly corrupt) heap and can re-fault, costing us
           the backtrace via the re-entrancy guard. */
        if(getenv("BOCL_BACKTRACE")){
            char buf[128];
            int len = snprintf(buf,sizeof(buf),
                               "\n=== bocl: fatal signal %d; fault address = %p ===\n",
                               sig, info ? info->si_addr : (void*)0);
            write(2, buf, (size_t)len);
            void* bt[64];
            int n = backtrace(bt, 64);
            backtrace_symbols_fd(bt, n, 2);}
        bocl_run_abort_hooks(sig);}
    signal(sig, SIG_DFL);
    raise(sig);}

/* BOCL:*STOP-HOOK* / *CONT-HOOK* — best effort, run from the SIGTSTP / SIGCONT
   handlers.  Unlike the abort hooks the process is NOT dying, so the GC is left
   enabled; the hooks should keep to async-safe work (terminal reset, reopen a
   connection, …).  We only enter Lisp when a hook is actually registered (cheap
   symbol check, no allocation otherwise), so default Ctrl-Z / resume behaviour is
   unchanged when no hook is set.  The Lisp runner wraps each hook in
   IGNORE-ERRORS. */
static void bocl_run_nullary_hooks(const char* hook_var, const char* runner){
    extern Ref BOCL_Package;
    if(BOCL_Package==NULL || !typep(BOCL_Package,PACKAGE_Class)){ return; }
    Ref present=BOCL_Package->slots->package.present;
    Ref hooks_sym=find_symbol(string_new_cstring(hook_var),present);
    if(!hooks_sym || nullp(symbol_value(hooks_sym))){ return; }
    Ref fn_sym=find_symbol(string_new_cstring(runner),present);
    if(!fn_sym || !symbol_fboundp(fn_sym)){ return; }
    MultipleValues mv;
    kernel_apply(symbol_function(fn_sym),NIL_Symbol,&mv);}

static void bocl_install_tstp_handler(void);

/* SIGTSTP (Ctrl-Z): run *STOP-HOOK*, then actually suspend.  SIGTSTP is blocked
   inside its own handler by default, so to make the stop take effect now we
   restore the default disposition, unblock it and re-raise; execution resumes
   here when the process is continued, where we re-arm this handler. */
static void bocl_tstp_handler(int sig){
    (void)sig;
    static volatile sig_atomic_t in_stop = 0;
    if(!in_stop){
        in_stop = 1;
        bocl_run_nullary_hooks("*STOP-HOOK*","%RUN-STOP-HOOKS");
        in_stop = 0;}
    signal(SIGTSTP, SIG_DFL);
    {   sigset_t m; sigemptyset(&m); sigaddset(&m, SIGTSTP);
        sigprocmask(SIG_UNBLOCK, &m, NULL);}
    raise(SIGTSTP);                 /* <-- the process suspends here */
    /* ---- resumed by SIGCONT ---- */
    bocl_install_tstp_handler();}   /* re-arm for the next Ctrl-Z */

/* SIGCONT: the process is resuming — run *CONT-HOOK*. */
static void bocl_cont_handler(int sig){
    (void)sig;
    static volatile sig_atomic_t in_cont = 0;
    if(in_cont){ return; }
    in_cont = 1;
    bocl_run_nullary_hooks("*CONT-HOOK*","%RUN-CONT-HOOKS");
    in_cont = 0;}

static void bocl_install_tstp_handler(void){
    struct sigaction sa;
    memset(&sa,0,sizeof(sa));
    sa.sa_handler = bocl_tstp_handler;
    sa.sa_flags = SA_RESTART;       /* resume a syscall interrupted by Ctrl-Z */
    sigaction(SIGTSTP,&sa,NULL);}

void kernel_initialize(void){
    /* Install the crash handler so BOCL:*ABORT-HOOK* runs when the process is
       terminating on a signal — both faults (SEGV/BUS/ABRT) and termination
       requests (TERM/INT/HUP).  The native backtrace dump stays gated on
       BOCL_BACKTRACE.  The handler re-raises with the default action so the
       process still dies as it would have. */
    {   struct sigaction sa;
        memset(&sa,0,sizeof(sa));
        sa.sa_sigaction = bocl_crash_handler;
        sa.sa_flags = SA_SIGINFO;
        sigaction(SIGSEGV,&sa,NULL);
        sigaction(SIGBUS, &sa,NULL);
        sigaction(SIGABRT,&sa,NULL);
        sigaction(SIGTERM,&sa,NULL);
        sigaction(SIGINT, &sa,NULL);
        sigaction(SIGHUP, &sa,NULL);}
    /* Job-control hooks (issue deferred-signal-hooks): run BOCL:*STOP-HOOK*
       before suspending on SIGTSTP (Ctrl-Z) and BOCL:*CONT-HOOK* on resume.
       These leave default suspend/resume behaviour intact when no hook is set. */
    bocl_install_tstp_handler();
    {   struct sigaction sa;
        memset(&sa,0,sizeof(sa));
        sa.sa_handler = bocl_cont_handler;
        sa.sa_flags = SA_RESTART;
        sigaction(SIGCONT,&sa,NULL);}
    memory_initialize();
    set_garbage_collection_enabled(false);

    kernel_objects_initialize();
    kernel_initialize_built_in_functions();
    kernel_initialize_arithmetic();
    kernel_initialize_integers();
    kernel_initialize_bignum();
    kernel_initialize_characters();
    kernel_initialize_features();

    structure_initialize(); /* before: */
    kernel_initialize_structure_functions();

    reader_initialize();
    stream_initialize();
    kernel_initialize_packages();
    kernel_initialize_hash_tables();
    kernel_initialize_conditions();
    kernel_initialize_sequences();
    bocl_register_load();

    kernel_initialize_rootset();
    set_rootset((Block*)kernel_rootset());
    set_garbage_collection_enabled(true);}



/**** THE END ****/
