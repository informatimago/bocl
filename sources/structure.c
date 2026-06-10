#include "structure.h"
#include "boerror.h"


Ref structure_class_instance_size(Ref structure_class){
    uword pool=open_live_pool();
    /*
    Return the total number of slots in instances of the structure_class.
    */
    check_class(structure_class,STRUCTURE_CLASS_Class);
    Ref result=integer_from_uword(structure_class->slots->structure_class.nslots);
    close_live_pool(pool);
    return result;}


Ref structure_class_instance_direct_slots(Ref structure_class){
    uword pool=open_live_pool();
    add_to_live_pool(structure_class);
    /*
    Return an a-list of (slot-name . index) for the direct slots of the structure_class.
    NOTE: do NOT close the pool before returning; the caller must keep result live.
    */
    check_class(structure_class,STRUCTURE_CLASS_Class);
    Ref superclass = car(structure_class->slots->structure_class.direct_superclasses);
    uword offset = superclass->slots->structure_class.nslots
            +(uword)integer_value(structure_class->slots->structure_class.offset);
    Ref alist = NIL_Symbol;
    dolist(sname,structure_class->slots->structure_class.slot_names){
        /* Do NOT use push() here — push() closes the live pool, which would
           remove the growing alist from GC protection.  A direct cons() call
           leaves the result in the pool so GC cannot collect it between
           iterations (the previous alist value is reachable through the new
           pair's CDR slot and is therefore protected transitively).          */
        alist=cons(cons(sname,integer_from_uword(offset)),alist);
        offset++;}
    (void)pool; /* pool intentionally left open — caller closes it after use */
    return alist;}


/* Return an a-list of (slot-name . index) for ALL slots of STRUCTURE_CLASS
   (including inherited ones).

   MEMOIZED: this used to cons a fresh a-list on EVERY call — and it is called on
   every variable reference (env_lexical_variable_name/_value etc.) — which was
   both a severe perf cost and a GC hazard (an allocation deep in the hot
   variable-lookup path).  The a-list is constant for a given class object, so we
   cache it in the class's GC-marked `cached_slots` slot.  A class redefinition
   allocates a fresh StructureClass (cached_slots == NULL), so the cache is
   automatically rebuilt; the cached list lives and dies with its class. */
Ref structure_class_instance_slots(Ref structure_class){
    check_class(structure_class,STRUCTURE_CLASS_Class);
    if(structure_class==STRUCTURE_OBJECT_Class){
        return NIL_Symbol;}
    Ref cached=structure_class->slots->structure_class.cached_slots;
    if(cached!=NULL){
        return cached;}
    uword pool=open_live_pool();
    add_to_live_pool(structure_class);
    Ref superclass = car(structure_class->slots->structure_class.direct_superclasses);
    /* copy_list the (memoized) superclass a-list before nconc: nconc is
       destructive and must not splice onto — and so corrupt — the cache. */
    Ref result=nconc(copy_list(structure_class_instance_slots(superclass)),
                     structure_class_instance_direct_slots(structure_class));
    structure_class->slots->structure_class.cached_slots=result;
    close_live_pool(pool);   /* result now rooted via the class's cached_slots */
    return result;}


bool structurep(Ref object){
    /* Whether the object is an instance of structure-object or a subclass */
    /* typep(structure_instance,structure_class) can be used to test a specific structure-class. */
    return typep(object,STRUCTURE_OBJECT_Class);}


static uword structure_index(Ref structure_instance,Ref index){
    if(symbolp(index)){
        Ref entry=assq(index,structure_class_instance_slots(structure_instance->class));
        if(nullp(entry)){
            ERROR("Invalid slot name %s",string_cstring(symbol_name(index)));}
        index=cdr(entry);}
    else{
        check_class(index,INTEGER_Class);}
    uword i=CHECK_WORD_TO_UWORD(integer_value(index));
    if(structure_instance->slots->vector_of_object.objcount<=i){
        OUT_OF_BOUNDS(structure_instance,
                      structure_class_instance_size(structure_instance),
                      index);}
    return i;}
    

Ref structure_get(Ref structure_instance,Ref index){
    uword pool=open_live_pool();
    add_to_live_pool(structure_instance);
    add_to_live_pool(index);
    /*
    index can be either an integer indexing the slots, or a symbol naming the wanted slot.
    return the slot value of the indexed slot.
    May signal an out-of-bound error.
    */
    Ref result=structure_instance->slots->vector_of_object.elements[structure_index(structure_instance,index)];
    close_live_pool(pool);
    return result;}


Ref structure_set(Ref new_value,Ref structure_instance,Ref index){
    uword pool=open_live_pool();
    add_to_live_pool(new_value);
    add_to_live_pool(structure_instance);
    add_to_live_pool(index);
    /*
    index can be either an integer indexing the slots, or a symbol naming the wanted slot.
    sets the slot value of the indexed slot, 
    return the new-value.
    May signal an out-of-bound error.
    */
    structure_instance->slots->vector_of_object.elements[structure_index(structure_instance,index)]=new_value;
    close_live_pool(pool);
    return new_value;}


void structure_initialize(void){
    /* Finalize the initialization of STRUCTURE-OBJECT */
    StructureClass* class=&(STRUCTURE_OBJECT_Class->slots->structure_class);
    class->objcount=3;
    class->slot_names=NIL_Symbol;
    class->nslots=0;}

                                
/**** THE END ****/
