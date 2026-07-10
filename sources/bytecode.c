/* bytecode.c — byte-code VM compiled-function object (issue vm-1).
   vm-1 is DESIGN + the runtime object only: the predicates, the constructor and
   the accessors for CompiledFunction / CompiledClosure (see bytecode.h for the
   ISA).  The VM execution loop is vm-2; until then bytecode_apply() signals a
   clear error rather than mis-dispatching. */

#include "bytecode.h"
#include "kernel.h"
#include "kernel_types.h"
#include "kernel_objects.h"
#include "kernel_functions.h"
#include "memory.h"
#include "boerror.h"

extern Ref Object_new(Ref class, Slots* slots);   /* defined in kernel.c (as in reader.c) */

int compiled_function_p(Ref object){ return class_of(object)==COMPILED_FUNCTION_Class; }
int compiled_closure_p(Ref object){  return class_of(object)==COMPILED_CLOSURE_Class; }

/* ---- CompiledFunction template ---------------------------------------------
   All eight slots are Refs, so objcount=8 and the GC traces them generically
   (same discipline as Function_slots).  Inputs are rooted across allocate(). */
static CompiledFunction* CompiledFunction_slots(Ref name, Ref lambda_list, Ref code,
                                                Ref constants, Ref arity, Ref max_stack,
                                                Ref n_locals, Ref source){
    uword pool=open_live_pool();
    add_to_live_pool(name);       add_to_live_pool(lambda_list); add_to_live_pool(code);
    add_to_live_pool(constants);  add_to_live_pool(arity);       add_to_live_pool(max_stack);
    add_to_live_pool(n_locals);   add_to_live_pool(source);
    CompiledFunction* that=allocate(sizeof(*that));
    close_live_pool(pool);
    that->objcount    = 8;
    that->lisp_name   = name;
    that->lambda_list = lambda_list;
    that->code        = code;
    that->constants   = constants;
    that->arity       = arity;
    that->max_stack   = max_stack;
    that->n_locals    = n_locals;
    that->source      = source;
    return that;}

Ref compiled_function_new(Ref name, Ref lambda_list, Ref code, Ref constants,
                          Ref arity, Ref max_stack, Ref n_locals, Ref source){
    return Object_new(COMPILED_FUNCTION_Class,
                      (Slots*)CompiledFunction_slots(name,lambda_list,code,constants,
                                                     arity,max_stack,n_locals,source));}

static CompiledClosure* CompiledClosure_slots(Ref function, Ref upvalues){
    uword pool=open_live_pool();
    add_to_live_pool(function);
    add_to_live_pool(upvalues);
    CompiledClosure* that=allocate(sizeof(*that));
    close_live_pool(pool);
    that->objcount = 2;
    that->function = function;
    that->upvalues = upvalues;
    return that;}

Ref compiled_closure_new(Ref function, Ref upvalues){
    return Object_new(COMPILED_CLOSURE_Class,(Slots*)CompiledClosure_slots(function,upvalues));}

/* ---- Accessors (checked) ---------------------------------------------------- */
Ref compiled_function_name(Ref cf){        check_type(cf,COMPILED_FUNCTION_Class); return cf->slots->compiled_function.lisp_name;}
Ref compiled_function_lambda_list(Ref cf){ check_type(cf,COMPILED_FUNCTION_Class); return cf->slots->compiled_function.lambda_list;}
Ref compiled_function_code(Ref cf){        check_type(cf,COMPILED_FUNCTION_Class); return cf->slots->compiled_function.code;}
Ref compiled_function_constants(Ref cf){   check_type(cf,COMPILED_FUNCTION_Class); return cf->slots->compiled_function.constants;}
Ref compiled_function_arity(Ref cf){       check_type(cf,COMPILED_FUNCTION_Class); return cf->slots->compiled_function.arity;}
Ref compiled_function_max_stack(Ref cf){   check_type(cf,COMPILED_FUNCTION_Class); return cf->slots->compiled_function.max_stack;}
Ref compiled_function_n_locals(Ref cf){    check_type(cf,COMPILED_FUNCTION_Class); return cf->slots->compiled_function.n_locals;}
Ref compiled_function_source(Ref cf){      check_type(cf,COMPILED_FUNCTION_Class); return cf->slots->compiled_function.source;}
Ref compiled_closure_function(Ref cc){     check_type(cc,COMPILED_CLOSURE_Class);  return cc->slots->compiled_closure.function;}
Ref compiled_closure_upvalues(Ref cc){     check_type(cc,COMPILED_CLOSURE_Class);  return cc->slots->compiled_closure.upvalues;}

/* ---- APPLY hook ------------------------------------------------------------
   vm-2 replaces this with the real VM loop; for vm-1 a compiled function exists
   and is FUNCTIONP, but running one signals a clear error. */
void bytecode_apply(Ref function, Ref arguments, MultipleValues* results){
    (void)function; (void)arguments; (void)results;
    ERROR("byte-code execution is not available yet (needs vm-2 VM loop)%s","");}
