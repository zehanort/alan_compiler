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
  
  case ID:

  case INT:
    return c32(t->num);

  case CHAR:
    return c8(t->num);

  case STRING:
    return Builder.CreateGlobalStringPtr(t->id);

  case VDEF:
    
  case FDEF:
    
  case FDECL:
    
  case PAR_VAL:

  case PAR_REF:

  case ASSIGN:

  case IF: {
    Value *CondV = codegen(t->left);
    Function *TheFunction = Builder.GetInsertBlock()->getParent();
    BasicBlock *ThenBB  = BasicBlock::Create(TheContext, "then", TheFunction);
    BasicBlock *MergeBB = BasicBlock::Create(TheContext, "endif", TheFunction);
    Builder.CreateCondBr(CondV, ThenBB, MergeBB);

    // Emit then block
    Builder.SetInsertPoint(ThenBB);
    codegen(t->right);
    Builder.CreateBr(MergeBB);
    
    // Change Insert Point
    Builder.SetInsertPoint(MergeBB);
    return nullptr;
  }

  case IFELSE: {
    Value *CondV = codegen(t->left->left);
    Function *TheFunction = Builder.GetInsertBlock()->getParent();
    BasicBlock *ThenBB  = BasicBlock::Create(TheContext, "then", TheFunction);
    BasicBlock *ElseBB  = BasicBlock::Create(TheContext, "else", TheFunction);
    BasicBlock *MergeBB = BasicBlock::Create(TheContext, "endif", TheFunction);
    Builder.CreateCondBr(CondV, ThenBB, ElseBB);

    // Emit then block
    Builder.SetInsertPoint(ThenBB);
    codegen(t->left->right);
    Builder.CreateBr(MergeBB);
    
    // Emit else block
    Builder.SetInsertPoint(ElseBB);
    codegen(t->right);
    Builder.CreateBr(MergeBB);

    // Change Insert Point
    Builder.SetInsertPoint(MergeBB);
    return nullptr;
  }

  case WHILE: {
    Function *TheFunction = Builder.GetInsertBlock()->getParent();
    BasicBlock *CondBB = BasicBlock::Create(TheContext, "cond", TheFunction);
    BasicBlock *LoopBB = BasicBlock::Create(TheContext, "loop", TheFunction);
    BasicBlock *AfterBB = BasicBlock::Create(TheContext, "after", TheFunction);
    
    // Emit Condition block
    Builder.SetInsertPoint(CondBB);
    Value *CondV = codegen(t->left);
    Builder.CreateCondBr(CondV, LoopBB, AfterBB);
    // Emit Loop block
    Builder.SetInsertPoint(LoopBB);
    codegen(t->right);
    Builder.CreateBr(CondBB);
    Builder.SetInsertPoint(AfterBB);
    return nullptr;
  }

  case RET:

  case SEQ:
    codegen(t->left);
    codegen(t->right);
    return;

  case FCALL:

  case FCALL_STMT:

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
