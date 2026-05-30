#include "bocl.h"
#include "memory.h"
#include "boerror.h"
#include "kernel.h"
#include "stream.h"
#include "reader.h"
#include "printer.h"
#include "variable.h"
#include "macros.h"
#include "eval.h"
#include "packages.h"
#include <time.h>
#include <sys/time.h>


Ref bocl_eval(Ref sexp){
    MultipleValues results={0};
    return evale(sexp,eval_global_environment(),&results);
}

Ref bocl_read_cstring(const char* text){
    Ref stream=stream_input_string(string_new_cstring(text),I(0),NIL_Symbol);
    Ref result=kernel_read(stream,NIL_Symbol,NIL_Symbol,NIL_Symbol);
    stream_close(stream);
    return result;}

Ref bocl_read_eval_cstring(const char* text){
    Ref out=variable_value(S(*STANDARD-OUTPUT*));
    Ref sexp=bocl_read_cstring(text);
    Ref result=bocl_eval(sexp);
    stream_finish_output(out);
    return result;}

Ref bocl_repl(Ref inp,Ref out){
    terpri(out);
    while(true){
        princ(T("bocl> "),out);
        stream_finish_output(out);
        Ref sexp=kernel_read(inp,NIL_Symbol,inp,NIL_Symbol);
        if((sexp==inp)||equal(sexp,list(S(QUIT),NULL))){
            return I(0);}
        Ref result=bocl_eval(sexp);
        print(result,out);
        terpri(out);}}

Ref bocl_read_eval_loop_cstring(const char* text){
    Ref out=variable_value(S(*STANDARD-OUTPUT*));
    Ref inp=stream_input_string(string_new_cstring(text),I(0),NIL_Symbol);
    Ref result=bocl_repl(inp,out);
    stream_close(inp);
    return result;}

Ref bocl_in_package(Ref package_designator){
    Ref pkg = package_find(package_designator);
    if(nullp(pkg)){
        ERROR("IN-PACKAGE: no such package %s",
              string_cstring(prin1_to_string(package_designator)));}
    set_symbol_value(S(*PACKAGE*), pkg);
    return pkg;}

Ref bocl_load_cstring(const char* path){
    Ref out = variable_value(S(*STANDARD-OUTPUT*));

    bool do_verbose = is_true(variable_value(S(*LOAD-VERBOSE*)));
    bool do_print   = is_true(variable_value(S(*LOAD-PRINT*)));
    Ref inp = stream_open(physical_pathname_new_cstring(path),
                             K(INPUT),
                             S(CHARACTER),
                             NIL_Symbol,        /* if-exists: irrelevant for :input */
                             K(ERROR),          /* if-does-not-exist */
                             K(DEFAULT));
    if(do_verbose){
        princ(T(";; Loading "),out);
        princ(pathname(inp),out);
        terpri(out);
        stream_finish_output(out);}
    /* kernel_read on EOF returns the stream itself (we pass `inp` as the
       eof-value), so the loop test `sexp != inp` terminates at EOF. */
    Ref sexp=kernel_read(inp,NIL_Symbol,inp,NIL_Symbol);
    while(sexp!=inp){
        if(do_print){
            prin1(sexp,out);
            terpri(out);
            stream_finish_output(out);}
        bocl_eval(sexp);
        sexp=kernel_read(inp,NIL_Symbol,inp,NIL_Symbol);}
    stream_close(inp);
    if(do_verbose){
        princ(T(";; Loaded "),out);
        princ(pathname(inp),out);
        terpri(out);
        stream_finish_output(out);}

    return T_Symbol;}


/* Kernel-visible (LOAD filespec) — minimal: no &key :verbose / :print /
   :if-does-not-exist yet. Those go on the Lisp side. */
static void kernel_LOAD(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref filespec=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    /* Accept a STRING (treat as namestring) or a PHYSICAL-PATHNAME. */
    char* path;
    if(typep(filespec,STRING_Class)){
        path=string_cstring(filespec);
    } else if(typep(filespec,PHYSICAL_PATHNAME_Class)){
        path=string_cstring(namestring(filespec));
    } else {
        TYPE_ERROR(filespec,STRING_Class);}
    Ref result=bocl_load_cstring(path);
    free(path);
    results->count=1;
    results->values[0]=result;
    close_live_pool(pool);}

/* APPEND is needed by backquote-expanded code; the kernel previously only
   shipped NCONC. */
extern Ref bocl_append(Ref lists);
extern Ref bocl_write_char(Ref ch, Ref stream);

static void kernel_WRITE_CHAR(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref ch=cons_car(current); current=cons_cdr(current);
    Ref stream=consp(current)?cons_car(current):variable_value(S(*STANDARD-OUTPUT*));
    results->count=1;
    results->values[0]=bocl_write_char(ch,stream);
    close_live_pool(pool);}

static void kernel_APPEND(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(arguments);
    results->count=1;
    results->values[0]=bocl_append(arguments);
    close_live_pool(pool);}

/* (%clos-null-p x) — returns T if X is C NULL (not a valid Lisp object),
   NIL otherwise.  Used to distinguish kernel find-class returning C NULL
   (class not found) from returning a real class object. */
static void kernel_CLOS_NULL_P(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(arguments);
    Ref x=cons_car(arguments);
    results->count=1;
    results->values[0]=(x==NULL)?T_Symbol:NIL_Symbol;
    close_live_pool(pool);}

/* (%clos-obj-p x) — T if X is a CLOS instance (%obj struct), bypassing bridge. */
static void kernel_CLOS_OBJ_P(unused Ref function, Ref arguments, MultipleValues* results){
    Ref x=cons_car(arguments);
    results->count=1;
    results->values[0]=clos_instance_p(x)?T_Symbol:NIL_Symbol;}

/* (%clos-cls-p x) — T if X is a CLOS class object (%cls struct). */
static void kernel_CLOS_CLS_P(unused Ref function, Ref arguments, MultipleValues* results){
    Ref x=cons_car(arguments);
    results->count=1;
    results->values[0]=clos_class_p(x)?T_Symbol:NIL_Symbol;}

/* (%clos-register obj-struct-class cls-struct-class) — wires up the CLOS
   type bridge so class_of() and subclassp() understand CLOS instances. */
static void kernel_CLOS_REGISTER(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(arguments);
    Ref current=arguments;
    Ref obj_sc=cons_car(current); current=cons_cdr(current);
    Ref cls_sc=cons_car(current);
    clos_register_bridge(obj_sc, cls_sc);
    results->count=1;
    results->values[0]=T_Symbol;
    close_live_pool(pool);}

/* ---- Slice 22 kernel built-ins ------------------------------------------ */

#include <sys/stat.h>
#include <unistd.h>

/* (CLOSE stream &key abort) — close a stream; returns T. */
static void kernel_CLOSE(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref stream=cons_car(arguments);
    /* :abort keyword is accepted but ignored — we always close */
    results->count=1;
    results->values[0]=stream_close_any(stream);
    close_live_pool(pool);}

/* (READ-BYTE stream &optional eof-error-p eof-value) */
static void kernel_READ_BYTE(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref stream      = cons_car(current); current=cons_cdr(current);
    Ref eof_errorp  = consp(current)?cons_car(current):T_Symbol; if(consp(current)) current=cons_cdr(current);
    Ref eof_value   = consp(current)?cons_car(current):NIL_Symbol;
    results->count=1;
    results->values[0]=stream_read_byte(stream,eof_errorp,eof_value);
    close_live_pool(pool);}

/* (WRITE-BYTE byte stream) */
static void kernel_WRITE_BYTE(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref byte_val = cons_car(current); current=cons_cdr(current);
    Ref stream   = cons_car(current);
    results->count=1;
    results->values[0]=stream_write_byte(byte_val, stream);
    close_live_pool(pool);}

/* (PROBE-FILE filespec) — returns the truename or NIL if not found. */
static void kernel_PROBE_FILE(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref filespec=cons_car(arguments);
    Ref name_ref;
    if(typep(filespec,STRING_Class)){
        name_ref=filespec;
    } else {
        name_ref=namestring(filespec);}
    char* path=string_cstring(name_ref);
    struct stat st;
    bool exists=(stat(path,&st)==0);
    free(path);
    results->count=1;
    results->values[0]=exists?name_ref:NIL_Symbol;
    close_live_pool(pool);}

/* (DELETE-FILE filespec) — delete a file; errors if it can't. */
static void kernel_DELETE_FILE(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref filespec=cons_car(arguments);
    Ref name_ref=typep(filespec,STRING_Class)?filespec:namestring(filespec);
    char* path=string_cstring(name_ref);
    int rc=remove(path);
    free(path);
    if(rc!=0){ ERROR("DELETE-FILE: failed to delete%s",""); }
    results->count=1;
    results->values[0]=T_Symbol;
    close_live_pool(pool);}

/* (RENAME-FILE filespec new-name) — rename a file; returns 3 values. */
static void kernel_RENAME_FILE(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref from=cons_car(current); current=cons_cdr(current);
    Ref to=cons_car(current);
    Ref from_str=typep(from,STRING_Class)?from:namestring(from);
    Ref to_str  =typep(to,  STRING_Class)?to  :namestring(to);
    char* fp=string_cstring(from_str);
    char* tp=string_cstring(to_str);
    int rc=rename(fp,tp);
    free(fp); free(tp);
    if(rc!=0){ ERROR("RENAME-FILE: failed to rename%s",""); }
    results->count=3;
    results->values[0]=from_str;
    results->values[1]=to_str;
    results->values[2]=to_str;
    close_live_pool(pool);}

/* (WILD-PATHNAME-P pathname &optional field-key) — stub: returns NIL. */
static void kernel_WILD_PATHNAME_P(unused Ref function, Ref arguments, MultipleValues* results){
    add_to_live_pool(arguments);
    results->count=1;
    results->values[0]=NIL_Symbol;}

/* (TYPE-OF object) — return a type specifier for OBJECT. */
static void kernel_TYPE_OF(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref obj=cons_car(arguments);
    Ref type;
    if(nullp(obj)){                  type=S(NULL); }
    else if(obj==T_Symbol){          type=symbol_intern_cstring("BOOLEAN"); }
    else if(integerp(obj)){
        /* Return (integer lo hi) as a proper Lisp list — the most specific
           standard range bucket for this value. */
        word v=integer_value(obj);
        Ref lo,hi;
        if(v>=0 && v<=255){
            lo=I(0); hi=I(255);
        } else if(v>=-128 && v<=127){
            lo=I(-128); hi=I(127);
        } else {
            /* Full signed-word range */
            lo=integer_from_word(WORD_MIN); hi=integer_from_word(WORD_MAX);}
        type=list(S(INTEGER),lo,hi,NULL); }
    else if(floatp(obj)){            type=symbol_intern_cstring("DOUBLE-FLOAT"); }
    else if(symbolp(obj)){
        if(keywordp(obj))             type=S(KEYWORD);
        else                          type=S(SYMBOL); }
    else if(typep(obj,CONS_Class)){  type=S(CONS); }
    else if(typep(obj,STRING_Class)){ type=symbol_intern_cstring("SIMPLE-STRING"); }
    else if(typep(obj,BIT_VECTOR_Class)){ type=symbol_intern_cstring("SIMPLE-BIT-VECTOR"); }
    else if(typep(obj,VECTOR_Class)){ type=symbol_intern_cstring("SIMPLE-VECTOR"); }
    else if(typep(obj,CHARACTER_Class)){ type=S(CHARACTER); }
    else if(typep(obj,HASH_TABLE_Class)){ type=symbol_intern_cstring("HASH-TABLE"); }
    else if(typep(obj,PACKAGE_Class)){ type=S(PACKAGE); }
    else if(typep(obj,FUNCTION_Class)){ type=S(FUNCTION); }
    else if(closurep(obj)){          type=S(FUNCTION); }
    else if(typep(obj,PATHNAME_Class)){ type=S(PATHNAME); }
    else if(typep(obj,FILE_STREAM_Class)){ type=symbol_intern_cstring("FILE-STREAM"); }
    else if(typep(obj,STRING_STREAM_Class)){ type=symbol_intern_cstring("STRING-STREAM"); }
    else if(typep(obj,RANDOM_STATE_Class)){ type=symbol_intern_cstring("RANDOM-STATE"); }
    else if(clos_instance_p(obj)){
        /* CLOS instance — return the class name */
        Ref cls_of=class_of(obj);
        type=(clos_class_p(cls_of)&&structure_get(cls_of,symbol_intern_cstring("NAME"))!=NIL_Symbol)
             ? structure_get(cls_of,symbol_intern_cstring("NAME"))
             : symbol_intern_cstring("STANDARD-OBJECT");}
    else if(structurep(obj)){
        /* Structure instance — return the structure class name */
        type=obj->class->slots->structure_class.name;}
    else{                            type=S(T); }
    results->count=1;
    results->values[0]=type;
    close_live_pool(pool);}

/* (INT-CHAR integer) — return the character with the given code. */
static void kernel_INT_CHAR(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref n=cons_car(arguments);
    check_type(n,INTEGER_Class);
    word code=integer_value(n);
    if(code<0||code>255){ ERROR("INT-CHAR: code out of range%s",""); }
    results->count=1;
    results->values[0]=octet_character((octet)code);
    close_live_pool(pool);}

/* (ARRAY-DIMENSIONS array) — return a list of the array's dimensions. */
static void kernel_ARRAY_DIMENSIONS(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref arr=cons_car(arguments);
    Ref dims;
    if(typep(arr,VECTOR_Class)||typep(arr,STRING_Class)||typep(arr,BIT_VECTOR_Class)){
        /* 1-D: single dimension */
        uword len;
        if(typep(arr,STRING_Class))         len=string_length(arr);
        else if(typep(arr,BIT_VECTOR_Class)) len=bit_vector_length(arr);
        else                                 len=arr->slots->array.dimensions[0];
        dims=list(integer_from_uword(len),NULL);
    } else {
        dims=NIL_Symbol;}
    results->count=1;
    results->values[0]=dims;
    close_live_pool(pool);}

/* (ARRAY-ELEMENT-TYPE array) — return T (bocl uses uniform typed arrays). */
static void kernel_ARRAY_ELEMENT_TYPE(unused Ref function, Ref arguments, MultipleValues* results){
    add_to_live_pool(arguments);
    results->count=1;
    results->values[0]=S(T);}

/* Note: INPUT-STREAM-P, OUTPUT-STREAM-P, OPEN-STREAM-P are already in
   built_in_functions.c — not duplicated here. */

/* ---- Slice 23: timing, write-string, sleep, clear-input, random ------- */

/* splitmix64 PRNG — fast, good quality, no external state needed. */
static uint64_t prng_state = 0x12345678ABCDEF01ULL;

static uint64_t splitmix64(void){
    uint64_t z = (prng_state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);}

/* (RANDOM limit &optional random-state) — uniform in [0,limit). */
static void kernel_RANDOM_FN(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref limit=cons_car(arguments);
    uint64_t r=splitmix64();
    Ref val;
    if(integerp(limit)){
        word lv=integer_value(limit);
        if(lv<=0){ ERROR("RANDOM: limit must be positive%s",""); }
        val=integer_from_word((word)(r%(uint64_t)lv));
    } else if(floatp(limit)){
        floating lv=float_value(limit);
        if(lv<=0.0){ ERROR("RANDOM: limit must be positive%s",""); }
        val=float_from_floating(((floating)(r>>11)/(floating)(1ULL<<53))*lv);
    } else {
        TYPE_ERROR(limit,NUMBER_Class); val=NIL_Symbol;}
    results->count=1;
    results->values[0]=val;
    close_live_pool(pool);}

/* (MAKE-RANDOM-STATE &optional state) — return T (use global PRNG). */
static void kernel_MAKE_RANDOM_STATE(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    results->count=1;
    results->values[0]=T_Symbol;  /* bocl uses a global PRNG; T = current state */
    close_live_pool(pool);}

/* CL universal time = seconds since 1900-01-01 00:00:00 UTC.
   Unix epoch is 1970-01-01 = 70 years after 1900. */
#define SECONDS_FROM_1900_TO_1970  2208988800UL

static void kernel_GET_UNIVERSAL_TIME(unused Ref function, Ref arguments, MultipleValues* results){
    add_to_live_pool(arguments);
    results->count=1;
    results->values[0]=integer_from_uword((uword)time(NULL)+(uword)SECONDS_FROM_1900_TO_1970);}

static void kernel_GET_INTERNAL_REAL_TIME(unused Ref function, Ref arguments, MultipleValues* results){
    add_to_live_pool(arguments);
    struct timeval tv; gettimeofday(&tv,NULL);
    uword us=(uword)tv.tv_sec*1000000UL+(uword)tv.tv_usec;
    results->count=1;
    results->values[0]=integer_from_uword(us);}

static void kernel_GET_INTERNAL_RUN_TIME(unused Ref function, Ref arguments, MultipleValues* results){
    add_to_live_pool(arguments);
    struct timespec ts; clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&ts);
    uword us=(uword)ts.tv_sec*1000000UL+(uword)ts.tv_nsec/1000UL;
    results->count=1;
    results->values[0]=integer_from_uword(us);}

/* (WRITE-STRING string &optional stream &key start end)
   Also used as (KERNEL-WRITE-STRING string stream start end) — positional. */
static void kernel_WRITE_STRING(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref str    = cons_car(current); current=cons_cdr(current);
    Ref stream = NIL_Symbol;
    Ref start  = NIL_Symbol;
    Ref end    = NIL_Symbol;
    if(consp(current)){
        stream=cons_car(current); current=cons_cdr(current);
        /* Accept both keyword (:start/:end) and positional (start end) forms. */
        if(consp(current)){
            Ref x=cons_car(current);
            if(keywordp(x)){
                /* Keyword args: skip :start/:end labels, take values. */
                while(consp(current)){
                    Ref k=cons_car(current); current=cons_cdr(current);
                    Ref v=consp(current)?cons_car(current):NIL_Symbol; if(consp(current)) current=cons_cdr(current);
                    if(eql(k,keyword_intern_cstring("START"))) start=v;
                    else if(eql(k,keyword_intern_cstring("END"))) end=v;}
            } else {
                /* Positional: start then end. */
                start=x; current=cons_cdr(current);
                if(consp(current)){ end=cons_car(current);}}}}
    if(nullp(stream)) stream=variable_value(S(*STANDARD-OUTPUT*));
    results->count=1;
    results->values[0]=bocl_write_string(str,stream,start,end);
    close_live_pool(pool);}

/* (SLEEP seconds) — non-blocking sleep using nanosleep. */
static void kernel_SLEEP(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref secs_ref=cons_car(current);
    double s=floatp(secs_ref)?float_value(secs_ref):(double)integer_value(secs_ref);
    if(s>0.0){
        struct timespec ts;
        ts.tv_sec=(time_t)s;
        ts.tv_nsec=(long)((s-(double)ts.tv_sec)*1e9);
        nanosleep(&ts,NULL);}
    results->count=1;
    results->values[0]=NIL_Symbol;
    close_live_pool(pool);}

/* (CLEAR-INPUT &optional stream) — discard buffered input. */
static void kernel_CLEAR_INPUT(unused Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(arguments);
    results->count=1;
    results->values[0]=NIL_Symbol;
    close_live_pool(pool);}

void bocl_register_load(void){
    Ref fname=S(LOAD);
    set_symbol_function(fname,built_in_function_new(fname,list(S(FILESPEC),NULL),kernel_LOAD));
    Ref af=S(APPEND);
    set_symbol_function(af,built_in_function_new(af,list(S(&REST),S(LISTS),NULL),kernel_APPEND));
    /* Helper for CLOS: detect C NULL (returned by kernel find-class for unknown names). */
    Ref cn=symbol_intern_cstring("%CLOS-NULL-P");
    set_symbol_function(cn,built_in_function_new(cn,list(S(X),NULL),kernel_CLOS_NULL_P));
    /* Predicates that bypass the class_of bridge — needed after %clos-register. */
    Ref cop=symbol_intern_cstring("%CLOS-OBJ-P");
    set_symbol_function(cop,built_in_function_new(cop,list(S(X),NULL),kernel_CLOS_OBJ_P));
    Ref ccp=symbol_intern_cstring("%CLOS-CLS-P");
    set_symbol_function(ccp,built_in_function_new(ccp,list(S(X),NULL),kernel_CLOS_CLS_P));
    /* Wire the CLOS type bridge: call from prelude after defining %obj/%cls structs. */
    Ref cr=symbol_intern_cstring("%CLOS-REGISTER");
    set_symbol_function(cr,built_in_function_new(cr,
        list(symbol_intern_cstring("OBJ-SC"),symbol_intern_cstring("CLS-SC"),NULL),
        kernel_CLOS_REGISTER));
    /* WRITE-CHAR — expose bocl_write_char as a Lisp built-in. */
    {   Ref wc_fn = symbol_intern_cstring("WRITE-CHAR");
        Ref wc_ll = list(symbol_intern_cstring("CHARACTER"),
                         S(&OPTIONAL), symbol_intern_cstring("STREAM"), NULL);
        set_symbol_function(wc_fn,built_in_function_new(wc_fn,wc_ll,kernel_WRITE_CHAR));}
    /* Slice 22 — I/O, pathnames, type-of, array, stream predicates. */
    {   Ref ll_s=list(S(STREAM),NULL);
        Ref ll_s_opt=list(S(STREAM),S(&REST),symbol_intern_cstring("KEYS"),NULL);
        Ref ll_b_s=list(symbol_intern_cstring("BYTE"),S(STREAM),NULL);
        Ref ll_s_o2=list(S(STREAM),S(&OPTIONAL),symbol_intern_cstring("EOF-ERROR-P"),symbol_intern_cstring("EOF-VALUE"),NULL);
        Ref ll_x=list(S(X),NULL);
        Ref ll_obj=list(symbol_intern_cstring("OBJECT"),NULL);
        Ref ll_arr=list(symbol_intern_cstring("ARRAY"),NULL);
        Ref ll_f=list(symbol_intern_cstring("FILESPEC"),NULL);
        Ref ll_f2=list(symbol_intern_cstring("FILESPEC"),symbol_intern_cstring("NEW-NAME"),NULL);
        Ref ll_f_o=list(symbol_intern_cstring("PATHNAME"),S(&OPTIONAL),symbol_intern_cstring("FIELD"),NULL);
        set_symbol_function(symbol_intern_cstring("CLOSE"),
            built_in_function_new(symbol_intern_cstring("CLOSE"),ll_s_opt,kernel_CLOSE));
        set_symbol_function(symbol_intern_cstring("READ-BYTE"),
            built_in_function_new(symbol_intern_cstring("READ-BYTE"),ll_s_o2,kernel_READ_BYTE));
        set_symbol_function(symbol_intern_cstring("WRITE-BYTE"),
            built_in_function_new(symbol_intern_cstring("WRITE-BYTE"),ll_b_s,kernel_WRITE_BYTE));
        set_symbol_function(symbol_intern_cstring("PROBE-FILE"),
            built_in_function_new(symbol_intern_cstring("PROBE-FILE"),ll_f,kernel_PROBE_FILE));
        set_symbol_function(symbol_intern_cstring("DELETE-FILE"),
            built_in_function_new(symbol_intern_cstring("DELETE-FILE"),ll_f,kernel_DELETE_FILE));
        set_symbol_function(symbol_intern_cstring("RENAME-FILE"),
            built_in_function_new(symbol_intern_cstring("RENAME-FILE"),ll_f2,kernel_RENAME_FILE));
        set_symbol_function(symbol_intern_cstring("WILD-PATHNAME-P"),
            built_in_function_new(symbol_intern_cstring("WILD-PATHNAME-P"),ll_f_o,kernel_WILD_PATHNAME_P));
        set_symbol_function(symbol_intern_cstring("TYPE-OF"),
            built_in_function_new(symbol_intern_cstring("TYPE-OF"),ll_obj,kernel_TYPE_OF));
        set_symbol_function(symbol_intern_cstring("INT-CHAR"),
            built_in_function_new(symbol_intern_cstring("INT-CHAR"),ll_x,kernel_INT_CHAR));
        set_symbol_function(symbol_intern_cstring("ARRAY-DIMENSIONS"),
            built_in_function_new(symbol_intern_cstring("ARRAY-DIMENSIONS"),ll_arr,kernel_ARRAY_DIMENSIONS));
        set_symbol_function(symbol_intern_cstring("ARRAY-ELEMENT-TYPE"),
            built_in_function_new(symbol_intern_cstring("ARRAY-ELEMENT-TYPE"),ll_arr,kernel_ARRAY_ELEMENT_TYPE));
        /* INPUT-STREAM-P, OUTPUT-STREAM-P, OPEN-STREAM-P already registered
           in kernel_initialize_built_in_functions() — skip here. */
    }
    /* Slice 23 — timing, write-string, sleep, clear-input. */
    {   Ref ll_e   = list(S(&REST),symbol_intern_cstring("ARGS"),NULL);
        Ref ll_x   = list(S(X),NULL);
        Ref ll_str = list(symbol_intern_cstring("STRING"),S(&REST),symbol_intern_cstring("OPTS"),NULL);
        Ref ll_s_o = list(S(SECONDS),NULL);
        Ref ll_si  = list(S(STREAM),S(&OPTIONAL),NULL);
        set_symbol_function(symbol_intern_cstring("GET-UNIVERSAL-TIME"),
            built_in_function_new(symbol_intern_cstring("GET-UNIVERSAL-TIME"),ll_e,kernel_GET_UNIVERSAL_TIME));
        set_symbol_function(symbol_intern_cstring("GET-INTERNAL-REAL-TIME"),
            built_in_function_new(symbol_intern_cstring("GET-INTERNAL-REAL-TIME"),ll_e,kernel_GET_INTERNAL_REAL_TIME));
        set_symbol_function(symbol_intern_cstring("GET-INTERNAL-RUN-TIME"),
            built_in_function_new(symbol_intern_cstring("GET-INTERNAL-RUN-TIME"),ll_e,kernel_GET_INTERNAL_RUN_TIME));
        set_symbol_function(symbol_intern_cstring("WRITE-STRING"),
            built_in_function_new(symbol_intern_cstring("WRITE-STRING"),ll_str,kernel_WRITE_STRING));
        /* The prelude's write-string calls kernel-write-string internally.
           Register it with positional (string stream start end) args. */
        Ref ll_ws4=list(symbol_intern_cstring("STRING"),S(STREAM),
                        symbol_intern_cstring("START"),symbol_intern_cstring("END"),NULL);
        set_symbol_function(symbol_intern_cstring("KERNEL-WRITE-STRING"),
            built_in_function_new(symbol_intern_cstring("KERNEL-WRITE-STRING"),ll_ws4,kernel_WRITE_STRING));
        set_symbol_function(symbol_intern_cstring("SLEEP"),
            built_in_function_new(symbol_intern_cstring("SLEEP"),ll_s_o,kernel_SLEEP));
        set_symbol_function(symbol_intern_cstring("CLEAR-INPUT"),
            built_in_function_new(symbol_intern_cstring("CLEAR-INPUT"),ll_si,kernel_CLEAR_INPUT));
        /* RANDOM / MAKE-RANDOM-STATE */
        Ref ll_rand=list(symbol_intern_cstring("LIMIT"),S(&OPTIONAL),symbol_intern_cstring("STATE"),NULL);
        Ref ll_rand2=list(S(&OPTIONAL),symbol_intern_cstring("STATE"),NULL);
        set_symbol_function(symbol_intern_cstring("RANDOM"),
            built_in_function_new(symbol_intern_cstring("RANDOM"),ll_rand,kernel_RANDOM_FN));
        set_symbol_function(symbol_intern_cstring("MAKE-RANDOM-STATE"),
            built_in_function_new(symbol_intern_cstring("MAKE-RANDOM-STATE"),ll_rand2,kernel_MAKE_RANDOM_STATE));
    }}

void bocl_call(Ref fname,Ref arguments){
    Ref out=variable_value(S(*STANDARD-OUTPUT*));
    princ(T("Calling: "),out);
    prin1(list(fname,arguments,NULL),out);
    print(symbol_function(fname),out);
    terpri(out);
    MultipleValues results={0};
    kernel_apply(symbol_function(fname),
                 arguments,
                 &results);
    const char* sep="--> ";
    for(uword i=0;i<results.count;i++){
        printf("%s",sep);
        sep="    ";
        prin1(results.values[i],out);
        terpri(out);}}

void bocl_test(){
    prin1(S(PAPA),variable_value(S(*STANDARD-OUTPUT*)));
    terpri(variable_value(S(*STANDARD-OUTPUT*)));
    bocl_call(S(CAR),list(list(S(FOO),NULL),NULL));}


/**** THE END ****/

