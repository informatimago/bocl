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
    */
    check_class(structure_class,STRUCTURE_CLASS_Class);
    Ref superclass = car(structure_class->slots->structure_class.direct_superclasses);
    uword offset = superclass->slots->structure_class.nslots
            +(uword)integer_value(structure_class->slots->structure_class.offset);
    Ref alist = NIL_Symbol;
    dolist(sname,structure_class->slots->structure_class.slot_names){
        push(cons(sname,integer_from_uword(offset)),&alist);
        offset++;}
    close_live_pool(pool);
    return alist;}


Ref structure_class_instance_slots(Ref structure_class){
    uword pool=open_live_pool();
    /*
    Return an a-list of (slot-name . index) for all the slots of the
    structure_class (including the slots defined in the superclasses).
    */
    check_class(structure_class,STRUCTURE_CLASS_Class);
    if(structure_class==STRUCTURE_OBJECT_Class){
        close_live_pool(pool);
        return NIL_Symbol;}
    Ref superclass = car(structure_class->slots->structure_class.direct_superclasses);
    Ref result=nconc(structure_class_instance_slots(superclass),
                     structure_class_instance_direct_slots(structure_class));
    close_live_pool(pool);
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
