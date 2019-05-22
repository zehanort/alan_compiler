#include "codegen.hpp"

llvm::Type * type_to_llvm(Type type, PassMode pm = PASS_BY_VALUE) {
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

Logger logger;

/*******************************************************
 * THE CODEGEN FUNCTION                                *
 * creates IR code (after a successful semantic check) *
 *******************************************************/

void createstdlib();

/*** this is the main codegen function (called by main()) ***/
void codegen(ASTNode *t) {
  /* step 1: initiate the module */
  TheModule = llvm::make_unique<llvm::Module>(filename, TheContext);
  logger.openScope();

  /* step 2: create alan stdlib functions */
  createstdlib();

  /* step 3: create the main function of the output program */
  llvm::FunctionType *MainType =
    llvm::FunctionType::get(i32, vector<llvm::Type*>{}, false);
  llvm::Function *MainF =
    llvm::Function::Create(MainType, llvm::Function::ExternalLinkage, "main", TheModule.get());
  llvm::BasicBlock *MainBB =
    llvm::BasicBlock::Create(TheContext, "entry", MainF);

  /* step 4: create LLVM IR of input program */
  t->codegen();

  /* step 5: create a call to the main function */
  llvm::Function *F = TheModule->getFunction(t->left->id);
  Builder.SetInsertPoint(MainBB);
  Builder.CreateCall(F, vector<llvm::Value*>{});
  Builder.CreateRet(c32(0));
  logger.closeScope();

  /* emit LLVM IR to stdout */
  TheModule->print(llvm::outs(), nullptr);
  return;
}

llvm::Value * ASTId::codegen() {
  /* id is an array */
  if (this->type->refType != nullptr) {
    auto *index = this->left->left->codegen();
    /* var is array by reference */
    if (logger.isPointer(this->id)) {
      auto *arr = Builder.CreateLoad(logger.getVarAlloca(this->id));
      auto *addr = Builder.CreateGEP(arr, index);
      return Builder.CreateLoad(addr);
    }
    /* var is a simple array */
    else {
      auto *addr = Builder.CreateGEP(logger.getVarAlloca(this->id), index);
      // auto *addr = Builder.CreateGEP(logger.getVarAlloca(this->id), vector<llvm::Value *>{c32(0), index});!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      return Builder.CreateLoad(addr);
    }
  }
  /* id is a simple variable */
  else {
    /* variable is by reference */
    if (logger.isPointer(this->id)) {
      auto *addr = Builder.CreateLoad(logger.getVarAlloca(this->id));
      return Builder.CreateLoad(addr);
    }
    /* variable is not by reference */
    else Builder.CreateLoad(logger.getVarAlloca(this->id));
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
  return Builder.CreateGlobalStringPtr(this->id.c_str());
}

llvm::Value * ASTVdef::codegen() {
  auto *vtype = type_to_llvm(this->type);
  auto *valloca = Builder.CreateAlloca(vtype, nullptr, this->id);
  /* log variable to be able to retrieve it later */
  logger.addVariable(this->id, vtype, valloca);
  return nullptr;
}

llvm::Value * ASTFdef::codegen() {
  logger.openScope();

  auto *params = this->left->left;
  auto *locdefs = this->left->right;
  string funcName = this->left->id;
  llvm::Type *retType = type_to_llvm(this->left->type);
  vector<string> parameterNames;
  vector<llvm::Type *> parameterTypes;

  /* step 1: log param types and names */
  while (params != nullptr) {
    parameterNames.push_back(params->left->id);
    parameterTypes.push_back(type_to_llvm(params->left->type, params->left->pm));
    params = params->right;
  }

  llvm::FunctionType *FT =
    llvm::FunctionType::get(retType, parameterTypes, false);

  llvm::Function *F =
    llvm::Function::Create(FT, llvm::Function::ExternalLinkage, funcName, TheModule.get());

  /* step 2: set all param names */
  unsigned Idx = 0;
  for (auto &arg : F->args()) arg.setName(parameterNames[Idx++]);

  /* step 3: create function instertion block */
  llvm::BasicBlock *BB = llvm::BasicBlock::Create(TheContext, "entry", F);
  Builder.SetInsertPoint(BB);

  /* step 4: create allocas for params */
  for (auto &arg : F->args()) {
  	auto *alloca = Builder.CreateAlloca(arg.getType(), nullptr, arg.getName());
    Builder.CreateStore(&arg, alloca);
    logger.addVariable(arg.getName(), arg.getType(), alloca);
  }

  /* step 5: codegen local defs and body */
  while (locdefs != nullptr) {
    locdefs->left->codegen();
    locdefs = locdefs->right;
  }

  this->right->codegen();

  /* step 6: check if return was omitted */
  /* --- case 1: function is proc, and return was omitted */
  if (F->getReturnType()->isVoidTy()) Builder.CreateRetVoid();
  /* --- case 2: return omitted, make it return 0 of the appropriate type */
  else if (!logger.returnAddedInScopeFunction()) {
    if (F->getReturnType()->isIntegerTy(32)) Builder.CreateRet(c32(0));
    else Builder.CreateRet(c8(0));
  }

  /* step 7: verify, done */
  llvm::verifyFunction(*F);
  logger.closeScope();
  return nullptr;
}

llvm::Value * ASTFdecl::codegen() { return nullptr; }

llvm::Value * ASTPar::codegen() { return nullptr; }

llvm::Value * ASTAssign::codegen() {
  auto *expr = this->right->codegen();
  llvm::Value *varAddr;
  /* var is array */
  if (this->left->type->refType != nullptr) {
    auto *index = this->left->left->codegen();
    /* var is array by reference */
    if (logger.isPointer(this->left->id))
    	varAddr = Builder.CreateGEP(Builder.CreateLoad(logger.getVarAlloca(this->left->id)), index);
    /* var is a simple array */
    else
    	// varAddr = Builder.CreateGEP(logger.getVarAlloca(this->left->id), vector<llvm::Value *>{c32(0), index});!!!!!!!!!!!!!!!!!!!!!!!!!!!
    	varAddr = Builder.CreateGEP(logger.getVarAlloca(this->left->id), index);
  }
  /* var is a simple variable */
  else {
    if (logger.isPointer(this->left->id))
      varAddr = Builder.CreateLoad(logger.getVarAlloca(this->left->id));
    /* var is not a pointer */
    else
    	varAddr = logger.getVarAlloca(this->left->id);
  }
  return Builder.CreateStore(expr, varAddr);
}

llvm::Value * ASTFcall::codegen() {
	auto *F = TheModule->getFunction(this->id);
	vector<llvm::Value*> argv;
	auto *ASTargs = this->left;

	for (auto &Arg : F->args()) {
    llvm::Value *arg;
    auto *ASTarg = ASTargs->left;
    /* If expected argument is by reference */
    if (Arg.getType()->isPointerTy()) {
      /* string literal */
      // if (typeid(ASTarg) == typeid(new ASTString("")))
      if (ASTarg->op == STRING)
        arg = ASTarg->codegen();
      /* variable */
      else {
        if (ASTarg->type->refType != nullptr) {
          auto index = ASTarg->left->codegen();
          if (logger.isPointer(ASTarg->id))
            arg = Builder.CreateGEP(Builder.CreateLoad(logger.getVarAlloca(ASTarg->id)), index);
          else
            arg = Builder.CreateGEP(logger.getVarAlloca(ASTarg->id), index);
        }
        else {
          if (logger.isPointer(ASTarg->id))
            arg = Builder.CreateLoad(logger.getVarAlloca(ASTarg->id));
          else {
            if (logger.getVarType(ASTarg->id)->isArrayTy())
              arg = Builder.CreateGEP(logger.getVarAlloca(ASTarg->id), c32(0));
            else
              arg = logger.getVarAlloca(ASTarg->id);
          }
        }
      }
    }
    /* If expected argument is by value */
    else
      arg = ASTarg->codegen();
    argv.push_back(arg);
    ASTargs = ASTargs->right;
	}

	return Builder.CreateCall(F, argv);
}

llvm::Value * ASTFcall_stmt::codegen() {
  return this->left->codegen();
}

llvm::Value * ASTIf::codegen() {
  llvm::Value *CondV = this->left->codegen();
  llvm::Function *TheFunction = Builder.GetInsertBlock()->getParent();
  llvm::BasicBlock *ThenBB  = llvm::BasicBlock::Create(TheContext, "then", TheFunction);
  llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(TheContext, "endif", TheFunction);
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
  llvm::Function *TheFunction = Builder.GetInsertBlock()->getParent();
  llvm::BasicBlock *ThenBB  = llvm::BasicBlock::Create(TheContext, "then", TheFunction);
  llvm::BasicBlock *ElseBB  = llvm::BasicBlock::Create(TheContext, "else", TheFunction);
  llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(TheContext, "endif", TheFunction);
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
  llvm::Function *TheFunction = Builder.GetInsertBlock()->getParent();
  llvm::BasicBlock *CondBB = llvm::BasicBlock::Create(TheContext, "cond", TheFunction);
  llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(TheContext, "loop", TheFunction);
  llvm::BasicBlock *AfterBB = llvm::BasicBlock::Create(TheContext, "after", TheFunction);
  
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
  logger.addReturn();
  if (this->left == nullptr) return Builder.CreateRetVoid();
  return Builder.CreateRet(this->left->codegen());
}

llvm::Value * ASTSeq::codegen() {
  this->left->codegen();
  if (this->right)
    this->right->codegen();
  return nullptr;
}

llvm::Value * ASTOp::codegen() {
  llvm::Value *l = this->left->codegen();
  llvm::Value *r = nullptr;
  if (this->op != NOT) r = this->right->codegen();
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
    case NOT:   return Builder.CreateNot(l, "nottmp");
  }
  return nullptr;
}

/* function that codegens ALAN stdlib functions */
void createstdlib() {
    llvm::FunctionType *FT;

    /*** write functions ***/
    FT = llvm::FunctionType::get(proc, vector<llvm::Type *>{i32}, false);
    llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "writeInteger", TheModule.get());
    
    FT = llvm::FunctionType::get(proc, vector<llvm::Type *>{i8}, false);
    llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "writeByte", TheModule.get());
    
    FT = llvm::FunctionType::get(proc, vector<llvm::Type *>{i8}, false);
    llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "writeChar", TheModule.get());

    FT = llvm::FunctionType::get(proc, vector<llvm::Type *>{i8->getPointerTo()}, false);
    llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "writeString", TheModule.get());

    /*** read functions ***/
    FT = llvm::FunctionType::get(i32, vector<llvm::Type *>{}, false);
    llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "readInteger", TheModule.get());
    
    FT = llvm::FunctionType::get(i8, vector<llvm::Type *>{}, false);
    llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "readByte", TheModule.get());
    
    FT = llvm::FunctionType::get(i8, vector<llvm::Type *>{}, false);
    llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "readChar", TheModule.get());

    FT = llvm::FunctionType::get(proc, vector<llvm::Type *>{i32, i8->getPointerTo()}, false);
    llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "readString", TheModule.get());

    /*** type casting functions ***/
    FT = llvm::FunctionType::get(i32, vector<llvm::Type *>{i8}, false);
    llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "extend", TheModule.get());

    FT = llvm::FunctionType::get(i8, vector<llvm::Type *>{i32}, false);
    llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "shrink", TheModule.get());

    /*** string manipulation functions ***/
    FT = llvm::FunctionType::get(i32, vector<llvm::Type *>{i8->getPointerTo()}, false);
    llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "strlen", TheModule.get());

    FT = llvm::FunctionType::get(i32, vector<llvm::Type *>{i8->getPointerTo(), i8->getPointerTo()}, false);
    llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "strcmp", TheModule.get());

    FT = llvm::FunctionType::get(proc, vector<llvm::Type *>{i8->getPointerTo(), i8->getPointerTo()}, false);
    llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "strcpy", TheModule.get());

    FT = llvm::FunctionType::get(proc, vector<llvm::Type *>{i8->getPointerTo(), i8->getPointerTo()}, false);
    llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "strcat", TheModule.get());
}
