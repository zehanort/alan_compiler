#include "codegen.hpp"

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

void ASTId::codegen() {
  switch (e->entryType) {
    case ENTRY_VARIABLE:

    case ENTRY_PARAMETER:

    case ENTRY_FUNCTION:

    default:
  }
}

void ASTInt::codegen() {
  return c32(t->num);
}

void ASTChar::codegen() {
  return c8(t->num);
}

void ASTString::codegen() {
  return Builder.CreateGlobalStringPtr(this->id);
}

void ASTVdef::codegen() {
}

void ASTFdef::codegen() {
}

void ASTFdecl::codegen() {
}

void ASTPar::codegen() {
}

void ASTAssign::codegen() {
}

void ASTFcall::codegen() {
}

void ASTFcall_stmt::codegen() {
}

void ASTIf::codegen() {
  Value *CondV = this->left->codegen();
  Function *TheFunction = Builder.GetInsertBlock()->getParent();
  BasicBlock *ThenBB  = BasicBlock::Create(TheContext, "then", TheFunction);
  BasicBlock *MergeBB = BasicBlock::Create(TheContext, "endif", TheFunction);
  Builder.CreateCondBr(CondV, ThenBB, MergeBB);

  // Emit then block
  Builder.SetInsertPoint(ThenBB);
  this->right->codegen();
  Builder.CreateBr(MergeBB);
  
  // Change Insert Point
  Builder.SetInsertPoint(MergeBB);
  return nullptr;
}

void ASTIfelse::codegen() {
  Value *CondV = this->left->left->codegen();
  Function *TheFunction = Builder.GetInsertBlock()->getParent();
  BasicBlock *ThenBB  = BasicBlock::Create(TheContext, "then", TheFunction);
  BasicBlock *ElseBB  = BasicBlock::Create(TheContext, "else", TheFunction);
  BasicBlock *MergeBB = BasicBlock::Create(TheContext, "endif", TheFunction);
  Builder.CreateCondBr(CondV, ThenBB, ElseBB);

  // Emit then block
  Builder.SetInsertPoint(ThenBB);
  this->left->right->codegen();
  Builder.CreateBr(MergeBB);
  
  // Emit else block
  Builder.SetInsertPoint(ElseBB);
  this->right->codegen();
  Builder.CreateBr(MergeBB);

  // Change Insert Point
  Builder.SetInsertPoint(MergeBB);
  return nullptr;
}

void ASTWhile::codegen() {
  Function *TheFunction = Builder.GetInsertBlock()->getParent();
  BasicBlock *CondBB = BasicBlock::Create(TheContext, "cond", TheFunction);
  BasicBlock *LoopBB = BasicBlock::Create(TheContext, "loop", TheFunction);
  BasicBlock *AfterBB = BasicBlock::Create(TheContext, "after", TheFunction);
  
  // Emit Condition block
  Builder.SetInsertPoint(CondBB);
  Value *CondV = this->left->codegen();
  Builder.CreateCondBr(CondV, LoopBB, AfterBB);
  // Emit Loop block
  Builder.SetInsertPoint(LoopBB);
  this->right->codegen();
  Builder.CreateBr(CondBB);
  Builder.SetInsertPoint(AfterBB);
  return nullptr;
}

void ASTRet::codegen() {
}

void ASTSeq::codegen() {
  this->left->codegen();
  this->right->codegen();
  return;
}

void ASTOp::codegen() {
  switch (this->op) {
    case PLUS: {
      Value *l = this->left->codegen();
      Value *r = this->right->codegen();
      return Builder.CreateAdd(l, r, "addtmp");
    }
      
    case MINUS: {
      Value *l = this->left->codegen();
      Value *r = this->right->codegen();
      return Builder.CreateSub(l, r, "subtmp");
    }
      
    case TIMES: {
      Value *l = this->left->codegen();
      Value *r = this->right->codegen();
      return Builder.CreateMul(l, r, "multmp");
    }
      
    case DIV: {
      Value *l = this->left->codegen();
      Value *r = this->right->codegen();
      return Builder.CreateSDiv(l, r, "divtmp");
    }

    case MOD: {
      Value *l = this->left->codegen();
      Value *r = this->right->codegen();
      return Builder.CreateSRem(l, r, "modtmp");
    }
      
    case EQ: {
      Value *l = this->left->codegen();
      Value *r = this->right->codegen();
      return Builder.CreateICmpEQ(l, r, "eqtmp");
    }

    case NE: {
      Value *l = this->left->codegen();
      Value *r = this->right->codegen();
      return Builder.CreateICmpNE(l, r, "neqtmp");
    }

    case LT: {
      Value *l = this->left->codegen();
      Value *r = this->right->codegen();
      return Builder.CreateICmpSLT(l, r, "lttmp");
    }

    case LE: {
      Value *l = this->left->codegen();
      Value *r = this->right->codegen();
      return Builder.CreateICmpSLE(l, r, "letmp");
    }

    case GT: {
      Value *l = this->left->codegen();
      Value *r = this->right->codegen();
      return Builder.CreateICmpSGT(l, r, "gttmp");
    }

    case GE: {
      Value *l = this->left->codegen();
      Value *r = this->right->codegen();
      return Builder.CreateICmpSGE(l, r, "getmp");
    }

    case AND: {
      Value *l = this->left->codegen();
      Value *r = this->right->codegen();
      return Builder.CreateAnd(l, r, "andtmp");
    }

    case OR: {
      Value *l = this->left->codegen();
      Value *r = this->right->codegen();
      return Builder.CreateOr(l, r, "ortmp");
    }

    case NOT: {
      Value *l = this->left->codegen();
      Value *r = this->right->codegen();
      return Builder.CreateNot(l, r, "nottmp");
    }
  }
}