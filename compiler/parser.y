%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "codegen.hpp"
#include "symbol.h"
#include "general.h"
#include "error.h"

void yyerror (const char *msg);

extern char *yytext;
extern int linecount;
extern const char *filename;
ast t;
%}

%union{
	ast a;
	char c;
	char *s;
	int n;
	Type t;
}

%token T_byte "byte"
%token T_else "else"
%token T_false "false"
%token T_if "if"
%token T_int "int"
%token T_proc "proc"
%token T_reference "reference"
%token T_return "return"
%token T_while "while"
%token T_true "true"
%token T_eq "=="
%token T_ne "!="
%token T_le "<="
%token T_ge ">="
%token<s> T_id
%token<n> T_const
%token<c> T_char
%token<s> T_string

%left '|'
%left '&'
%left T_eq T_ne '>' '<' T_le T_ge
%left '+' '-'
%left '*' '/' '%'
%left '!'
%left UMINUS UPLUS

%type<a> program
%type<a> func-def
%type<a> fpar-list
%type<a> fpar-def
%type<a> local-def-list
%type<a> local-def
%type<t> type
%type<t> data-type
%type<t> r-type
%type<a> var-def
%type<a> compound-stmt
%type<a> stmt-list
%type<a> stmt
%type<a> func-call
%type<a> expr-list
%type<a> expr
%type<a> l-value
%type<a> cond

%%

program:
	func-def { t = $$ = $1; }
;

func-def:
	T_id '(' fpar-list ')' ':' r-type local-def-list compound-stmt { $$ = ast_fdef(ast_fdecl($1, $6, $3, $7), $8); }
;

fpar-list:
	/* nothing */ { $$ = NULL; }
|	fpar-def { $$ = ast_seq($1, NULL); }
|	fpar-def ',' fpar-list { $$ = ast_seq($1, $3); }
;

fpar-def:
	T_id ':' type { $$ = ast_par($1, $3, PASS_BY_VALUE); }
|	T_id ':' "reference" type { $$ = ast_par($1, $4, PASS_BY_REFERENCE); }
;

local-def-list:
	/* nothing */ { $$ = NULL; }
|	local-def local-def-list { $$ = ast_seq($1, $2); }
;

local-def:
	func-def { $$ = $1; }
|	var-def { $$ = $1; }
;

type:
	data-type { $$ = $1; }
|	data-type '[' ']' { $$ = typeIArray($1); }
;

data-type:
	"int" { $$ = typeInteger; }
|	"byte" { $$ = typeChar; }
;

r-type:
	data-type { $$ = $1; }
|	"proc" { $$ = typeVoid; }
;

var-def:
	T_id ':' data-type ';' { $$ = ast_vdef($1, $3, 0); }
|	T_id ':' data-type '[' T_const ']' ';' { $$ = ast_vdef($1, $3, $5); }
;

compound-stmt:
	'{' stmt-list '}' { $$ = $2; }
;

stmt-list:
	/* nothing */ { $$ = NULL; }
|	stmt stmt-list { $$ = ast_seq($1, $2); }
;

stmt:
	';' { $$ = NULL; }
|	l-value '=' expr ';' { $$ = ast_assign($1, $3); }
|	compound-stmt { $$ = $1; }
|	func-call ';' { $$ = ast_fcall_stmt($1); }
|	"if" '(' cond ')' stmt { $$ = ast_if($3, $5); }
|	"if" '(' cond ')' stmt "else" stmt { $$ = ast_ifelse(ast_if($3, $5), $7); }
|	"while" '(' cond ')' stmt { $$ = ast_while($3, $5); }
|	"return" ';' { $$ = ast_ret(NULL); }
|	"return" expr ';' { $$ = ast_ret($2); }
;

func-call:
	T_id '(' expr-list ')' { $$ = ast_fcall($1, $3); }
;

expr-list:
	/* nothing */ { $$ = NULL; }
|	expr { $$ = ast_seq($1, NULL); }
|	expr ',' expr-list { $$ = ast_seq($1, $3); }
;

expr:
	T_const { $$ = ast_int($1); }
|	T_char { $$ = ast_char($1); }
|	l-value { $$ = $1; }
|	'(' expr ')' { $$ = $2; }
|	func-call { $$ = $1; }
|	expr '+' expr { $$ = ast_op($1, PLUS, $3); }
|	expr '-' expr { $$ = ast_op($1, MINUS, $3); }
|	expr '*' expr { $$ = ast_op($1, TIMES, $3); }
|	expr '/' expr { $$ = ast_op($1, DIV, $3); }
|	expr '%' expr { $$ = ast_op($1, MOD, $3); }
|	'+' expr { $$ = ast_op(ast_int(0), PLUS, $2); }		%prec UPLUS
|	'-' expr { $$ = ast_op(ast_int(0), MINUS, $2); }	%prec UMINUS
;

l-value:
	T_id { $$ = ast_id($1, NULL); }
|	T_id '[' expr ']' { $$ = ast_id($1, $3); }
|	T_string { $$ = ast_string($1); }
;

cond:
	"true" { $$ = ast_int(1); }
|	"false" { $$ = ast_int(0); }
|	'(' cond ')' { $$ = $2; }
|	'!' cond { $$ = ast_op(NULL, NOT, $2); }
|	expr "==" expr { $$ = ast_op($1, EQ, $3); }
|	expr "!=" expr { $$ = ast_op($1, NE, $3); }
|	expr '<' expr { $$ = ast_op($1, LT, $3); }
|	expr '>' expr { $$ = ast_op($1, GT, $3); }
|	expr "<=" expr { $$ = ast_op($1, LE, $3); }
|	expr ">=" expr { $$ = ast_op($1, GE, $3); }
|	cond '&' cond { $$ = ast_op($1, AND, $3); }
|	cond '|' cond { $$ = ast_op($1, OR, $3); }
;

%%

void yyerror (const char *msg) {
	fatal("%s in \"%s\"\n", msg, yytext);
}

int main() {
	filename = "sourcecode";
	linecount = 1;
	if (yyparse()) return 1;
	printf("Syntax analysis of input program was successful.\n");
	initSymbolTable(997);
	openScope();
	initLibFunctions();
	ast_sem(t);
	closeScope();
	destroySymbolTable();
	printf("Semantic analysis of input program was completed.\n");
	return sem_failed;
}

