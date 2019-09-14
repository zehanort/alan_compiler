#ifndef __CODEGEN_HPP__
#define __CODEGEN_HPP__

#include <string>
#include <unordered_map>
#include <vector>
#include <typeinfo>

#include "ast.hpp"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Scalar.h>
#if defined(LLVM_VERSION_MAJOR) && LLVM_VERSION_MAJOR >= 4
#include <llvm/Transforms/Scalar/GVN.h>
#endif

extern const char* filename;

/* ---------------------------------------------------------------------
   ---------- global LLVM variables related to the LLVM suite ----------
   --------------------------------------------------------------------- */

static llvm::LLVMContext TheContext;
static llvm::IRBuilder<> Builder(TheContext);
static std::unique_ptr<llvm::Module> TheModule;
static std::unique_ptr<llvm::legacy::FunctionPassManager> TheFPM;

// useful LLVM types:
static llvm::Type * i8   = llvm::IntegerType::get(TheContext, 8);
static llvm::Type * i32  = llvm::IntegerType::get(TheContext, 32);
static llvm::Type * proc = llvm::Type::getVoidTy(TheContext);

// useful LLVM helper functions...
// ...for bytes
inline llvm::ConstantInt* c8(char c) {
  return llvm::ConstantInt::get(TheContext, llvm::APInt(8, c, true));
}
// ...for integers
inline llvm::ConstantInt* c32(int n) {
  return llvm::ConstantInt::get(TheContext, llvm::APInt(32, n, true));
}

// function that translates symbol table types to llvm types
llvm::Type *type_to_llvm(Type type, PassMode pm);

void codegen(ASTNode *t);


/* ---------------------------------------------------------------------
   ------------------------------- Scopelog ----------------------------
   ---------------------------------------------------------------------
   > variableTyoes:     types of all variables
   > variableAllocas:   addresses of the stack slots of all variables
   > functions:         all functions
 ----------------------------------------------------------------------- */

typedef struct {
    unordered_map<string, llvm::Type*> variableTypes;
    unordered_map<string, llvm::AllocaInst*> variableAllocas;
    unordered_map<string, llvm::Function*> functions;
} scopeLog;


/* ---------------------------------------------------------------------
   ------------- Logger: scope, variable and function info -------------
   --------------------------------------------------------------------- */

class Logger {
private:
    vector<scopeLog> scopeLogs;
public:
    Logger() {
        this->openScope();
    };
    
    // create and push new scopelog
    void openScope() {
        scopeLog sl;
        this->scopeLogs.push_back(sl);
    };
    
    // pop scopelog
    void closeScope() {
        this->scopeLogs.pop_back();
    };

    // add a variable to current scopelog
    void addVariable(string id, llvm::Type *type, llvm::AllocaInst *alloca) {
        this->scopeLogs.back().variableTypes[id] = type;
        this->scopeLogs.back().variableAllocas[id] = alloca;
    };

    // lookup variable by id and return type
    llvm::Type * getVarType(string id) {
        for (auto it = this->scopeLogs.rbegin(); it != this->scopeLogs.rend(); ++it) {
            if (!(it->variableTypes.find(id) == it->variableTypes.end()))
                return it->variableTypes[id];
        }
        // if sem was ok, this point should be unreachable
        internal("Variable \"%s\" not in scope.", id);
        return nullptr;
    };

    // lookup variable by id and return address of stack slot
    llvm::AllocaInst * getVarAlloca(string id) {
        for (auto it = this->scopeLogs.rbegin(); it != this->scopeLogs.rend(); ++it) {
            if (!(it->variableAllocas.find(id) == it->variableAllocas.end()))
                return it->variableAllocas[id];
        }
        // if sem was ok, this point should be unreachable
        internal("Variable \"%s\" not in scope.", id);
        return nullptr;
    };

    // lookup variable by id and return true if it is a pointer and false otherwise
    bool isPointer(string id) {
        for (auto it = this->scopeLogs.rbegin(); it != this->scopeLogs.rend(); ++it) {
            if (!(it->variableTypes.find(id) == it->variableTypes.end()))
                return it->variableTypes[id]->isPointerTy();
        }
        // if sem was ok, this point should be unreachable
        internal("Variable \"%s\" not in scope.", id);
        return false;
    };

    // add function to scopelog
    void addFunctionInScope(string fname, llvm::Function *F) {
    		this->scopeLogs.back().functions[fname] = F;
    };

    // lookup function by id
    llvm::Function * getFunctionInScope(string id) {
        for (auto it = this->scopeLogs.rbegin(); it != this->scopeLogs.rend(); ++it) {
            if (!(it->functions.find(id) == it->functions.end()))
                return it->functions[id];
        }
        // if sem was ok, this point should be unreachable
        internal("Function \"%s\" not in scope.", id);
        return nullptr;
    };

    // getter
    unordered_map<string, llvm::Type*> getCurrentScopeVarTypes() {
        return this->scopeLogs.back().variableTypes;
    };

    // getter
    unordered_map<string, llvm::AllocaInst*> getCurrentScopeVarAllocas() {
        return this->scopeLogs.back().variableAllocas;
    };
};

#endif
