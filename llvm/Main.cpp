#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include "Script.hpp"

extern "C" {
#include "../SCI/src/ErrMsg.c"
#include "../SCI/src/Log.c"
#include "../SCI/src/List.c"
#include "../SCI/src/FileIO.c"
#include "../SCI/src/Resource.c"
#include "../SCI/src/ResName.c"
#include "../SCI/src/VolLoad.c"
#include "../SCI/src/Crypt.c"
#include "../SCI/src/FarData.c"
} // extern "C"

llvm::IntegerType *g_sizeTy = NULL;

int main(int argc, char *argv[])
{
    g_isExternal = true;
    InitResource(NULL);

    llvm::LLVMContext &ctx = llvm::getGlobalContext();

    g_sizeTy = llvm::Type::getInt32Ty(ctx);

    std::unique_ptr<llvm::Module> m(new llvm::Module("Script000", ctx));
    llvm::StructType* s = llvm::StructType::create(ctx, "abc");

    std::vector<llvm::Type *> args;
    args.push_back(llvm::Type::getInt32Ty(ctx));
    args.push_back(llvm::Type::getInt8PtrTy(ctx));
    s->setBody(args);
    
    args.push_back(llvm::Type::getInt8PtrTy(ctx));
    // accepts a char*, is vararg, and returns int
    llvm::FunctionType *printfType =
        llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx), args, true);
    llvm::Constant *printfFunc =
        m->getOrInsertFunction("printf", printfType);

    args.clear();
    args.push_back(s);
    llvm::FunctionType *funcType =
        llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx), args, false);
    llvm::Function *mainFunc = llvm::Function::Create(
        funcType, llvm::Function::ExternalLinkage, "testfunc", m.get());
    llvm::BasicBlock *entry =
        llvm::BasicBlock::Create(ctx, "entrypoint", mainFunc);


    ScriptManager scriptMgr(llvm::getGlobalContext());
    scriptMgr.load();

    //m->getTypeByName()
    m->dump();
    return 0;
}
