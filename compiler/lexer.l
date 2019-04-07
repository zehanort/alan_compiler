%{
#include <stdio.h>
#include <string.h>
#include "ast.hpp"
#include "parser.hpp"
#include "symbol.h"
#include "general.h"

#define T_eof 0

/* a global counter for nested multiline comments */
int nesting_level = 0;

void yyerror (const char *msg);
%}

L [A-Za-z]
D [0-9]
W [ \t\r]
HEX (\\x([0-9a-fA-F]){2})
ESC "\\n"|"\\t"|"\\r"|"\\0"|"\\\\"|"\\\'"|"\\\""|(\\x([0-9]|[a-fA-F]){2})
OP [=+\-*/%!&|<>]
SEP [()\[\]{},:;]
COMMENTS_1_LINE "--".*\n

/*
 * an exlusive state will be used
 * for multiline comments (possibly nested)
 */
%x MULLINE_COMMENT

%%

"byte" 			{ return T_byte; }
"else" 			{ return T_else; }
"false" 		{ return T_false; }
"if" 			{ return T_if; }
"int" 			{ return T_int; }
"proc"			{ return T_proc; }
"reference" 	{ return T_reference; }
"return" 		{ return T_return; }
"while" 		{ return T_while; }
"true"			{ return T_true; }
"=="			{ return T_eq; }
"!="			{ return T_ne; }
"<="			{ return T_le; }
">="			{ return T_ge; }

{L}({L}|{D}|_)*			{ yylval.s = stringCopy(yytext); yylval.s[strlen(yytext)] = '\0'; return T_id; }

{D}+					{ yylval.n = atoi(yytext); return T_const; }

\'[^\n\'\"\\]\'			{ yylval.c = yytext[0]; return T_char; }
\'{ESC}\'				{ yylval.c = yytext[0]; return T_char; }

\"(\\.|[^\\"^\n])*\"	{ yylval.s = stringCopy(yytext); yylval.s[strlen(yytext)] = '\0'; return T_string; }

{OP}					{ return yytext[0]; }

{SEP}					{ return yytext[0]; }

{W}+					{ /* nothing */ }
\n						{ linecount++; }

{COMMENTS_1_LINE}		{ linecount++; }

"(*"						{ BEGIN(MULLINE_COMMENT); nesting_level = 1; }
<MULLINE_COMMENT>"(*"		{ nesting_level++; }
<MULLINE_COMMENT>"*"+")"	{ if (--nesting_level == 0) BEGIN(INITIAL); }
<MULLINE_COMMENT>"*"+		{ /* nothing */ }
<MULLINE_COMMENT>[^(*\n]+	{ /* nothing */ }
<MULLINE_COMMENT>\n			{ linecount++; }

<<EOF>>					{ return T_eof; }

.						{ yyerror("illegal token"); }

%%

