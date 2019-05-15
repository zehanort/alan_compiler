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

/* Utilities for book-keeping while codegen-ing */
// class ScopeLogger {
// private:
//     unordered_map<string, llvm::Type*> varTypes;
//     unordered_map<string, llvm::AllocaInst*> varAddrs;
// public:
//     ScopeLogger() {};
//     ~ScopeLogger() {};
//     void addVariable(string id, llvm::Type* type, llvm::AllocaInst* alloca) {
//         varTypes[id] = type;
//         varAddrs[id] = alloca;
//     }
// }
typedef struct {
    unordered_map<string, llvm::Type*> varTypes;
    unordered_map<string, llvm::AllocaInst*> varAddrs;
    unordered_map<string, llvm::AllocaInst*> ptrValues;
} scopeLog;

class Logger {
private:
    vector<scopeLog> scopeLogs;
public:
    Logger() {
        this->openScope();
    };
    ~Logger() {};
    
    void openScope() {
        scopeLog sl;
        scopeLogs.push_back(sl);
    };
    
    void closeScope() {
        scopeLogs.pop_back();
    };

    void addVariable(string id, llvm::Type *type, llvm::AllocaInst *addr) {
        scopeLogs.back().varTypes[id] = type;
        scopeLogs.back().varAddrs[id] = addr;
    };

    void addPointer(string id, llvm::Type *type, llvm::AllocaInst *addr, llvm::AllocaInst *points_to) {
        scopeLogs.back().varTypes[id] = type;
        scopeLogs.back().varAddrs[id] = addr;
        scopeLogs.back().ptrValues[id] = points_to;
    }

    llvm::Type * getVarType(string id) {
        for (auto it = this->scopeLogs.rbegin(); it != this->scopeLogs.rend(); ++i) {
            if (!(it->varTypes.find(id) == it->varTypes.end()))
                return it->varTypes[id];
        }
        /* if sem was ok, this point should be unreachable */
        return false;
    };

    llvm::AllocaInst * getVarAddr(string id) {
        for (auto it = this->scopeLogs.rbegin(); it != this->scopeLogs.rend(); ++i) {
            if (!(it->varAddrs.find(id) == it->varAddrs.end()))
                return it->varAddrs[id];
        }
        /* if sem was ok, this point should be unreachable */
        return false;
    };

    llvm::AllocaInst * getPtrValue(string id) {
        for (auto it = this->scopeLogs.rbegin(); it != this->scopeLogs.rend(); ++i) {
            if (!(it->ptrValues.find(id) == it->ptrValues.end()))
                return it->ptrValues[id];
        }
        /* if sem was ok, this point should be unreachable */
        return false;
    }

    bool isPointer(string id) {
        for (auto it = this->scopeLogs.rbegin(); it != this->scopeLogs.rend(); ++i) {
            if (!(it->varTypes.find(id) == it->varTypes.end()))
                return it->varTypes[id]->isPointerTy();
        }
        /* if sem was ok, this point should be unreachable */
        return false;
    };
};

#endif
