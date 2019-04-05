#ifndef __AST_H__
#define __AST_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "symbol.h"
#include "general.h"

typedef struct f_node {
  SymbolEntry * function;
  struct f_node * prev;
} * func;

typedef enum {
  ID, INT, CHAR, STRING,										            	// terminals
  VDEF, FDEF, FDECL, PAR_VAL, PAR_REF,		            		// definitions
  ASSIGN, FCALL, FCALL_STMT, IF, IFELSE, WHILE, RET, SEQ, // statements
  EQ, NE, LE, GE, LT, GT, AND, OR, NOT,		             		// condition operators
  PLUS, MINUS, TIMES, DIV, MOD,				             				// expression operators
} kind;

typedef struct node {
  kind k;
  char *id;
  int num;
  struct node *left, *right;
  Type type;
  int line;
  int nesting_diff; // ID and ASSIGN nodes
  int offset;       // ID and ASSIGN nodes
  int num_vars;     // for SCOPES (of functions) -> FDECL nodes
} *ast;

ast ast_id (char *id, ast index);
ast ast_int (int n);
ast ast_char (char c);
ast ast_string (char *s);
ast ast_vdef (char *id, Type t, int n);      // n>0 for array, n=0 otherwise
ast ast_fdef (ast l, ast r);
ast ast_fdecl (char *id, Type t, ast l, ast r);
ast ast_par (char *c, Type t, PassMode pm);
ast ast_assign (ast l, ast r);
ast ast_fcall (char *id, ast r);
ast ast_fcall_stmt (ast r);
ast ast_if (ast l, ast r);
ast ast_ifelse (ast l, ast r);
ast ast_while (ast l, ast r);
ast ast_ret (ast r);
ast ast_seq (ast l, ast r);
ast ast_op (ast l, kind op, ast r);

void ast_sem (ast t);

#endif
