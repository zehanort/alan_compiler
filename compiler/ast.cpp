#include "ast.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace std;

SymbolEntry * currFunction;
func funcList = NULL;

void createFuncList(SymbolEntry * firstFunction) {
   funcList = new(sizeof(*funcList));
   funcList->function = firstFunction;
   funcList->prev = NULL;
   currFunction = funcList->function;
}

void addFunctionToList(SymbolEntry * newFunction) {
   func newFunc = new(sizeof(*newFunc));
   newFunc->function = newFunction;
   newFunc->prev = funcList;
   funcList = newFunc;
   currFunction = funcList->function;
}

void goToPrevFunction() {
   func temp = funcList->prev;
   delete(funcList);
   funcList = temp;
   currFunction = funcList->function;   
}

void checkTypes(Type l, Type r, char *op) {
  if (!equalType(l, r))
  	error("type mismatch in %s operator", op);
  if (!equalType(l, typeInteger) && !equalType(l, typeChar))  // !!!!!!!!!! WARNING, NEED TO CHECK RIGHT TYPE TOO?
    error("only int and byte types supported by %s operator", op);
}

SymbolEntry * lookup(string id) {
  return lookupEntry(id.c_str(), LOOKUP_ALL_SCOPES, true);
}

ASTNode::ASTNode() {
  this->line = linecount;
}

ASTId::ASTId(string id, ASTNode *index) {
  this->k = ID;
  this->id = id;
  this->index = index;
}

ASTInt::ASTInt(int n) {
  this->k = INT;
  this->n = n;
}

ASTChar::ASTChar(char c) {
  this->k = CHAR;
  this->c = c;
}

ASTString::ASTString(string id) {
  this->k = STRING;
  this->id = id;
}

ASTVdef::ASTVdef(string id, Type type, int n) {
  this->k = VDEF;
  this->id = id;
  if (n == 0)
    this->type = type;
  else  // this variable is an array
    this->type = typeArray(n, type);
}

ASTFdef::ASTFdef(ASTNode *fdecl, ASTNode *body) {
  this->k = FDEF;
  this->fdecl = fdecl;
  this->body = body;
}

ASTFdecl::ASTFdecl(string name, Type type, ASTSeq *param, ASTNode *locdef) {
  this->k = FDECL;
  this->name = name;
  this->type = type;
  this->param = param;
  this->locdef = locdef;
}

ASTPar::ASTPar(string name, Type type, PassMode pm) {
  this->name = name;
  this->type = type;
  if (pm == PASS_BY_VALUE)
    this->k = PAR_VAL;
  else
    this->k = PAR_REF;
}

ASTAssign::ASTAssign(ASTNode *lval, ASTNode *expr) {
  this->k = ASSIGN;
  this->lval = lval;
  this->expr = expr;
}

ASTFcall::ASTFcall(string name, ASTNode *param) {
  this->k = FCALL;
  this->name = name;
  this->param = param;
}

ASTFcall_stmt::ASTFcall_stmt(ASTNode *fcall) {
  this->k = FCALL_STMT;
  this->fcall = fcall;
}

ASTIf::ASTIf(ASTNode *cond, ASTNode *ifstmt) {
  this->k = IF;
  this->cond = cond;
  this->ifstmt = ifstmt;
}

ASTIfelse::ASTIfelse(ASTNode *ifnode, ASTNode *elsestmt) {
  this->k = IFELSE;
  this->ifnode = ifnode;
  this->elsestmt = elsestmt;
}

ASTWhile::ASTWhile(ASTNode *cond, ASTNode *stmt) {
  this->k = WHILE;
  this->cond = cond;
  this->stmt = stmt;
}

ASTRet::ASTRet(ASTNode *expr) {
  this->k = RET;
  this->expr = expr;
}

ASTSeq::ASTSeq(ASTNode *first, ASTNode *list) {
  this->k = SEQ;
  this->first = first;
  this->list = list;
}

ASTOp::ASTOp(ASTNode *left, kind op, ASTNode *right) {
  this->k = op;
  this->left = left;
  this->right = right;
}

void ASTId::sem() {
	linecount = line;
  if (index) index->sem();
  SymbolEntry *e = lookup(id);
  if (!e) {     // ID not found; dummy value for type
    type = typeBoolean;
    return;
  }
  switch (e->entryType) {
    case ENTRY_VARIABLE:
    if (index == NULL)
      type = e->u.eVariable.type;
    else {
      if (e->u.eVariable.type->kind != TYPE_ARRAY && e->u.eVariable.type->kind != TYPE_IARRAY)
        error("indexed identifier is not an array");
      type = e->u.eVariable.type->refType;
    }
    break;

    case ENTRY_PARAMETER:
    if (index == NULL)
      type = e->u.eParameter.type;
    else {
      if (e->u.eParameter.type->kind != TYPE_ARRAY && e->u.eParameter.type->kind != TYPE_IARRAY)
        error("indexed identifier is not an array");
      type = e->u.eParameter.type->refType;
    }
    break;

    case ENTRY_FUNCTION:
    type = e->u.eFunction.resultType;
    break;

    default:
    internal("garbage in symbol table");
  }
  nesting_diff = currentScope->nestingLevel - e->nestingLevel;
  offset = e->u.eVariable.offset;
  return;
}

void ASTInt::sem() {
	linecount = line;
  type = typeInteger;
  return;
}

void ASTChar::sem() {
	linecount = line;
  type = typeChar;
  return;
}

void ASTString::sem() {
	linecount = line;
  type = typeArray(id.length(), typeChar);
  return;
}

void ASTVdef::sem() {
	linecount = line;
  if (type->kind == TYPE_ARRAY && type->size <= 0)
    error("illegal size of array in variable definition");
  newVariable(id.c_str(), type);
  return;
}

void ASTFdef::sem() {
	linecount = line;
  fdecl->sem();
  if (body) body->sem();
  closeScope();
  if (!funcList->prev) currFunction = NULL;
  else goToPrevFunction();
  return;
}

void ASTFdecl::sem() {
	linecount = line;
  SymbolEntry * f = newFunction(name.c_str());
  openScope();
  if (!funcList) createFuncList(f);
  else addFunctionToList(f);
  // in case of error in function declaration:
  if (!f)
    return;
  if (param) param->sem();
  endFunctionHeader(currFunction, type);
  if (locdef) locdef->sem();
  num_vars = currentScope->negOffset;
  return;
}

void ASTPar::sem() {
	linecount = line;
  switch (k) {
    case PAR_VAL:
    if (type->kind == TYPE_ARRAY || type->kind == TYPE_IARRAY)
      error("an array can not be passed by value as a parameter to a function");
    newParameter(name.c_str(), type, PASS_BY_VALUE, currFunction);
    return;

    case PAR_REF:
    newParameter(name.c_str(), type, PASS_BY_REFERENCE, currFunction);
    return;
  }
}

void ASTAssign::sem() {
	linecount = line;
  lval->sem();
  if (lval->type->kind == TYPE_ARRAY || lval->type->kind == TYPE_IARRAY)
    error("left side of assignment can not be an array");
  expr->sem();
  SymbolEntry * e = lookupEntry(lval->id, LOOKUP_ALL_SCOPES, false);
  if (!e)
    return;
  if (expr->type==NULL)
    return;
  if (!equalType(lval->type, expr->type))
    error("type mismatch in assignment");
  return;
}

void ASTFcall::sem() {
	linecount = line;
  SymbolEntry * f = lookup(name);
  if (!f)
    return;
  if (f->entryType != ENTRY_FUNCTION)
    error("%s is not a function", name);
  type = f->u.eFunction.resultType;
  if (param) param->sem();

  ASTNode *currPar = param;
  SymbolEntry *expectedPar = f->u.eFunction.firstArgument;

  while (currPar) {
    if (!expectedPar) {
      error("expected less function parameters");
      return;
    }

    Type expectedParType = expectedPar->u.eParameter.type;
    Type currParType = currPar->first->type;

    if (expectedPar->u.eParameter.mode == PASS_BY_REFERENCE) {
    // if actual parameter:
    // --> has no id
      if (!currPar->first->id) {
        error("parameters passed by reference must be l-values");
        return;
      }
      // --> is not an l-value
      Type charArrayType = typeArray(strlen(currPar->first->id), typeChar);
      if (!equalType(currParType, charArrayType) && !lookupEntry(currPar->first->id, LOOKUP_ALL_SCOPES, false)) {
        error("parameters passed by reference must be l-values");
        return;
      }
    }

    if (expectedParType->kind == TYPE_IARRAY) {
      if ((currParType->kind != TYPE_ARRAY) && (currParType->kind != TYPE_IARRAY))
        error("function parameter expected to be an array");
      if (!equalType(expectedParType->refType, currParType->refType))
        error("function parameter expected to be an array of different type");
    }
    else {
      if (!equalType(expectedParType, currParType))
        error("function parameter type mismatch");
    }
    currPar = currPar->list;
    expectedPar = expectedPar->u.eParameter.next;
  }

  if (expectedPar)
    error("expected more function parameters");
  return;
}

void ASTFcall_stmt::sem() {
	linecount = line;
  fcall->sem();
  SymbolEntry * f = lookupEntry(fcall->name, LOOKUP_ALL_SCOPES, false);
  if (!f)
    return;
  if (f->u.eFunction.resultType->kind != TYPE_VOID)
    error("functions called as statements must be declared as proc");
  return;
}

void ASTIf::sem() {
	linecount = line;
  cond->sem();
  if (!equalType(cond->type, typeBoolean))
    error("if expects a boolean condition");
  if (ifstmt) ifstmt->sem();
  return;
}

void ASTIfelse::sem() {
	linecount = line;
  ifnode->sem();
  if (elsestmt) elsestmt->sem();
  return;
}

void ASTWhile::sem() {
	linecount = line;
  cond->sem();
  if (!equalType(cond->type, typeBoolean))
    error("while loop expects a boolean condition");
  if (stmt) stmt->sem();
  return;
}

void ASTRet::sem() {
	linecount = line;
  if (!currFunction)    // no current function? what?
    internal("return expression used outside of function body");
    else if (expr) {    // if we have the form "return e", check e and function result type
      expr->sem();
    if (!equalType(currFunction->u.eFunction.resultType, expr->type))
      error("result type of function and return value mismatch");
  }
  else if (currFunction->u.eFunction.resultType->kind != TYPE_VOID)   // if we have the form "return" and fun is not proc
    error("function defined as proc can not return anything");
  return;
}

void ASTSeq::sem() {
	linecount = line;
  first->sem();
  if (list) list->sem();
  return;
}

void ASTOp::sem() {
	linecount = line;
  left->sem();
  right->sem();
  switch (k) {
    case PLUS:
    if (left == NULL) {
      if (!equalType(right->type, typeInteger))
        error("signedness only supported by int type");
      else {
        type = typeInteger;
        return;
      }
    }
    checkTypes(left->type, right->type, "+");
    type = right->type;
    return;

    case MINUS:
    if (left == NULL) {
      if (!equalType(right->type, typeInteger))
        error("signedness only supported by int type");
      else {
        type = typeInteger;
        return;
      }
    }
    checkTypes(left->type, right->type, "-");
    type = right->type;
    return;

    case TIMES:
    checkTypes(left->type, right->type, "*");
    type = right->type;
    return;

    case DIV:
    checkTypes(left->type, right->type, "/");
    type = right->type;
    return;

    case MOD:
    checkTypes(left->type, right->type, "%%");
    type = right->type;
    return;

    case EQ:
    checkTypes(left->type, right->type, "==");
    type = typeBoolean;
    return;

    case NE:
    checkTypes(left->type, right->type, "!=");
    type = typeBoolean;
    return;

    case LT:
    checkTypes(left->type, right->type, "<");
    type = typeBoolean;
    return;

    case LE:
    checkTypes(left->type, right->type, "<=");
    type = typeBoolean;
    return;

    case GT:
    checkTypes(left->type, right->type, ">");
    type = typeBoolean;
    return;

    case GE:
    checkTypes(left->type, right->type, ">=");
    type = typeBoolean;
    return;

    case AND:
    if (!equalType(left->type, typeBoolean) ||
        !equalType(right->type, typeBoolean))
      error("only boolean conditions supported by & operator");
    type = typeBoolean;
    return;

    case OR:
    if (!equalType(left->type, typeBoolean) ||
        !equalType(right->type, typeBoolean))
      error("only boolean conditions supported by | operator");
    type = typeBoolean;
    return;

    case NOT:
    if (!equalType(right->type, typeBoolean))
      error("only boolean conditions supported by ! operator");
    type = typeBoolean;
    return;
  }