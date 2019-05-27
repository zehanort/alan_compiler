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
      llvmtype = llvm::ArrayType::get(type_to_llvm(type->refType), type->size);
      break;
    case TYPE_IARRAY:
      llvmtype = type_to_llvm(type->refType);
      break;
    default: internal("cannot cast semantic type to LLVM type");
  }
  if (pm == PASS_BY_REFERENCE) return llvmtype->getPointerTo();
  return llvmtype;
}

Logger logger;

llvm::Value *calcAddr (ASTNode *var, string function) {
	llvm::Value *addr;
	llvm::Type *t;
	/* dereference if necessary */
	if (logger.isPointer(var->id)) {
		addr = Builder.CreateLoad(logger.getVarAlloca(var->id));
		t = logger.getVarType(var->id)->getPointerElementType();
	}
	else {
		addr = logger.getVarAlloca(var->id);
		t = logger.getVarType(var->id);
	}
  
  if (var->type->refType != nullptr) {
    /* id is an array */
    if (t->isArrayTy()) {
      cerr << "*** " << function << " *** : " << var->id << " [] is array" << endl;
      addr = Builder.CreateGEP(addr, vector<llvm::Value *>{c32(0), c32(0)});
    }
    
    /* id is an iarray */
    else {
      cerr << "*** " << function << " *** : " << var->id << " [] is iarray" << endl;
      addr = Builder.CreateGEP(addr, c32(0));
    }
  }
  
  /* id is a variable */
  else {
    cerr << "*** " << function << " *** : " << var->id << " [] is var" << endl;
    
    /* variable is element of array (a[2]) */
    if (t->isArrayTy()) {
      cerr << "*** " << function << " *** : " << var->id << " [][] is var, array element" << endl;
      auto *index = var->left->codegen();
      addr = Builder.CreateGEP(addr, vector<llvm::Value *>{c32(0), index});
    }
    
    else {
	    /* variable is element of iarray (a[2]) */
    	if (var->left != nullptr) {
	      cerr << "*** " << function << " *** : " << var->id << " [][] is var, iarray element" << endl;
  	    auto *index = var->left->codegen();
    	  addr = Builder.CreateGEP(addr, index);
    	}
    
	    /* variable is as simple as it gets */
	    else {
	      cerr << "*** " << function << " *** : " << var->id << " [][] is var, normal" << endl;
	    }
	  }
  }
  return addr;
}

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
  logger.addFunctionInScope("main", MainF);
  llvm::BasicBlock *MainBB =
    llvm::BasicBlock::Create(TheContext, "entry", MainF);

  /* step 4: create LLVM IR of input program */
  t->codegen();

  /* step 5: create a call to the main function */
  llvm::Function *F = logger.getFunctionInScope(t->left->id);
  Builder.SetInsertPoint(MainBB);
  Builder.CreateCall(F, vector<llvm::Value*>{});
  Builder.CreateRet(c32(0));
  logger.closeScope();

  /* emit LLVM IR to stdout */
  TheModule->print(llvm::outs(), nullptr);
  return;
}

llvm::Value * ASTId::codegen() {
	return Builder.CreateLoad(calcAddr(this, "ID"));
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
 	cerr << "*** FD *** : " << this->left->id << endl;
 	auto *params = this->left->left;
  auto *locdefs = this->left->right;
  string Fname = this->left->id;
  llvm::Type *retType = type_to_llvm(this->left->type);
  vector<string> parameterNames;
  vector<llvm::Type *> parameterTypes;
  vector<string> outerScopeVarsNames;
  unordered_map<string, llvm::Type*> outerScopeVarsTypes;
  unordered_map<string, llvm::AllocaInst*> outerScopeVarsAllocas;

  /* step 1a: log param types and names */
  while (params != nullptr) {
    parameterNames.push_back(params->left->id);
    parameterTypes.push_back(type_to_llvm(params->left->type, params->left->pm));
    params = params->right;
  }

  /* step 1b: add references to outer scope variables as parameters */
  outerScopeVarsTypes = logger.getCurrentScopeVarTypes();
  outerScopeVarsAllocas = logger.getCurrentScopeVarAllocas();
  for (auto var: outerScopeVarsTypes) outerScopeVarsNames.push_back(var.first);

  llvm::Type *varType;
  for (string var : outerScopeVarsNames) {
    /* skip shadowed outer scope variables */
    if (find(parameterNames.begin(), parameterNames.end(), var) != parameterNames.end()) continue;
    varType = outerScopeVarsTypes[var];
    parameterNames.push_back(var);
    /* if var is pointer, leave it as it is */
    if (varType->isPointerTy()) parameterTypes.push_back(varType);
    /* else, we need to pass a reference to it as parameter */
    else parameterTypes.push_back(varType->getPointerTo());
  }

  llvm::FunctionType *FT =
    llvm::FunctionType::get(retType, parameterTypes, false);

  llvm::Function *F =
    llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Fname, TheModule.get());

  logger.addFunctionInScope(Fname, F);
  logger.openScope();

  /* step 2: set all param names */
  unsigned Idx = 0;
  for (auto &arg : F->args()) arg.setName(parameterNames[Idx++]);

  llvm::BasicBlock *BB = llvm::BasicBlock::Create(TheContext, "entry", F);
  Builder.SetInsertPoint(BB);

  /* step 3: create allocas for params */
  for (auto &arg : F->args()) {
    auto *alloca = Builder.CreateAlloca(arg.getType(), nullptr, arg.getName());
    Builder.CreateStore(&arg, alloca);
    logger.addVariable(arg.getName(), arg.getType(), alloca);
  }

  /* step 4: codegen local defs */
  while (locdefs != nullptr) {
    locdefs->left->codegen();
    locdefs = locdefs->right;
	  Builder.SetInsertPoint(BB);
  }

  /* step 5: codegen body */
  if (this->right != nullptr)
  	this->right->codegen();

  /* step 6: check if return was omitted */
  if (!logger.returnAddedInScopeFunction(Fname)) {
  	auto *retType = F->getReturnType();
    if (retType->isIntegerTy(32)) Builder.CreateRet(c32(0));
    else if (retType->isIntegerTy(8)) Builder.CreateRet(c8(0));
    else Builder.CreateRetVoid();
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
	auto *addr = calcAddr(this->left, "AS");
	return Builder.CreateStore(expr, addr);
}

llvm::Value * ASTFcall::codegen() {
	cerr << "*** FC *** : " << this->id << endl;
	llvm::Function *F = logger.getFunctionInScope(this->id);
	vector<llvm::Value*> argv;
	auto *ASTargs = this->left;

	for (auto &Arg : F->args()) {

    llvm::Value *arg;
    /* function with no params, only outer scope ones */
    if (ASTargs == nullptr) {
      argv.push_back(logger.getVarAlloca(Arg.getName().str()));
      continue;      
    }
    
    auto *ASTarg = ASTargs->left;

    /* check if done with real params (outer scope vars left) */
    if (ASTarg == nullptr) {
      argv.push_back(logger.getVarAlloca(Arg.getName().str()));
      continue;
    }

    /* If expected argument is by value */
 		if (!Arg.getType()->isPointerTy()) {
	    arg = ASTarg->codegen();
 		}
 		else {
 			/* string literal */
      if (ASTarg->op == STRING) {
        arg = ASTarg->codegen();
      }
      /* variable */
      else {
      	arg = calcAddr(ASTarg, "ID");
      }
 		}
    argv.push_back(arg);
    if (ASTargs->left != nullptr) ASTargs = ASTargs->right;
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
  logger.openScope();
  this->right->codegen();
  if (!logger.wildRetExists()) Builder.CreateBr(MergeBB);
  logger.closeScope();

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
  logger.openScope();
  this->left->right->codegen();
  if (!logger.wildRetExists()) Builder.CreateBr(MergeBB);
  logger.closeScope();
  
  // Emit else block
  Builder.SetInsertPoint(ElseBB);
  logger.openScope();
  this->right->codegen();
  if (!logger.wildRetExists()) Builder.CreateBr(MergeBB);
  logger.closeScope();

  // Change Insert Point
  Builder.SetInsertPoint(MergeBB);
  return nullptr;
}

llvm::Value * ASTWhile::codegen() {
  llvm::Function *TheFunction = Builder.GetInsertBlock()->getParent();
  llvm::BasicBlock *CondBB = llvm::BasicBlock::Create(TheContext, "cond", TheFunction);
  llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(TheContext, "loop", TheFunction);
  llvm::BasicBlock *AfterBB = llvm::BasicBlock::Create(TheContext, "after", TheFunction);
  Builder.CreateBr(CondBB);
  
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
  logger.addReturn(Builder.GetInsertBlock()->getParent()->getName().str());
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
    default:		internal("unknown operation to codegen");
  }
  return nullptr;
}

/* function that codegens ALAN stdlib functions */
void createstdlib() {
    llvm::FunctionType *FT;
    vector<llvm::Function*> libFunctions;

    /*** write functions ***/
    FT = llvm::FunctionType::get(proc, vector<llvm::Type *>{i32}, false);
    libFunctions.push_back(llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "writeInteger", TheModule.get()));

    FT = llvm::FunctionType::get(proc, vector<llvm::Type *>{i8}, false);
    libFunctions.push_back(llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "writeByte", TheModule.get()));

    FT = llvm::FunctionType::get(proc, vector<llvm::Type *>{i8}, false);
    libFunctions.push_back(llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "writeChar", TheModule.get()));

    FT = llvm::FunctionType::get(proc, vector<llvm::Type *>{i8->getPointerTo()}, false);
    libFunctions.push_back(llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "writeString", TheModule.get()));

    /*** read functions ***/
    FT = llvm::FunctionType::get(i32, vector<llvm::Type *>{}, false);
    libFunctions.push_back(llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "readInteger", TheModule.get()));
    
    FT = llvm::FunctionType::get(i8, vector<llvm::Type *>{}, false);
    libFunctions.push_back(llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "readByte", TheModule.get()));
    
    FT = llvm::FunctionType::get(i8, vector<llvm::Type *>{}, false);
    libFunctions.push_back(llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "readChar", TheModule.get()));

    FT = llvm::FunctionType::get(proc, vector<llvm::Type *>{i32, i8->getPointerTo()}, false);
 		libFunctions.push_back(llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "readString", TheModule.get()));

    /*** type casting functions ***/
    FT = llvm::FunctionType::get(i32, vector<llvm::Type *>{i8}, false);
    libFunctions.push_back(llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "extend", TheModule.get()));

    FT = llvm::FunctionType::get(i8, vector<llvm::Type *>{i32}, false);
    libFunctions.push_back(llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "shrink", TheModule.get()));

    /*** string manipulation functions ***/
    FT = llvm::FunctionType::get(i32, vector<llvm::Type *>{i8->getPointerTo()}, false);
    libFunctions.push_back(llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "strlen", TheModule.get()));

    FT = llvm::FunctionType::get(i32, vector<llvm::Type *>{i8->getPointerTo(), i8->getPointerTo()}, false);
    libFunctions.push_back(llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "strcmp", TheModule.get()));

    FT = llvm::FunctionType::get(proc, vector<llvm::Type *>{i8->getPointerTo(), i8->getPointerTo()}, false);
    libFunctions.push_back(llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "strcpy", TheModule.get()));

    FT = llvm::FunctionType::get(proc, vector<llvm::Type *>{i8->getPointerTo(), i8->getPointerTo()}, false);
    libFunctions.push_back(llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "strcat", TheModule.get()));

    for (auto F: libFunctions) logger.addFunctionInScope(F->getName().str(), F);
}
