/* tests/test_clos.c — CUnit tests for Slice 19: Minimal CLOS
   Tests:  DEFCLASS, MAKE-INSTANCE, SLOT-VALUE / (SETF SLOT-VALUE),
           SLOT-BOUNDP, SLOT-MAKUNBOUND, DEFGENERIC, DEFMETHOD,
           CALL-NEXT-METHOD, NEXT-METHOD-P, CLASS-OF, FIND-CLASS,
           TYPEP (CLOS), WITH-SLOTS, PRINT-OBJECT. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_test.h"
#include "kernel.h"
#include "packages.h"
#include "bocl.h"
#include "macros.h"

TEST_SUITE_SETUP(clos_init){
    kernel_initialize();
    bocl_test_load_prelude();
    bocl_in_package(COMMON_LISP_Package);   /* bocl-internal names live in CL */
    return CUE_SUCCESS;}

TEST_SUITE_TEARDOWN(clos_clean){
    return CUE_SUCCESS;}

static Ref e(const char* text){ return bocl_test_eval(text); }

/* ---- 1. Basic DEFCLASS -------------------------------------------------- */

static void test_defclass_basic(void){
    e("(defclass point () ((x :initarg :x :accessor point-x :initform 0)"
      "                    (y :initarg :y :accessor point-y :initform 0)))");
    TEST_EQ(eql, T_Symbol,   e("(typep (make-instance 'point) 'point)"));
    TEST_EQ(eql, NIL_Symbol, e("(typep 42 'point)"));}

/* ---- 2. Slot accessors --------------------------------------------------- */

static void test_accessors(void){
    e("(defclass point2 () ((x :initarg :x :accessor p2-x)"
      "                     (y :initarg :y :accessor p2-y)))");
    TEST_EQ(eql, I(3), e("(p2-x (make-instance 'point2 :x 3 :y 4))"));
    TEST_EQ(eql, I(4), e("(p2-y (make-instance 'point2 :x 3 :y 4))"));}

/* ---- 3. :initarg and :initform ------------------------------------------ */

static void test_initarg_initform(void){
    e("(defclass rect () ((w :initarg :w :accessor rect-w :initform 100)"
      "                   (h :initarg :h :accessor rect-h :initform 50)))");
    TEST_EQ(eql, I(100), e("(rect-w (make-instance 'rect))"));
    TEST_EQ(eql, I(50),  e("(rect-h (make-instance 'rect))"));
    TEST_EQ(eql, I(30),  e("(rect-w (make-instance 'rect :w 30))")  );}

/* ---- 4. Unbound slot error ----------------------------------------------- */

static void test_unbound_slot(void){
    e("(defclass cell () ((val :initarg :val)))");
    /* unbound slot signals an error, caught by handler-case */
    TEST_EQ(eql, T_Symbol,
            e("(handler-case (slot-value (make-instance 'cell) 'val)"
              "  (error (e) t))"));}

/* ---- 5. (SETF SLOT-VALUE) ----------------------------------------------- */

static void test_setf_slot_value(void){
    e("(defclass node () ((data :initarg :data :accessor node-data)))");
    TEST_EQ(eql, I(99),
            e("(let ((n (make-instance 'node :data 1)))"
              "  (setf (slot-value n 'data) 99)"
              "  (slot-value n 'data))"));}

/* ---- 6. SLOT-BOUNDP and SLOT-MAKUNBOUND ---------------------------------- */

static void test_slot_boundp(void){
    e("(defclass thing () ((name :initarg :name)))");
    TEST_EQ(eql, NIL_Symbol, e("(slot-boundp (make-instance 'thing) 'name)"));
    TEST_EQ(eql, T_Symbol,   e("(slot-boundp (make-instance 'thing :name \"x\") 'name)"));
    TEST_EQ(eql, NIL_Symbol,
            e("(let ((obj (make-instance 'thing :name \"x\")))"
              "  (slot-makunbound obj 'name)"
              "  (slot-boundp obj 'name))"));}

/* ---- 7. CLASS-OF --------------------------------------------------------- */

static void test_class_of(void){
    e("(defclass foo () ())");
    TEST_EQ(eql, T_Symbol,
            e("(let ((obj (make-instance 'foo)))"
              "  (eq (class-of obj) (find-class 'foo)))")); }

/* ---- 8. FIND-CLASS ------------------------------------------------------- */

static void test_find_class(void){
    e("(defclass bar () ())");
    TEST_EQ(eql, T_Symbol,  e("(standard-class-p (find-class 'bar))"));
    TEST_EQ(eql, NIL_Symbol, e("(find-class 'bar-does-not-exist nil)"));}

/* ---- 9. Slot inheritance ------------------------------------------------- */

static void test_inheritance(void){
    e("(defclass animal () ((name :initarg :name :accessor animal-name)))");
    e("(defclass dog (animal) ((breed :initarg :breed :accessor dog-breed)))");
    TEST_EQ(equal, T("Rex"),
            e("(animal-name (make-instance 'dog :name \"Rex\" :breed \"Lab\"))"));
    TEST_EQ(equal, T("Lab"),
            e("(dog-breed (make-instance 'dog :name \"Rex\" :breed \"Lab\"))"));}

/* ---- 10. TYPEP with CLOS class ------------------------------------------ */

static void test_typep_clos(void){
    e("(defclass vehicle () ())");
    e("(defclass car (vehicle) ())");
    TEST_EQ(eql, T_Symbol,   e("(typep (make-instance 'car) 'car)"));
    TEST_EQ(eql, T_Symbol,   e("(typep (make-instance 'car) 'vehicle)"));
    TEST_EQ(eql, NIL_Symbol, e("(typep (make-instance 'vehicle) 'car)"));}

/* ---- 11. DEFGENERIC + DEFMETHOD dispatch -------------------------------- */

static void test_dispatch(void){
    e("(defclass fruit () ())");
    e("(defclass apple (fruit) ())");
    e("(defgeneric describe-it (x))");
    e("(defmethod describe-it ((x fruit)) 'fruit)");
    e("(defmethod describe-it ((x apple)) 'apple)");
    TEST_EQ(eql, S(APPLE), e("(describe-it (make-instance 'apple))"));
    TEST_EQ(eql, S(FRUIT), e("(describe-it (make-instance 'fruit))"));}

/* ---- 12. Dispatch selectivity ------------------------------------------- */

static void test_dispatch_selectivity(void){
    e("(defclass a () ())");
    e("(defclass b (a) ())");
    e("(defclass c (b) ())");
    e("(defgeneric level (x))");
    e("(defmethod level ((x a)) 'level-a)");
    e("(defmethod level ((x b)) 'level-b)");
    /* most-specific method wins */
    TEST_EQ(eql, S(LEVEL-B), e("(level (make-instance 'b))"));
    TEST_EQ(eql, S(LEVEL-B), e("(level (make-instance 'c))"));
    TEST_EQ(eql, S(LEVEL-A), e("(level (make-instance 'a))"));}

/* ---- 13. CALL-NEXT-METHOD ----------------------------------------------- */

static void test_call_next_method(void){
    e("(defclass base () ((n :initarg :n :accessor obj-n)))");
    e("(defclass derived (base) ())");
    e("(defgeneric doubled (x))");
    e("(defmethod doubled ((x base)) (obj-n x))");
    e("(defmethod doubled ((x derived)) (* 2 (call-next-method)))");
    TEST_EQ(eql, I(5),  e("(doubled (make-instance 'base :n 5))"));
    TEST_EQ(eql, I(10), e("(doubled (make-instance 'derived :n 5))"));}

/* ---- 14. NEXT-METHOD-P -------------------------------------------------- */

static void test_next_method_p(void){
    e("(defclass root1 () ())");
    e("(defclass leaf1 (root1) ())");
    e("(defgeneric has-next (x))");
    e("(defmethod has-next ((x root1)) nil)");
    e("(defmethod has-next ((x leaf1)) (next-method-p))");
    TEST_EQ(eql, T_Symbol, e("(has-next (make-instance 'leaf1))"));}

/* ---- 15. :before method ------------------------------------------------- */

static void test_before_method(void){
    e("(defclass sensor () ((readings :initarg :readings :initform nil :accessor sensor-readings)))");
    e("(defgeneric take-reading (s v))");
    e("(defmethod take-reading ((s sensor) v) v)");
    e("(defmethod take-reading :before ((s sensor) v) (push v (sensor-readings s)))");
    /* :before fires before primary; primary returns v */
    TEST_EQ(eql, I(42),
            e("(let ((s (make-instance 'sensor)))"
              "  (take-reading s 42))")); }

/* ---- 16. :after method -------------------------------------------------- */

static void test_after_method(void){
    e("(defclass log-obj () ((log :initform 0 :accessor log-count)))");
    e("(defgeneric ping (obj))");
    e("(defmethod ping ((obj log-obj)) 'pong)");
    e("(defmethod ping :after ((obj log-obj)) (incf (log-count obj)))");
    TEST_EQ(eql, S(PONG), e("(ping (make-instance 'log-obj))"));
    TEST_EQ(eql, I(2),
            e("(let ((o (make-instance 'log-obj)))"
              "  (ping o) (ping o)"
              "  (log-count o))"));}

/* ---- 17. :around + call-next-method ------------------------------------- */

static void test_around_method(void){
    e("(defclass wrapped () ((val :initarg :val :accessor wrapped-val)))");
    e("(defgeneric compute (w))");
    e("(defmethod compute ((w wrapped)) (wrapped-val w))");
    e("(defmethod compute :around ((w wrapped)) (* 10 (call-next-method)))");
    TEST_EQ(eql, I(70), e("(compute (make-instance 'wrapped :val 7))"));}

/* ---- 18. WITH-SLOTS ----------------------------------------------------- */

static void test_with_slots(void){
    e("(defclass point3 () ((x :initarg :x) (y :initarg :y)))");
    TEST_EQ(eql, I(7),
            e("(with-slots (x y) (make-instance 'point3 :x 3 :y 4)"
              "  (+ x y))")); }

/* ---- 19. INITIALIZE-INSTANCE override ----------------------------------- */

static void test_initialize_instance(void){
    e("(defclass validated () ((n :initarg :n :accessor v-n)))");
    e("(defmethod initialize-instance :after ((obj validated) &rest args)"
      "  (when (< (v-n obj) 0) (error \"negative n\")))");
    TEST_EQ(eql, I(5), e("(v-n (make-instance 'validated :n 5))"));
    TEST_EQ(eql, T_Symbol,
            e("(handler-case (make-instance 'validated :n -1)"
              "  (error (e) t))"));}

/* ---- 20. PRINT-OBJECT --------------------------------------------------- */

static void test_print_object(void){
    e("(defclass named () ((name :initarg :name :accessor named-name)))");
    /* Override print-object to write the name string to the stream. */
    e("(defmethod print-object ((obj named) stream)"
      "  (princ (named-name obj) stream))");
    /* prin1 should call our print-object for CLOS instances */
    TEST_EQ(equal, T("Alice"),
            e("(with-output-to-string (s)"
              "  (prin1 (make-instance 'named :name \"Alice\") s))"));}

/* PRINT-OBJECT dispatch on KERNEL object types, not just CLOS instances
   (issue package-print-object-method). */
static void test_print_object_kernel(void){
    /* A package prints readably via its PRINT-OBJECT method. */
    TEST_EQ(equal, T("#<PACKAGE \"KEYWORD\">"),
            e("(prin1-to-string (find-package \"KEYWORD\"))"));
    /* A user method can dispatch on a kernel class (READTABLE). */
    e("(defmethod print-object ((rt readtable) s) (princ \"MY-RT\" s) rt)");
    TEST_EQ(equal, T("MY-RT"),
            e("(prin1-to-string *readtable*)"));
    /* An object with no specific method falls back to the raw #<CLASS id> form. */
    TEST_EQ(eql, T_Symbol,
            e("(let ((s (prin1-to-string (make-string-output-stream))))"
              "  (and (string= (subseq s 0 2) \"#<\") (search \"STREAM\" s) t))"));
    /* *PRINT-READABLY* T on an unreadable object signals PRINT-NOT-READABLE. */
    TEST_EQ(eql, T_Symbol,
            e("(handler-case (let ((*print-readably* t))"
              "                 (prin1-to-string (find-package \"KEYWORD\")) nil)"
              "  (print-not-readable () t))"));}


TEST_INITIALIZE(test_clos);
TEST_SUITE(test_clos, clos_init, clos_clean)
    TEST_CASE("defclass basic",           test_defclass_basic)
    TEST_CASE("slot accessors",           test_accessors)
    TEST_CASE(":initarg / :initform",     test_initarg_initform)
    TEST_CASE("unbound slot error",       test_unbound_slot)
    TEST_CASE("(setf slot-value)",        test_setf_slot_value)
    TEST_CASE("slot-boundp/makunbound",   test_slot_boundp)
    TEST_CASE("class-of",                 test_class_of)
    TEST_CASE("find-class",               test_find_class)
    TEST_CASE("slot inheritance",         test_inheritance)
    TEST_CASE("typep with CLOS class",    test_typep_clos)
    TEST_CASE("defgeneric+method dispatch", test_dispatch)
    TEST_CASE("dispatch selectivity",     test_dispatch_selectivity)
    TEST_CASE("call-next-method",         test_call_next_method)
    TEST_CASE("next-method-p",            test_next_method_p)
    TEST_CASE(":before method",           test_before_method)
    TEST_CASE(":after method",            test_after_method)
    TEST_CASE(":around+call-next",        test_around_method)
    TEST_CASE("with-slots",               test_with_slots)
    TEST_CASE("initialize-instance :after", test_initialize_instance)
    TEST_CASE("print-object",             test_print_object)
    TEST_CASE("print-object kernel types", test_print_object_kernel)
    TEST_FINALIZE()
