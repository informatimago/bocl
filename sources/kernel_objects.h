/* Do not edit -- generated from generate-kernel.lisp */
#ifndef kernel_objects_h
#define kernel_objects_h
#include "kernel.h"

extern Object  uninitialized_object;

extern Ref kernel_classes;  /* list of class objects */
extern Ref NIL_Symbol;
extern Ref T_Symbol;
extern Ref BIT_Symbol;
extern Ref KERNEL_Symbol;
extern Ref KEYWORD_Symbol;
extern Ref CHARACTER_Symbol;
extern Ref KERNEL_obarray;  /* list of interned symbols */
extern Ref KEYWORD_obarray; /* list of interned keywords */
extern Ref UNBOUND_Object;

extern Ref BLOCK_Symbol;
extern Ref CATCH_Symbol;
extern Ref EVAL_WHEN_Symbol;
extern Ref FLET_Symbol;
extern Ref FUNCTION_Symbol;
extern Ref GO_Symbol;
extern Ref IF_Symbol;
extern Ref LABELS_Symbol;
extern Ref LET_Symbol;
extern Ref LET__Symbol;
extern Ref LOAD_TIME_VALUE_Symbol;
extern Ref LOCALLY_Symbol;
extern Ref MACROLET_Symbol;
extern Ref MULTIPLE_VALUE_CALL_Symbol;
extern Ref MULTIPLE_VALUE_PROG1_Symbol;
extern Ref PROGN_Symbol;
extern Ref PROGV_Symbol;
extern Ref QUOTE_Symbol;
extern Ref RETURN_FROM_Symbol;
extern Ref SETQ_Symbol;
extern Ref SYMBOL_MACROLET_Symbol;
extern Ref TAGBODY_Symbol;
extern Ref THE_Symbol;
extern Ref THROW_Symbol;
extern Ref UNWIND_PROTECT_Symbol;

extern Ref T_Class;
extern Ref CHARACTER_Class;
extern Ref ARRAY_Class;
extern Ref STRING_Class;
extern Ref VECTOR_Class;
extern Ref BIT_VECTOR_Class;
extern Ref SEQUENCE_Class;
extern Ref CONS_Class;
extern Ref NULL_Class;
extern Ref SYMBOL_Class;
extern Ref ENVIRONMENT_Class;
extern Ref LIST_Class;
extern Ref COMPLEX_Class;
extern Ref NUMBER_Class;
extern Ref FLOAT_Class;
extern Ref REAL_Class;
extern Ref RATIO_Class;
extern Ref RATIONAL_Class;
extern Ref INTEGER_Class;
extern Ref SIGNED_BYTE_Class;
extern Ref BIT_Class;
extern Ref UNSIGNED_BYTE_Class;
extern Ref HASH_TABLE_Class;
extern Ref RESTART_Class;
extern Ref RANDOM_STATE_Class;
extern Ref PACKAGE_Class;
extern Ref READTABLE_Class;
extern Ref LOGICAL_PATHNAME_Class;
extern Ref PHYSICAL_PATHNAME_Class;
extern Ref PATHNAME_Class;
extern Ref FILE_STREAM_Class;
extern Ref STRING_STREAM_Class;
extern Ref TWO_WAY_STREAM_Class;
extern Ref BROADCAST_STREAM_Class;
extern Ref ECHO_STREAM_Class;
extern Ref CONCATENATED_STREAM_Class;
extern Ref SYNONYM_STREAM_Class;
extern Ref FD_STREAM_Class;
extern Ref STREAM_Class;
extern Ref CLOSURE_Class;
extern Ref COMPILED_CLOSURE_Class;
extern Ref FUNCTION_Class;
extern Ref COMPILED_FUNCTION_Class;
extern Ref BUILT_IN_FUNCTION_Class;
extern Ref STANDARD_OBJECT_Class;
extern Ref FUNCALLABLE_STANDARD_OBJECT_Class;
extern Ref STANDARD_GENERIC_FUNCTION_Class;
extern Ref GENERIC_FUNCTION_Class;
extern Ref SLOT_DEFINITION_Class;
extern Ref STANDARD_DIRECT_SLOT_DEFINITION_Class;
extern Ref DIRECT_SLOT_DEFINITION_Class;
extern Ref STANDARD_EFFECTIVE_SLOT_DEFINITION_Class;
extern Ref EFFECTIVE_SLOT_DEFINITION_Class;
extern Ref STANDARD_SLOT_DEFINITION_Class;
extern Ref METHOD_COMBINATION_Class;
extern Ref EQL_SPECIALIZER_Class;
extern Ref SPECIALIZER_Class;
extern Ref FUNCALLABLE_STANDARD_CLASS_Class;
extern Ref STANDARD_CLASS_Class;
extern Ref BUILT_IN_CLASS_Class;
extern Ref STRUCTURE_CLASS_Class;
extern Ref STRUCTURE_OBJECT_Class;
extern Ref FORWARD_REFERENCED_CLASS_Class;
extern Ref CLASS_Class;
extern Ref METAOBJECT_Class;
extern Ref METHOD_Class;
extern Ref STANDARD_METHOD_Class;
extern Ref STANDARD_WRITER_METHOD_Class;
extern Ref STANDARD_READER_METHOD_Class;
extern Ref STANDARD_ACCESSOR_METHOD_Class;
extern Ref STYLE_WARNING_Class;
extern Ref SIMPLE_WARNING_Class;
extern Ref WARNING_Class;
extern Ref CONDITION_Class;
extern Ref STORAGE_CONDITION_Class;
extern Ref SERIOUS_CONDITION_Class;
extern Ref SIMPLE_ERROR_Class;
extern Ref SIMPLE_TYPE_ERROR_Class;
extern Ref SIMPLE_CONDITION_Class;
extern Ref TYPE_ERROR_Class;
extern Ref CONTROL_ERROR_Class;
extern Ref PARSE_ERROR_Class;
extern Ref READER_ERROR_Class;
extern Ref END_OF_FILE_Class;
extern Ref STREAM_ERROR_Class;
extern Ref FILE_ERROR_Class;
extern Ref PRINT_NOT_READABLE_Class;
extern Ref DIVISION_BY_ZERO_Class;
extern Ref FLOATING_POINT_UNDERFLOW_Class;
extern Ref FLOATING_POINT_INVALID_OPERATION_Class;
extern Ref FLOATING_POINT_INEXACT_Class;
extern Ref FLOATING_POINT_OVERFLOW_Class;
extern Ref ARITHMETIC_ERROR_Class;
extern Ref PACKAGE_ERROR_Class;
extern Ref PROGRAM_ERROR_Class;
extern Ref ERROR_Class;
extern Ref UNBOUND_VARIABLE_Class;
extern Ref UNBOUND_SLOT_Class;
extern Ref UNDEFINED_FUNCTION_Class;
extern Ref CELL_ERROR_Class;

void kernel_objects_initialize(void);

#endif
