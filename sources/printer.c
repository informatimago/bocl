#include <ctype.h>
#include "printer.h"
#include "kernel.h"
#include "kernel_objects.h"
#include "macros.h"
#include "data.h"
#include "variable.h"
#include "stream.h"
#include "bostring.h"
#include "character.h"

/* Printer */

Ref prin1_to_string(Ref object){
    Ref stream=stream_output_string(S(CHARACTER));
    prin1(object,stream);
    return string_output_stream_get_string(stream);}

Ref print(Ref object,Ref stream){
    terpri(stream);
    prin1(object,stream);
    return object;}

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

    /* if(nullp(object)){ */
    /*     fprintf(out,"NIL"); */
    /*     return object;} */

    if(keywordp(object) ){
        fprintf(out,":");
        princ(symbol_name(object),stream);
        return object;}

    if(symbolp(object)){
        princ(symbol_name(object),stream);
        return object;}

    if(consp(object)){
        Ref current=object;
        fprintf(out,"(");
        prin1(car(current),stream);
        current=cdr(current);
        while(consp(current)){
            fprintf(out," ");
            prin1(car(current),stream);
            current=cdr(current);}
        if(!nullp(current)){
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

    if(vectorp(object)){
        /* Pre-fix this branch used car/cdr on the vector and crashed at
           "got VECTOR, expected LIST" the first time anyone called
           (vector …). A vector prints as #(e1 e2 …). Vectors are
           Array-backed under the hood, so iterate via array.elements. */
        fprintf(out,"#(");
        uword len = vector_length(object);
        for(uword i=0; i<len; i++){
            if(i>0){ fprintf(out," "); }
            prin1(object->slots->array.elements->elements[i], stream);}
        fprintf(out,")");
        return object;}

    fprintf(out,"#<");
    prin1(class_name(class_of(object)),stream);

    if(built_in_function_p(object)){
        fprintf(out," ");
        prin1(function_name(object),stream);}
    /* else if(){ */
    /* } */
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

    /* if(nullp(object)){ */
    /*     fprintf(out,"NIL"); */
    /*     return object;} */

    if(keywordp(object)||symbolp(object)){
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
        /* --- parse numeric prefix --- */
        word  prefix   = 0;
        bool  has_pfx  = false;
        bool  colonp   = false;
        bool  atsignp  = false;
        bool  neg_pfx  = false;
        if(i<end && ctrl[i]=='-'){ neg_pfx=true; i++;}
        while(i<end && ctrl[i]>='0' && ctrl[i]<='9'){
            prefix = prefix*10 + (ctrl[i]-'0');
            has_pfx=true; i++;}
        if(neg_pfx && has_pfx) prefix=-prefix;
        /* ~v: take prefix from next arg */
        if(i<end && toupper((unsigned char)ctrl[i])=='V'){
            Ref a=fmt_pop(args_ptr);
            prefix=integerp(a)?integer_value(a):0;
            has_pfx=true; i++;}
        /* ~#: prefix = count of remaining args */
        if(i<end && ctrl[i]=='#'){
            word cnt=0; for(Ref p=*args_ptr;consp(p);p=cdr(p)) cnt++;
            prefix=cnt; has_pfx=true; i++;}
        /* modifiers */
        while(i<end && (ctrl[i]==':' || ctrl[i]=='@')){
            if(ctrl[i]==':') colonp  =true;
            else             atsignp =true;
            i++;}
        if(i>=end) break;
        char   d=(char)toupper((unsigned char)ctrl[i]); i++;
        word  count=has_pfx ? prefix : 1;

        switch(d){
          case 'A': {   /* PRINC */
              Ref arg=fmt_pop(args_ptr);
              if(colonp && nullp(arg)) fputs("()",out);
              else princ(arg,stream);
              break;}
          case 'S': case 'W': {  /* PRIN1 */
              prin1(fmt_pop(args_ptr),stream);
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
          case 'F': case 'E': case 'G': {  /* float — simplified */
              princ(fmt_pop(args_ptr),stream);
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
              for(word k=0;k<count;k++) fputc(' ',out);
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
