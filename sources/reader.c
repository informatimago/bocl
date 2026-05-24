#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "reader.h"
#include "macros.h"
#include "kernel.h"
#include "variable.h"
#include "boerror.h"
#include "cons.h"
#include "bostring.h"
#include "character.h"

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
| (       | lists               | (f o o) (no dotted list)      |
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
            return octet_character(parser->ch);}
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

static Ref parse_list(parser_state* parser){
    /* We've read opening (  we need to read objects util closing ) */
    Ref result=NIL_Symbol;
    advance_character(parser);
    skip_spaces(parser);
    if(parser->eof){
        return NIL_Symbol;}
    while(parser->ch!=')'){
        Ref element=parse_object(parser);
        push(element,&result);
        skip_spaces(parser);
        if(parser->eof){
            return NIL_Symbol;}}
    /* Consume the closing ')' so an outer parse_list doesn't see it as its own. */
    advance_character(parser);
    parser->eof=false;
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
              ungetc(parser->ch,stream_cstream(parser->input_stream));
              return bit_vector_new_octets(CHECK_SIZE_TO_UWORD(parser->length),parser->token);}}}

static Ref scan_integer(parser_state* parser){
    /* [-+]?[0-9]+\.?  — sign is allowed ONLY at position 0; otherwise a
       token like "1+" would mis-parse as the integer 1 and the "+" would
       be silently dropped. */
    const char* token=(char*)parser->token;
    word sign=1;
    bool value_set=false;
    word value=0;
    size_t pos=0;
    word digit=0;
    if(token[0]=='-'){ sign=-1; pos=1; }
    else if(token[0]=='+'){ sign=+1; pos=1; }
    while(token[pos]){
        switch(token[pos]){
          case '0':case '1':case '2':case '3':case '4':
          case '5':case '6':case '7':case '8':case '9':
              digit=token[pos]-'0';
              if((WORD_MAX/10<value)
                  ||((WORD_MAX/10==value)&&(WORD_MAX-10*(WORD_MAX/10)<=digit))){
                  ARITHMETIC_ERROR("Integer too big maximum %"WORD_FORMAT,WORD_MAX);}
              value=(value*10)+digit;
              value_set=true;
              pos++;
              break;
          case '.':
              if(token[pos+1]=='\0'){
                  pos++;
                  break;}
              return NIL_Symbol;
          default:
              return NIL_Symbol;}}
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

static Ref scan_symbol(parser_state* parser,bool* valid){
    const char* token=(const char*)octet_upcase(parser->token);
    const char* colon=strchr(token,':');
    (*valid)=false;
    if(!colon){
        (*valid)=true;
        return symbol_intern_cstring(token);}
    if(colon==token){
        colon=strchr(token+1,':');
        if(!colon){
            (*valid)=true;
            return keyword_intern_cstring(token+1);}
        /* two or more colons */
        return NIL_Symbol;}
    /* colon in the middle -- no package yet */
    return NIL_Symbol;}

static Ref parse_token(parser_state* parser){
    parser->length=0;
    while(!parser->error && !parser->eof && constituentp(parser->ch)){
        check_token_too_long(parser,"Token");
        parser->token[parser->length++]=parser->ch;
        advance_character(parser);}
    check_parser_error(parser);
    parser->token[parser->length]='\0';
    bool valid=true;
    Ref result=scan_integer(parser);
    if(result==NIL_Symbol){
        result=scan_float(parser);}
    if(result==NIL_Symbol){
        result=scan_symbol(parser,&valid);}
    if(!valid){
        READER_ERROR(parser->input_stream,"Invalid token: %s",parser->token);}
    parser->eof=false; /* We've got a result, so defer EOF */
    return result;}

static Ref parse_object(parser_state* parser){
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
      case '"':
          return parse_string(parser);
      case '(':
          return parse_list(parser);
      case '#':
          parser->ch=read_octet(parser->input_stream,&parser->eof,&parser->error);
          check_parser_error(parser);
          if(parser->eof){
              return NIL_Symbol;}
          switch(parser->ch){
            case '\\': return parse_character(parser);
            case '(':  return parse_vector(parser);
            case '*':  return parse_bit_vector(parser);
            default:
                READER_ERROR(parser->input_stream,"Invalid syntax #%c",parser->ch);}
          break;
      default:
          if(constituentp(parser->ch)){
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


Ref kernel_read(Ref input_stream,Ref eof_error_p,Ref eof_value,unused Ref recursive_p){
    parser_state parser;
    parser.input_stream=input_stream;
    parser.length=0;
    parser.ch=' ';
    parser.eof=false;
    parser.error=false;
    parser.escape=false;
    advance_character(&parser);
    skip_spaces(&parser);
    check_parser_error(&parser);
    Ref object=parse_object(&parser);
    if(parser.eof){
        if(nullp(eof_error_p)){
            return eof_value;}
        END_OF_FILE(input_stream);}
    if(parser.error){ /* Should not occur here */
        STREAM_ERROR(input_stream);}
    return object;}

Ref kernel_read_preserving_whitespace(Ref input_stream,Ref eof_error_p,Ref eof_value,unused Ref recursive_p){
    return kernel_read(input_stream,eof_error_p,eof_value,recursive_p);}

Ref kernel_read_delimited_list(Ref character,Ref input_stream,unused Ref recursive_p){
    NOT_IMPLEMENTED_YET();
    return S(NIL);
}

Ref kernel_read_from_string(Ref input_string,Ref eof_error_p,Ref eof_value,Ref start,Ref end,Ref preserve_whitespace){
    NOT_IMPLEMENTED_YET();
    return S(NIL);
}


void reader_initialize(){
    define_parameter(READ_BASE,integer_from_word(10));
    define_parameter(READ_DEFAULT_FLOAT_FORMAT,S(SINGLE-FLOAT));
    define_parameter(READ_EVAL,S(T));
    define_parameter(READ_SUPPRESS,S(NIL));
    define_parameter(READTABLE,S(NIL));}


/**** THE END ****/
