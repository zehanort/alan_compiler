#include "error.hpp"
#include "general.hpp"
#include "symbol.hpp"
#include "ast.hpp"
#include <stack>

using namespace std;

/* some globals to keep track of functions */
stack<SymbolEntry *> funcList;
SymbolEntry *currFunction;

void checkTypes(Type l, Type r, const char *op) {
  if (!equalType(l, r))
  	error("type mismatch in %s operator", op);
  if (!equalType(l, typeInteger) && !equalType(l, typeChar))  // !!!!!!!!!! WARNING, NEED TO CHECK RIGHT TYPE TOO?
    error("only int and byte types supported by %s operator", op);
}

SymbolEntry * lookup(string id) {
  return lookupEntry(id.c_str(), LOOKUP_ALL_SCOPES, true);
}

void ASTId::sem() {
	linecount = line;
  if (left) left->sem();
  SymbolEntry *e = lookup(id);
  if (!e) {     // ID not found; dummy value for type
    type = typeBoolean;
    return;
  }
  switch (e->entryType) {
    case ENTRY_VARIABLE:
    if (left == NULL)
      type = e->u.eVariable.type;
    else {
      if (e->u.eVariable.type->kind != TYPE_ARRAY && e->u.eVariable.type->kind != TYPE_IARRAY)
        error("indexed identifier is not an array");
      type = e->u.eVariable.type->refType;
    }
    break;

    case ENTRY_PARAMETER:
    if (left == NULL)
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
  left->sem();
  if (right) right->sem();
  closeScope();
  funcList.pop();
  if (funcList.empty()) currFunction = NULL;
  else currFunction = funcList.top();
  return;
}

void ASTFdecl::sem() {
	linecount = line;
  currFunction = newFunction(id.c_str());
  openScope();
  funcList.push(currFunction);
  // in case of error in function declaration:
  if (!currFunction)
    return;
  if (left) left->sem();
  endFunctionHeader(currFunction, type);
  if (right) right->sem();
  num_vars = currentScope->negOffset;
  return;
}

void ASTPar::sem() {
	linecount = line;
  if (pm == PASS_BY_VALUE) {
    if (type->kind == TYPE_ARRAY || type->kind == TYPE_IARRAY)
      error("an array can not be passed by value as a parameter to a function");
    newParameter(id.c_str(), type, PASS_BY_VALUE, currFunction);
  }
  else
    newParameter(id.c_str(), type, PASS_BY_REFERENCE, currFunction);
  return;
}

void ASTAssign::sem() {
	linecount = line;
  left->sem();
  if (left->type->kind == TYPE_ARRAY || left->type->kind == TYPE_IARRAY)
    error("left side of assignment can not be an array");
  right->sem();
  SymbolEntry * e = lookupEntry(left->id.c_str(), LOOKUP_ALL_SCOPES, false);
  if (!e)
    return;
  if (right->type==NULL)
    return;
  if (!equalType(left->type, right->type))
    error("type mismatch in assignment");
  return;
}

void ASTFcall::sem() {
	linecount = line;
  SymbolEntry * f = lookup(id);
  if (!f)
    return;
  if (f->entryType != ENTRY_FUNCTION)
    error("%s is not a function", id);
  type = f->u.eFunction.resultType;
  if (left) left->sem();

  ASTNode *currPar = left;
  SymbolEntry *expectedPar = f->u.eFunction.firstArgument;

  while (currPar) {
    if (!expectedPar) {
      error("expected less function parameters");
      return;
    }

    Type expectedParType = expectedPar->u.eParameter.type;
    Type currParType = currPar->left->type;

    if (expectedPar->u.eParameter.mode == PASS_BY_REFERENCE) {
    // if actual parameter:
    // --> has no id
      if (currPar->left->id.empty()) {
        error("parameters passed by reference must be l-values");
        return;
      }
      // --> is not an l-value
      Type charArrayType = typeArray(currPar->left->id.length(), typeChar);
      if (!equalType(currParType, charArrayType) && !lookupEntry(currPar->left->id.c_str(), LOOKUP_ALL_SCOPES, false)) {
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
    currPar = currPar->right;
    expectedPar = expectedPar->u.eParameter.next;
  }

  if (expectedPar)
    error("expected more function parameters");
  return;
}

void ASTFcall_stmt::sem() {
	linecount = line;
  left->sem();
  SymbolEntry * f = lookupEntry(left->id.c_str(), LOOKUP_ALL_SCOPES, false);
  if (!f)
    return;
  if (f->u.eFunction.resultType->kind != TYPE_VOID)
    error("functions called as statements must be declared as proc");
  return;
}

void ASTIf::sem() {
	linecount = line;
  left->sem();
  if (!equalType(left->type, typeBoolean))
    error("if expects a boolean condition");
  if (right) right->sem();
  return;
}

void ASTIfelse::sem() {
	linecount = line;
  left->sem();
  if (right) right->sem();
  return;
}

void ASTWhile::sem() {
	linecount = line;
  left->sem();
  if (!equalType(left->type, typeBoolean))
    error("while loop expects a boolean condition");
  if (right) right->sem();
  return;
}

void ASTRet::sem() {
	linecount = line;
  if (!currFunction)  // no current function? what?
    internal("return expression used outside of function body");
  else if (left) {    // if we have the form "return e", check e and function result type
    left->sem();
    if (!equalType(currFunction->u.eFunction.resultType, left->type))
      error("result type of function and return value mismatch");
  }
  else if (currFunction->u.eFunction.resultType->kind != TYPE_VOID)   // if we have the form "return" and fun is not proc
    error("function defined as proc can not return anything");
  return;
}

void ASTSeq::sem() {
	linecount = line;
  left->sem();
  if (right) right->sem();
  return;
}

void ASTOp::sem() {
	linecount = line;
  left->sem();
  right->sem();
  switch (op) {
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

    default: internal("undefined operator");
  }
}
