#ifndef __AST_HPP__
#define __AST_HPP__

#include <iostream>
#include <string>
#include "general.hpp"
#include "symbol.hpp"

#include <llvm/IR/Value.h>

using namespace std;

typedef enum {
  EQ, NE, LE, GE, LT, GT, AND, OR, NOT, // condition operators
  PLUS, MINUS, TIMES, DIV, MOD,         // expression operators
  STRING                                // needed for codegen :(
} kind;

/***************************
 * Abstract AST Node Class *
 ***************************/
class ASTNode {

public:
  int line = linecount;  // line number
  kind op;               // kind of operation (ASTOp only)
  string id;             // name (vars, functions, chars)
  Type type;             // ASTExpr only
  int num;               // numeric value of ints/bytes
  ASTNode *left, *right; // left and right (generic) AST nodes
  PassMode pm;           // ASTPar only
  int nesting_diff;      // ASTId and ASTAssign only
  int offset;            // ASTId and ASTAssign only
  int num_vars;          // ASTFdecl only

  virtual void sem() = 0;
  virtual llvm::Value * codegen() = 0;

protected:
  // various constructors needed by children classes
  ASTNode(string s, ASTNode *l) : id(s), left(l) {};
  ASTNode(int n) : num(n) {};
  ASTNode(char c) { id = c; };
  ASTNode(string s) : op(STRING), id(s) {};
  ASTNode(string s, Type t, int n) : id(s) {
    if (n == 0)
      this->type = t;
    else // this variable is an array
      this->type = typeArray(n, t);
  };
  ASTNode(string s, Type t, ASTNode *l, ASTNode *r) : id(s), type(t), left(l), right(r) {};
  ASTNode(string s, Type t, PassMode pm) : id(s), type(t), pm(pm) {};
  ASTNode(ASTNode *l, kind op, ASTNode *r) : op(op), left(l), right(r) {};
  ASTNode(ASTNode *l) : left(l) {};
  ASTNode(ASTNode *l, ASTNode *r) : left(l), right(r) {};

  virtual ~ASTNode() {
    delete left;
    delete right;
  };
};

/************************************************************
 * AST Node Classes used in the AST representation of input *
 ************************************************************/
class ASTId : public ASTNode {
public:
  ASTId(string id, ASTNode *index) : ASTNode(id, index) {};
  void sem();
  llvm::Value * codegen();
};

class ASTInt : public ASTNode {
public:
  ASTInt(int n) : ASTNode(n) {};
  void sem();
  llvm::Value * codegen();
};

class ASTChar : public ASTNode {
public:
  ASTChar(char c) : ASTNode(c) {};
  void sem();
  llvm::Value * codegen();
};

class ASTString : public ASTNode {
public:
  ASTString(string id) : ASTNode(id) {};
  void sem();
  llvm::Value * codegen();
};

class ASTVdef : public ASTNode {
public:
  ASTVdef(string id, Type type, int n) : ASTNode(id, type, n) {};
  void sem();
  llvm::Value * codegen();
};

class ASTSeq : public ASTNode {
public:
  ASTSeq(ASTNode *hd, ASTNode *tl) : ASTNode(hd, tl) {};
  void sem();
  llvm::Value * codegen();
};

class ASTFdef : public ASTNode {
public:
  ASTFdef(ASTNode *fdecl, ASTNode *body) : ASTNode(fdecl, body) {};
  void sem();
  llvm::Value * codegen();
};

class ASTFdecl : public ASTNode {
public:
  ASTFdecl(string name, Type t, ASTNode *params, ASTNode *locdef) : ASTNode(name, t, params, locdef) {};
  void sem();
  llvm::Value * codegen();
};

class ASTPar : public ASTNode {
public:
  ASTPar(string name, Type t, PassMode pm) : ASTNode(name, t, pm) {};
  void sem();
  llvm::Value * codegen();
};

class ASTAssign : public ASTNode {
public:
  ASTAssign(ASTNode *lval, ASTNode *expr) : ASTNode(lval, expr) {};
  void sem();
  llvm::Value * codegen();
};

class ASTFcall : public ASTNode {
public:
  ASTFcall(string name, ASTNode *params) : ASTNode(name, params) {};
  void sem();
  llvm::Value * codegen();
};

class ASTFcall_stmt : public ASTNode {
public:
  ASTFcall_stmt(ASTNode *fcall) : ASTNode(fcall) {};
  void sem();
  llvm::Value * codegen();
};

class ASTIf : public ASTNode {
public:
  ASTIf(ASTNode *cond, ASTNode *ifblock) : ASTNode(cond, ifblock) {};
  void sem();
  llvm::Value * codegen();
};

class ASTIfelse : public ASTNode {
public:
  ASTIfelse(ASTNode *ifnode, ASTNode *elseblock) : ASTNode(ifnode, elseblock) {};
  void sem();
  llvm::Value * codegen();
};

class ASTWhile : public ASTNode {
public:
  ASTWhile(ASTNode *cond, ASTNode *body) : ASTNode(cond, body) {};
  void sem();
  llvm::Value * codegen();
};

class ASTRet : public ASTNode {
public:
  ASTRet(ASTNode *expr) : ASTNode(expr) {};
  void sem();
  llvm::Value * codegen();
};

class ASTOp : public ASTNode {
public:
  ASTOp(ASTNode *left, kind op, ASTNode *right) : ASTNode(left, op, right) {};
  void sem();
  llvm::Value * codegen();
};

#endif
