#pragma once
#ifndef _Decl_HPP_
#define _Decl_HPP_

#include "Types.hpp"
#include <llvm/IR/Function.h>

BEGIN_NAMESPACE_SCI

llvm::Function* GetFunctionDecl(llvm::Function *orig, llvm::Module *module);
llvm::GlobalVariable* GetGlobalVariableDecl(llvm::GlobalVariable *orig, llvm::Module *module);

END_NAMESPACE_SCI

#endif // !_Decl_HPP_
