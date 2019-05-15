#include "codegen.hpp"

llvm::Type * type_to_llvm(Type type, PassMode pm) {
  llvm::Type *llvmtype;
  switch (type->kind) {
    case TYPE_VOID:
      llvmtype = proc;
      break;
    case TYPE_BOOLEAN:
    case TYPE_INTEGER:
      llvmtype = i32;
      break;
    case TYPE_CHAR:
      llvmtype = i8;
      break;
    case TYPE_ARRAY:
      llvmtype = llvm::ArrayType::get(type_to_llvm(type->refType), sizeOfType(type));
      break;
    case TYPE_IARRAY:
      llvmtype = type_to_llvm(type->refType);
  }
  if (pm == PASS_BY_REFERENCE) return llvmtype->getPointerTo();
  return llvmtype;
}

/*******************************************************
 * THE CODEGEN FUNCTION                                *
 * creates IR code (after a successful semantic check) *
 *******************************************************/

Logger logger;

llvm::Value * ASTId::codegen() {
  /* id is an array */
  if (var->type = TYPE_ARRAY) {
    auto *index = this->left->left->codegen();
    llvm::Value *retval;
    /* var is array by reference */
    if (logger.isPointer(var->id)) {
      auto *tmp = Builder.CreateLoad(logger.getPtrValue(var->id));
      auto *addr = Builder.CreateGEP(tmp, index);
      return Builder.CreateLoad(addr);
    }
    /* var is a simple array */
    else {
      auto *addr = Builder.CreateGEP(
        logger.getVarAddr(this->id),
        std::vector<llvm::Value *>{c32(0), index}
      );
      return Builder.CreateLoad(addr);
    }
  }
  /* id is a simple variable */
  else {
    /* variable is by reference */
    if (logger.isPointer(this->id)) {
      auto *addr = Builder.CreateLoad(logger.getPtrValue(this->id));
      return Builder.CreateLoad(addr);
    }
    /* variable is not by reference */
    else Builder.CreateLoad(logger.getVarAddr(this->id));
  }
  /* should be unreachable */
  return nullptr;
}

llvm::Value * ASTInt::codegen() {
  return c32(this->num);
}

llvm::Value * ASTChar::codegen() {
  return c8(this->id[0]);
}

llvm::Value * ASTString::codegen() {
  return Builder.CreateGlobalStringPtr(this->id);
}

llvm::Value * ASTVdef::codegen() {
  auto *vtype = type_to_llvm(this->type);
  auto *valloca = Builder.CreateAlloca(vtype, nullptr, this->id);
  /* log variable to be able to retrieve it later */
  logger.addVariable(this->id, vtype, valloca);
  return nullptr;
}

llvm::Value * ASTFdef::codegen() {
}

llvm::Value * ASTFdecl::codegen() {
}

llvm::Value * ASTPar::codegen() {
  logger.addParameter(this->id, type_to_llvm(this->type), this->pm);
  return nullptr;
}

llvm::Value * ASTAssign::codegen() {
  auto *expr = this->right->codegen();
  /* var is array */
  if (var->type = TYPE_ARRAY) {
    auto *index = this->left->left->codegen();
    llvm::Value *retval;
    /* var is array by reference */
    if (logger.isPointer(var->id)) {
      retval = Builder.CreateLoad(logger.getPtrValue(var->id));
      retval = Builder.CreateGEP(retval, index);
    }
    /* var is a simple array */
    else {
      retval = Builder.CreateGEP(
        logger.getVarAddr(var->id),
        vector<llvm::Value *>{c32(0), index}
      );
    }
    return Builder.CreateStore(expr, retval);
  }
  /* var is a simple variable */
  else {
    auto *var = this->left->codegen();
    /* var is a pointer */
    if (logger.isPointer(var->id)) {
      auto *varAddr = Builder.CreateLoad(logger.getPtrValue(var->id));
      return Builder.CreateStore(expr, varAddr);
    }
    /* var is not a pointer */
    else return Builder.CreateStore(expr, logger.getVarAddr(var->id));
  }
  /* should be unreachable */
  return nullptr;
}

llvm::Value * ASTFcall::codegen() {
}

llvm::Value * ASTFcall_stmt::codegen() {
  return this->left->codegen();
}

llvm::Value * ASTIf::codegen() {
  llvm::Value *CondV = this->left->codegen();
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

llvm::Value * ASTIfelse::codegen() {
  llvm::Value *CondV = this->left->left->codegen();
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

llvm::Value * ASTWhile::codegen() {
  Function *TheFunction = Builder.GetInsertBlock()->getParent();
  BasicBlock *CondBB = BasicBlock::Create(TheContext, "cond", TheFunction);
  BasicBlock *LoopBB = BasicBlock::Create(TheContext, "loop", TheFunction);
  BasicBlock *AfterBB = BasicBlock::Create(TheContext, "after", TheFunction);
  
  // Emit Condition block
  Builder.SetInsertPoint(CondBB);
  llvm::Value *CondV = this->left->codegen();
  Builder.CreateCondBr(CondV, LoopBB, AfterBB);
  // Emit Loop block
  Builder.SetInsertPoint(LoopBB);
  this->right->codegen();
  Builder.CreateBr(CondBB);
  Builder.SetInsertPoint(AfterBB);
  return nullptr;
}

llvm::Value * ASTRet::codegen() {
  if (this->left == nullptr) return Builder.CreateRetVoid();
  return Builder.CreateRet(this->left->codegen());
}

llvm::Value * ASTSeq::codegen() {
  this->left->codegen();
  this->right->codegen();
  return nullptr;
}

llvm::Value * ASTOp::codegen() {
  llvm::Value *l = this->left->codegen();
  llvm::Value *r = this->right->codegen();
  switch (this->op) {
    case PLUS:  return Builder.CreateAdd(l, r, "addtmp");
    case MINUS: return Builder.CreateSub(l, r, "subtmp");
    case TIMES: return Builder.CreateMul(l, r, "multmp");
    case DIV:   return Builder.CreateSDiv(l, r, "divtmp");
    case MOD:   return Builder.CreateSRem(l, r, "modtmp");
    case EQ:    return Builder.CreateICmpEQ(l, r, "eqtmp");
    case NE:    return Builder.CreateICmpNE(l, r, "neqtmp");
    case LT:    return Builder.CreateICmpSLT(l, r, "lttmp");
    case LE:    return Builder.CreateICmpSLE(l, r, "letmp");
    case GT:    return Builder.CreateICmpSGT(l, r, "gttmp");
    case GE:    return Builder.CreateICmpSGE(l, r, "getmp");
    case AND:   return Builder.CreateAnd(l, r, "andtmp");
    case OR:    return Builder.CreateOr(l, r, "ortmp");
    case NOT:   return Builder.CreateNot(l, r, "nottmp");
  }
  return nullptr;
}
