#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "string_list.h"
#include "kernel.h"
#include "bocl.h"

static char* source="http://gitlab.com/informatimago/bocl/";
static char* bocl_lisp="(defun repl () (catch 'quit (do () () (print (eval (read))))))";
static char* pname="bocl";
static char* initfile="~/.boclrc";

static bool do_repl=true;
static bool do_wait=false;
static bool do_help=false;
static bool do_version=false;
static char* terminal_encoding="iso-8859-1";
static bool do_norc=false;
static bool do_quiet=false;
static bool do_verbose=false;
static bool do_print=false;
static bool do_batch=false;
static bool do_sigtrap=true;
typedef enum { on_error_debug, on_error_exit, on_error_abort, on_error_appease } on_error_action;
static on_error_action on_error=on_error_exit;

static string_list* initfiles=NULL;
static string_list* expressions_and_loads=NULL;
static char* package=NULL;


static void print_help(void){
    struct {
        char* option;
        char* description;
    }  options[]={
        {"-h, --help",                   "print this text"},
        {"-V, --version",                "print (LISP-IMPLEMENTATION-VERSION) and exit"},
        {"-E encoding",                  "set encodings"},
        {"-Eterminal encoding",          "specify character encoding to use for *TERMINAL-IO*"},
        {"--terminal-encoding encoding", "specify character encoding to use for *TERMINAL-IO*"},
        {"-n, -norc, --no-init",         "suppress loading of default init file ~/.boclrc"},
        {"-x, -e, --eval form",          "evaluate form (may need to quote form in shell)"},
        {"-l, --load  file",             "load file"},
        {"-Q, -q, --quiet, --silent",    "if --batch, also suppress printing of heralds, prompts."},
        {"--verbose",                    "verbosity level: affects banner, *LOAD-VERBOSE*, *COMPILE-VERBOSE*, and *LOAD-PRINT*, *COMPILE-PRINT*"},
        {"-b, --batch",                  "exit when EOF on *STANDARD-INPUT*"},
        {"--no-sigtrap",                 "obscure option for running under GDB"},
        {"-on-error action",             "action can be one of debug, exit, abort, appease"},
        {"-i file",                      "load initfile (can be repeated)"},
        {"-p package",                   "start in the package"},
        {"-repl, --repl",                "enter the interactive read-eval-print loop when done (default)"},
        {"-quit, --quit",                "quit when argument processing is done"},
        {"-w, --wait",                   "wait for a keypress after program termination"},
        {NULL,NULL}};

    printf("%s usage:\n",pname);
    for(int i=0;options[i].option;i++){
        printf("  %30s  %s\n",options[i].option,options[i].description);}}

static void process_options(int argc,char** argv){


}

static void print_banner(void){
    printf("\nWelcome to ");
    bocl_read_eval_cstring("(WRITE-LINE (LISP-IMPLEMENTATION-TYPE))");
    printf("Copyright 2021 Pascal J. Bourguignon\n");
    printf("Licensed under AGPL; source codes available at:\n");
    printf("%s\n\n",source);}

static void print_version(void){
    bocl_read_eval_cstring("(WRITE-LINE (LISP-IMPLEMENTATION-VERSION))");}

static void wait_for_termination(int status){
    printf("Enter RET to terminate with status %d.\n",status);
    char buffer[80];
    fgets(buffer,sizeof(buffer)-1,stdin);}

int main(int argc,char** argv){

    int status=0;
    kernel_initialize();
    bocl_read_eval_loop_cstring(bocl_lisp);
    process_options(argc,argv);

    if(do_version){
        print_version();
        exit(0);}

    if(do_help){
        print_help();
        exit(0);}

    if(do_batch){
        bocl_read_eval_cstring("(DEFPARAMETER BOCL:*BATCH*  T)");}
    else{
        bocl_read_eval_cstring("(DEFPARAMETER BOCL:*BATCH*  NIL)");}

    if(!do_quiet && !do_batch){
        print_banner();}

    if(do_verbose){
        bocl_read_eval_cstring("(SETQ *LOAD-VERBOSE*    T)");
        bocl_read_eval_cstring("(SETQ *COMPILE-VERBOSE* T)");}

    if(do_print){
        bocl_read_eval_cstring("(SETQ *LOAD-PRINT*      T)");
        bocl_read_eval_cstring("(SETQ *COMPILE-PRINT*   T)");}

    initfiles=string_list_cons(initfile,initfiles);
    {do_string_list(initfile,initfiles){
            bocl_load_cstring(initfile);}}
    string_list_free(initfiles);
    initfiles=NULL;

    {do_string_list(expression,expressions_and_loads){
            bocl_read_eval_cstring(expression);}}
    string_list_free(expressions_and_loads);
    expressions_and_loads=NULL;

    if(package){
        bocl_in_package(bocl_read_cstring(package));}

    if(do_repl){
        Object* result=bocl_read_eval_cstring("(BOCL:REPL)");
        if(integer_p(result)){
            status=integer_value(result);}}

    if(do_wait){
        wait_for_termination(status);}

    return status;}
