#ifndef structure_h
#define structure_h
#include "kernel.h"

/* Note: those declarations are redundent with those generated in
kernel_functions.h, but we have docstring comments here. ;-) */

Ref make_structure_class(Ref name,Ref direct_superclass,Ref offset,Ref slots);
/* 
Creates a class that has structure-class metaclass,
direct_superclass or structure-object as superclass.

SLOTS is a list of symbols naming the additionnal slots to the new
structure class.

(An instance will have the concatenation of all the slots lists of all
the superclasses of its structure-class).
*/

Ref structure_class_instance_size(Ref structure_class);
/*
Return the total number of slots in instances of the structure_class.
*/

Ref structure_class_instance_direct_slots(Ref structure_class);
/*
Return an a-list of direct slots to index for the slots added by the structure_class.
*/

Ref structure_class_instance_slots(Ref structure_class);
/*
Return an a-list of all the slots to index for the instances of the
structure_class (including the slots defined in the superclasses).
*/

Ref make_structure_instance(Ref structure_class);
/*
Creates an instance of the structure_class. 
All slots are initialized to NIL.
*/

bool structurep(Ref object);
/* Whether the object is an instance of structure-object or a subclass */

/* typep(structure_instance,structure_class) */

Ref structure_get(Ref structure_instance,Ref index);
/*
index can be either an integer indexing the slots, or a symbol naming the wanted slot.
return the slot value of the indexed slot.
May signal an out-of-bound error.
*/

Ref structure_set(Ref new_value,Ref structure_instance,Ref index);
/*
index can be either an integer indexing the slots, or a symbol naming the wanted slot.
sets the slot value of the indexed slot, 
return the new-value.
May signal an out-of-bound error.
*/


void structure_initialize(void);

#endif
