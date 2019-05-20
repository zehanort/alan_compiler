#ifndef __CODEGEN_HPP__
#define __CODEGEN_HPP__

#include <string>
#include <unordered_map>
#include <vector>

#include "ast.hpp"

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

/******************************
 * Global LLVM variables      *
 * related to the LLVM suite. *
 ******************************/
static llvm::LLVMContext TheContext;
static llvm::IRBuilder<> Builder(TheContext);
static std::unique_ptr<Module> TheModule;
static std::unique_ptr<legacy::FunctionPassManager> TheFPM;

// Global LLVM variables related to the generated code.
static llvm::Function *TheWriteInteger;
static llvm::Function *TheWriteString;

// Useful LLVM types.
static llvm::Type * i8   = llvm::IntegerType::get(TheContext, 8);
static llvm::Type * i32  = llvm::IntegerType::get(TheContext, 32);
static llvm::Type * proc = llvm::Type::getVoidTy(TheContext);

// Useful LLVM helper functions...
// ...for bytes
inline llvm::ConstantInt* c8(char c) {
  return llvm::ConstantInt::get(TheContext, APInt(8, c, true));
}
// ...for integers
inline llvm::ConstantInt* c32(int n) {
  return llvm::ConstantInt::get(TheContext, APInt(32, n, true));
}

llvm::Type *type_to_llvm(Type type, PassMode pm);

typedef struct {
    unordered_map<string, llvm::Type*> variableTypes;
    unordered_map<string, llvm::AllocaInst*> variableAllocas;
    bool returnAdded;
} scopeLog;

class Logger {
private:
    vector<scopeLog> scopeLogs;
public:
    Logger() {
        this->openScope();
    };
    
    void openScope() {
        scopeLog sl;
        sl.returnAdded = false;
        this->scopeLogs.push_back(sl);
    };
    
    void closeScope() {
        this->scopeLogs.pop_back();
    };

    void addVariable(string id, llvm::Type *type, llvm::AllocaInst *alloca) {
        this->scopeLogs.back()->variableTypes[id] = type;
        this->scopeLogs.back()->variableAllocas[id] = alloca;
    };

    llvm::Type * getVarType(string id) {
        for (auto it = this->scopeLogs.rbegin(); it != this->scopeLogs.rend(); ++i) {
            if (!(it->variableTypes.find(id) == it->variableTypes.end()))
                return it->variableTypes[id];
        }
        /* if sem was ok, this point should be unreachable */
        fatal("Variable \"%s\" not in scope.", id);
        return false;
    };

    llvm::AllocaInst * getVarAlloca(string id) {
        for (auto it = this->scopeLogs.rbegin(); it != this->scopeLogs.rend(); ++it) {
            if (!(it->variableAllocas.find(id) == it->variableAllocas.end()))
                return it->variableAllocas[id];
        }
        /* if sem was ok, this point should be unreachable */
        fatal("Variable \"%s\" not in scope.", id);
        return false;
    };

    bool isPointer(string id) {
        for (auto it = this->scopeLogs.rbegin(); it != this->scopeLogs.rend(); ++it) {
            if (!(it->variableTypes.find(id) == it->variableTypes.end()))
                return it->variableTypes[id]->isPointerTy();
        }
        /* if sem was ok, this point should be unreachable */
        fatal("Variable \"%s\" not in scope.", id);
        return false;
    };

    void addReturn() {
        this->scopeLogs.back()->returnAdded = true;
    };

    bool returnAddedInScopeFunction() {
        return this->scopeLogs.back()->returnAdded;
    };
};

#endif
