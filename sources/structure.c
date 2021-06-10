#include "structure.h"
#include "boerror.h"


Ref structure_class_instance_size(Ref structure_class){
    WITH_LIVE_POOL;
    /*
    Return the total number of slots in instances of the structure_class.
    */
    check_class(structure_class,STRUCTURE_CLASS_Class);
    LP_RETURN1(integer_from_uword(structure_class->slots->structure_class.nslots));}


Ref structure_class_instance_direct_slots(Ref structure_class){
    /*
    Return the list of direct slots of the structure_class.
    */
    check_class(structure_class,STRUCTURE_CLASS_Class);
    return structure_class->slots->structure_class.slot_names;}


Ref structure_class_instance_slots(Ref structure_class){
    WITH_LIVE_POOL;
    /*
    Return the list of all the slots of the structure_class (including the
    slots defined in the superclasses).
    */
    check_class(structure_class,STRUCTURE_CLASS_Class);
    if(structure_class==STRUCTURE_OBJECT_Class){
        LP_RETURN1(NIL_Symbol);}
    LP_RETURN1(nconc(structure_class_instance_slots(car(structure_class->slots->structure_class.direct_superclasses)),
                     copy_list(structure_class->slots->structure_class.slot_names)));}



bool structurep(Ref object){
    /* Whether the object is an instance of structure-object or a subclass */
    /* typep(structure_instance,structure_class) can be used to test a specific structure-class. */
    return typep(object,STRUCTURE_OBJECT_Class);}


static uword structure_index(Ref structure_instance,Ref index){
    if(symbolp(index)){
        Ref pos=list_position(index,structure_class_instance_slots(structure_instance->class));
        if(nullp(pos)){
            ERROR("Invalid slot name %s",string_cstring(symbol_name(index)));}
        index=pos;}
    else{
        check_class(index,INTEGER_Class);}
    uword i=CHECK_WORD_TO_UWORD(integer_value(index));
    if(structure_instance->slots->vector_of_object.objcount<=i){
        OUT_OF_BOUNDS(structure_instance,
                      structure_class_instance_size(structure_instance),
                      index);}
    return i;}
    

Ref structure_get(Ref structure_instance,Ref index){
    WITH_LIVE_POOL;
    /*
    index can be either an integer indexing the slots, or a symbol naming the wanted slot.
    return the slot value of the indexed slot.
    May signal an out-of-bound error.
    */
    LP_RETURN1(structure_instance->slots->vector_of_object.elements[structure_index(structure_instance,index)]);}


Ref structure_set(Ref new_value,Ref structure_instance,Ref index){
    WITH_LIVE_POOL;
    /*
    index can be either an integer indexing the slots, or a symbol naming the wanted slot.
    sets the slot value of the indexed slot, 
    return the new-value.
    May signal an out-of-bound error.
    */
    structure_instance->slots->vector_of_object.elements[structure_index(structure_instance,index)]=new_value;
    LP_RETURN1(new_value);}


void structure_initialize(void){
    /* Finalize the initialization of STRUCTURE-OBJECT */
    StructureClass* class=&(STRUCTURE_OBJECT_Class->slots->structure_class);
    class->objcount=3;
    class->slot_names=NIL_Symbol;
    class->nslots=0;}

                                
/**** THE END ****/
