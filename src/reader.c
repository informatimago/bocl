#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "reader.h"
#include "macros.h"
#include "kernel.h"
#include "variable.h"
#include "error.h"
#include "cons.h"

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
    Object* input_stream;
    Object* list;
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

static octet read_octet(Object* input_stream,bool* eof,bool* error){
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
                  c=NEL;
                  int next=getc(cstream);
                  if((next!=EOF)&&(next!=LF)){
                      ungetc(next,cstream);}
                  break;
              case LF:
                  c=NEL;
                  break;}}
        (*error)=false;
        (*eof)=false;}
    return (octet)c;}

static octet peek_octet(Object* input_stream,bool* eof,bool* error){
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
                  c=NEL;
                  int next=getc(cstream);
                  if(next==EOF){
                      ungetc(CR,cstream);}
                  else if(next!=LF){
                      ungetc(next,cstream);}
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

static bool constituentp(octet ch){
    if(160<=ch){
        return true;}
    switch(ch){
      case ' ':
      case '"':
      case '#':
      case '\'':
      case '(':
      case ')':
      case ',':       
      case '\\':       
      case '`':       
      case '|':       
          return false;}            
    if((33<=ch)&&(ch<127)){
        return true;}
    return false;}

static bool whitespacep(octet character){
    switch(character){
      case 9: /* TAB */
      case 10: /* LF */
      case 11: /* VT */
      case 12: /* FF */
      case 13: /* CR */
      case 32: /* SPC */
      case 0x85: /* NEL */
          return true;
      default:
          return false;}}

static void check_parser_error(parser_state* parser){
    if(parser->error){
        STREAM_ERROR(parser->input_stream);}
    if(parser->eof){
        END_OF_FILE(parser->input_stream);}}

static octet advance_character(parser_state* parser){
    return ((parser->ch=read_octet(parser->input_stream,&parser->eof,&parser->error)));}

static void skip_spaces(parser_state* parser){
    advance_character(parser);
    while(!parser->eof && !parser->error && (whitespacep(parser->ch) || (parser->ch==';'))){
        if(parser->ch==';'){
            while(!parser->eof && !parser->error && (parser->ch!=NEL)){
                advance_character(parser);}}
        else{
            advance_character(parser);}}
    check_parser_error(parser);}

static Object* parse_object(parser_state* parser);

static void parse_character_name(parser_state* parser){
    parser->length=0;
    while(!(parser->eof) && ((parser->ch=='-')
                             ||(parser->ch=='_')
                             ||isalnum(parser->ch))){
        parser->token[parser->length++]=parser->ch;
        advance_character(parser);
        if(parser->error){
            check_parser_error(parser);}}}

static Object* parse_character(parser_state* parser){
    advance_character(parser);
    check_parser_error(parser);
    if(isalpha(parser->ch)){
        parser->length=1;
        parser->token[0]=parser->ch;
        parse_character_name(parser);
        parser->token[parser->length]='\0';
        Object* character=name_character(string_new_cstring((char*)parser->token));
        if(null_p(character)){
            READER_ERROR(parser->input_stream,"Invalid character name %s",parser->token);}
        else{
            return character;}}
    else{
        return octet_character(parser->ch);}}


static Object* parse_string(parser_state* parser){
    parser->length=0;
    parser->escape=false;
    while(true){
        advance_character(parser);
        check_parser_error(parser);
        if(parser->escape){
            if(ARRAY_TOTAL_SIZE_LIMIT<=parser->length){
                READER_ERROR(parser->input_stream,"String too long (more than %lu)",ARRAY_TOTAL_SIZE_LIMIT);}
            parser->token[parser->length++]=parser->ch;}
        else{
            switch(parser->ch){
              case '\\': parser->escape=true;
              case '"':
                  parser->token[parser->length]='\0';
                  return string_new_cstring((char*)parser->token);
              default:
                  if(ARRAY_TOTAL_SIZE_LIMIT<=parser->length){
                      READER_ERROR(parser->input_stream,"String too long (more than %lu)",ARRAY_TOTAL_SIZE_LIMIT);}
                  parser->token[parser->length++]=parser->ch;}}}}

static Object* parse_list(parser_state* parser){
    parser->list=NIL_Symbol;
    skip_spaces(parser);
    while(parser->ch!=')'){
        Object* element=parse_object(parser);
        push(element,&parser->list);
        skip_spaces(parser);}
    return nreverse(parser->list);}

static Object* parse_vector(parser_state* parser){
    return vector_new(parse_list(parser));}

static Object* parse_bit_vector(parser_state* parser){
    parser->length=0;
    while(true){
        advance_character(parser);
        check_parser_error(parser);
        switch(parser->ch){
          case '0':
          case '1':
            if(ARRAY_TOTAL_SIZE_LIMIT<=parser->length){
                READER_ERROR(parser->input_stream,"Bit vector too long (more than %lu)",ARRAY_TOTAL_SIZE_LIMIT);}
            parser->token[parser->length++]=parser->ch&1;
            break;
          default:
              ungetc(parser->ch,stream_cstream(parser->input_stream));
              return bit_vector_new_octets(parser->length,parser->token);}}}

static Object* parse_token(parser_state* parser);

static Object* parse_object(parser_state* parser){
    switch(parser->ch){
        /* We cannot have a ';' since skip_space(parser); eats the comments too. */
      case '\'':  skip_spaces(parser); return cons(S(QUOTE),parse_object(parser));
      case '"':   return parse_string(parser);
      case '(':   return parse_list(parser);
      case '#':
          parser->ch=read_octet(parser->input_stream,&parser->eof,&parser->error);
          check_parser_error(parser);
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

static Object* scan_integer(parser_state* parser);
static Object* scan_float(parser_state* parser);
static Object* scan_symbol(parser_state* parser);

static Object* parse_token(parser_state* parser){
    parser->length=0;
    parser->token[parser->length++]=parser->ch;
    advance_character(parser);
    while(!parser->error && !parser->eof && constituentp(parser->ch)){
        if(ARRAY_TOTAL_SIZE_LIMIT<=parser->length){
            READER_ERROR(parser->input_stream,"Token too long (more than %lu)",ARRAY_TOTAL_SIZE_LIMIT);}
        parser->token[parser->length++]=parser->ch;
        advance_character(parser);}
    if(parser->error){
        STREAM_ERROR(parser->input_stream);}
    else{
        parser->token[parser->length]='\0';
        Object* result=scan_integer(parser);
        if(result==NIL_Symbol){
            result=scan_float(parser);}
        if(result==NIL_Symbol){
            result=scan_symbol(parser);}
        if(result==NIL_Symbol){
            READER_ERROR(parser->input_stream,"Invalid token: %s",parser->token);}
        return result;}}


static Object* scan_integer(parser_state* parser){
    /* [-+]?[0-9]+\.? */
    const char* token=(char*)parser->token;
    word sign=1;
    word value=0;
    size_t pos=0;
    word digit=0;
    while(token[pos]){
        switch(token[pos]){
          case '-':
              sign=-1;
              pos++;
              break;
          case '+':
              sign=+1;
              pos++;
              break;
          case '0':case '1':case '2':case '3':case '4':
          case '5':case '6':case '7':case '8':case '9':
              digit=token[pos]-'0';
              if((WORD_MAX/10<value)
                  ||((WORD_MAX/10==value)&&(WORD_MAX-10*(WORD_MAX/10)<=digit))){
                  ARITHMETIC_ERROR("Integer too big maximum %"WORD_FORMAT,WORD_MAX);}
              value=(value*10)+digit;
              pos++;
              break;
          case '.':
              if(token[pos+1]=='\0'){
                  pos++;
                  break;}
              return NIL_Symbol;
          default:
              return NIL_Symbol;}}
    return integer_from_word(sign*value);}

static Object* scan_float(parser_state* parser){
    /* [-+]?[0-9]*(\.[0-9]+)?([DdEeFfLlSs][-+]?[0-9]+)? */
    /* [-+]?[0-9]+(\.[0-9]*)?[DdEeFfLlSs][-+]?[0-9]+ */
    const char* token=(char*)parser->token;
    size_t pos=0;
    switch(token[pos]){
      case '+':case '-':
          pos++;
          break;}
    if(token[pos]=='.'){
        pos++;
        if(!isdigit(token[pos])){
            return NIL_Symbol;}
        while(isdigit(token[pos++]));}
    else{
        if(!isdigit(token[pos])){
            return NIL_Symbol;}
        while(isdigit(token[pos++]));
        if(token[pos]=='.'){
            pos++;
            while(isdigit(token[pos++]));}}
    if(token[pos]!='\0'){
        switch(token[pos]){
          case 'D':case'E':case'F':case'L':case'S':
          case 'd':case'e':case'f':case'l':case's':
              pos++;
          default:
              return NIL_Symbol;}
        switch(token[pos]){
          case'+': case'-':
              pos++;
              break;}
        if(!isdigit(token[pos])){
            return NIL_Symbol;}
        while(isdigit(token[pos++]));}
    if(token[pos]!='\0'){
        return NIL_Symbol;}
    floating value;
    int n=sscanf(token,"%le",&value);
    if(n!=1){
        return NIL_Symbol;}
    return float_from_floating(value);}
    
static Object* scan_symbol(parser_state* parser){
    const char* token=(char*)parser->token;
    const char* colon=strchr(token,':');
    if(!colon){
        return symbol_intern_cstring(token);}
    if(colon==token){
        colon=strchr(token+1,':');
        if(!colon){
            return keyword_intern_cstring(token);}
        return NIL_Symbol;}
    /* no package yet */
    return NIL_Symbol;}
    

Object* kernel_read_char(Object* input_stream,Object* eof_error_p,Object* eof_value,unused Object* recursive_p){
    bool eof;
    bool err;
    octet ch=read_octet(input_stream,&eof,&err);
    if(eof){
        if(null_p(eof_error_p)){
            return eof_value;}
        END_OF_FILE(input_stream);}
    if(err){
        STREAM_ERROR(input_stream);}
    return octet_character(ch);}

Object* kernel_peek_char(Object* peek_type,Object* input_stream,Object* eof_error_p,Object* eof_value,unused Object* recursive_p){
    bool eof;
    bool err;
    octet ch;
    if(eql(T_Symbol,peek_type)){
        do{
            ch=read_octet(input_stream,&eof,&err);
            if(eof){
                if(null_p(eof_error_p)){
                    return eof_value;}
                END_OF_FILE(input_stream);}
            if(err){
                STREAM_ERROR(input_stream);}
        }while(whitespacep(ch));
        ungetc(ch,stream_cstream(input_stream));
        return octet_character(ch);}
    else if(null_p(peek_type)){
        ch=peek_octet(input_stream,&eof,&err);
        if(eof){
            if(null_p(eof_error_p)){
                return eof_value;}
            END_OF_FILE(input_stream);}
        if(err){
            STREAM_ERROR(input_stream);}
        return octet_character(ch);}
    else if(character_p(peek_type)){
        octet expect=(octet)integer_value(character_code(peek_type));
        do{
            ch=read_octet(input_stream,&eof,&err);
            if(eof){
                if(null_p(eof_error_p)){
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


Object* kernel_read(Object* input_stream,Object* eof_error_p,Object* eof_value,unused Object* recursive_p){
    parser_state parser;
    parser.input_stream=input_stream;
    parser.list=NIL_Symbol;
    parser.length=0;
    parser.ch=' ';
    parser.eof=false;
    parser.error=false;
    parser.escape=false;
    Object* object=parse_object(&parser);
    if(parser.eof){
        if(null_p(eof_error_p)){
            return eof_value;}
        END_OF_FILE(input_stream);}
    if(parser.error){
        STREAM_ERROR(input_stream);}
    return object;}

Object* kernel_read_preserving_whitespace(Object* input_stream,Object* eof_error_p,Object* eof_value,unused Object* recursive_p){
    return kernel_read(input_stream,eof_error_p,eof_value,recursive_p);}

Object* kernel_read_delimited_list(Object* character,Object* input_stream,unused Object* recursive_p){
    NOT_IMPLEMENTED_YET();
    return S(NIL);
}

Object* kernel_read_from_string(Object* input_string,Object* eof_error_p,Object* eof_value,Object* start,Object* end,Object* preserve_whitespace){
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
