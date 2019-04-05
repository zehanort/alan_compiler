#include "ast.h"

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

static ast ast_make(kind k, char *s, int n, ast l, ast r, Type t) {
  ast p = new(sizeof(struct node));
  p->id = stringCopy(s);
  p->k = k;
  p->num = n;
  p->left = l;
  p->right = r;
  p->type = t;
  p->line = linecount;
  return p;
}

ast ast_id(char *s, ast index) {
  return ast_make(ID, s, 0, NULL, index, NULL);
}

ast ast_int(int n) {
  return ast_make(INT, '\0', n, NULL, NULL, NULL);
}

ast ast_char(char b) {
  return ast_make(CHAR, '\0', b, NULL, NULL, NULL);
}

ast ast_string(char *s) {
  return ast_make(STRING, s, 0, NULL, NULL, NULL);
}

ast ast_vdef(char *id, Type t, int n) {
  if (n == 0)
    return ast_make(VDEF, id, 0, NULL, NULL, t);
  else {    // this variable is an array
    return ast_make(VDEF, id, 0, NULL, NULL, typeArray(n, t));
  }
}

ast ast_fdef(ast l, ast r) {
  return ast_make(FDEF, '\0', 0, l, r, NULL);
}

ast ast_fdecl(char *id, Type t, ast l, ast r) {
  return ast_make(FDECL, id, 0, l, r, t);
}

ast ast_par(char *id, Type t, PassMode pm) {
  if (pm == PASS_BY_VALUE)
    return ast_make(PAR_VAL, id, 0, NULL, NULL, t);
  else
    return ast_make(PAR_REF, id, 0, NULL, NULL, t);
}

ast ast_assign(ast l, ast r) {
  return ast_make(ASSIGN, '\0', 0, l, r, NULL);
}

ast ast_fcall(char *id, ast r) {
  return ast_make(FCALL, id, 0, NULL, r, NULL);
}

ast ast_fcall_stmt(ast r) {
  return ast_make(FCALL_STMT, '\0', 0, NULL, r, NULL);
}

ast ast_if(ast l, ast r) {
  return ast_make(IF, '\0', 0, l, r, NULL);
}

ast ast_ifelse(ast l, ast r) {
  return ast_make(IFELSE, '\0', 0, l, r, NULL);
}

ast ast_while(ast l, ast r) {
  return ast_make(WHILE, '\0', 0, l, r, NULL);
}

ast ast_ret(ast r) {
  return ast_make(RET, '\0', 0, NULL, r, NULL);
}

ast ast_seq(ast l, ast r) {
  return ast_make(SEQ, '\0', 0, l, r, NULL);
}

ast ast_op(ast l, kind op, ast r) {
  return ast_make(op, '\0', 0, l, r, NULL);
}

SymbolEntry * lookup(char *id) {
  return lookupEntry(id, LOOKUP_ALL_SCOPES, true);
}

void ast_sem(ast t) {
  if (t == NULL) return;
  linecount = t->line;
  switch (t->k) {
  
  case ID: {
    ast_sem(t->right);
    SymbolEntry *e = lookup(t->id);
    if (!e) {   // ID not found; dummy value for type
      t->type = typeBoolean;
      return;
    }
    switch (e->entryType) {
    
    case ENTRY_VARIABLE:
      if (t->right == NULL)
        t->type = e->u.eVariable.type;
      else {
        if (e->u.eVariable.type->kind != TYPE_ARRAY && e->u.eVariable.type->kind != TYPE_IARRAY)
          error("indexed identifier is not an array");
        t->type = e->u.eVariable.type->refType;
      }
      break;

    case ENTRY_PARAMETER:
      if (t->right == NULL)
        t->type = e->u.eParameter.type;
      else {
        if (e->u.eParameter.type->kind != TYPE_ARRAY && e->u.eParameter.type->kind != TYPE_IARRAY)
          error("indexed identifier is not an array");
        t->type = e->u.eParameter.type->refType;
      }
      break;

    case ENTRY_FUNCTION:
      t->type = e->u.eFunction.resultType;
      break;

    default:
      internal("garbage in symbol table");
    }
    t->nesting_diff = currentScope->nestingLevel - e->nestingLevel;
    t->offset = e->u.eVariable.offset;
    return;
  }

  case INT:
    t->type = typeInteger;
    return;

  case CHAR:
    t->type = typeChar;
    return; 
  
  case STRING:
    t->type = typeArray(strlen(t->id), typeChar);
    return;

  case VDEF:
    if (t->type->kind == TYPE_ARRAY && t->type->size <= 0)
      error("illegal size of array in variable definition");
    newVariable(t->id, t->type);
    return;

  case FDEF:
    ast_sem(t->left);
    ast_sem(t->right);
    closeScope();
    if (!funcList->prev) currFunction = NULL;
    else goToPrevFunction();
    return;

  case FDECL: {
    SymbolEntry * f = newFunction(t->id);
    openScope();
    if (!funcList) createFuncList(f);
    else addFunctionToList(f);
    // in case of error in function declaration:
    if (!f)
      return;
    ast_sem(t->left);
    endFunctionHeader(currFunction, t->type);
    ast_sem(t->right);
    t->num_vars = currentScope->negOffset;
    return;
  }

  case PAR_VAL:
    if (t->type->kind == TYPE_ARRAY || t->type->kind == TYPE_IARRAY)
      error("an array can not be passed by value as a parameter to a function");
    newParameter(t->id, t->type, PASS_BY_VALUE, currFunction);
    return;

  case PAR_REF:
    newParameter(t->id, t->type, PASS_BY_REFERENCE, currFunction);
    return;

  case ASSIGN: {
    ast_sem(t->left);
    if (t->left->type->kind == TYPE_ARRAY || t->left->type->kind == TYPE_IARRAY)
      error("left side of assignment can not be an array");
    ast_sem(t->right);
    SymbolEntry * e = lookupEntry(t->left->id, LOOKUP_ALL_SCOPES, false);
    if (!e)
      return;
    if (t->right->type==NULL)
      return;
    if (!equalType(t->left->type, t->right->type))
      error("type mismatch in assignment");
    return;
  }

  case IF:
    ast_sem(t->left);
    if (!equalType(t->left->type, typeBoolean))
      error("if expects a boolean condition");
    ast_sem(t->right);
    return;

  case IFELSE:
    ast_sem(t->left);
    ast_sem(t->right);
    return;

  case WHILE:
    ast_sem(t->left);
    if (!equalType(t->left->type, typeBoolean))
      error("while loop expects a boolean condition");
    ast_sem(t->right);
    return;

  case RET:
    if (!currFunction)    // no current function? what?
      internal("return expression used outside of function body");
    else if (t->right) {  // if we have the form "return e", check e and function result type
      ast_sem(t->right);
      if (!equalType(currFunction->u.eFunction.resultType, t->right->type))
        error("result type of function and return value mismatch");
    }
    else if (currFunction->u.eFunction.resultType->kind != TYPE_VOID)   // if we have the form "return" and fun is not proc
      error("function defined as proc can not return anything");
    return;

  case SEQ:
    ast_sem(t->left);
    ast_sem(t->right);
    return;

  case FCALL: {
    SymbolEntry * f = lookup(t->id);
    if (!f)
      return;
    if (f->entryType != ENTRY_FUNCTION)
      error("%s is not a function", t->id);
    t->type = f->u.eFunction.resultType;
    ast_sem(t->right);

    ast currPar = t->right;
    SymbolEntry * expectedPar = f->u.eFunction.firstArgument;

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
        if (!currPar->left->id) {
          error("parameters passed by reference must be l-values");
          return;
        }
        // --> is not an l-value
        Type charArrayType = typeArray(strlen(currPar->left->id), typeChar);
        if (!equalType(currParType, charArrayType) && !lookupEntry(currPar->left->id, LOOKUP_ALL_SCOPES, false)) {
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

  case FCALL_STMT: {
    ast_sem(t->right);
    SymbolEntry * f = lookupEntry(t->right->id, LOOKUP_ALL_SCOPES, false);
    if (!f)
      return;
    if (f->u.eFunction.resultType->kind != TYPE_VOID)
      error("functions called as statements must be declared as proc");
    return;
  }

  case PLUS:
    ast_sem(t->left);
    ast_sem(t->right);
    if (t->left == NULL) {
      if (!equalType(t->right->type, typeInteger))
        error("signedness only supported by int type");
      else {
        t->type = typeInteger;
        return;
      }
    }
    checkTypes(t->left->type, t->right->type, "+");
    t->type = t->right->type;
    return;
    
  case MINUS:
    ast_sem(t->left);
    ast_sem(t->right);
    if (t->left == NULL) {
      if (!equalType(t->right->type, typeInteger))
        error("signedness only supported by int type");
      else {
        t->type = typeInteger;
        return;
      }
    }
    checkTypes(t->left->type, t->right->type, "-");
    t->type = t->right->type;
    return;
    
  case TIMES:
    ast_sem(t->left);
    ast_sem(t->right);
    checkTypes(t->left->type, t->right->type, "*");
    t->type = t->right->type;
    return;
    
  case DIV:
    ast_sem(t->left);
    ast_sem(t->right);
    checkTypes(t->left->type, t->right->type, "/");
    t->type = t->right->type;
    return;
    
  case MOD:
    ast_sem(t->left);
    ast_sem(t->right);
    checkTypes(t->left->type, t->right->type, "%%");
    t->type = t->right->type;
    return;
    
  case EQ:
    ast_sem(t->left);
    ast_sem(t->right);
    checkTypes(t->left->type, t->right->type, "==");
    t->type = typeBoolean;
    return;

  case NE:
    ast_sem(t->left);
    ast_sem(t->right);
    checkTypes(t->left->type, t->right->type, "!=");
    t->type = typeBoolean;
    return;

  case LT:
    ast_sem(t->left);
    ast_sem(t->right);
    checkTypes(t->left->type, t->right->type, "<");
    t->type = typeBoolean;
    return;

  case LE:
    ast_sem(t->left);
    ast_sem(t->right);
    checkTypes(t->left->type, t->right->type, "<=");
    t->type = typeBoolean;
    return;

  case GT:
    ast_sem(t->left);
    ast_sem(t->right);
    checkTypes(t->left->type, t->right->type, ">");
    t->type = typeBoolean;
    return;

  case GE:
    ast_sem(t->left);
    ast_sem(t->right);
    checkTypes(t->left->type, t->right->type, ">=");
    t->type = typeBoolean;
    return;

  case AND:
    ast_sem(t->left);
    ast_sem(t->right);
    if (!equalType(t->left->type, typeBoolean) ||
        !equalType(t->right->type, typeBoolean))
      error("only boolean conditions supported by & operator");
    t->type = typeBoolean;
    return;

  case OR:
    ast_sem(t->left);
    ast_sem(t->right);
    if (!equalType(t->left->type, typeBoolean) ||
        !equalType(t->right->type, typeBoolean))
      error("only boolean conditions supported by | operator");
    t->type = typeBoolean;
    return;

  case NOT:
    ast_sem(t->right);
    if (!equalType(t->right->type, typeBoolean))
      error("only boolean conditions supported by ! operator");
    t->type = typeBoolean;
    return;
  }
}
