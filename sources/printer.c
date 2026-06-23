#include <ctype.h>
#include <string.h>
#include "printer.h"
#include "kernel.h"
#include "kernel_objects.h"
#include "macros.h"
#include "data.h"
#include "variable.h"
#include "stream.h"
#include "bostring.h"
#include "character.h"
#include "packages.h"

/* Printer */

Ref prin1_to_string(Ref object){
    Ref stream=stream_output_string(S(CHARACTER));
    prin1(object,stream);
    return string_output_stream_get_string(stream);}

Ref princ_to_string(Ref object){
    Ref stream=stream_output_string(S(CHARACTER));
    princ(object,stream);
    return string_output_stream_get_string(stream);}

Ref print(Ref object,Ref stream){
    terpri(stream);
    prin1(object,stream);
    return object;}

/* Conditions are kernel objects (not %obj CLOS instances), so the Lisp-level
   prin1 dispatch never reaches them.  Route them through the Lisp PRINT-OBJECT
   generic here, so the method on CONDITION (and any user/library methods) prints
   the formatted report instead of "#<SIMPLE-ERROR 0x…>".  Returns true if it
   handled OBJECT.  A static guard prevents unbounded re-entry when a condition's
   report itself references another condition. */
/* Defined below; the array printer passes the current *PRINT-LEVEL* depth to the
   prelude so each array dimension can add a further level. */
static int g_print_level_depth;

/* %md-array (multidimensional / adjustable / displaced arrays) are Lisp structs
   the kernel printer cannot introspect, so it asks the prelude helper
   %MD-ARRAY-PRINT-STRING for the aggregate #nA(…) / #(…) string.  A NIL/non-string
   return means "not handled" (not a %md-array, or *PRINT-ARRAY* is NIL) → the
   kernel falls back to #<…>.  Returns true iff it printed OBJECT. */
static bool print_md_array(Ref object,FILE* out,bool escape){
    if(!structurep(object)){ return false; }
    Ref fn=S(%MD-ARRAY-PRINT-STRING);
    if(!symbol_fboundp(fn)){ return false; }
    uword pool=open_live_pool();
    add_to_live_pool(object);
    MultipleValues mv; mv.count=0;
    /* Pass the current *PRINT-LEVEL* depth as BASE so the Lisp printer knows the
       array's own nesting level (its dimensions add further levels on top). */
    kernel_apply(symbol_function(fn),
                 list(object, escape?T_Symbol:NIL_Symbol,
                      integer_from_word((word)g_print_level_depth), NULL), &mv);
    close_live_pool(pool);
    if(mv.count>0 && stringp(mv.values[0])){
        fputs(string_cstring(mv.values[0]),out);
        return true; }
    return false;
}

static bool g_in_condition_print = false;
static bool print_condition_via_print_object(Ref object,Ref stream,bool escape){
    if(g_in_condition_print){ return false; }
    if(!typep(object,CONDITION_Class)){ return false; }
    Ref po=S(PRINT-OBJECT);
    if(!symbol_fboundp(po)){ return false; }   /* before the prelude defines it */
    g_in_condition_print=true;
    /* The C printer's prin1/princ do not themselves bind *PRINT-ESCAPE*, so set
       it around the dispatch (PRIN1 => escaped #<…>, PRINC => bare report). */
    Ref pe=S(*PRINT-ESCAPE*);
    Ref saved=symbol_value(pe);
    set_symbol_value(pe, escape?T_Symbol:NIL_Symbol);
    uword pool=open_live_pool();
    add_to_live_pool(object); add_to_live_pool(stream); add_to_live_pool(saved);
    MultipleValues mv;
    kernel_apply(symbol_function(po),list(object,stream,NULL),&mv);
    close_live_pool(pool);
    set_symbol_value(pe, saved);
    g_in_condition_print=false;
    return true;}

/* ---- *PRINT-CIRCLE* support + recursion backstop ------------------------- */
/* When *PRINT-CIRCLE* is true, a pre-pass records every cons reachable from the
   object and marks the ones referenced more than once; the print pass labels
   those with #n= on first appearance and #n# afterwards, so shared and circular
   structure prints (and reads back) correctly instead of recursing forever.
   PRIN1_MAX_DEPTH is a backstop against deep/uncircle-printed structure. */
#define PRIN1_MAX_DEPTH 16384
static int g_prin1_depth = 0;

/* *PRINT-LEVEL* nesting depth: 0 at the top object, +1 per composite (list /
   vector / array) entered.  When a composite would print at depth >= *PRINT-LEVEL*
   it prints "#" instead (CLHS 22.1.3.4). */
static int g_print_level_depth = 0;
/* *PRINT-READABLY* true forces *PRINT-LEVEL* / *PRINT-LENGTH* to be ignored
   (treated as NIL), so abbreviation never breaks read/print consistency
   (CLHS 22.1.3.11). */
static word current_print_level(void){
    if(!nullp(variable_value(S(*PRINT-READABLY*)))){ return -1; }
    Ref lv = variable_value(S(*PRINT-LEVEL*));
    return integerp(lv) ? integer_value(lv) : -1;   /* -1 => unlimited */
}
static word current_print_length(void){
    if(!nullp(variable_value(S(*PRINT-READABLY*)))){ return -1; }
    Ref ln = variable_value(S(*PRINT-LENGTH*));
    return integerp(ln) ? integer_value(ln) : -1;   /* -1 => unlimited */
}
/* True when *PRINT-ARRAY* is NIL — vectors/bit-vectors/arrays then print #<…>.
   *PRINT-READABLY* overrides *PRINT-ARRAY*: readable output cannot use the #<…>
   form, so under *PRINT-READABLY* every array (incl. bit-vectors) prints in full
   (CLHS 22.1.3.4 / 22.1.3.11). */
static bool print_array_off(void){
    if(!nullp(variable_value(S(*PRINT-READABLY*)))){ return false; }
    return nullp(variable_value(S(*PRINT-ARRAY*)));
}

/* Set the *PRINT-LEVEL* depth counter from Lisp.  The aggregate #nA(…) array
   printer (%MD-EMIT, in the prelude) delegates each element to the kernel
   PRIN1/PRINC, which read this counter to decide *PRINT-LEVEL* abbreviation;
   the Lisp printer syncs the counter to the element's true nesting depth first
   so a list/vector element abbreviates ("#") at the correct level. */
void printer_set_level_depth(word d){ g_print_level_depth = (int)d; }

typedef struct { Ref obj; int label; } CircleEntry;  /* label: 0 once, -1 shared, >0 assigned */
static CircleEntry* g_circle = NULL;
static int g_circle_count = 0, g_circle_cap = 0, g_circle_next_label = 0;
static bool g_circle_active = false;

static int circle_index(Ref obj){
    for(int i=0;i<g_circle_count;i++){ if(g_circle[i].obj==obj){ return i; } }
    return -1;}

static void circle_scan(Ref obj,int depth){
    if(depth>PRIN1_MAX_DEPTH){ return; }
    bool is_cons=consp(obj), is_vec=vectorp(obj);
    if(!is_cons && !is_vec){ return; }
    int idx=circle_index(obj);
    if(idx>=0){ g_circle[idx].label=-1; return; }   /* second visit: shared */
    if(g_circle_count>=g_circle_cap){
        g_circle_cap = g_circle_cap ? g_circle_cap*2 : 256;
        g_circle = realloc(g_circle,(size_t)g_circle_cap*sizeof(CircleEntry)); }
    g_circle[g_circle_count].obj=obj;
    g_circle[g_circle_count].label=0;
    g_circle_count++;
    if(is_cons){ circle_scan(car(obj),depth+1); circle_scan(cdr(obj),depth+1); }
    else { uword len=vector_length(obj);
           for(uword i=0;i<len;i++){ circle_scan(obj->slots->array.elements->elements[i],depth+1); } }}

/* --- Symbol-name escaping for *PRINT-ESCAPE* (PRIN1) -----------------------
   A symbol name must be printed so it reads back to the same symbol.  When the
   raw name would NOT (it is empty, all dots, looks like a number, or contains a
   character the reader treats specially or would case-fold), wrap the whole
   name in vertical bars, backslash-escaping any | or \ inside.  Vertical-bar
   quoting preserves case and every constituent verbatim, so it is always a
   correct (if sometimes non-minimal) readable form. */

static bool token_is_number_like(const char* s, uword len){
    /* Recognises the common decimal numeric tokens the reader accepts at
       *READ-BASE* 10: integers (optional sign, optional trailing dot), ratios,
       and floats.  Over-recognition only causes extra (harmless) escaping. */
    if(len==0){ return false; }
    uword i=0;
    if(s[i]=='+'||s[i]=='-'){ i++; }
    uword digits=0, dots=0, slashes=0, exp=0;
    for(;i<len;i++){
        char c=s[i];
        if(c>='0'&&c<='9'){ digits++; }
        else if(c=='.'){ dots++; }
        else if(c=='/'){ slashes++; }
        else if(c=='e'||c=='E'||c=='d'||c=='D'||c=='f'||c=='F'||
                c=='s'||c=='S'||c=='l'||c=='L'){
            exp++;
            if(i+1<len && (s[i+1]=='+'||s[i+1]=='-')){ i++; } }
        else{ return false; }              /* a non-numeric constituent */
    }
    if(digits==0){ return false; }         /* must contain at least one digit */
    if(dots==0 && slashes==0 && exp==0){ return true; }   /* integer */
    if(slashes==1 && dots==0 && exp==0){ return true; }   /* ratio a/b */
    if(slashes==0 && dots<=1){ return true; }             /* float / decimal */
    return false;
}

/* readtable-case of *READTABLE*: 0=:UPCASE 1=:DOWNCASE 2=:PRESERVE 3=:INVERT. */
static int current_readtable_case(void){
    Ref rt = variable_value(S(*READTABLE*));
    if(!typep(rt, READTABLE_Class)){ return 0; }
    Ref rc = rt->slots->readtable.readcase;
    if(symbolp(rc)){
        char* nm = string_cstring(symbol_name(rc));
        if(strcmp(nm,"DOWNCASE")==0){ return 1; }
        if(strcmp(nm,"PRESERVE")==0){ return 2; }
        if(strcmp(nm,"INVERT")==0){ return 3; }
    }
    return 0;
}

/* A name needs |…| escaping (under *PRINT-ESCAPE*) when it would not read back as
   the same symbol: empty, leading #, all dots, a special/whitespace char, a
   numeric-looking token, or a LETTER the readtable case would fold differently —
   a lowercase letter under :UPCASE, an uppercase one under :DOWNCASE.  :PRESERVE
   and :INVERT round-trip without case escaping. */
static bool symbol_name_needs_escape(const char* s, uword len, int rcase){
    if(len==0){ return true; }
    if(s[0]=='#'){ return true; }
    bool all_dots=true;
    for(uword i=0;i<len;i++){ if(s[i]!='.'){ all_dots=false; break; } }
    if(all_dots){ return true; }
    for(uword i=0;i<len;i++){
        unsigned char c=(unsigned char)s[i];
        if(c<=' ' || c==0x7f){ return true; }
        switch(c){
          case '(': case ')': case '"': case '\'': case '`':
          case ',': case ';': case '\\': case '|': case ':':
            return true;
        }
        if(rcase==0 && c>='a' && c<='z'){ return true; }
        if(rcase==1 && c>='A' && c<='Z'){ return true; }
    }
    if(token_is_number_like(s,len)){ return true; }
    return false;
}

/* *PRINT-CASE*: 0 = :UPCASE (the default), 1 = :DOWNCASE, 2 = :CAPITALIZE. */
static int current_print_case(void){
    Ref c = variable_value(S(*PRINT-CASE*));
    if(symbolp(c)){
        char* nm = string_cstring(symbol_name(c));
        if(strcmp(nm,"DOWNCASE")==0){ return 1; }
        if(strcmp(nm,"CAPITALIZE")==0){ return 2; }
    }
    return 0;
}

/* Apply *PRINT-CASE* PCASE to a foldable letter C (WORD_START = first char of a
   word, for :CAPITALIZE). */
static unsigned char fold_char(unsigned char c, int pcase, bool word_start){
    if(pcase==0){ return (c>='a'&&c<='z') ? (unsigned char)(c-32) : c; }   /* :UPCASE */
    if(pcase==1){ return (c>='A'&&c<='Z') ? (unsigned char)(c+32) : c; }   /* :DOWNCASE */
    if(word_start){ return (c>='a'&&c<='z') ? (unsigned char)(c-32) : c; } /* :CAPITALIZE */
    return (c>='A'&&c<='Z') ? (unsigned char)(c+32) : c;
}

/* Add NAME's ASCII letter-case presence to *UP / *LO, but only if NAME would be
   printed unescaped under READTABLE-CASE RCASE — an escaped |…| component prints
   verbatim and so contributes no readtable-foldable letters.  Used to decide
   :INVERT over a whole "pkg::name" token rather than each component in isolation
   (a uniform-case token inverts on read; a mixed one is preserved). */
static void invert_scan(Ref name, int rcase, bool* up, bool* lo){
    char* s   = string_cstring(name);
    uword len = string_length(name);
    if(symbol_name_needs_escape(s,len,rcase)){ return; }
    for(uword i=0;i<len;i++){
        unsigned char c=(unsigned char)s[i];
        if(c>='A'&&c<='Z'){ *up=true; } else if(c>='a'&&c<='z'){ *lo=true; }
    }
}

/* Write a symbol/package name to OUT honouring READTABLE-CASE and *PRINT-CASE*
   (CLHS 22.1.3.3.2).  ESCAPE selects the *PRINT-ESCAPE* behaviour: when true and
   the name would not read back, wrap it in |…| (verbatim — a correct readable
   form under any readtable case); otherwise emit the case-folded name.  INV drives
   the :INVERT decision: -1 = decide from this name alone (the default); 0 = do not
   invert; 1 = invert — so a caller can force a consistent decision across the
   package prefix and the symbol name of one printed token. */
static void print_name_escaped(Ref name, FILE* out, bool escape, int inv){
    char* s   = string_cstring(name);
    uword len = string_length(name);
    int rcase = current_readtable_case();
    if(escape && symbol_name_needs_escape(s,len,rcase)){
        fputc('|',out);
        for(uword i=0;i<len;i++){
            char c=s[i];
            if(c=='|' || c=='\\'){ fputc('\\',out); }
            fputc(c,out); }
        fputc('|',out);
        return; }
    if(rcase==2){ fwrite(s,1,(size_t)len,out); return; }   /* :PRESERVE — verbatim */
    if(rcase==3){                                           /* :INVERT — flip a uniform-case token */
        bool uniform;
        if(inv>=0){ uniform=(inv==1); }
        else{
            bool up=false, lo=false;
            for(uword i=0;i<len;i++){
                unsigned char c=(unsigned char)s[i];
                if(c>='A'&&c<='Z'){ up=true; } else if(c>='a'&&c<='z'){ lo=true; } }
            uniform=(up!=lo); }
        for(uword i=0;i<len;i++){
            unsigned char c=(unsigned char)s[i];
            if(uniform){
                if(c>='A'&&c<='Z'){ c=(unsigned char)(c+32); }
                else if(c>='a'&&c<='z'){ c=(unsigned char)(c-32); } }
            fputc(c,out); }
        return; }
    /* :UPCASE / :DOWNCASE — fold the foldable letters (uppercase under :UPCASE,
       lowercase under :DOWNCASE) per *PRINT-CASE*; emit the rest verbatim. */
    int pcase=current_print_case();
    bool word_start=true;
    for(uword i=0;i<len;i++){
        unsigned char c=(unsigned char)s[i];
        bool alnum=(c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9');
        bool foldable = (rcase==0) ? (c>='A'&&c<='Z') : (c>='a'&&c<='z');
        fputc(foldable ? fold_char(c,pcase,word_start) : c, out);
        word_start = !alnum;
    }
}

/* --- Radix-aware integer / ratio printing (*PRINT-BASE* / *PRINT-RADIX*) ----
   Both PRIN1 and PRINC honour *PRINT-BASE* (2..36) and *PRINT-RADIX*; the printer
   is recursive, so the radix applies to every integer/ratio leaf (in lists,
   vectors, …), not just a top-level number. */

static void fmt_write_integer(FILE* out, word n, int radix, bool atsignp);

static int current_print_base(void){
    Ref b = variable_value(S(*PRINT-BASE*));
    int base = integerp(b) ? (int)integer_value(b) : 10;
    if(base<2){ base=2; }
    if(base>36){ base=36; }
    return base;
}

/* Write integer N (fixnum or bignum), with sign, in BASE, straight to OUT. */
static void write_integer_in_base(FILE* out, Ref n, int base){
    if(bignump(n)){ print_bignum_radix(n,out,base); }
    else          { fmt_write_integer(out,integer_value(n),base,false); }
}

/* Emit the *PRINT-RADIX* prefix for BASE.  Returns true iff the caller must
   append a trailing "." — the base-10 INTEGER radix marker (CLHS 22.1.3.1.2);
   a ratio in base 10 uses a "#10r" prefix instead, never the trailing dot. */
static bool emit_radix_prefix(FILE* out, int base, bool radix, bool is_ratio){
    if(!radix){ return false; }
    if(base==10){
        if(is_ratio){ fputs("#10r",out); return false; }
        return true; }
    switch(base){
      case 2:  fputs("#b",out); break;
      case 8:  fputs("#o",out); break;
      case 16: fputs("#x",out); break;
      default: fprintf(out,"#%dr",base); break; }
    return false;
}

static void print_integer_radix(FILE* out, Ref n){
    int base=current_print_base();
    bool radix=!nullp(variable_value(S(*PRINT-RADIX*)));
    bool dot=emit_radix_prefix(out,base,radix,false);
    write_integer_in_base(out,n,base);
    if(dot){ fputc('.',out); }
}

static void print_ratio_radix(FILE* out, Ref n){
    int base=current_print_base();
    bool radix=!nullp(variable_value(S(*PRINT-RADIX*)));
    emit_radix_prefix(out,base,radix,true);
    write_integer_in_base(out,ratio_numerator(n),base);
    fputc('/',out);
    write_integer_in_base(out,ratio_denominator(n),base);
}

static Ref prin1_inner(Ref object,Ref stream);
Ref prin1(Ref object,Ref stream){
    if(g_prin1_depth >= PRIN1_MAX_DEPTH){
        fprintf(stream_output_cstream(stream),"...");
        return object;}
    bool top = (g_prin1_depth==0);
    bool saved_active=g_circle_active; int saved_count=g_circle_count, saved_label=g_circle_next_label;
    if(top){ g_print_level_depth=0; }
    if(top){
        Ref pc=variable_value(S(*PRINT-CIRCLE*));
        if(!nullp(pc)){ g_circle_count=0; g_circle_next_label=0; circle_scan(object,0); g_circle_active=true; }
        else { g_circle_active=false; }}
    g_prin1_depth++;
    Ref r = prin1_inner(object,stream);
    g_prin1_depth--;
    if(top){ g_circle_active=saved_active; g_circle_count=saved_count; g_circle_next_label=saved_label; }
    return r;}

static Ref prin1_inner(Ref object,Ref stream){
    FILE* out=stream_output_cstream(stream);

    if(characterp(object)){
        octet code=(octet)integer_value(character_code(object));
        if(printablep(code)){
            fprintf(out,"#\\%c",(char)code);}
        else{
            fprintf(out,"#\\%s",string_cstring(character_name(object)));}
        return object;}

    if(floatp(object)){
        fprintf(out,"%g",float_value(object));
        return object;}

    if(integerp(object) || bignump(object)){
        print_integer_radix(out,object);
        return object;}

    if(ratiop(object)){
        print_ratio_radix(out,object);
        return object;}

    if(complexp(object)){
        fprintf(out,"#C(");
        prin1(complex_realpart(object), stream);
        fprintf(out," ");
        prin1(complex_imagpart(object), stream);
        fprintf(out,")");
        return object;}

    /* if(nullp(object)){ */
    /*     fprintf(out,"NIL"); */
    /*     return object;} */

    if(keywordp(object) ){
        fprintf(out,":");
        print_name_escaped(symbol_name(object),out,true,-1);
        return object;}

    if(symbolp(object)){
        /* prin1 qualifies a symbol by package when it is not accessible as
           itself in *PACKAGE*, so the written form reads back to the same symbol
           (needed for fasls and readable output across packages).  Each name
           component is escaped so a symbol whose name has lowercase, special, or
           numeric-looking characters reads back unchanged. */
        Ref pkgname; int sep;
        symbol_print_prefix(object,&pkgname,&sep);
        /* Under :INVERT the reader treats "pkg::name" as one extended token and
           inverts only when ALL its unescaped letters share a case.  So decide the
           inversion over the package prefix and symbol name TOGETHER and force the
           same choice on both components — inverting each in isolation could emit a
           mixed-case token (e.g. tpkg::ZEBRA) that no longer reads back. */
        int inv = -1;
        if(current_readtable_case()==3 && (sep==1 || sep==2)){
            bool up=false, lo=false;
            invert_scan(pkgname, 3, &up, &lo);
            invert_scan(symbol_name(object), 3, &up, &lo);
            inv = (up!=lo) ? 1 : 0; }
        /* #: prefix for an uninterned symbol when *PRINT-GENSYM* is true, or
           always when *PRINT-READABLY* (an uninterned symbol must read back). */
        if(sep==3){ if(!nullp(variable_value(S(*PRINT-GENSYM*))) ||
                       !nullp(variable_value(S(*PRINT-READABLY*)))){ fprintf(out,"#:"); } }
        else if(sep!=0){
            print_name_escaped(pkgname,out,true,inv);
            fprintf(out, sep==1 ? ":" : "::"); }
        print_name_escaped(symbol_name(object),out,true,inv);
        return object;}

    if(consp(object)){
        /* *PRINT-LEVEL*: a list deeper than the limit prints as "#". */
        word plevel = current_print_level();
        if(plevel >= 0 && g_print_level_depth >= plevel){ fputc('#',out); return object; }
        /* *PRINT-CIRCLE*: emit #n# for an already-printed shared cons, #n= the
           first time. */
        if(g_circle_active){
            int idx=circle_index(object);
            if(idx>=0){
                int lab=g_circle[idx].label;
                if(lab>0){ fprintf(out,"#%d#",lab); return object; }
                if(lab==-1){ g_circle[idx].label = ++g_circle_next_label;
                             fprintf(out,"#%d=",g_circle_next_label); }}}
        /* Respect *PRINT-LENGTH* — truncate after this many elements. */
        word print_length = current_print_length();
        if(print_length == 0){ fprintf(out,"(...)"); return object; }
        g_print_level_depth++;
        Ref current=object;
        fprintf(out,"(");
        word elem=0;
        prin1(car(current),stream); elem++;
        current=cdr(current);
        /* Floyd cycle detection on the CDR spine is the backstop when
           *PRINT-CIRCLE* is OFF; when ON, a shared CDR cell is printed via its
           label (". #n#") instead of being walked. */
        Ref slow=object; int advance_slow=0;
        while(consp(current)){
            if(g_circle_active){
                int idx=circle_index(current);
                if(idx>=0 && g_circle[idx].label!=0){
                    fprintf(out," . "); prin1(current,stream);
                    current=NIL_Symbol; break; }}
            if(print_length >= 0 && elem >= print_length){
                fprintf(out," ...");
                break;}
            fprintf(out," ");
            prin1(car(current),stream);
            current=cdr(current);
            elem++;
            if(advance_slow){ slow=cdr(slow); }
            advance_slow=!advance_slow;
            if(current==slow){ fprintf(out," . #<circular>"); current=NIL_Symbol; break; }}
        if(!nullp(current) && !consp(current)){
            fprintf(out," . ");
            prin1(current, stream);}
        fprintf(out,")");
        g_print_level_depth--;
        return object;}

    if(stringp(object)){
        char* p=string_cstring(object);
        fprintf(out,"\"");
        while(*p){
            switch(*p){
              case '\\': fprintf(out,"\\\\"); break;
              case '"': fprintf(out,"\\\""); break;
              default: fprintf(out,"%c",*p); break;}
            p++;}
        fprintf(out,"\"");
        return object;}

    /* BIT-VECTOR must come before VECTOR (bit-vectors are a subtype). */
    if(bit_vector_p(object)){
        if(print_array_off()){
            fprintf(out,"#<"); prin1(class_name(class_of(object)),stream);
            fprintf(out," %p>",(void*)object); return object; }
        fprintf(out,"#*");
        uword blen = bit_vector_length(object);
        for(uword i=0; i<blen; i++){
            /* Use the safe bit() accessor which handles all layout details. */
            Ref bval = bit(object, integer_from_word((word)i));
            fprintf(out,"%ld",(long)integer_value(bval));}
        return object;}

    if(vectorp(object)){
        /* *PRINT-ARRAY* NIL => print the unreadable #<…> form instead of #(…). */
        if(print_array_off()){
            fprintf(out,"#<"); prin1(class_name(class_of(object)),stream);
            fprintf(out," %p>",(void*)object); return object; }
        /* *PRINT-LEVEL*: a vector deeper than the limit prints as "#". */
        word plevel = current_print_level();
        if(plevel >= 0 && g_print_level_depth >= plevel){ fputc('#',out); return object; }
        /* A vector prints as #(e1 e2 …). Vectors are
           Array-backed under the hood, so iterate via array.elements. */
        if(g_circle_active){
            int idx=circle_index(object);
            if(idx>=0){
                int lab=g_circle[idx].label;
                if(lab>0){ fprintf(out,"#%d#",lab); return object; }
                if(lab==-1){ g_circle[idx].label = ++g_circle_next_label;
                             fprintf(out,"#%d=",g_circle_next_label); }}}
        word plen = current_print_length();
        if(plen == 0){ fprintf(out,"#(...)"); return object; }
        g_print_level_depth++;
        fprintf(out,"#(");
        uword len = vector_length(object);
        for(uword i=0; i<len; i++){
            if(i>0){ fprintf(out," "); }
            if(plen >= 0 && (word)i >= plen){ fprintf(out,"..."); break; }
            prin1(object->slots->array.elements->elements[i], stream);}
        fprintf(out,")");
        g_print_level_depth--;
        return object;}

    /* Pathnames print in their readable #P"namestring" form (both physical and
       logical pathnames are PATHNAME_Class). */
    if(typep(object,PATHNAME_Class)){
        fprintf(out,"#P");
        prin1(namestring(object),stream);   /* prints the namestring as "…" */
        return object;}

    /* Multidimensional / adjustable / displaced arrays print as #nA(…) / #(…). */
    if(print_md_array(object,out,true)){ return object; }

    /* Conditions print via the Lisp PRINT-OBJECT generic (formatted report). */
    if(print_condition_via_print_object(object,stream,true)){ return object; }

    /* Dispatch to PRINT-OBJECT for CLOS instances (%obj structs).
       clos_instance_p() returns true only after (%clos-register) is called,
       so prelude-loading is safe — no CLOS objects exist yet at that point.
       We pin object and stream in the live pool so the GC cannot collect
       them while print-object is running. */
    if(clos_instance_p(object)){
        /* For CLOS instances, class_of returns a %cls struct (not CLASS_Class),
           so we cannot call class_name() on it.  Read the NAME slot directly.
           The Lisp-level print-object generic can be called from Lisp code;
           we only provide the safe #<ClassName addr> fallback here. */
        Ref clos_cls = class_of(object);
        Ref nm_sym   = symbol_intern_cstring("NAME");
        Ref cls_name = (clos_class_p(clos_cls))
                       ? structure_get(clos_cls, nm_sym)
                       : NIL_Symbol;
        fprintf(out,"#<");
        if(symbolp(cls_name)) prin1(cls_name, stream);
        else fprintf(out,"CLOS-OBJECT");
        fprintf(out," %p>", (void*)object);
        return object;}

    fprintf(out,"#<");
    prin1(class_name(class_of(object)),stream);

    if(built_in_function_p(object)){
        fprintf(out," ");
        prin1(function_name(object),stream);}
    if(classp(object)){
        fprintf(out," ");
        prin1(class_name(object),stream);}
    fprintf(out," %p>",(void*)object);
    return object;}


Ref princ(Ref object,Ref stream){
    FILE* out=stream_output_cstream(stream);

    if(characterp(object)){
        octet code=(octet)integer_value(character_code(object));
        fprintf(out,"%c",(char)code);
        return object;}

    if(floatp(object)){
        fprintf(out,"%f",float_value(object));
        return object;}

    if(integerp(object) || bignump(object)){
        print_integer_radix(out,object);
        return object;}

    if(ratiop(object)){
        print_ratio_radix(out,object);
        return object;}

    if(complexp(object)){
        fprintf(out,"#C(");
        princ(complex_realpart(object), stream);
        fprintf(out," ");
        princ(complex_imagpart(object), stream);
        fprintf(out,")");
        return object;}

    /* if(nullp(object)){ */
    /*     fprintf(out,"NIL"); */
    /*     return object;} */

    /* PRINC (*PRINT-ESCAPE* nil): only the symbol NAME is printed — no package
       prefix and no keyword colon (CLHS 22.1.3.3) — case-folded per READTABLE-CASE
       and *PRINT-CASE*. */
    if(keywordp(object)){
        print_name_escaped(symbol_name(object),out,false,-1);
        return object;}

    if(symbolp(object)){
        print_name_escaped(symbol_name(object),out,false,-1);
        return object;}

    if(stringp(object)){
        char* p=string_cstring(object);
        fprintf(out,"%s",p);
        return object;}

    if(consp(object)){
        word plevel = current_print_level();
        if(plevel >= 0 && g_print_level_depth >= plevel){ fputc('#',out); return object; }
        word plen = current_print_length();
        if(plen == 0){ fprintf(out,"(...)"); return object; }
        g_print_level_depth++;
        Ref current=object;
        fprintf(out,"(");
        princ(car(current),stream);
        current=cdr(current);
        word elem=1;
        while(consp(current)){
            if(plen >= 0 && elem >= plen){ fprintf(out," ..."); current=NIL_Symbol; break; }
            fprintf(out," ");
            princ(car(current),stream);
            current=cdr(current);
            elem++;}
        if(!nullp(current) && !consp(current)){
            fprintf(out," . ");
            princ(current, stream);}
        fprintf(out,")");
        g_print_level_depth--;
        return object;}

    /* BIT-VECTOR / VECTOR — the aesthetic (#*… / #(…)) form coincides with the
       readable one, so PRINC prints them like PRIN1.  BIT-VECTOR before VECTOR
       (a bit-vector is a vector subtype). */
    if(bit_vector_p(object)){
        if(print_array_off()){
            fprintf(out,"#<"); princ(class_name(class_of(object)),stream);
            fprintf(out," %p>",(void*)object); return object; }
        fprintf(out,"#*");
        uword blen = bit_vector_length(object);
        for(uword i=0; i<blen; i++){
            fprintf(out,"%ld",(long)integer_value(bit(object,integer_from_word((word)i))));}
        return object;}

    if(vectorp(object)){
        if(print_array_off()){
            fprintf(out,"#<"); princ(class_name(class_of(object)),stream);
            fprintf(out," %p>",(void*)object); return object; }
        word plevel = current_print_level();
        if(plevel >= 0 && g_print_level_depth >= plevel){ fputc('#',out); return object; }
        word plen = current_print_length();
        if(plen == 0){ fprintf(out,"#(...)"); return object; }
        g_print_level_depth++;
        fprintf(out,"#(");
        uword len = vector_length(object);
        for(uword i=0; i<len; i++){
            if(i>0){ fprintf(out," "); }
            if(plen >= 0 && (word)i >= plen){ fprintf(out,"..."); break; }
            princ(object->slots->array.elements->elements[i], stream);}
        fprintf(out,")");
        g_print_level_depth--;
        return object;}

    /* PRINC of a pathname writes its namestring (no #P"…" escaping). */
    if(typep(object,PATHNAME_Class)){
        princ(namestring(object),stream);
        return object;}

    /* Multidimensional / adjustable / displaced arrays print as #nA(…) / #(…). */
    if(print_md_array(object,out,false)){ return object; }

    /* Conditions print via the Lisp PRINT-OBJECT generic (formatted report). */
    if(print_condition_via_print_object(object,stream,false)){ return object; }

    /* Safe fallback for CLOS instances in princ too. */
    if(clos_instance_p(object)){
        Ref clos_cls = class_of(object);
        Ref nm_sym   = symbol_intern_cstring("NAME");
        Ref cls_name = (clos_class_p(clos_cls))
                       ? structure_get(clos_cls, nm_sym)
                       : NIL_Symbol;
        fprintf(out,"#<");
        if(symbolp(cls_name)) princ(cls_name, stream);
        else fprintf(out,"CLOS-OBJECT");
        fprintf(out," %p>", (void*)object);
        return object;}
    fprintf(out,"#<");
    princ(class_name(class_of(object)),stream);
    if(typep(object,CLASS_Class)){
        fprintf(out," ");
        princ(class_name(object),stream);}
    fprintf(out," %p>",(void*)object);
    return object;}


Ref terpri(Ref stream){
    FILE* out=stream_output_cstream(stream);
    fprintf(out,"\n");
    return NIL_Symbol;}

/* ---- Slice 10 additions ------------------------------------------------- */

Ref bocl_write_char(Ref ch, Ref stream){
    FILE* out = stream_output_cstream(stream);
    octet code = (octet)integer_value(character_code(ch));
    fputc((int)code, out);
    return ch;}

Ref bocl_write_string(Ref string, Ref stream, Ref start_ref, Ref end_ref){
    FILE* out  = stream_output_cstream(stream);
    uword len  = string_length(string);
    octet* buf = string_octets(string);
    uword start = nullp(start_ref) ? 0   : (uword)integer_value(start_ref);
    uword end   = nullp(end_ref)   ? len : (uword)integer_value(end_ref);
    if(start > len) start = len;
    if(end   > len) end   = len;
    for(uword i = start; i < end; i++)
        fputc((int)buf[i], out);
    return string;}

Ref bocl_fresh_line(Ref stream){
    /* Simplified: always emits a newline.  A full implementation would track
       the column and skip the newline when already at column 0. */
    FILE* out = stream_output_cstream(stream);
    fprintf(out, "\n");
    return T_Symbol;}

/* ---- Slice 10: C-level FORMAT runner ------------------------------------ */

/* Write integer N in RADIX to OUT.  ATSIGNP forces a '+' prefix on
   non-negative values.  Returns nothing.  Radix must be 2..36. */
static void fmt_write_integer(FILE* out, word n, int radix, bool atsignp){
    if(n==0){ if(atsignp) fputc('+',out); fputc('0',out); return;}
    char tmp[128]; int len=0;
    bool neg=(n<0);
    uword v = neg ? (uword)(-(n+1))+1 : (uword)n;   /* safe abs */
    uword r = (uword)radix;
    while(v>0){
        int d=(int)(v%r);
        tmp[len++]=(char)(d<10 ? '0'+d : 'A'+d-10);
        v/=r;}
    if(neg)       fputc('-',out);
    else if(atsignp) fputc('+',out);
    for(int i=len-1;i>=0;i--) fputc((unsigned char)tmp[i],out);}

/* Scan CTRL[POS..END) for the `~}` that closes the `~{` we just entered.
   Returns the index of the `~` in `~}`, or (uword)-1 if not found.
   Handles nesting. */
static uword fmt_find_close(const octet* ctrl, uword pos, uword end){
    int depth=1;
    while(pos<end){
        if(ctrl[pos]!='~'){ pos++; continue;}
        pos++;               /* skip ~ */
        while(pos<end && ctrl[pos]>='0' && ctrl[pos]<='9') pos++;  /* skip digits */
        while(pos<end && (ctrl[pos]==':' || ctrl[pos]=='@'))  pos++;/* skip modifiers */
        if(pos>=end) break;
        char d=(char)toupper((unsigned char)ctrl[pos]);
        if(d=='{') depth++;
        else if(d=='}'){
            depth--;
            if(depth==0) return pos-1; /* pos-1 is the ~ */}
        pos++;}
    return (uword)-1;}

/* Pop the first element from *args_ptr (a cons list).
   Returns the element; advances *args_ptr to the rest.
   If the list is empty, returns NIL_Symbol and leaves *args_ptr unchanged. */
static Ref fmt_pop(Ref* args_ptr){
    if(nullp(*args_ptr)) return NIL_Symbol;
    Ref v=car(*args_ptr);
    *args_ptr=cdr(*args_ptr);
    return v;}

/* Core format runner.  Processes ctrl_string->octets[start..end),
   writing to STREAM.  Arguments are consumed from *args_ptr.
   Returns the remaining args list (for ~{} nesting).             */
Ref bocl_format_run(Ref stream, Ref ctrl_string, uword start, uword end, Ref* args_ptr){
    FILE* out=stream_output_cstream(stream);
    const octet* ctrl=string_octets(ctrl_string);
    uword i=start;
    while(i<end){
        octet c=ctrl[i]; i++;
        if(c!='~'){ fputc((int)c,out); continue;}
        /* --- parse comma-separated numeric parameters (~w,d,m,p,...X) --- */
        /* Up to 7 params; missing params flagged with WORD_MIN sentinel. */
#define FMT_PARAMS_MAX 7
        word  fmt_params[FMT_PARAMS_MAX];
        bool  fmt_has[FMT_PARAMS_MAX];
        for(int _p=0;_p<FMT_PARAMS_MAX;_p++){ fmt_params[_p]=0; fmt_has[_p]=false; }
        int   n_params=0;
        bool  colonp   = false;
        bool  atsignp  = false;
        /* Parse one numeric param; return whether a param was parsed. */
        for(;;){
            if(n_params >= FMT_PARAMS_MAX) break;
            bool neg=false; word v=0; bool got=false;
            if(i<end && ctrl[i]=='-'){ neg=true; i++; }
            while(i<end && ctrl[i]>='0' && ctrl[i]<='9'){
                v=v*10+(ctrl[i]-'0'); got=true; i++;}
            if(neg && got) v=-v;
            if(!got && !neg && i<end && toupper((unsigned char)ctrl[i])=='V'){
                Ref a=fmt_pop(args_ptr); v=integerp(a)?integer_value(a):0; got=true; i++;}
            if(!got && !neg && i<end && ctrl[i]=='#'){
                word cnt=0; for(Ref p=*args_ptr;consp(p);p=cdr(p)) cnt++;
                v=cnt; got=true; i++;}
            if(got){ fmt_params[n_params]=v; fmt_has[n_params]=true; }
            n_params++;
            if(i<end && ctrl[i]==',') i++;  /* consume comma, loop for next */
            else break;}
        /* modifiers */
        while(i<end && (ctrl[i]==':' || ctrl[i]=='@')){
            if(ctrl[i]==':') colonp  =true;
            else             atsignp =true;
            i++;}
        if(i>=end) break;
        char   d=(char)toupper((unsigned char)ctrl[i]); i++;
        /* Backward-compat aliases: first param → prefix/count */
        word  prefix  = fmt_has[0] ? fmt_params[0] : 1;
        bool  has_pfx = fmt_has[0];
        word  count   = has_pfx ? prefix : 1;

        switch(d){
          case 'A': {   /* PRINC with optional min-col (~mincol,colinc,minpad,padcharA) */
              word mincol  = fmt_has[0] ? fmt_params[0] : 0;
              char padchar = (fmt_has[3] && fmt_params[3]>=32) ? (char)fmt_params[3] : ' ';
              Ref arg=fmt_pop(args_ptr);
              if(mincol > 0){
                  /* Use princ_to_string equivalent: get the representation first */
                  Ref repr = (colonp && nullp(arg))
                             ? string_new_cstring("()")
                             : (stringp(arg) ? arg : prin1_to_string(arg));
                  /* For PRINC we want without quotes — strip them for non-strings/chars */
                  if(consp(arg) || symbolp(arg) || integerp(arg) || floatp(arg)){
                      /* These look the same in princ and prin1 for these types */ }
                  /* Write chars */
                  octet* bytes = string_octets(repr);
                  uword  rlen  = string_length(repr);
                  /* For strings, skip surrounding quotes from prin1 */
                  if(stringp(arg)){
                      fwrite(bytes, 1, rlen, out);
                      for(word _p=(word)rlen;_p<mincol;_p++) fputc(padchar,out);
                  } else {
                      princ(arg,stream);
                      /* We can't easily know actual char count; just pad relative to repr */
                      char* cs=string_cstring(repr);
                      uword slen=strlen(cs); free(cs);
                      for(word _p=(word)slen;_p<mincol;_p++) fputc(padchar,out);}
              } else {
                  if(colonp && nullp(arg)) fputs("()",out);
                  else princ(arg,stream);}
              break;}
          case 'S': case 'W': {  /* PRIN1 with optional min-col */
              word mincol = fmt_has[0] ? fmt_params[0] : 0;
              char padchar= (fmt_has[3] && fmt_params[3]>=32) ? (char)fmt_params[3] : ' ';
              Ref arg=fmt_pop(args_ptr);
              if(mincol > 0){
                  Ref repr=prin1_to_string(arg);
                  char* cs=string_cstring(repr);
                  uword slen=strlen(cs);
                  fputs(cs,out); free(cs);
                  for(word _p=(word)slen;_p<mincol;_p++) fputc(padchar,out);
              } else {
                  prin1(arg,stream);}
              break;}
          case 'D': {
              Ref arg=fmt_pop(args_ptr);
              if(integerp(arg)) fmt_write_integer(out,integer_value(arg),10,atsignp);
              else              princ(arg,stream);
              break;}
          case 'B': {
              Ref arg=fmt_pop(args_ptr);
              if(integerp(arg)) fmt_write_integer(out,integer_value(arg),2,atsignp);
              else              princ(arg,stream);
              break;}
          case 'O': {
              Ref arg=fmt_pop(args_ptr);
              if(integerp(arg)) fmt_write_integer(out,integer_value(arg),8,atsignp);
              else              princ(arg,stream);
              break;}
          case 'X': {
              Ref arg=fmt_pop(args_ptr);
              if(integerp(arg)) fmt_write_integer(out,integer_value(arg),16,atsignp);
              else              princ(arg,stream);
              break;}
          case 'R': {
              Ref arg=fmt_pop(args_ptr);
              int radix=has_pfx ? (int)prefix : 10;
              if(radix<2) radix=2; if(radix>36) radix=36;
              if(integerp(arg)) fmt_write_integer(out,integer_value(arg),radix,atsignp);
              else              princ(arg,stream);
              break;}
          case 'C': {
              Ref ch=fmt_pop(args_ptr);
              if(colonp) prin1(ch,stream);
              else if(characterp(ch)){
                  octet code=(octet)integer_value(character_code(ch));
                  fputc((int)code,out);}
              break;}
          case 'F': case 'E': case 'G': {
              /* ~w,dF — fixed float: w=total width, d=decimal digits.
                 ~dF alone — d decimal digits (no width constraint).
                 ~F  alone — print as-is with default precision. */
              Ref arg = fmt_pop(args_ptr);
              floating v = floatp(arg)  ? float_value(arg)
                         : integerp(arg)? (floating)integer_value(arg)
                         : 0.0;
              if(fmt_has[0] || fmt_has[1]){
                  /* Have explicit parameters */
                  int dec_places = fmt_has[1] ? (int)fmt_params[1]
                                 : fmt_has[0] ? (int)fmt_params[0]
                                 : 6;
                  int width      = (fmt_has[0] && fmt_has[1]) ? (int)fmt_params[0] : 0;
                  char fmtbuf[64];
                  char outbuf[256];
                  if(width > 0)
                      snprintf(fmtbuf,sizeof(fmtbuf),"%%%d.%df",width,dec_places);
                  else
                      snprintf(fmtbuf,sizeof(fmtbuf),"%%.%df",dec_places);
                  snprintf(outbuf,sizeof(outbuf),fmtbuf,(double)v);
                  fputs(outbuf,out);
              } else {
                  /* No explicit parameters: use princ for default output */
                  princ(arg,stream);}
              break;}
          case '%': {
              for(word k=0;k<count;k++) fputc('\n',out);
              break;}
          case '&': {
              for(word k=0;k<count;k++) fputc('\n',out);
              break;}
          case '~': {
              for(word k=0;k<count;k++) fputc('~',out);
              break;}
          case 'P': {
              Ref n=fmt_pop(args_ptr);
              word nv=integerp(n)?integer_value(n):0;
              if(atsignp){
                  fputs(nv==1?"y":"ies",out);
              } else {
                  fputs(nv==1?"":"s",out);}
              break;}
          case 'T': {
              /* ~nT — emit spaces to reach column N (simplified: emit N spaces) */
              for(word k=0;k<count;k++) fputc(' ',out);
              break;}
          case 'I': {
              /* ~nI — indent (bocl: emit N spaces, default 0) */
              for(word k=0;k<count;k++) fputc(' ',out);
              break;}
          case 'N': {
              /* ~N — conditional newline (bocl: always emit one newline) */
              for(word k=0;k<count;k++) fputc('\n',out);
              break;}
          case '*': {
              for(word k=0;k<count;k++) fmt_pop(args_ptr);
              break;}
          case '{': {
              uword close_pos=fmt_find_close(ctrl,i,end);
              uword sub_end  =(close_pos!=(uword)-1) ? close_pos : end;
              Ref list_arg=fmt_pop(args_ptr);
              while(consp(list_arg)){
                  Ref sub_args=list_arg;
                  /* Use sub_args as the arg list for this iteration.
                     Each element of list_arg is consumed one by one. */
                  bocl_format_run(stream,ctrl_string,i,sub_end,&sub_args);
                  /* If nothing was consumed, break to avoid infinite loop. */
                  if(sub_args==list_arg) break;
                  list_arg=sub_args;}
              if(close_pos!=(uword)-1) i=close_pos+2; /* skip ~} */
              break;}
          case '}': goto done; /* end of ~{~} — stop this invocation */
          case '[': {
              /* ~[clause0~;clause1~;...~] — select clause by integer arg.
                 ~:[false~;true~] — boolean: use arg as T/NIL.
                 ~@[clause~]     — if arg non-nil, format clause (leave arg). */
              if(atsignp){
                  /* ~@[clause~] — format clause only if next arg is non-nil */
                  Ref test_arg = consp(*args_ptr) ? car(*args_ptr) : NIL_Symbol;
                  /* find matching ~] */
                  uword cls = i;
                  while(cls < end && !(ctrl[cls]=='~' && cls+1<end &&
                         toupper((unsigned char)ctrl[cls+1])==']'))
                      cls++;
                  if(!nullp(test_arg)){
                      /* do NOT consume the arg — leave it for the clause */
                      bocl_format_run(stream,ctrl_string,i,cls,args_ptr);}
                  else{
                      fmt_pop(args_ptr);}  /* skip the arg */
                  i = (cls < end) ? cls+2 : end;
              } else if(colonp){
                  /* ~:[false~;true~] */
                  Ref bool_arg = fmt_pop(args_ptr);
                  /* find ~; and ~] */
                  uword semi=i; while(semi<end && !(ctrl[semi]=='~' && semi+1<end && ctrl[semi+1]==';')) semi++;
                  uword close=semi+2; while(close<end && !(ctrl[close]=='~' && close+1<end && toupper((unsigned char)ctrl[close+1])==']')) close++;
                  if(nullp(bool_arg)){
                      bocl_format_run(stream,ctrl_string,i,semi,args_ptr);
                  } else {
                      bocl_format_run(stream,ctrl_string,semi+2,close,args_ptr);}
                  i=(close<end)?close+2:end;
              } else {
                  /* ~[c0~;c1~;...~] — pick clause N (integer arg) */
                  Ref n_ref = fmt_pop(args_ptr);
                  word n = integerp(n_ref) ? integer_value(n_ref) : 0;
                  /* We need to collect clause boundaries */
                  /* Simple approach: walk and count ~; separators */
                  uword close_bracket=i;
                  /* reparse: find all ~; and ~] at this nesting level */
                  /* For simplicity, iterate to find the n-th clause */
                  uword pos=i;
                  uword sel_start=i, sel_end=i;
                  bool  found=false;
                  word  idx=0;
                  uword seg_start=i;
                  while(pos<end){
                      if(ctrl[pos]=='~' && pos+1<end){
                          char dc=(char)toupper((unsigned char)ctrl[pos+1]);
                          if(dc==';'){
                              if(idx==n){ sel_start=seg_start; sel_end=pos; found=true; }
                              idx++; pos+=2; seg_start=pos; continue;}
                          if(dc==']'){
                              if(idx==n){ sel_start=seg_start; sel_end=pos; found=true; }
                              close_bracket=pos; pos+=2; break;}}
                      pos++;}
                  if(found) bocl_format_run(stream,ctrl_string,sel_start,sel_end,args_ptr);
                  i=close_bracket<end?close_bracket+2:end;}
              break;}
          case ']': goto done; /* end of ~[~] */
          case '?': {
              /* ~? — indirect: pop ctrl-string and args-list from arg stack */
              Ref sub_ctrl = fmt_pop(args_ptr);
              Ref sub_args = fmt_pop(args_ptr);
              if(stringp(sub_ctrl)){
                  uword sc_len=string_length(sub_ctrl);
                  bocl_format_run(stream,sub_ctrl,0,sc_len,&sub_args);}
              break;}
          case '^': {  /* ~^ — escape upward: stop if no more args remain */
              if(nullp(*args_ptr)) goto done;
              break;}
          default:  break;}} /* unknown — silently ignore */
done:
    return *args_ptr;}

void kernel_FORMAT_RUN(Ref function,Ref arguments,MultipleValues* results){
    uword pool=open_live_pool();
    add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref par_stream      = pop_mandatory_argument(function,&current);
    Ref par_ctrl        = pop_mandatory_argument(function,&current);
    Ref par_start_ref   = pop_mandatory_argument(function,&current);
    Ref par_end_ref     = pop_mandatory_argument(function,&current);
    Ref par_args        = pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    uword start = (uword)integer_value(par_start_ref);
    uword end   = (uword)integer_value(par_end_ref);
    bocl_format_run(par_stream,par_ctrl,start,end,&par_args);
    results->count=1;
    results->values[0]=NIL_Symbol;
    close_live_pool(pool);}

/**** THE END ****/
