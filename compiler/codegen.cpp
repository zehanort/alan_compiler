#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "codegen.hpp"
#include "error.h"
#include "symbol.h"
#include "general.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Scalar.h>
#if defined(LLVM_VERSION_MAJOR) && LLVM_VERSION_MAJOR >= 4
#include <llvm/Transforms/Scalar/GVN.h>
#endif

using namespace llvm;

/******************************
 * Global LLVM variables      *
 * related to the LLVM suite. *
 ******************************/
static LLVMContext TheContext;
static IRBuilder<> Builder(TheContext);
static std::unique_ptr<Module> TheModule;
static std::unique_ptr<legacy::FunctionPassManager> TheFPM;

// Global LLVM variables related to the generated code.
static Function *TheWriteInteger;
static Function *TheWriteString;

// Useful LLVM types.
static Type * i8 = IntegerType::get(TheContext, 8);
static Type * i16 = IntegerType::get(TheContext, 16);
static Type * i32 = IntegerType::get(TheContext, 32);
static Type * i64 = IntegerType::get(TheContext, 64);

// Useful LLVM helper functions...
// ...for bytes
inline ConstantInt* c8(char c) {
  return ConstantInt::get(TheContext, APInt(8, c, true));
}
// ...for integers
inline ConstantInt* c32(int n) {
  return ConstantInt::get(TheContext, APInt(32, n, true));
}

/***************************************
 * THE CODEGEN FUNCTION                *
 * creates IR code (after a successful *
 * semantic check)                     *
 ***************************************/

Value * codegen(ast t) {
  if (t == nullptr) return nullptr;
  switch (t->k) {
  
  case ID: {
    Value * index = codegen(t->right);
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
    return c32(t->num);

  case CHAR:
    return c8(t->num);

  case STRING:
    return Builder.CreateGlobalStringPtr(t->id);

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

  case IF: {
    Value *CondV = codegen(t->left);
    if (!CondV)
      return nullptr;

    Function *TheFunction = Builder.GetInsertBlock()->getParent();
    BasicBlock *ThenBB  = BasicBlock::Create(TheContext, "then", TheFunction);
    BasicBlock *ElseBB  = BasicBlock::Create(TheContext, "else");
    BasicBlock *MergeBB = BasicBlock::Create(TheContext, "ifcont");
    Builder.CreateCondBr(CondV, ThenBB, ElseBB);

    // Emit then block.
    Builder.SetInsertPoint(ThenBB);
    Value *ThenV = codegen(t->right);
    if (!ThenV)
      return nullptr;

    Builder.CreateBr(MergeBB);
    // Codegen of 'Then' can change the current block, update ThenBB for the PHI.
    ThenBB = Builder.GetInsertBlock();

    // Emit else block.
    TheFunction->getBasicBlockList().push_back(ElseBB);
    Builder.SetInsertPoint(ElseBB);
    Builder.CreateBr(MergeBB);

    // Emit merge block.
    TheFunction->getBasicBlockList().push_back(MergeBB);
    Builder.SetInsertPoint(MergeBB);
    PHINode *PN = Builder.CreatePHI(Type::getDoubleTy(TheContext), 2, "iftmp");

    PN->addIncoming(ThenV, ThenBB);
    PN->addIncoming(ElseV, ElseBB);
    return PN;
  }

  case IFELSE: {
    Value *CondV = codegen(t->left->left);
    if (!CondV)
      return nullptr;

    Function *TheFunction = Builder.GetInsertBlock()->getParent();
    BasicBlock *ThenBB  = BasicBlock::Create(TheContext, "then", TheFunction);
    BasicBlock *ElseBB  = BasicBlock::Create(TheContext, "else");
    BasicBlock *MergeBB = BasicBlock::Create(TheContext, "ifcont");
    Builder.CreateCondBr(CondV, ThenBB, ElseBB);

    // Emit then block.
    Builder.SetInsertPoint(ThenBB);
    Value *ThenV = codegen(t->left->right);
    if (!ThenV)
      return nullptr;

    Builder.CreateBr(MergeBB);
    // Codegen of 'Then' can change the current block, update ThenBB for the PHI.
    ThenBB = Builder.GetInsertBlock();

    // Emit else block.
    TheFunction->getBasicBlockList().push_back(ElseBB);
    Builder.SetInsertPoint(ElseBB);

    Value *ElseV = codegen(t->right);
    if (!ElseV)
      return nullptr;

    Builder.CreateBr(MergeBB);
    // codegen of 'Else' can change the current block, update ElseBB for the PHI.
    ElseBB = Builder.GetInsertBlock();

    // Emit merge block.
    TheFunction->getBasicBlockList().push_back(MergeBB);
    Builder.SetInsertPoint(MergeBB);
    PHINode *PN = Builder.CreatePHI(Type::getDoubleTy(TheContext), 2, "iftmp");

    PN->addIncoming(ThenV, ThenBB);
    PN->addIncoming(ElseV, ElseBB);
    return PN;
  }

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

  case PLUS: {
    Value *l = codegen(t->left);
    Value *r = codegen(t->right);
    return Builder.CreateAdd(l, r, "addtmp");
  }
    
  case MINUS: {
    Value *l = codegen(t->left);
    Value *r = codegen(t->right);
    return Builder.CreateSub(l, r, "subtmp");
  }
    
  case TIMES: {
    Value *l = codegen(t->left);
    Value *r = codegen(t->right);
    return Builder.CreateMul(l, r, "multmp");
  }
    
  case DIV: {
    Value *l = codegen(t->left);
    Value *r = codegen(t->right);
    return Builder.CreateSDiv(l, r, "divtmp");
  }

  case MOD: {
    Value *l = codegen(t->left);
    Value *r = codegen(t->right);
    return Builder.CreateSRem(l, r, "modtmp");
  }
    
  case EQ: {
    Value *l = codegen(t->left);
    Value *r = codegen(t->right);
    return Builder.CreateICmpEQ(l, r, "eqtmp");
  }

  case NE: {
    Value *l = codegen(t->left);
    Value *r = codegen(t->right);
    return Builder.CreateICmpNE(l, r, "neqtmp");
  }

  case LT: {
    Value *l = codegen(t->left);
    Value *r = codegen(t->right);
    return Builder.CreateICmpSLT(l, r, "lttmp");
  }

  case LE: {
    Value *l = codegen(t->left);
    Value *r = codegen(t->right);
    return Builder.CreateICmpSLE(l, r, "letmp");
  }

  case GT: {
    Value *l = codegen(t->left);
    Value *r = codegen(t->right);
    return Builder.CreateICmpSGT(l, r, "gttmp");
  }

  case GE: {
    Value *l = codegen(t->left);
    Value *r = codegen(t->right);
    return Builder.CreateICmpSGE(l, r, "getmp");
  }

  case AND: {
    Value *l = codegen(t->left);
    Value *r = codegen(t->right);
    return Builder.CreateAnd(l, r, "andtmp");
  }

  case OR: {
    Value *l = codegen(t->left);
    Value *r = codegen(t->right);
    return Builder.CreateOr(l, r, "ortmp");
  }

  case NOT: {
    Value *l = codegen(t->left);
    Value *r = codegen(t->right);
    return Builder.CreateNot(l, r, "nottmp");
  }
}
