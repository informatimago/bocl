#include <string.h>
#include "packages.h"
#include "kernel.h"
#include "cons.h"
#include "variable.h"

/* Object_new is defined in kernel.c but not declared in any header. */
extern Ref Object_new(Ref class, Slots* slots);

Ref COMMON_LISP_Package      = NULL;
Ref COMMON_LISP_USER_Package = NULL;
Ref KEYWORD_Package          = NULL;
Ref all_packages             = NULL;

/* ------------------------------------------------------------------ */
/*  Constructor + registry                                              */
/* ------------------------------------------------------------------ */

static Package* Package_slots(Ref name, Ref nicknames){
    Package* that = allocate(sizeof(*that));
    that->objcount = 5;
    that->name      = name;
    that->nicknames = nicknames;
    that->symbols   = NIL_Symbol;
    that->exported  = NIL_Symbol;
    that->used      = NIL_Symbol;
    return that;}

static Ref make_package(Ref name, Ref nicknames){
    return Object_new(PACKAGE_Class, (Slots*)Package_slots(name, nicknames));}

/* Prepend a (name . pkg) entry to the package registry.  all_packages is a
   permanent sentinel cons (see kernel_initialize_packages): its identity never
   changes, so it stays reachable from the GC rootset across reassignments, and
   we add entries by mutating its CDR in place.  This is what keeps the registry
   alive through GC — without it, FIND-PACKAGE / LIST-ALL-PACKAGES lose every
   package the first time the collector runs. */
static void registry_add(Ref name, Ref pkg){
    Ref pair = cons(name, pkg);
    rplacd(all_packages, cons(pair, cons_cdr(all_packages)));}

Ref package_new(const char* name, const char* const* nicknames){
    Ref nn = NIL_Symbol;
    if(nicknames){
        for(int i = 0; nicknames[i]; i++){
            push(string_new_cstring(nicknames[i]), &nn);}
        nn = nreverse(nn);}
    Ref pkg = make_package(string_new_cstring(name), nn);
    /* Register pkg in the global all-packages alist (name → pkg, and each nickname → pkg). */
    registry_add(string_new_cstring(name), pkg);
    if(nicknames){
        for(int i = 0; nicknames[i]; i++){
            registry_add(string_new_cstring(nicknames[i]), pkg);}}
    return pkg;}

/* ------------------------------------------------------------------ */
/*  Lookups                                                             */
/* ------------------------------------------------------------------ */

Ref package_find(Ref designator){
    /* Accept STRING, SYMBOL (use symbol-name), or PACKAGE (return as-is). */
    if(typep(designator, PACKAGE_Class)){ return designator; }
    Ref name;
    if(typep(designator, STRING_Class)){ name = designator; }
    else if(symbolp(designator))       { name = symbol_name(designator); }
    else                                { TYPE_ERROR(designator, STRING_Class); }
    Ref entry = cons_cdr(all_packages);
    while(consp(entry)){
        Ref pair = cons_car(entry);
        if(string_equal(cons_car(pair), name)){
            return cons_cdr(pair);}
        entry = cons_cdr(entry);}
    return NIL_Symbol;}

void package_add_symbol(Ref pkg, Ref sym){
    check_type(pkg, PACKAGE_Class);
    if(nullp(memq(sym, pkg->slots->package.symbols))){
        pkg->slots->package.symbols = cons(sym, pkg->slots->package.symbols);}}

void package_export(Ref pkg, Ref sym){
    check_type(pkg, PACKAGE_Class);
    package_add_symbol(pkg, sym);
    if(nullp(memq(sym, pkg->slots->package.exported))){
        pkg->slots->package.exported = cons(sym, pkg->slots->package.exported);}}

Ref current_package(void){
    Ref p = symbol_value(S(*PACKAGE*));
    /* If somehow not bound or not a package, fall back to CL-USER. */
    if(!typep(p, PACKAGE_Class)){ return COMMON_LISP_USER_Package; }
    return p;}

/* ------------------------------------------------------------------ */
/*  Kernel built-ins                                                    */
/* ------------------------------------------------------------------ */

static void kernel_FIND_PACKAGE(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref designator = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    results->count = 1;
    results->values[0] = package_find(designator);
    close_live_pool(pool);}

static void kernel_PACKAGE_NAME(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref pkg = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    Ref p = package_find(pkg);
    if(nullp(p)){ ERROR("PACKAGE-NAME: no such package %s",
                        string_cstring(prin1_to_string(pkg))); }
    results->count = 1;
    results->values[0] = p->slots->package.name;
    close_live_pool(pool);}

static void kernel_PACKAGE_NICKNAMES(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref pkg = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    Ref p = package_find(pkg);
    if(nullp(p)){ ERROR("PACKAGE-NICKNAMES: no such package %s",
                        string_cstring(prin1_to_string(pkg))); }
    results->count = 1;
    results->values[0] = p->slots->package.nicknames;
    close_live_pool(pool);}

static void kernel_MAKE_PACKAGE(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref name_designator = pop_mandatory_argument(function, &current);
    Ref nicknames = pop_key_argument(function, &current, K(NICKNAMES), NIL_Symbol);
    Ref use = pop_key_argument(function, &current, K(USE), NIL_Symbol);
    Ref name;
    if(typep(name_designator, STRING_Class)){ name = name_designator; }
    else if(symbolp(name_designator))       { name = symbol_name(name_designator); }
    else                                     { TYPE_ERROR(name_designator, STRING_Class); }
    /* Coerce nicknames entries to strings. */
    Ref clean_nn = NIL_Symbol;
    Ref it = nicknames;
    while(consp(it)){
        Ref n = cons_car(it);
        if(symbolp(n))                       { n = symbol_name(n); }
        else if(!typep(n, STRING_Class))     { TYPE_ERROR(n, STRING_Class); }
        push(n, &clean_nn);
        it = cons_cdr(it);}
    clean_nn = nreverse(clean_nn);
    Ref pkg = make_package(name, clean_nn);
    registry_add(name, pkg);
    it = clean_nn;
    while(consp(it)){
        registry_add(cons_car(it), pkg);
        it = cons_cdr(it);}
    /* :use list — each entry is a package designator. */
    it = use;
    while(consp(it)){
        Ref u = package_find(cons_car(it));
        if(nullp(u)){ ERROR("MAKE-PACKAGE: unknown package in :use %s",
                            string_cstring(prin1_to_string(cons_car(it)))); }
        pkg->slots->package.used = cons(u, pkg->slots->package.used);
        it = cons_cdr(it);}
    results->count = 1;
    results->values[0] = pkg;
    close_live_pool(pool);}

/* Add a single package (designator) to IN_PKG's use list. */
static void use_one_package(Ref to_use_des, Ref in_pkg){
    Ref to_use = package_find(to_use_des);
    if(nullp(to_use)){ ERROR("USE-PACKAGE: no such package %s",
                             string_cstring(prin1_to_string(to_use_des))); }
    if(nullp(memq(to_use, in_pkg->slots->package.used))){
        in_pkg->slots->package.used = cons(to_use, in_pkg->slots->package.used);}}

static void kernel_USE_PACKAGE(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref to_use_des = pop_mandatory_argument(function, &current);
    Ref in_pkg_des = nullp(current) ? current_package() : pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    Ref in_pkg = package_find(in_pkg_des);
    if(nullp(in_pkg)){ ERROR("USE-PACKAGE: no such package %s",
                             string_cstring(prin1_to_string(in_pkg_des))); }
    /* packages-to-use is a list of designators OR a single designator. */
    if(consp(to_use_des)){
        Ref it = to_use_des;
        while(consp(it)){ use_one_package(cons_car(it), in_pkg); it = cons_cdr(it); }
    } else if(!nullp(to_use_des)){
        use_one_package(to_use_des, in_pkg);}
    results->count = 1;
    results->values[0] = T_Symbol;
    close_live_pool(pool);}

static void kernel_EXPORT(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref syms_arg = pop_mandatory_argument(function, &current);
    Ref pkg_des = nullp(current) ? current_package() : pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    Ref pkg = package_find(pkg_des);
    if(nullp(pkg)){ ERROR("EXPORT: no such package %s",
                          string_cstring(prin1_to_string(pkg_des))); }
    /* syms_arg may be a single symbol or a list of symbols. */
    if(symbolp(syms_arg)){
        package_export(pkg, syms_arg);
    } else {
        Ref it = syms_arg;
        while(consp(it)){
            Ref s = cons_car(it);
            if(!symbolp(s)){ TYPE_ERROR(s, SYMBOL_Class); }
            package_export(pkg, s);
            it = cons_cdr(it);}}
    results->count = 1;
    results->values[0] = T_Symbol;
    close_live_pool(pool);}

static void kp_INTERN(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref name = pop_mandatory_argument(function, &current);
    Ref pkg_des = nullp(current) ? current_package() : pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    if(!typep(name, STRING_Class)){ TYPE_ERROR(name, STRING_Class); }
    Ref pkg = package_find(pkg_des);
    if(nullp(pkg)){ ERROR("INTERN: no such package %s",
                          string_cstring(prin1_to_string(pkg_des))); }
    /* For Phase 3 we share the global KERNEL_obarray; symbol_intern handles
       the find-or-create. The package gets the symbol added to its list. */
    Ref sym = symbol_intern(name);
    package_add_symbol(pkg, sym);
    results->count = 1;
    results->values[0] = sym;
    close_live_pool(pool);}

static void kp_FIND_SYMBOL(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref name = pop_mandatory_argument(function, &current);
    Ref pkg_des = nullp(current) ? current_package() : pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    if(!typep(name, STRING_Class)){ TYPE_ERROR(name, STRING_Class); }
    Ref pkg = package_find(pkg_des);
    if(nullp(pkg)){ ERROR("FIND-SYMBOL: no such package %s",
                          string_cstring(prin1_to_string(pkg_des))); }
    /* Walk the package's symbols + its used packages' exported lists. */
    Ref it = pkg->slots->package.symbols;
    while(consp(it)){
        Ref s = cons_car(it);
        if(string_equal(symbol_name(s), name)){
            results->count = 1;
            results->values[0] = s;
            close_live_pool(pool);
            return;}
        it = cons_cdr(it);}
    Ref used = pkg->slots->package.used;
    while(consp(used)){
        Ref u = cons_car(used);
        Ref ex = u->slots->package.exported;
        while(consp(ex)){
            Ref s = cons_car(ex);
            if(string_equal(symbol_name(s), name)){
                results->count = 1;
                results->values[0] = s;
                close_live_pool(pool);
                return;}
            ex = cons_cdr(ex);}
        used = cons_cdr(used);}
    results->count = 1;
    results->values[0] = NIL_Symbol;
    close_live_pool(pool);}

static void kernel_PACKAGE(Ref function, Ref arguments, MultipleValues* results){
    /* (PACKAGEP obj) — a useful one-off. */
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref obj = pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    results->count = 1;
    results->values[0] = as_boolean(typep(obj, PACKAGE_Class));
    close_live_pool(pool);}

static void kernel_PACKAGE_SYMBOLS(Ref function, Ref arguments, MultipleValues* results){
    /* (PACKAGE-SYMBOLS package-designator) → list of all interned symbols */
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref pkg_des = nullp(current) ? current_package() : pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    Ref pkg = package_find(pkg_des);
    if(nullp(pkg)){ ERROR("PACKAGE-SYMBOLS: no such package %s",
                          string_cstring(prin1_to_string(pkg_des))); }
    results->count = 1;
    results->values[0] = pkg->slots->package.symbols;
    close_live_pool(pool);}

static void kernel_PACKAGE_EXPORTED_SYMBOLS(Ref function, Ref arguments, MultipleValues* results){
    /* (PACKAGE-EXPORTED-SYMBOLS package-designator) → list of exported symbols */
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current = arguments;
    Ref pkg_des = nullp(current) ? current_package() : pop_mandatory_argument(function, &current);
    check_no_more_arguments(function, current);
    Ref pkg = package_find(pkg_des);
    if(nullp(pkg)){ ERROR("PACKAGE-EXPORTED-SYMBOLS: no such package %s",
                          string_cstring(prin1_to_string(pkg_des))); }
    results->count = 1;
    results->values[0] = pkg->slots->package.exported;
    close_live_pool(pool);}

static void kernel_LIST_ALL_PACKAGES(Ref function, Ref arguments, MultipleValues* results){
    uword pool = open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    check_no_more_arguments(function, arguments);
    /* all_packages is name→pkg; dedupe to just the pkg list. */
    Ref pkgs = NIL_Symbol;
    Ref it = cons_cdr(all_packages);
    while(consp(it)){
        Ref pkg = cons_cdr(cons_car(it));
        if(nullp(memq(pkg, pkgs))){
            pkgs = cons(pkg, pkgs);}
        it = cons_cdr(it);}
    results->count = 1;
    results->values[0] = pkgs;
    close_live_pool(pool);}

static void register_fn(Ref fname, Ref lambda_list, KernelFunctionPtr fn){
    set_symbol_function(fname, built_in_function_new(fname, lambda_list, fn));}

void kernel_initialize_packages(void){
    /* Permanent sentinel header: entries live in its CDR.  Its constant
       identity lets the GC rootset (kernel_initialize_rootset) keep the whole
       registry alive even as packages are added at run time. */
    all_packages = cons(NIL_Symbol, NIL_Symbol);

    static const char* cl_nicknames[]      = { "CL",      NULL };
    static const char* cl_user_nicknames[] = { "CL-USER", NULL };
    COMMON_LISP_Package      = package_new("COMMON-LISP",      cl_nicknames);
    COMMON_LISP_USER_Package = package_new("COMMON-LISP-USER", cl_user_nicknames);
    KEYWORD_Package          = package_new("KEYWORD",          NULL);

    /* CL-USER uses CL. */
    COMMON_LISP_USER_Package->slots->package.used =
        cons(COMMON_LISP_Package, COMMON_LISP_USER_Package->slots->package.used);

    /* *PACKAGE* defaults to CL-USER. */
    define_parameter(S(*PACKAGE*), COMMON_LISP_USER_Package);

    /* Register kernel-visible built-ins. Lambda lists use simple positional
       descriptors; the &optional package designator is handled inline. */
    register_fn(S(FIND-PACKAGE),       list(S(NAME),NULL),                          kernel_FIND_PACKAGE);
    register_fn(S(PACKAGE-NAME),       list(S(PACKAGE),NULL),                       kernel_PACKAGE_NAME);
    register_fn(S(PACKAGE-NICKNAMES),  list(S(PACKAGE),NULL),                       kernel_PACKAGE_NICKNAMES);
    register_fn(S(MAKE-PACKAGE),       list(S(NAME),NULL),                          kernel_MAKE_PACKAGE);
    register_fn(S(USE-PACKAGE),        list(S(PACKAGES-TO-USE),NULL),               kernel_USE_PACKAGE);
    register_fn(S(EXPORT),             list(S(SYMBOLS),NULL),                       kernel_EXPORT);
    register_fn(S(INTERN),             list(S(NAME),NULL),                          kp_INTERN);
    register_fn(S(FIND-SYMBOL),        list(S(NAME),NULL),                          kp_FIND_SYMBOL);
    register_fn(S(PACKAGEP),                    list(S(OBJECT),NULL),  kernel_PACKAGE);
    register_fn(S(LIST-ALL-PACKAGES),           NIL_Symbol,            kernel_LIST_ALL_PACKAGES);
    register_fn(S(PACKAGE-SYMBOLS),             list(S(PACKAGE),NULL), kernel_PACKAGE_SYMBOLS);
    register_fn(S(PACKAGE-EXPORTED-SYMBOLS),    list(S(PACKAGE),NULL), kernel_PACKAGE_EXPORTED_SYMBOLS);
}
