#ifndef __AST_H__
#define __AST_H__

#include <stdio.h>
#include <stdlib.h>
#include <string>
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

class ASTNode {
public:
  kind k;
  Type type;
  int line;

  ASTNode();
  void sem();
};

class ASTId : public ASTNode {
public:
  std::string id;
  ASTNode *index;
  int nesting_diff;
  int offset;

  ASTId(std::string id, ASTNode *index);
  void sem();
};

class ASTInt : public ASTNode {
public:
  int n;
  
  ASTInt(int n);
  void sem();
};

class ASTChar : public ASTNode {
public:
  char c;
  
  ASTChar(char c);
  void sem();
};

class ASTString : public ASTNode {
public:
  std::string id;
  
  ASTString(std::string id);
  void sem();
};

class ASTVdef : public ASTNode {
public:
  std::string id;
  int n;    // n>0 for array, n=0 otherwise

  ASTVdef(std::string id, Type type, int n);
  void sem();
};

class ASTSeq : public ASTNode {
public:
  ASTNode *first;
  ASTNode *list;

  ASTSeq(ASTNode *first, ASTNode *list);
  void sem();
};

class ASTFdef : public ASTNode {
public:
  ASTNode *fdecl;
  ASTNode *body;

  ASTFdef(ASTNode *fdecl, ASTNode *body);
  void sem();
};

class ASTFdecl : public ASTNode {
public:
  std::string name;
  ASTSeq *param;
  ASTNode *locdef;
  int num_vars;     // for SCOPES (of functions)

  ASTFdecl(std::string name, Type type, ASTSeq *param, ASTNode *locdef);
  void sem();
};

class ASTPar : public ASTNode {
public:
  std::string name;
  PassMode pm;

  ASTPar(std::string name, Type type, PassMode pm);
  void sem();
};

class ASTAssign : public ASTNode {
public:
  ASTNode *lval;
  ASTNode *expr;
  // maybe??
  // int nesting_diff;
  // int offset;

  ASTAssign(ASTNode *lval, ASTNode *expr);
  void sem();
};

class ASTFcall : public ASTNode {
public:
  std::string name;
  ASTNode *param;

  ASTFcall(std::string name, ASTNode *param);
  void sem();
};

class ASTFcall_stmt : public ASTNode {
public:
  ASTNode *fcall;

  ASTFcall_stmt(ASTNode *fcall);
  void sem();
};

class ASTIf : public ASTNode {
public:
  ASTNode *cond;
  ASTNode *ifstmt;

  ASTIf(ASTNode *cond, ASTNode *ifstmt);
  void sem();
};

class ASTIfelse : public ASTNode {
public:
  ASTNode *ifnode;
  ASTNode *elsestmt;

  ASTIfelse(ASTNode *ifnode, ASTNode *elsestmt);
  void sem();
};

class ASTWhile : public ASTNode {
public:
  ASTNode *cond;
  ASTNode *stmt;

  ASTWhile(ASTNode *cond, ASTNode *stmt);
  void sem();
};

class ASTRet : public ASTNode {
public:
  ASTNode *expr;

  ASTRet(ASTNode *expr);
  void sem();
};

class ASTOp : public ASTNode {
public:
  kind op;
  ASTNode *left;
  ASTNode *right;

  ASTOp(ASTNode *left, kind op, ASTNode *right);
  void sem();
};

#endif
