#include "error.hpp"
#include "general.hpp"
#include "symbol.hpp"
#include "ast.hpp"
#include <stack>

using namespace std;

// some globals to keep track of functions
stack<SymbolEntry *> funcList;
SymbolEntry *currFunction;
// flags used to define if there is a return instruction in non-proc function
int notIf, funcRet;

// function that calls error() if types l and r are not the same or not supported by operator op
void checkTypes(Type l, Type r, const char *op) {
  if (!equalType(l, r))
  	error("type mismatch in %s operator", op);
  if (!equalType(l, typeInteger) && !equalType(l, typeChar))
    error("only int and byte types supported by %s operator", op);
}

// function that looks up and returns SymbolEntry named id (in any scope)
SymbolEntry * lookup(string id) {
  return lookupEntry(id.c_str(), LOOKUP_ALL_SCOPES, true);
}

/* ---------------------------------------------------------------------
   ------------------ sem() method: semantic analysis ------------------
   --------------------------------------------------------------------- */

// semantic analysis of ASTId Node
void ASTId::sem() {
	linecount = line;
  if (left) left->sem();											// semantic analysis of index (if any)
  SymbolEntry *e = lookup(id);								// lookup in symbol table
  // if ID not found -> dummy value for type and return
  if (!e) {
    type = typeBoolean;
    return;
  }
  // if ID is found:
  switch (e->entryType) {
  	// if entry is a variable:
    case ENTRY_VARIABLE:
    // if there is no index, node's type <- variable's type
    if (left == NULL)
      type = e->u.eVariable.type;
    // if there is an index, node's type <- array's elements' type (if of course entry is an array)
    else {
      if (e->u.eVariable.type->kind != TYPE_ARRAY && e->u.eVariable.type->kind != TYPE_IARRAY)
        error("indexed identifier is not an array");
      type = e->u.eVariable.type->refType;
    }
    break;

    // if entry is a parameter:
    case ENTRY_PARAMETER:
    // if there is no index, node's type <- parameter's type
    if (left == NULL)
      type = e->u.eParameter.type;
    // if there is an index, node's type <- array's elements' type (if of course entry is an array)
    else {
      if (e->u.eParameter.type->kind != TYPE_ARRAY && e->u.eParameter.type->kind != TYPE_IARRAY)
        error("indexed identifier is not an array");
      type = e->u.eParameter.type->refType;
    }
    break;

    // if entry is a function:
    case ENTRY_FUNCTION:
    // node's type <- function's type
    type = e->u.eFunction.resultType;
    break;

    default:
    internal("garbage in symbol table");
  }
  nesting_diff = currentScope->nestingLevel - e->nestingLevel;
  offset = e->u.eVariable.offset;
  return;
}

// semantic analysis of ASTInt Node
void ASTInt::sem() {
	linecount = line;
  type = typeInteger;
  return;
}

// semantic analysis of ASTChar Node
void ASTChar::sem() {
	linecount = line;
  type = typeChar;
  return;
}

// semantic analysis of ASTString Node
void ASTString::sem() {
	linecount = line;
  type = typeArray(id.length(), typeChar);
  return;
}

// semantic analysis of ASTVdef Node
void ASTVdef::sem() {
	linecount = line;
  if (type->kind == TYPE_ARRAY && type->size <= 0)
    error("illegal size of array in variable definition");
  newVariable(id.c_str(), type);							// create new variable
  return;
}

// semantic analysis of ASTFdef Node
void ASTFdef::sem() {
	linecount = line;
  notIf = 1;																	// notIf = 1 <-> not inside if instruction
	if (left->type->kind == TYPE_VOID)					// for proc functions:
		funcRet = 1;															// --> no ret instr needed
	else 																				// for non-proc functions:
		funcRet = 0;															// --> set funcRet = 0 (no ret instr found in function main body)
  left->sem();																// semantic analysis of Fdecl
	if (right) right->sem();										// semantic analysis of function body (compound statement) if any
  closeScope();																// close function scope (after body)
  funcList.pop();															// pop from funcList
  // update currFunction:
  if (funcList.empty())												// for main() function
  	currFunction = NULL;
  else {																			// for other functions
  	currFunction = funcList.top();
  	if (!funcRet)															// warning if no ret instr was found in function body (outside if instr)
  		warning(("Control may reach end of non-proc function " + left->id + "().").c_str());
  }
  return;
}

// semantic analysis of ASTFdecl Node
void ASTFdecl::sem() {
	linecount = line;
  currFunction = newFunction(id.c_str());			// make this currFunction
  openScope();																// open function scope (before parameter and local def semantic analysis)
  funcList.push(currFunction);								// push into funcList
  // in case of error in function declaration:
  if (!currFunction)
    return;
  if (left) left->sem();											// semantic analysis of parameters (if any)
  endFunctionHeader(currFunction, type);
  if (right) right->sem();										// semantic analysis of local definitions (if any)
  num_vars = currentScope->negOffset;
  return;
}

// semantic analysis of ASTPar Node
void ASTPar::sem() {
	linecount = line;
	// create new parameter after checking passmode)
  if (pm == PASS_BY_VALUE) {
    if (type->kind == TYPE_ARRAY || type->kind == TYPE_IARRAY)
      error("an array can not be passed by value as a parameter to a function");
    newParameter(id.c_str(), type, PASS_BY_VALUE, currFunction);
  }
  else
    newParameter(id.c_str(), type, PASS_BY_REFERENCE, currFunction);
  return;
}

// semantic analysis of ASTAssign Node
void ASTAssign::sem() {
	linecount = line;
  left->sem();																// semantic analysis of l-value
  if (left->type->kind == TYPE_ARRAY || left->type->kind == TYPE_IARRAY)
    error("left side of assignment can not be an array");
  right->sem();																// semantic analysis of expression
  SymbolEntry * e = lookupEntry(left->id.c_str(), LOOKUP_ALL_SCOPES, false);
  if (!e)
    return;
  if (right->type==NULL)
    return;
  if (!equalType(left->type, right->type))
    error("type mismatch in assignment");
  return;
}

// semantic analysis of ASTFcall Node
void ASTFcall::sem() {
	linecount = line;
  SymbolEntry * f = lookup(id);								// lookup function
  if (!f)
    return;
  if (f->entryType != ENTRY_FUNCTION)
    error("%s is not a function", id);
  type = f->u.eFunction.resultType;						// node's type <- function's type
  if (left) left->sem();											// semantic analysis of parameter list

  ASTNode *currPar = left;																	// currPar <- 1st given parameter
  SymbolEntry *expectedPar = f->u.eFunction.firstArgument;	// expectedPar <- 1st expected parameter

  while (currPar) {
    if (!expectedPar) {
      error("expected less function parameters");
      return;
    }

    Type expectedParType = expectedPar->u.eParameter.type;
    Type currParType = currPar->left->type;

    // expected parameter passed by reference:
    if (expectedPar->u.eParameter.mode == PASS_BY_REFERENCE) {
    // error if actual parameter:
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

    // expected parameter is array:
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

    currPar = currPar->right;																// next given parameter
    expectedPar = expectedPar->u.eParameter.next;						// next expected parameter
  }

  if (expectedPar)
    error("expected more function parameters");
  return;
}

// semantic analysis of ASTFcall_stmt Node
void ASTFcall_stmt::sem() {
	linecount = line;
  left->sem();																// semantic analysis of function call
  SymbolEntry * f = lookupEntry(left->id.c_str(), LOOKUP_ALL_SCOPES, false);
  if (!f)
    return;
  if (f->u.eFunction.resultType->kind != TYPE_VOID)
    error("functions called as statements must be declared as proc");
  return;
}

// semantic analysis of ASTIf Node
void ASTIf::sem() {
	linecount = line;
  left->sem();																// semantic analysis of condition
  if (!equalType(left->type, typeBoolean))
    error("if expects a boolean condition");
	int notIf_ = notIf;													// save previous state of notIf (in case of nested ifs)
	notIf = 0;																	// notIf = 0 <-> inside if instruction body
  if (right) right->sem();										// semantic analysis of (if) compound statement, if any
	notIf = notIf_;															// restore previous state of notIf
  return;
}

// semantic analysis of ASTIfelse Node
void ASTIfelse::sem() {
	linecount = line;
  left->sem();
	int notIf_ = notIf;													// save previous state of notIf (in case of nested ifs)
	notIf = 0;																	// notIf = 0 <-> inside else instruction body
  if (right) right->sem();										// semantic analysis of (else) compound statement, if any
  notIf = notIf_;															// restore previous state of notIf
  return;
}

// semantic analysis of ASTWhile Node
void ASTWhile::sem() {
	linecount = line;
  left->sem();																// semantic analysis of condition
  if (!equalType(left->type, typeBoolean))
    error("while loop expects a boolean condition");
  if (right) right->sem();										// semantic analysis of compound statement (if any)
  return;
}

// semantic analysis of ASTRet Node
void ASTRet::sem() {
	linecount = line;
  if (!currFunction)													// no current function? what?
    internal("return expression used outside of function body");
  else if (left) {    												// if we have the form "return e", check e and function result type
    left->sem();															// semantic analysis of returned expression
    if (!equalType(currFunction->u.eFunction.resultType, left->type))
      error("result type of function and return value mismatch");
  }
  else if (currFunction->u.eFunction.resultType->kind != TYPE_VOID)
    error("return with no value, in non-proc function");
  if (notIf) funcRet = 1;											// if we are NOT inside the body of an if instr, ret instr exists
  return;
}

// semantic analysis of ASTSeq Node
void ASTSeq::sem() {
	linecount = line;
  if (left) left->sem();											// semantic analysis of 1st node
  if (right) right->sem();										// semantic analysis of the rest of seq
  return;
}

// semantic analysis of ASTOp Node
void ASTOp::sem() {
	linecount = line;
  if (left)  left->sem();											// semantic analysis of left expression
  if (right) right->sem();										// semantic analysis of right expression
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

    case TRUE_:
    case FALSE_:
    type = typeBoolean;
    return;

    default: internal("undefined operator");
  }
}
