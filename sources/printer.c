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

Ref prin1(Ref object,Ref stream){
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

    if(integerp(object)){
        fprintf(out,"%"WORD_FORMAT,integer_value(object));
        return object;}

    if(ratiop(object)){
        fprintf(out,"%"WORD_FORMAT"/%"WORD_FORMAT,
                integer_value(ratio_numerator(object)),
                integer_value(ratio_denominator(object)));
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
        princ(symbol_name(object),stream);
        return object;}

    if(symbolp(object)){
        /* prin1 qualifies a symbol by package when it is not accessible as
           itself in *PACKAGE*, so the written form reads back to the same symbol
           (needed for fasls and readable output across packages). */
        Ref pkgname; int sep;
        symbol_print_prefix(object,&pkgname,&sep);
        if(sep==3){ fprintf(out,"#:"); }
        else if(sep!=0){
            princ(pkgname,stream);
            fprintf(out, sep==1 ? ":" : "::"); }
        princ(symbol_name(object),stream);
        return object;}

    if(consp(object)){
        /* Respect *PRINT-LENGTH* — truncate after this many elements. */
        Ref plen_ref = variable_value(S(*PRINT-LENGTH*));
        word print_length = (integerp(plen_ref)) ? integer_value(plen_ref) : -1;
        Ref current=object;
        fprintf(out,"(");
        word elem=0;
        prin1(car(current),stream); elem++;
        current=cdr(current);
        while(consp(current)){
            if(print_length >= 0 && elem >= print_length){
                fprintf(out," ...");
                break;}
            fprintf(out," ");
            prin1(car(current),stream);
            current=cdr(current);
            elem++;}
        if(!nullp(current) && !consp(current)){
            fprintf(out," . ");
            prin1(current, stream);}
        fprintf(out,")");
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
        fprintf(out,"#*");
        uword blen = bit_vector_length(object);
        for(uword i=0; i<blen; i++){
            /* Use the safe bit() accessor which handles all layout details. */
            Ref bval = bit(object, integer_from_word((word)i));
            fprintf(out,"%ld",(long)integer_value(bval));}
        return object;}

    if(vectorp(object)){
        /* A vector prints as #(e1 e2 …). Vectors are
           Array-backed under the hood, so iterate via array.elements. */
        fprintf(out,"#(");
        uword len = vector_length(object);
        for(uword i=0; i<len; i++){
            if(i>0){ fprintf(out," "); }
            prin1(object->slots->array.elements->elements[i], stream);}
        fprintf(out,")");
        return object;}

    /* Pathnames print in their readable #P"namestring" form (both physical and
       logical pathnames are PATHNAME_Class). */
    if(typep(object,PATHNAME_Class)){
        fprintf(out,"#P");
        prin1(namestring(object),stream);   /* prints the namestring as "…" */
        return object;}

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

    if(integerp(object)){
        fprintf(out,"%"WORD_FORMAT,integer_value(object));
        return object;}

    if(ratiop(object)){
        fprintf(out,"%"WORD_FORMAT"/%"WORD_FORMAT,
                integer_value(ratio_numerator(object)),
                integer_value(ratio_denominator(object)));
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

    if(keywordp(object)){
        fputc(':',out);
        princ(symbol_name(object),stream);
        return object;}

    if(symbolp(object)){
        princ(symbol_name(object),stream);
        return object;}

    if(stringp(object)){
        char* p=string_cstring(object);
        fprintf(out,"%s",p);
        return object;}

    if(consp(object)){
        Ref current=object;
        fprintf(out,"(");
        princ(car(current),stream);
        current=cdr(current);
        while(consp(current)){
            fprintf(out," ");
            princ(car(current),stream);
            current=cdr(current);}
        if(!nullp(current)){
            fprintf(out," . ");
            princ(current, stream);}
        fprintf(out,")");
        return object;}

    /* PRINC of a pathname writes its namestring (no #P"…" escaping). */
    if(typep(object,PATHNAME_Class)){
        princ(namestring(object),stream);
        return object;}

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
