#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "reader.h"
#include "macros.h"
#include "kernel.h"
#include "packages.h"
#include "variable.h"
#include "boerror.h"
#include "cons.h"
#include "bostring.h"
#include "character.h"
#include "stream.h"

/* The recursive_p parameters are unused, since we don't deal with #=/## references */


/*
A minimal lisp reader, with hard-wired readtable must be provided to
read CL library lisp files.

- parse symbols without package qualifier, and without escapes.
- parse keywords with a colon prefix.
- parse integers
- parse floating-point numbers

Hard-wired reader macros:

| character | lisp object         | example                         |
|-----------+---------------------+---------------------------------|
| ;       | comments            | ; foo                         |
| '       | quote               | 'foo                          |
| "       | strings             | "foo"  \\ and \" escapes. |
| (       | lists               | (f o o) (a . b) (a b . c)    |
| #\      | characters          | #\f                           |
| #(      | vectors             | #(f o o)                      |
| #*      | bit-vectors         | #*101010                      |



numeric-token  ::=  integer |
                                   ratio   |
                                   float
integer        ::=  [sign]
                                   decimal-digit+
                                   decimal-point |
                                   [sign]
                                   digit+
ratio          ::=  [sign]
                                   {digit}+
                                   slash
                                   {digit}+
float          ::=  [sign]
                                   {decimal-digit}*
                                   decimal-point
                                   {decimal-digit}+
                                   [exponent]
                    |
                                   [sign]
                                   {decimal-digit}+
                                   [decimal-point
                                           {decimal-digit}*]
                                   exponent
exponent       ::=  exponent-marker
                                   [sign]
                                   {digit}+

sign---a sign.
slash---a slash
decimal-point---a dot.
exponent-marker---an exponent marker.
decimal-digit---a digit in radix 10.
digit---a digit in the current input radix.


integer
[-+]?[0-9]+\.?

float
[-+]?[0-9]*(\.[0-9]+)?([DdEeFfLlSs][-+]?[0-9]+)?
[-+]?[0-9]+(\.[0-9]*)?[DdEeFfLlSs][-+]?[0-9]+

*/

typedef struct parser_state {
    Ref input_stream;
    size_t length;
    octet ch;
    octet filler;
    bool eof;
    bool error;
    bool escape;
    bool suppress;   /* *READ-SUPPRESS*: reading a #+/#- discarded form */
    octet token[ARRAY_TOTAL_SIZE_LIMIT+1];
} parser_state;

#define CR  13    /* #\Return */
#define LF  10    /* #\Linefeed */
#define NEL 0x85  /* #\Newline */

static octet read_octet(Ref input_stream,bool* eof,bool* error){
    FILE* cstream=stream_cstream(input_stream);
    clearerr(cstream);
    int c=getc(cstream);
    if(c==EOF){
        (*error)=(0!=ferror(cstream));
        (*eof)=(0!=feof(cstream));}
    else{
        if(type_character==stream_type(input_stream)){
            switch(c){
              case CR:
                  {
                      c=NEL;
                      int next=getc(cstream);
                      if((next!=EOF)&&(next!=LF)){
                          ungetc(next,cstream);}}
                  break;
              case LF:
                  c=NEL;
                  break;
              default:
                  break;}}
        (*error)=false;
        (*eof)=false;}
    return (octet)c;}

static octet peek_octet(Ref input_stream,bool* eof,bool* error){
    FILE* cstream=stream_cstream(input_stream);
    clearerr(cstream);
    int c=getc(cstream);
    if(c==EOF){
        (*error)=(0!=ferror(cstream));
        (*eof)=(0!=feof(cstream));}
    else{
        if(type_character==stream_type(input_stream)){
            switch(c){
              case CR:
                  {
                      c=NEL;
                      int next=getc(cstream);
                      if(next==EOF){
                          ungetc(CR,cstream);}
                      else if(next!=LF){
                          ungetc(next,cstream);}}
                  break;
              case LF:
                  c=NEL;
                  ungetc(LF,cstream);
                  break;
              default:
                  ungetc(c,cstream);}}
        else{
            ungetc(c,cstream);}
        (*error)=false;
        (*eof)=false;}
    return (octet)c;}

static void check_parser_error(parser_state* parser){
    if(parser->error){
        STREAM_ERROR(parser->input_stream);}}

static void check_token_too_long(parser_state* parser,const char* what){
    if(ARRAY_TOTAL_SIZE_LIMIT<=parser->length){
        READER_ERROR(parser->input_stream,"%s too long (more than %lu)",
                     what,ARRAY_TOTAL_SIZE_LIMIT);}}

/* if(parser->eof){ */
/*     END_OF_FILE(parser->input_stream);} */

static octet advance_character(parser_state* parser){
    return ((parser->ch=read_octet(parser->input_stream,&parser->eof,&parser->error)));}

static void skip_spaces(parser_state* parser){
    while(!parser->eof && !parser->error && (whitespacep(parser->ch) || (parser->ch==';'))){
        if(parser->ch==';'){
            while(!parser->eof && !parser->error && (parser->ch!=NEL)){
                advance_character(parser);}}
        else{
            advance_character(parser);}}
    check_parser_error(parser);}

static void parse_character_name(parser_state* parser){
    parser->length=0;
    while(!(parser->eof) && ((parser->ch=='-')
                             ||(parser->ch=='_')
                             ||isalnum(parser->ch))){
        check_token_too_long(parser,"Character name");
        parser->token[parser->length++]=parser->ch;
        advance_character(parser);
        check_parser_error(parser);}}

static Ref parse_character(parser_state* parser){
    /* We've read #\   We need to read the character or character name */
    advance_character(parser);
    check_parser_error(parser);
    if(parser->eof){
        return NIL_Symbol;}
    if(isalpha(parser->ch)){
        parse_character_name(parser);
        parser->token[parser->length]='\0';
        if(parser->length==1){
            /* 1-non-alpha char */
            parser->eof=false;
            return octet_character(parser->token[0]);}
        else{
            Ref character=name_character(string_new_cstring((char*)parser->token));
            if(nullp(character)){
                READER_ERROR(parser->input_stream,"Invalid character name %s",parser->token);}
            else{
                parser->eof=false;
                return character;}}}
    else{
        /* 1-non-alpha char */
        if(printablep(parser->ch)){
            Ref ch_obj=octet_character(parser->ch);
            /* Consume the character so the caller's parse_list / parse_token
               doesn't see #\( as an open paren etc. (Pre-fix, #\( inside a
               list was read as #\ plus an unmatched paren.) */
            advance_character(parser);
            parser->eof=false;
            return ch_obj;}
        else{
            READER_ERROR(parser->input_stream,"Invalid character after #\\ -- code 0x%02x (%d)",
                         parser->ch,parser->ch);}}}

static Ref parse_string(parser_state* parser){
    /* We've read opening "  we need to read the string until closing " */
    parser->length=0;
    parser->escape=false;
    while(true){
        advance_character(parser);
        check_parser_error(parser);
        if(parser->eof){
            /* in the middle of a string */
            return NIL_Symbol;}
        if(parser->escape){
            parser->escape=false;
            check_token_too_long(parser,"String");
            parser->token[parser->length++]=parser->ch;}
        else{
            switch(parser->ch){
              case '\\':
                  parser->escape=true;
                  break;
              case '"':
                  parser->token[parser->length]='\0';
                  /* Consume the closing '"' so the caller's state stays consistent
                     (parse_token leaves ch at the post-token char; do the same). */
                  advance_character(parser);
                  parser->eof=false;
                  return string_new_cstring((char*)parser->token);
              default:
                  check_token_too_long(parser,"String");
                  parser->token[parser->length++]=parser->ch;}}}}

static Ref parse_object(parser_state* parser);

/* Readtable support (defined near reader_initialize). */
static Ref g_standard_readtable, g_standard_macro, g_dispatch_marker;
static Ref current_readtable(void);
static Ref rt_macro(Ref rt,int ch);
static Ref rtv_get(Ref vec,int i);
static bool token_continues(Ref rt,int ch);

/* Recursively expand a backquoted form into LIST / APPEND / CONS / QUOTE
   nodes. The reader uses tagged conses (UNQUOTE x) and (UNQUOTE-SPLICING x)
   for `,x` and `,@x`; this function eliminates them. Only one level of
   backquote is supported — nested ``…`x…`` is not. */
static Ref backquote_expand(Ref form);

static Ref backquote_element(Ref element){
    if(consp(element)){
        Ref head = car(element);
        if(head == S(UNQUOTE)){
            /* `,x` inside a list → (LIST x) at that position. */
            return list(S(LIST), cadr(element), NULL);}
        if(head == S(UNQUOTE-SPLICING)){
            /* `,@x` inside a list → x itself, to be spliced via APPEND. */
            return cadr(element);}}
    return list(S(LIST), backquote_expand(element), NULL);}

static Ref backquote_expand(Ref form){
    if(nullp(form)){
        return list(S(QUOTE), NIL_Symbol, NULL);}
    if(!consp(form)){
        return list(S(QUOTE), form, NULL);}
    Ref head = car(form);
    if(head == S(UNQUOTE)){
        /* `,x` at the top of the backquote — produce x directly. */
        return cadr(form);}
    if(head == S(UNQUOTE-SPLICING)){
        /* `,@x` at top-level is illegal in CL — flag at eval time. */
        return list(S(QUOTE), form, NULL);}
    /* Otherwise: iterate. Each element becomes either (LIST elt-expansion)
       or (LIST x) for ,x or x for ,@x. Combine with APPEND. */
    Ref parts = NIL_Symbol;
    Ref tail = form;
    while(consp(tail)){
        push(backquote_element(car(tail)), &parts);
        tail = cdr(tail);}
    parts = nreverse(parts);
    if(!nullp(tail)){
        /* Dotted tail: backquote it and append. */
        Ref tail_expansion = backquote_expand(tail);
        Ref new_parts = NIL_Symbol;
        Ref p = parts;
        while(consp(p)){ push(cons_car(p), &new_parts); p = cons_cdr(p); }
        push(tail_expansion, &new_parts);
        parts = nreverse(new_parts);}
    return cons(S(APPEND), parts);}

/* Unique sentinel returned by #+/#- when the feature test says skip.
   parse_list, parse_vector and kernel_read filter it out and keep
   reading. Allocated lazily on first use; not in any package, not
   reachable from user code. */
static Ref reader_skip_marker = NULL;
static Ref get_skip_marker(void){
    if(reader_skip_marker == NULL){
        reader_skip_marker = cons(NIL_Symbol, NIL_Symbol);
        push_to_rootset(reader_skip_marker);}
    return reader_skip_marker;}

/* CL reads the feature expression with *PACKAGE* bound to KEYWORD, so a
   bare `bocl` in `#+bocl` means `:bocl`. We don't switch packages while
   reading; instead, we recursively coerce non-operator symbols to keywords
   before testing. AND/OR/NOT stay as ordinary symbols. */
static Ref coerce_feature_expr(Ref expr){
    if(symbolp(expr)){
        if(expr == S(AND) || expr == S(OR) || expr == S(NOT)){ return expr; }
        if(keywordp(expr)){ return expr; }
        char* name = string_cstring(symbol_name(expr));
        Ref kw = keyword_intern_cstring(name);
        free(name);
        return kw;}
    if(consp(expr)){
        Ref head = coerce_feature_expr(car(expr));
        Ref tail_new = NIL_Symbol;
        Ref it = cdr(expr);
        while(consp(it)){
            push(coerce_feature_expr(car(it)), &tail_new);
            it = cdr(it);}
        tail_new = nreverse(tail_new);
        return cons(head, tail_new);}
    return expr;}

/* Check whether a feature expression is satisfied against *FEATURES*.
   The expression is either a keyword/symbol (member of *FEATURES*), or
   (AND e1 e2 …), (OR e1 e2 …), (NOT e). */
static bool feature_present_p(Ref expr){
    if(symbolp(expr)){
        Ref feats = variable_value(S(*FEATURES*));
        return !nullp(memq(expr, feats));}
    if(consp(expr)){
        Ref head = car(expr);
        Ref tail = cdr(expr);
        if(head == S(AND)){
            while(consp(tail)){
                if(!feature_present_p(car(tail))){ return false; }
                tail = cdr(tail);}
            return true;}
        if(head == S(OR)){
            while(consp(tail)){
                if(feature_present_p(car(tail))){ return true; }
                tail = cdr(tail);}
            return false;}
        if(head == S(NOT)){
            return !feature_present_p(car(tail));}}
    return false;}

static Ref parse_list(parser_state* parser){
    /* We've read opening (  we need to read objects until closing ).
       Supports dotted-pair notation: (a b . cdr) */
    Ref result=NIL_Symbol;
    Ref dotted_cdr=NIL_Symbol;
    bool is_dotted=false;
    advance_character(parser);
    skip_spaces(parser);
    /* EOF before the closing ')' means the object ended in the middle of a list
       — signal END-OF-FILE rather than looping (PARSE_OBJECT defers EOF after a
       token, so parser->eof alone is unreliable here; consult the stream). */
    if(parser->eof || feof(stream_cstream(parser->input_stream))){
        END_OF_FILE(parser->input_stream);}
    while(parser->ch!=')'){
        /* Detect dotted-pair separator: a lone '.' followed by a non-constituent
           character (whitespace, close-paren, string, etc.).  The token ".5" or
           ".." is NOT a separator — constituentp('.') is true so they continue. */
        if(parser->ch=='.'){
            bool eof2=false,err2=false;
            octet next=peek_octet(parser->input_stream,&eof2,&err2);
            if(eof2 || !constituentp(next)){
                /* Consume the dot, skip whitespace, read the cdr object. */
                advance_character(parser);
                skip_spaces(parser);
                if(!parser->eof){
                    dotted_cdr=parse_object(parser);
                    is_dotted=true;}
                skip_spaces(parser);
                break;}}
        Ref element=parse_object(parser);
        if(element != reader_skip_marker){
            push(element,&result);}
        skip_spaces(parser);
        if(parser->eof || feof(stream_cstream(parser->input_stream))){
            END_OF_FILE(parser->input_stream);}}
    /* Consume the closing ')' so an outer parse_list doesn't see it as its own. */
    advance_character(parser);
    parser->eof=false;
    if(is_dotted){
        /* nreconc: reverse result in-place with dotted_cdr as the final cdr. */
        Ref head=dotted_cdr;
        while(consp(result)){
            Ref next=cdr(result);
            rplacd(result,head);
            head=result;
            result=next;}
        return head;}
    return nreverse(result);}

static Ref parse_vector(parser_state* parser){
    Ref elements=parse_list(parser);
    if(parser->eof){
        return NIL_Symbol;}
    return vector_new(elements);}

static Ref parse_bit_vector(parser_state* parser){
    parser->length=0;
    while(true){
        advance_character(parser);
        check_parser_error(parser);
        if(parser->eof){
            /* EOF, we're done with the vector, postpone EOF */
            parser->eof=false;
            return bit_vector_new_octets(CHECK_SIZE_TO_UWORD(parser->length),parser->token);}
        switch(parser->ch){
          case '0':
          case '1':
              check_token_too_long(parser,"Bit vector");
              parser->token[parser->length++]=parser->ch&1;
              break;
          default:
              /* A non-bit character terminates the bit run.  Leave it in
                 parser->ch as the one-char lookahead — exactly like parse_token —
                 and do NOT push it back: the caller (parse_list) and the
                 top-level read already treat parser->ch as the terminating char.
                 An ungetc here would DUPLICATE the delimiter (it would sit both
                 in parser->ch and back in the stream), desyncing the next read
                 and surfacing as a spurious stray ')' on the following form. */
              return bit_vector_new_octets(CHECK_SIZE_TO_UWORD(parser->length),parser->token);}}}

/* The value of a digit character in BASE (2..36), or -1 if it is not a digit. */
static int digit_in_base(int c,int base){
    int d;
    if(c>='0'&&c<='9'){ d=c-'0'; }
    else if(c>='a'&&c<='z'){ d=c-'a'+10; }
    else if(c>='A'&&c<='Z'){ d=c-'A'+10; }
    else { return -1; }
    return (d<base)?d:-1;}

/* The current input radix (*READ-BASE*), clamped to 2..36, default 10. */
static int current_read_base(void){
    Ref rb=symbol_value(READ_BASE);
    if(integerp(rb)){ word b=integer_value(rb); if(b>=2 && b<=36){ return (int)b; } }
    return 10;}

/* scan_ratio: recognise "[-+]?digits/digits" in the current radix. */
static Ref scan_ratio(parser_state* parser){
    const char* token = (char*)parser->token;
    int base=current_read_base();
    const char* slash = strchr(token, '/');
    if(!slash || slash==token) return NIL_Symbol;  /* no slash or at start */
    const char* p = token;
    if(*p=='-'||*p=='+') p++;
    if(p>=slash) return NIL_Symbol;                /* no numerator digits */
    while(p < slash){
        if(digit_in_base((unsigned char)*p,base)<0) return NIL_Symbol;
        p++;}
    const char* q = slash+1;
    if(*q=='\0') return NIL_Symbol;
    while(*q){
        if(digit_in_base((unsigned char)*q,base)<0) return NIL_Symbol;
        q++;}
    /* Parse each half as a fixnum-or-bignum so ratios whose numerator or
       denominator overflows a word read back exactly (e.g. 2/123…321).  The
       numerator substring keeps its optional leading sign; parse_bignum_cstring
       handles the sign and demotes to a fixnum when the value fits. */
    size_t num_len = (size_t)(slash - token);
    char* num_str = malloc(num_len + 1);
    for(size_t i=0;i<num_len;i++){ num_str[i]=token[i]; }
    num_str[num_len]='\0';
    Ref num = parse_bignum_cstring(num_str, base);
    free(num_str);
    Ref den = parse_bignum_cstring(slash+1, base);
    if(nullp(num) || nullp(den)){ return NIL_Symbol; }
    if(integerp(den) && integer_value(den)==0){ return NIL_Symbol; }  /* x/0: not a ratio */
    return ratio_new(num, den);}

static Ref scan_integer(parser_state* parser){
    /* [-+]?digits+\.?  in the current radix (*READ-BASE*).  Sign is allowed ONLY
       at position 0.  A trailing '.' forces base 10 (CL: "123." is decimal). */
    const char* token=(char*)parser->token;
    int base=current_read_base();
    size_t len=0; while(token[len]){ len++; }
    if(len>0 && token[len-1]=='.'){ base=10; }   /* decimal-point integer */
    word sign=1;
    size_t pos=0;
    if(token[0]=='-'){ sign=-1; pos=1; }
    else if(token[0]=='+'){ sign=+1; pos=1; }
    bool value_set=false;
    word value=0;
    while(token[pos]){
        if(token[pos]=='.' && token[pos+1]=='\0'){ pos++; break; }   /* trailing . */
        int digit=digit_in_base((unsigned char)token[pos],base);
        if(digit<0){ return NIL_Symbol; }
        if((WORD_MAX/base<value)
            ||((WORD_MAX/base==value)&&(WORD_MAX-base*(WORD_MAX/base)<=(word)digit))){
            /* Overflow: build a BIGNUM from the whole token via the C engine
               (it handles the optional sign and trailing decimal point). */
            return parse_bignum_cstring(token, base);}
        value=(value*base)+digit;
        value_set=true;
        pos++;}
    return (value_set)
            ?integer_from_word(sign*value)
            :NIL_Symbol;}

static Ref scan_float(parser_state* parser){
    /* [-+]?[0-9]*(\.[0-9]+)?([DdEeFfLlSs][-+]?[0-9]+)? */
    /* [-+]?[0-9]+(\.[0-9]*)?[DdEeFfLlSs][-+]?[0-9]+ */
    const char* token=(char*)parser->token;
    size_t pos=0;
    switch(token[pos]){
      case '+':case '-':
          pos++;
          break;
      default:
          break;}
    if(token[pos]=='.'){
        pos++;
        if(!isdigit(token[pos])){
            return NIL_Symbol;}
        while(isdigit(token[pos])){
            pos++;}}
    else{
        if(!isdigit(token[pos])){
            return NIL_Symbol;}
        while(isdigit(token[pos])){
            pos++;}
        if(token[pos]=='.'){
            pos++;
            while(isdigit(token[pos])){
                pos++;}}}
    if(token[pos]!='\0'){
        switch(token[pos]){
          case 'D':case'E':case'F':case'L':case'S':
          case 'd':case'e':case'f':case'l':case's':
              pos++;
              break;
          default:
              return NIL_Symbol;}
        switch(token[pos]){
          case'+': case'-':
              pos++;
              break;
          default:
              break;}
        if(!isdigit(token[pos])){
            return NIL_Symbol;}
        while(isdigit(token[pos])){
            pos++;}}
    if(token[pos]!='\0'){
        return NIL_Symbol;}
    floating value;
    int n=sscanf(token,"%le",&value);
    if(n!=1){
        return NIL_Symbol;}
    return float_from_floating(value);}

/* Fold an (unescaped) token's case per the current readtable's READTABLE-CASE,
   in place; returns the token.  :INVERT inverts a single-case token and leaves a
   mixed-case one untouched. */
static char* apply_readtable_case(octet* token){
    Ref rc = current_readtable()->slots->readtable.readcase;
    if(rc == K(DOWNCASE)){ return (char*)octet_downcase(token); }
    if(rc == K(PRESERVE)){ return (char*)token; }
    if(rc == K(INVERT)){
        bool up=false, lo=false;
        for(octet* p=token; *p; p++){
            if(*p>='A'&&*p<='Z'){ up=true; }
            else if(*p>='a'&&*p<='z'){ lo=true; }}
        if(up && !lo){ return (char*)octet_downcase(token); }
        if(lo && !up){ return (char*)octet_upcase(token); }
        return (char*)token;}                    /* mixed or no letters → preserve */
    return (char*)octet_upcase(token);           /* :UPCASE (the default) */
}

static Ref scan_symbol(parser_state* parser,bool* valid){
    /* Under *READ-SUPPRESS* (reading a #+/#- discarded form) the token is
       consumed but produces no symbol: don't intern, and don't error on an
       unknown package qualifier. */
    if(parser->suppress){ (*valid)=true; return NIL_Symbol; }
    const char* token=apply_readtable_case(parser->token);
    const char* colon=strchr(token,':');
    (*valid)=false;
    /* No colon: a plain symbol interned in the current package (*PACKAGE*),
       following its use-list. */
    if(!colon){
        (*valid)=true;
        return package_intern_current_cstring(token);}
    /* Leading colon: a keyword (:foo). */
    if(colon==token){
        if(token[1]==':'){
            /* "::foo" is invalid (empty package). */
            return NIL_Symbol;}
        (*valid)=true;
        return keyword_intern_cstring(token+1);}
    /* Mid-token colon: pkg:sym (one colon — sym must be external in pkg) or
       pkg::sym (two colons — internal access, interns in pkg). */
    size_t pkg_len = (size_t)(colon - token);
    char pkg_buf[256];
    if(pkg_len >= sizeof(pkg_buf)){
        return NIL_Symbol;}
    for(size_t i=0;i<pkg_len;i++){ pkg_buf[i]=token[i]; }
    pkg_buf[pkg_len]='\0';
    bool internal_access = (colon[1]==':');
    const char* name_part = internal_access ? colon+2 : colon+1;
    if(*name_part=='\0' || strchr(name_part,':')){
        return NIL_Symbol;}
    (*valid)=true;
    Ref pkg = package_find_cstring(pkg_buf);
    if(nullp(pkg)){
        ERROR("READ: there is no package named %s",pkg_buf);}
    if(pkg == KEYWORD_Package){
        return keyword_intern_cstring(name_part);}
    if(internal_access){
        return package_intern_cstring_in(name_part,pkg);}   /* pkg::sym */
    /* pkg:sym — the symbol must be present AND external in pkg.  NULL (not
       NIL_Symbol) means not-found, so reading CL:NIL yields the symbol NIL
       rather than spuriously erroring. */
    Ref sym = package_find_external_cstring(name_part,pkg);
    if(!sym){
        ERROR("READ: symbol %s is not external in package %s",name_part,pkg_buf);}
    return sym;}

/* CL upcase of a single octet (matches octet_upcase in bostring.c). */
static octet reader_upcase1(octet c){
    if(((97<=c)&&(c<=122)) || ((224<=c)&&(c<=254)&&(c!=247))){ return (octet)(c-32); }
    return c;}

/* CL downcase of a single octet (mirror of reader_upcase1; matches octet_downcase). */
static octet reader_downcase1(octet c){
    if(((65<=c)&&(c<=90)) || ((192<=c)&&(c<=222)&&(c!=215))){ return (octet)(c+32); }
    return c;}

/* Apply the current readtable's READTABLE-CASE to the UNESCAPED characters of an
   already-tokenized symbol name: a character is folded only when esc[i] is false;
   escaped characters (esc[i] true) keep their literal case.  This is the
   escape-aware twin of apply_readtable_case (used by the non-escaped path): under
   :UPCASE/:DOWNCASE each unescaped letter folds individually; :PRESERVE leaves the
   name verbatim; :INVERT inverts the unescaped letters only when they are uniformly
   one case (mixed or none → preserved), considering ASCII letters as the printer
   does.  Package-marker colons are unescaped non-letters, so they pass through. */
static void reader_case_unescaped(octet* token, halfword len, const bool* esc){
    Ref rc = current_readtable()->slots->readtable.readcase;
    if(rc == K(PRESERVE)){ return; }
    if(rc == K(INVERT)){
        bool up=false, lo=false;
        for(halfword i=0;i<len;i++){
            if(esc[i]){ continue; }
            octet c=token[i];
            if(c>='A'&&c<='Z'){ up=true; }
            else if(c>='a'&&c<='z'){ lo=true; }}
        if(up==lo){ return; }                 /* mixed-case or no letters → preserve */
        for(halfword i=0;i<len;i++){
            if(esc[i]){ continue; }
            octet c=token[i];
            if(c>='A'&&c<='Z'){ token[i]=(octet)(c+32); }
            else if(c>='a'&&c<='z'){ token[i]=(octet)(c-32); }}
        return;}
    bool down = (rc == K(DOWNCASE));
    for(halfword i=0;i<len;i++){
        if(esc[i]){ continue; }
        token[i] = down ? reader_downcase1(token[i]) : reader_upcase1(token[i]);}
}

static Ref parse_token(parser_state* parser){
    Ref rt=current_readtable();
    parser->length=0;
    /* `escaped` becomes true at the first single-escape (\) or multi-escape (|…|)
       in the token: such a token is unconditionally a SYMBOL.  Escaped characters
       keep their literal case; unescaped ones still take the readtable case.  We
       store every character verbatim, recording in esc[] which positions came from
       an escape, then fold the unescaped characters per READTABLE-CASE in one pass
       once the whole token is in hand (reader_case_unescaped) — :INVERT needs the
       full token to decide case uniformity, so we cannot fold a character early. */
    bool escaped=false;
    bool esc[ARRAY_TOTAL_SIZE_LIMIT+1];   /* esc[i] = token[i] came from an escape */
    /* Positions of the first (and immediately-following second) UNESCAPED colon:
       these are package markers even in an escaped token, so :|a| is the keyword
       |a|, not a symbol named ":a". -1 = none seen. */
    int colon1=-1, colon2=-1;
    while(!parser->error && !parser->eof){
        if(parser->ch=='\\'){
            escaped=true;
            advance_character(parser);
            if(parser->eof){
                READER_ERROR(parser->input_stream,"Single escape \\ at end of input%s","");}
            check_token_too_long(parser,"Token");
            esc[parser->length]=true;
            parser->token[parser->length++]=parser->ch;   /* escaped: verbatim */
            advance_character(parser);}
        else if(parser->ch=='|'){
            escaped=true;
            for(;;){
                advance_character(parser);
                if(parser->eof){
                    READER_ERROR(parser->input_stream,"Unterminated |...| symbol%s","");}
                if(parser->ch=='\\'){
                    advance_character(parser);   /* the escaped char, taken literally */
                    if(parser->eof){
                        READER_ERROR(parser->input_stream,"Unterminated |...| symbol%s","");}}
                else if(parser->ch=='|'){ break; }
                check_token_too_long(parser,"Token");
                esc[parser->length]=true;
                parser->token[parser->length++]=parser->ch;}
            advance_character(parser);}           /* past the closing | */
        else if(token_continues(rt,parser->ch)){
            check_token_too_long(parser,"Token");
            if(parser->ch==':'){
                if(colon1<0){ colon1=(int)parser->length; }
                else if(colon2<0 && (int)parser->length==colon1+1){ colon2=(int)parser->length; }}
            esc[parser->length]=false;
            parser->token[parser->length++]=parser->ch;   /* unescaped: case-folded below */
            advance_character(parser);}
        else{ break; }}
    /* A token terminated by end-of-file is still a complete token: clear the EOF
       flag (as parse_list does after ')') so kernel_read returns the object
       instead of signalling end-of-file.  e.g. (read-from-string "foo"), or a
       reader macro doing (read stream) on the last token before EOF. */
    if(parser->eof && parser->length>0){ parser->eof=false; }
    check_parser_error(parser);
    parser->token[parser->length]='\0';
    if(escaped){
        /* Escaped tokens are always symbols.  Fold the unescaped characters per
           READTABLE-CASE (escaped ones stay verbatim); after this the name is
           correctly cased, so no number scan or further folding.  An UNESCAPED
           colon is still a package marker (so :|a| is the keyword |a|, foo:|bar|
           qualifies in FOO), while an escaped colon is just a name character. */
        reader_case_unescaped(parser->token,(halfword)parser->length,esc);
        parser->eof=false;
        const char* tok=(const char*)parser->token;
        if(colon1<0){
            return package_intern_current_cstring(tok);}
        if(colon1==0){
            return keyword_intern_cstring(tok + (colon2==1 ? 2 : 1));}
        {   char pkg_buf[256];
            if((size_t)colon1>=sizeof(pkg_buf)){ return NIL_Symbol; }
            for(int i=0;i<colon1;i++){ pkg_buf[i]=tok[i]; }
            pkg_buf[colon1]='\0';
            const char* name_part=tok + (colon2==colon1+1 ? colon1+2 : colon1+1);
            Ref pkg=package_find_cstring(pkg_buf);
            if(nullp(pkg)){ ERROR("READ: there is no package named %s",pkg_buf); }
            if(pkg==KEYWORD_Package){ return keyword_intern_cstring(name_part); }
            if(colon2==colon1+1){ return package_intern_cstring_in(name_part,pkg); }
            Ref sym=package_find_external_cstring(name_part,pkg);
            if(sym){ return sym; }
            ERROR("READ: %s is not external in package %s",name_part,pkg_buf); }}
    bool valid=true;
    Ref result=scan_integer(parser);
    if(result==NIL_Symbol){
        result=scan_ratio(parser);}
    if(result==NIL_Symbol){
        result=scan_float(parser);}
    if(result==NIL_Symbol){
        result=scan_symbol(parser,&valid);}
    if(!valid){
        READER_ERROR(parser->input_stream,"Invalid token: %s",parser->token);}
    parser->eof=false; /* We've got a result, so defer EOF */
    return result;}

/* Invoke a reader-macro function FN as (fn stream char [arg]); the stream is
   already positioned just past CHAR.  Refresh the parser's lookahead afterwards
   and treat a trailing EOF as a plain terminator.  Returns the macro's value, or
   the skip marker if it returned no values. */
static Ref call_reader_macro(parser_state* parser, Ref fn, octet ch, bool with_arg, Ref arg){
    uword pool=open_live_pool();
    add_to_live_pool(fn); add_to_live_pool(parser->input_stream); add_to_live_pool(arg);
    Ref args = with_arg
        ? list(parser->input_stream, code_character(I(ch)), arg, NULL)
        : list(parser->input_stream, code_character(I(ch)), NULL);
    add_to_live_pool(args);
    MultipleValues mv;
    kernel_apply(fn, args, &mv);
    Ref res=(mv.count==0)?get_skip_marker():mv.values[0];
    close_live_pool(pool);
    add_to_live_pool(res);            /* re-root: close_live_pool trimmed it */
    advance_character(parser);        /* refresh lookahead past the macro's input */
    parser->eof=false;               /* object complete; EOF here is just the terminator */
    return res;}

static Ref parse_object(parser_state* parser){
    /* Reader-macro characters in the current readtable.  A built-in marker
       (g_standard_macro / g_dispatch_marker for the standard '#') is handled by
       the hardwired dispatch below; any other function is a user reader macro,
       and a g_dispatch_marker on a non-'#' character is a user dispatch macro
       (made with make-dispatch-macro-character). */
    if(!parser->suppress){
        Ref rt=current_readtable();
        Ref m=rt_macro(rt,parser->ch);
        if(m!=NIL_Symbol && m!=g_standard_macro){
            if(m==g_dispatch_marker && parser->ch!='#'){
                octet disp=parser->ch;
                octet sub=read_octet(parser->input_stream,&parser->eof,&parser->error);
                check_parser_error(parser);
                Ref subtab=rtv_get(rt->slots->readtable.dispatch,disp);
                int idx=sub; if(idx>='a'&&idx<='z'){ idx-=32; }
                Ref fn=(!nullp(subtab)&&idx>=0&&idx<128)?rtv_get(subtab,idx):NIL_Symbol;
                if(nullp(fn)){ READER_ERROR(parser->input_stream,
                                            "No dispatch function for %c",(char)sub); }
                return call_reader_macro(parser, fn, sub, true, NIL_Symbol);}
            if(m!=g_dispatch_marker){
                return call_reader_macro(parser, m, parser->ch, false, NIL_Symbol);}}}
    switch(parser->ch){
        /* We cannot have a ';' since skip_space(parser); eats the comments too. */
      case '\'':
          {
              advance_character(parser);
              skip_spaces(parser);
              if(parser->eof){
                  return NIL_Symbol;}
              Ref literal=parse_object(parser);
              return (parser->eof)
                      ? NIL_Symbol
                      : list(S(QUOTE),literal,NULL);}
      case '`':
          {
              advance_character(parser);
              skip_spaces(parser);
              if(parser->eof){ return NIL_Symbol; }
              Ref inner = parse_object(parser);
              return backquote_expand(inner);}
      case ',':
          {
              advance_character(parser);
              bool splicing = false;
              if(parser->ch == '@'){
                  splicing = true;
                  advance_character(parser);}
              skip_spaces(parser);
              if(parser->eof){ return NIL_Symbol; }
              Ref inner = parse_object(parser);
              return list(splicing ? S(UNQUOTE-SPLICING) : S(UNQUOTE),
                          inner, NULL);}
      case '"':
          return parse_string(parser);
      case '|':
          /* |...| — multi-escape.  A leading bar is just the first segment of a
             token that may concatenate more bars, single escapes, unescaped
             constituents, and PACKAGE markers (|pkg|::|name|).  parse_token
             handles all of that (it enters its multi-escape branch on this '|'),
             so delegate rather than re-implement a broken shortcut here. */
          return parse_token(parser);
      case '(':
          return parse_list(parser);
      case '#': {
          word disp_arg=-1;   /* the optional numeric infix argument, e.g. #3A */
          parser->ch=read_octet(parser->input_stream,&parser->eof,&parser->error);
          check_parser_error(parser);
          if(parser->eof){
              return NIL_Symbol;}
          if(parser->ch>='0' && parser->ch<='9'){
              disp_arg=0;
              while(parser->ch>='0' && parser->ch<='9'){
                  disp_arg=disp_arg*10+(parser->ch-'0');
                  parser->ch=read_octet(parser->input_stream,&parser->eof,&parser->error);
                  check_parser_error(parser);
                  if(parser->eof){ return NIL_Symbol; }}}
          switch(parser->ch){
            case '\\': return parse_character(parser);
            case '(':  return parse_vector(parser);
            case '*':  return parse_bit_vector(parser);
            case 'c': case 'C': {  /* #C(re im) — complex literal */
                advance_character(parser);   /* skip 'C'/'c' */
                skip_spaces(parser);
                if(parser->ch != '('){
                    READER_ERROR(parser->input_stream,"Expected '(' after #C%s","");}
                advance_character(parser);   /* skip '(' */
                skip_spaces(parser);
                Ref re = parse_object(parser);
                skip_spaces(parser);
                Ref im = parse_object(parser);
                skip_spaces(parser);
                if(parser->ch != ')'){
                    READER_ERROR(parser->input_stream,"Expected ')' after #C parts%s","");}
                advance_character(parser);   /* skip ')' */
                parser->eof=false;           /* a closing ')' at EOF is fine (as in parse_list) */
                return complex_new(re, im);}

            case '\'': {  /* #'name => (FUNCTION name) */
                advance_character(parser);
                skip_spaces(parser);
                Ref form=parse_object(parser);
                return list(FUNCTION_Symbol, form, NULL); }
            case '|': {  /* #|...|# — nestable block comment. */
                int depth=1;
                octet prev=0;
                while(depth>0){
                    advance_character(parser);
                    if(parser->eof){
                        READER_ERROR(parser->input_stream,"Unterminated #| block comment%s","");}
                    if(prev=='#' && parser->ch=='|'){ depth++; prev=0; }
                    else if(prev=='|' && parser->ch=='#'){ depth--; prev=0; }
                    else { prev=parser->ch; }}
                advance_character(parser);    /* past the closing # */
                /* A block comment is whitespace.  Return the skip marker so the
                   caller (parse_list / kernel_read) resumes scanning rather than
                   forcing another object — correct even when the comment abuts
                   ')' or EOF, e.g. "(a #| |# )".  (The old code recursively read
                   the next object, which mis-read the ')' as an object.) */
                return get_skip_marker(); }
            case '+':
            case '-': {  /* #+feature form / #-feature form */
                bool negate = (parser->ch == '-');
                advance_character(parser);
                skip_spaces(parser);
                Ref feature_expr = parse_object(parser);
                feature_expr = coerce_feature_expr(feature_expr);
                bool present = feature_present_p(feature_expr);
                skip_spaces(parser);
                if(parser->eof){ return get_skip_marker(); }
                if(present ^ negate){
                    return parse_object(parser);
                } else {
                    /* Skip the next form silently; callers (parse_list /
                       kernel_read) recognise reader_skip_marker.  Bind
                       *READ-SUPPRESS* (parser->suppress) so the discarded form
                       neither interns symbols nor errors on unknown packages
                       (e.g. "#+allegro (excl:foo ...)" on a non-Allegro build). */
                    bool saved=parser->suppress;
                    parser->suppress=true;
                    parse_object(parser);
                    parser->suppress=saved;
                    return get_skip_marker();}}
            case ':': {  /* #:name => a fresh uninterned symbol named NAME */
                advance_character(parser);   /* skip ':' */
                parser->length=0;
                bool esc=false;   /* | … | or \ used — keep escaped case verbatim */
                Ref rt=current_readtable();   /* match parse_token's continuation rule */
                while(!parser->error && !parser->eof){
                    if(parser->ch=='\\'){
                        if(!esc){ for(halfword i=0;i<parser->length;i++){ parser->token[i]=reader_upcase1(parser->token[i]); } esc=true; }
                        advance_character(parser);
                        if(parser->eof){ break; }
                        check_token_too_long(parser,"Token");
                        parser->token[parser->length++]=parser->ch;
                        advance_character(parser);}
                    else if(parser->ch=='|'){
                        if(!esc){ for(halfword i=0;i<parser->length;i++){ parser->token[i]=reader_upcase1(parser->token[i]); } esc=true; }
                        for(;;){
                            advance_character(parser);
                            if(parser->eof){ break; }
                            if(parser->ch=='\\'){ advance_character(parser); if(parser->eof){ break; } }
                            else if(parser->ch=='|'){ break; }
                            check_token_too_long(parser,"Token");
                            parser->token[parser->length++]=parser->ch;}
                        advance_character(parser);}
                    else if(token_continues(rt,parser->ch)){
                        /* Continues across constituents AND non-terminating macro
                           chars (e.g. # mid-name), matching parse_token; the old
                           constituentp() stopped the name at the first #. */
                        check_token_too_long(parser,"Token");
                        parser->token[parser->length++]=esc?reader_upcase1(parser->ch):parser->ch;
                        advance_character(parser);}
                    else{ break; }}
                check_parser_error(parser);
                parser->token[parser->length]='\0';
                parser->eof=false;   /* a name terminated by EOF is still valid */
                if(!esc){ octet_upcase(parser->token); }
                return symbol_new(string_new_cstring((const char*)parser->token)); }
            case '.': {  /* #.form => evaluate FORM at read time (read-eval) */
                extern Ref bocl_eval(Ref);
                advance_character(parser);   /* skip '.' */
                skip_spaces(parser);
                Ref form=parse_object(parser);
                return bocl_eval(form); }
            case 'p': case 'P': {  /* #P"..." => a pathname */
                advance_character(parser);   /* skip 'P' */
                skip_spaces(parser);
                Ref designator=parse_object(parser);
                return pathname(designator); }
            default: {
                /* User-defined #-dispatch sub-character?
                   (set-dispatch-macro-character #\# sub fn) — call (fn stream
                   sub-char arg); bocl does not parse the numeric infix arg. */
                Ref rt=current_readtable();
                Ref subtab=rtv_get(rt->slots->readtable.dispatch,'#');
                octet sub=parser->ch;
                int idx=sub; if(idx>='a'&&idx<='z'){ idx-=32; }
                Ref fn=(!nullp(subtab)&&idx>=0&&idx<128)?rtv_get(subtab,idx):NIL_Symbol;
                if(!nullp(fn)){ return call_reader_macro(parser, fn, sub, true,
                                                         disp_arg>=0?I(disp_arg):NIL_Symbol); }
                READER_ERROR(parser->input_stream,"Invalid syntax #%c",parser->ch);}}
          break;}
      default:
          if(constituentp(parser->ch) || parser->ch=='\\'){
              /* '\' is the single-escape: it begins a symbol token whose first
                 character is taken literally (parse_token handles the escape). */
              return parse_token(parser);}
          else{
              /* TODO: free the name! */
              READER_ERROR(parser->input_stream,"Invalid character #\\%s",
                           string_cstring(character_name(octet_character(parser->ch))));}}}


Ref kernel_read_char(Ref input_stream,Ref eof_error_p,Ref eof_value,unused Ref recursive_p){
    bool eof;
    bool err;
    octet ch=read_octet(input_stream,&eof,&err);
    if(eof){
        if(nullp(eof_error_p)){
            return eof_value;}
        END_OF_FILE(input_stream);}
    if(err){
        STREAM_ERROR(input_stream);}
    return octet_character(ch);}

Ref kernel_peek_char(Ref peek_type,Ref input_stream,Ref eof_error_p,Ref eof_value,unused Ref recursive_p){
    bool eof;
    bool err;
    octet ch;
    if(eql(T_Symbol,peek_type)){
        do{
            ch=read_octet(input_stream,&eof,&err);
            if(eof){
                if(nullp(eof_error_p)){
                    return eof_value;}
                END_OF_FILE(input_stream);}
            if(err){
                STREAM_ERROR(input_stream);}
        }while(whitespacep(ch));
        ungetc(ch,stream_cstream(input_stream));
        return octet_character(ch);}
    else if(nullp(peek_type)){
        ch=peek_octet(input_stream,&eof,&err);
        if(eof){
            if(nullp(eof_error_p)){
                return eof_value;}
            END_OF_FILE(input_stream);}
        if(err){
            STREAM_ERROR(input_stream);}
        return octet_character(ch);}
    else if(characterp(peek_type)){
        octet expect=(octet)integer_value(character_code(peek_type));
        do{
            ch=read_octet(input_stream,&eof,&err);
            if(eof){
                if(nullp(eof_error_p)){
                    return eof_value;}
                END_OF_FILE(input_stream);}
            if(err){
                STREAM_ERROR(input_stream);}
        }while(ch!=expect);
        ungetc(ch,stream_cstream(input_stream));
        return octet_character(ch);}
    else{
        /* TODO: (or (member t nil) character) */
        TYPE_ERROR(peek_type,CHARACTER_Class);}}


static Ref read_impl(Ref input_stream,Ref eof_error_p,Ref eof_value,Ref recursive_p,bool preserve){
    /* #n= / #n# labels are scoped to one top-level read: clear the label table
       at the start of a non-recursive read (a recursive read — e.g. (read
       stream) inside a reader macro — keeps the labels currently in scope). */
    if(nullp(recursive_p)){ set_symbol_value(S(*READ-LABELS*),NIL_Symbol); }
    parser_state parser;
    parser.input_stream=input_stream;
    parser.length=0;
    parser.ch=' ';
    parser.eof=false;
    parser.error=false;
    parser.escape=false;
    parser.suppress=false;
    advance_character(&parser);
    skip_spaces(&parser);
    check_parser_error(&parser);
    /* Empty/whitespace-only input: don't call parse_object, which would mis-
       interpret the EOF byte (0xFF) as a constituent and intern an empty
       symbol. Return the eof-value (or signal) right here. */
    if(parser.eof){
        if(nullp(eof_error_p)){
            return eof_value;}
        END_OF_FILE(input_stream);}
    Ref object;
    /* Loop while we keep getting reader_skip_marker (from #+/#- that skipped). */
    while(true){
        object = parse_object(&parser);
        if(object != reader_skip_marker){ break; }
        skip_spaces(&parser);
        if(parser.eof){
            if(nullp(eof_error_p)){ return eof_value; }
            END_OF_FILE(input_stream);}}
    if(parser.eof){
        if(nullp(eof_error_p)){
            return eof_value;}
        END_OF_FILE(input_stream);}
    if(parser.error){ /* Should not occur here */
        STREAM_ERROR(input_stream);}
    /* Push back the one-char lookahead (the char that terminated OBJECT) so it
       isn't lost across reads.  This makes two top-level forms with no separating
       whitespace read correctly — "(a)(b)" no longer drops the second '(' — and,
       crucially, lets a recursive read (e.g. inside a reader macro) leave a ')'
       that belongs to an enclosing list for the outer parse_list to consume.  We
       push back any non-whitespace, non-EOF char (a genuine stray top-level ')'
       then surfaces as an error on the next read, which is conforming).
       Plain READ drops trailing whitespace (harmless, and read_octet already
       translated newlines so re-reading a pushed-back NEL would misbehave);
       READ-PRESERVING-WHITESPACE (preserve=true) pushes it back so the caller
       sees it (e.g. (read-preserving-whitespace) then (read-char) yields the
       separating space). */
    if(parser.ch != (octet)EOF && (preserve || !whitespacep(parser.ch))){
        ungetc((int)parser.ch, stream_cstream(input_stream));}
    return object;}

Ref kernel_read(Ref input_stream,Ref eof_error_p,Ref eof_value,Ref recursive_p){
    return read_impl(input_stream,eof_error_p,eof_value,recursive_p,false);}

Ref kernel_read_preserving_whitespace(Ref input_stream,Ref eof_error_p,Ref eof_value,Ref recursive_p){
    return read_impl(input_stream,eof_error_p,eof_value,recursive_p,true);}

Ref kernel_read_delimited_list(Ref character,Ref input_stream,unused Ref recursive_p){
    NOT_IMPLEMENTED_YET();
    return S(NIL);
}

Ref kernel_unread_char(Ref ch, Ref stream){
    octet code = (octet)integer_value(character_code(ch));
    FILE* cstream = stream_cstream(stream);
    ungetc((int)code, cstream);
    return ch;}

Ref kernel_read_from_string(Ref input_string,Ref eof_error_p,Ref eof_value,Ref start,Ref end,Ref preserve_whitespace){
    Ref stream = stream_input_string(input_string, start, end);
    Ref result;
    if(is_true(preserve_whitespace))
        result=kernel_read_preserving_whitespace(stream, eof_error_p, eof_value, NIL_Symbol);
    else
        result=kernel_read(stream, eof_error_p, eof_value, NIL_Symbol);
    stream_close(stream);
    return result;}


/* ============================================================ */
/*  Readtable & reader macros                                    */
/* ============================================================ */
/* Character syntax types (CL standard; mirror the reference reader). */
#define CS_INVALID                0
#define CS_WHITESPACE             1
#define CS_SINGLE_ESCAPE          2
#define CS_MULTIPLE_ESCAPE        3
#define CS_CONSTITUENT            4
#define CS_TERMINATING_MACRO      5
#define CS_NON_TERMINATING_MACRO  6

extern Ref Object_new(Ref class, Slots* slots);

/* g_standard_readtable / g_standard_macro / g_dispatch_marker are declared near
   the top (forward-declared for parse_token/parse_object).  The two markers are
   stored in the macros vector for the standard macro characters: the reader
   recognises them by identity and uses its built-in (fast) dispatch; any OTHER
   (user) function in the macros vector is invoked as a reader macro. */

static Ref rtv_get(Ref vec,int i){ return vec->slots->array.elements->elements[i]; }
static void rtv_set(Ref vec,int i,Ref v){ vec->slots->array.elements->elements[i]=v; }
static Ref make_rt_vector(Ref fill){
    Ref lst=NIL_Symbol;
    for(int i=0;i<128;i++){ lst=cons(fill,lst); }
    return vector_new(lst);}

static int rt_syntax(Ref rt,int ch){
    if(ch<0||ch>=128){ return CS_CONSTITUENT; }
    return (int)integer_value(rtv_get(rt->slots->readtable.syntax,ch));}
static Ref rt_macro(Ref rt,int ch){
    if(ch<0||ch>=128){ return NIL_Symbol; }
    return rtv_get(rt->slots->readtable.macros,ch);}

/* The readtable in force (*READTABLE*), or the standard one as a fallback. */
static Ref current_readtable(void){
    Ref rt=symbol_value(READTABLE);
    if(!typep(rt,READTABLE_Class)){ return g_standard_readtable; }
    return rt;}

/* A token continues across a character iff that character is a constituent or a
   non-terminating macro character (whitespace and terminating macros end it).
   whitespacep() is the authority for whitespace — it knows the reader's NEL
   newline (code 133, outside the 0..127 syntax vector). */
static bool token_continues(Ref rt,int ch){
    if(whitespacep((octet)ch)){ return false; }
    int s=rt_syntax(rt,ch);
    return s==CS_CONSTITUENT || s==CS_NON_TERMINATING_MACRO;}

/* The standard macro markers ARE callable: (fn stream char).  By the reader-macro
   convention CHAR has already been consumed and STREAM sits just past it — which
   is exactly the parser's lookahead invariant (p.ch = the char already read,
   stream positioned after it).  So we seed a parser_state with p.ch = CHAR and
   re-enter parse_object, reusing the entire hardwired dispatch (parse_list for
   '(', parse_string for '"', the '#' dispatch, etc.) with no duplication.  The
   one leftover lookahead char is pushed back so the caller's stream position is
   right.  This is what lets reader macros call reader macros, and user code do
   (funcall (get-macro-character #\\() stream char). */
static void rm_standard_dispatch(Ref function, Ref arguments, MultipleValues* results){
    (void)function;
    Ref stream=car(arguments);
    octet ch=(octet)integer_value(character_code(cadr(arguments)));
    parser_state p;
    p.input_stream=stream;
    p.length=0;
    p.ch=ch;
    p.eof=false; p.error=false; p.escape=false; p.suppress=false;
    Ref r=parse_object(&p);
    if(!p.eof){ ungetc((int)p.ch, stream_cstream(stream)); }
    if(r==get_skip_marker()){ results->count=0; }   /* a no-value macro (comment, #+) */
    else { results->count=1; results->values[0]=r; }}

static Ref make_standard_readtable(void){
    uword pool=open_live_pool();
    Ref syntax=make_rt_vector(I(CS_CONSTITUENT));   add_to_live_pool(syntax);
    Ref macros=make_rt_vector(NIL_Symbol);          add_to_live_pool(macros);
    Ref dispatch=make_rt_vector(NIL_Symbol);        add_to_live_pool(dispatch);
    /* whitespace */
    int ws[]={' ','\t','\n','\r','\f',11,-1};
    for(int i=0;ws[i]>=0;i++){ rtv_set(syntax,ws[i],I(CS_WHITESPACE)); }
    rtv_set(syntax,'\\',I(CS_SINGLE_ESCAPE));
    rtv_set(syntax,'|', I(CS_MULTIPLE_ESCAPE));
    /* terminating macro characters */
    const char* term="()'\";`,";
    for(const char* p=term;*p;p++){
        rtv_set(syntax,(int)(unsigned char)*p,I(CS_TERMINATING_MACRO));
        rtv_set(macros,(int)(unsigned char)*p,g_standard_macro);}
    /* # — a non-terminating dispatch macro */
    rtv_set(syntax,'#',I(CS_NON_TERMINATING_MACRO));
    rtv_set(macros,'#',g_dispatch_marker);
    rtv_set(dispatch,'#',make_rt_vector(NIL_Symbol));
    Readtable* rt=(Readtable*)allocate(sizeof(Readtable));
    rt->syntax=syntax; rt->macros=macros; rt->dispatch=dispatch; rt->readcase=K(UPCASE);
    rt->objcount=4;
    Ref result=Object_new(READTABLE_Class,(Slots*)rt);
    close_live_pool(pool);
    add_to_live_pool(result);
    return result;}

static Ref readtable_arg(Ref* current, Ref function){
    Ref rt = nullp(*current) ? current_readtable() : pop_mandatory_argument(function,current);
    if(!typep(rt,READTABLE_Class)){ TYPE_ERROR(rt,READTABLE_Class); }
    return rt;}

static int char_arg(Ref obj){
    if(!typep(obj,CHARACTER_Class)){ TYPE_ERROR(obj,CHARACTER_Class); }
    return (int)integer_value(character_code(obj));}

static void k_READTABLEP(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments; Ref obj=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    results->count=1; results->values[0]=as_boolean(typep(obj,READTABLE_Class));
    close_live_pool(pool);}

static void k_GET_MACRO_CHARACTER(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    int ch=char_arg(pop_mandatory_argument(function,&current));
    Ref rt=readtable_arg(&current,function);
    check_no_more_arguments(function,current);
    int s=rt_syntax(rt,ch);
    results->count=2;
    results->values[0]=rt_macro(rt,ch);
    results->values[1]=as_boolean(s==CS_NON_TERMINATING_MACRO);
    close_live_pool(pool);}

static void k_SET_MACRO_CHARACTER(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    int ch=char_arg(pop_mandatory_argument(function,&current));
    Ref fn=pop_mandatory_argument(function,&current);
    Ref non_term=nullp(current)?NIL_Symbol:pop_mandatory_argument(function,&current);
    Ref rt=readtable_arg(&current,function);
    check_no_more_arguments(function,current);
    if(ch<0||ch>=128){ ERROR("SET-MACRO-CHARACTER: only codes 0..127 supported%s",""); }
    rtv_set(rt->slots->readtable.syntax,ch,
            I(is_true(non_term)?CS_NON_TERMINATING_MACRO:CS_TERMINATING_MACRO));
    rtv_set(rt->slots->readtable.macros,ch,fn);
    results->count=1; results->values[0]=T_Symbol; close_live_pool(pool);}

static void k_MAKE_DISPATCH_MACRO_CHARACTER(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    int ch=char_arg(pop_mandatory_argument(function,&current));
    Ref non_term=nullp(current)?NIL_Symbol:pop_mandatory_argument(function,&current);
    Ref rt=readtable_arg(&current,function);
    check_no_more_arguments(function,current);
    if(ch<0||ch>=128){ ERROR("MAKE-DISPATCH-MACRO-CHARACTER: only codes 0..127 supported%s",""); }
    rtv_set(rt->slots->readtable.syntax,ch,
            I(is_true(non_term)?CS_NON_TERMINATING_MACRO:CS_TERMINATING_MACRO));
    rtv_set(rt->slots->readtable.macros,ch,g_dispatch_marker);
    rtv_set(rt->slots->readtable.dispatch,ch,make_rt_vector(NIL_Symbol));
    results->count=1; results->values[0]=T_Symbol; close_live_pool(pool);}

static void k_GET_DISPATCH_MACRO_CHARACTER(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    int disp=char_arg(pop_mandatory_argument(function,&current));
    int sub =char_arg(pop_mandatory_argument(function,&current));
    Ref rt=readtable_arg(&current,function);
    check_no_more_arguments(function,current);
    Ref subtab=(disp>=0&&disp<128)?rtv_get(rt->slots->readtable.dispatch,disp):NIL_Symbol;
    if(nullp(subtab)){ ERROR("GET-DISPATCH-MACRO-CHARACTER: not a dispatch character%s",""); }
    if(sub>='a'&&sub<='z'){ sub-=32; }   /* CL upcases the sub-character */
    results->count=1;
    results->values[0]=(sub>=0&&sub<128)?rtv_get(subtab,sub):NIL_Symbol;
    close_live_pool(pool);}

static void k_SET_DISPATCH_MACRO_CHARACTER(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    int disp=char_arg(pop_mandatory_argument(function,&current));
    int sub =char_arg(pop_mandatory_argument(function,&current));
    Ref fn=pop_mandatory_argument(function,&current);
    Ref rt=readtable_arg(&current,function);
    check_no_more_arguments(function,current);
    Ref subtab=(disp>=0&&disp<128)?rtv_get(rt->slots->readtable.dispatch,disp):NIL_Symbol;
    if(nullp(subtab)){ ERROR("SET-DISPATCH-MACRO-CHARACTER: not a dispatch character%s",""); }
    if(sub>='a'&&sub<='z'){ sub-=32; }
    if(sub>=0&&sub<128){ rtv_set(subtab,sub,fn); }
    results->count=1; results->values[0]=T_Symbol; close_live_pool(pool);}

static void k_COPY_READTABLE(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref from=nullp(current)?current_readtable():pop_mandatory_argument(function,&current);
    /* (the optional TO-READTABLE arg is accepted but a fresh copy is returned) */
    if(!nullp(current)){ (void)pop_mandatory_argument(function,&current); }
    check_no_more_arguments(function,current);
    if(nullp(from)){ from=g_standard_readtable; }   /* (copy-readtable nil) => standard */
    if(!typep(from,READTABLE_Class)){ TYPE_ERROR(from,READTABLE_Class); }
    add_to_live_pool(from);
    Ref syntax=make_rt_vector(I(CS_CONSTITUENT)); add_to_live_pool(syntax);
    Ref macros=make_rt_vector(NIL_Symbol);        add_to_live_pool(macros);
    Ref dispatch=make_rt_vector(NIL_Symbol);      add_to_live_pool(dispatch);
    for(int i=0;i<128;i++){
        rtv_set(syntax,i,rtv_get(from->slots->readtable.syntax,i));
        rtv_set(macros,i,rtv_get(from->slots->readtable.macros,i));
        Ref sub=rtv_get(from->slots->readtable.dispatch,i);
        if(!nullp(sub)){
            Ref nsub=make_rt_vector(NIL_Symbol);
            for(int j=0;j<128;j++){ rtv_set(nsub,j,rtv_get(sub,j)); }
            rtv_set(dispatch,i,nsub);}}
    Readtable* rt=(Readtable*)allocate(sizeof(Readtable));
    rt->syntax=syntax; rt->macros=macros; rt->dispatch=dispatch;
    rt->readcase=from->slots->readtable.readcase; rt->objcount=4;
    Ref result=Object_new(READTABLE_Class,(Slots*)rt);
    close_live_pool(pool); add_to_live_pool(result);
    results->count=1; results->values[0]=result;}

static void k_READTABLE_CASE(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments; Ref rt=readtable_arg(&current, function);
    check_no_more_arguments(function,current);
    results->count=1; results->values[0]=rt->slots->readtable.readcase;
    close_live_pool(pool);}

/* Backing for (SETF (READTABLE-CASE rt) mode). */
static void k_SET_READTABLE_CASE(Ref function, Ref arguments, MultipleValues* results){
    uword pool=open_live_pool(); add_to_live_pool(function); add_to_live_pool(arguments);
    Ref current=arguments;
    Ref rt=readtable_arg(&current, function);
    Ref mode=pop_mandatory_argument(function,&current);
    check_no_more_arguments(function,current);
    if(mode!=K(UPCASE)&&mode!=K(DOWNCASE)&&mode!=K(PRESERVE)&&mode!=K(INVERT)){
        ERROR("(SETF READTABLE-CASE): invalid mode %s",
              string_cstring(prin1_to_string(mode)));}
    rt->slots->readtable.readcase=mode;
    results->count=1; results->values[0]=mode;
    close_live_pool(pool);}

static void register_reader_fn(Ref fname, Ref lambda_list, KernelFunctionPtr fn){
    set_symbol_function(fname, built_in_function_new(fname, lambda_list, fn));}

void reader_initialize(){
    define_parameter(READ_BASE,integer_from_word(10));
    define_parameter(READ_DEFAULT_FLOAT_FORMAT,S(SINGLE-FLOAT));
    define_parameter(READ_EVAL,S(T));
    define_parameter(READ_SUPPRESS,S(NIL));
    define_parameter(S(*READ-LABELS*),NIL_Symbol);   /* #n= / #n# label table */
    /* Build the standard readtable and the standard-macro markers. */
    g_standard_macro  = built_in_function_new(S(%STANDARD-READER-MACRO),
                                              list(S(STREAM),S(CHAR),NULL), rm_standard_dispatch);
    g_dispatch_marker = built_in_function_new(S(%STANDARD-DISPATCH-MACRO),
                                              list(S(STREAM),S(CHAR),NULL), rm_standard_dispatch);
    push_to_rootset(g_standard_macro);
    push_to_rootset(g_dispatch_marker);
    g_standard_readtable = make_standard_readtable();
    push_to_rootset(g_standard_readtable);
    define_parameter(READTABLE,g_standard_readtable);
    register_reader_fn(S(READTABLEP),                    list(S(OBJECT),NULL),               k_READTABLEP);
    register_reader_fn(S(GET-MACRO-CHARACTER),           list(S(CHAR),NULL),                 k_GET_MACRO_CHARACTER);
    register_reader_fn(S(SET-MACRO-CHARACTER),           list(S(CHAR),S(FN),NULL),           k_SET_MACRO_CHARACTER);
    register_reader_fn(S(GET-DISPATCH-MACRO-CHARACTER),  list(S(DISP),S(SUB),NULL),          k_GET_DISPATCH_MACRO_CHARACTER);
    register_reader_fn(S(SET-DISPATCH-MACRO-CHARACTER),  list(S(DISP),S(SUB),S(FN),NULL),    k_SET_DISPATCH_MACRO_CHARACTER);
    register_reader_fn(S(MAKE-DISPATCH-MACRO-CHARACTER), list(S(CHAR),NULL),                 k_MAKE_DISPATCH_MACRO_CHARACTER);
    register_reader_fn(S(COPY-READTABLE),                NIL_Symbol,                         k_COPY_READTABLE);
    register_reader_fn(S(READTABLE-CASE),                NIL_Symbol,                         k_READTABLE_CASE);
    register_reader_fn(symbol_intern_cstring("%SET-READTABLE-CASE"),
                       NIL_Symbol,                                                           k_SET_READTABLE_CASE);}


/**** THE END ****/
