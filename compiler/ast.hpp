#ifndef __AST_HPP__
#define __AST_HPP__

#include <string>
#include "general.hpp"
#include "symbol.hpp"

using namespace std;

typedef struct f_node {
  SymbolEntry * function;
  struct f_node * prev;
} * func;

typedef enum {
  EQ, NE, LE, GE, LT, GT, AND, OR, NOT,		             		// condition operators
  PLUS, MINUS, TIMES, DIV, MOD,				             				// expression operators
} kind;

class ASTNode {
public:
  int line;

  ASTNode() : line(linecount) {};
  void sem();
};

class ASTId : public ASTNode {
public:
  string id;
  ASTNode *index;
  int nesting_diff;
  int offset;

  ASTId(string id, ASTNode *index) : id(id), index(index) {};
  void sem();
};

class ASTInt : public ASTNode {
public:
  int n;
  
  ASTInt(int n) : n(n) {};
  void sem();
};

class ASTChar : public ASTNode {
public:
  char c;
  
  ASTChar(char c) : c(c) {};
  void sem();
};

class ASTString : public ASTNode {
public:
  string id;
  
  ASTString(string id) : id(id) {};
  void sem();
};

class ASTVdef : public ASTNode {
public:
  string id;
  Type type;

  ASTVdef(string id, Type type, int n) : id(id) {
    if (n == 0)
      this->type = type;
    else  // this variable is an array
      this->type = typeArray(n, type);
  };
  void sem();
};

class ASTSeq : public ASTNode {
public:
  ASTNode *first;
  ASTNode *list;

  ASTSeq(ASTNode *first, ASTNode *list) : first(first), list(list) {};
  void sem();
};

class ASTFdef : public ASTNode {
public:
  ASTNode *fdecl;
  ASTNode *body;

  ASTFdef(ASTNode *fdecl, ASTNode *body) : fdecl(fdecl), body(body) {};
  void sem();
};

class ASTFdecl : public ASTNode {
public:
  string name;
  Type type;
  ASTNode *param;
  ASTNode *locdef;
  int num_vars;     // for SCOPES (of functions)

  ASTFdecl(string name, Type type, ASTNode *param, ASTNode *locdef) :
    name(name), type(type), param(param), locdef(locdef) {};
  void sem();
};

class ASTPar : public ASTNode {
public:
  string name;
  Type type;
  PassMode pm;

  ASTPar(string name, Type type, PassMode pm) : name(name), type(type), pm(pm) {};
  void sem();
};

class ASTAssign : public ASTNode {
public:
  ASTNode *lval;
  ASTNode *expr;
  // maybe??
  // int nesting_diff;
  // int offset;

  ASTAssign(ASTNode *lval, ASTNode *expr) : lval(lval), expr(expr) {};
  void sem();
};

class ASTFcall : public ASTNode {
public:
  string name;
  ASTNode *param;

  ASTFcall(string name, ASTNode *param) : name(name), param(param) {};
  void sem();
};

class ASTFcall_stmt : public ASTNode {
public:
  ASTNode *fcall;

  ASTFcall_stmt(ASTNode *fcall) : fcall(fcall) {};
  void sem();
};

class ASTIf : public ASTNode {
public:
  ASTNode *cond;
  ASTNode *ifstmt;

  ASTIf(ASTNode *cond, ASTNode *ifstmt) : cond(cond), ifstmt(ifstmt) {};
  void sem();
};

class ASTIfelse : public ASTNode {
public:
  ASTNode *ifnode;
  ASTNode *elsestmt;

  ASTIfelse(ASTNode *ifnode, ASTNode *elsestmt) : ifnode(ifnode), elsestmt(elsestmt) {};
  void sem();
};

class ASTWhile : public ASTNode {
public:
  ASTNode *cond;
  ASTNode *stmt;

  ASTWhile(ASTNode *cond, ASTNode *stmt) : cond(cond), stmt(stmt) {};
  void sem();
};

class ASTRet : public ASTNode {
public:
  ASTNode *expr;

  ASTRet(ASTNode *expr) : expr(expr) {};
  void sem();
};

class ASTOp : public ASTNode {
public:
  kind op;
  ASTNode *left;
  ASTNode *right;

  ASTOp(ASTNode *left, kind op, ASTNode *right) : op(op), left(left), right(right) {};
  void sem();
};

#endif
