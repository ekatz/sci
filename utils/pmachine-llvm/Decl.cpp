#include "Decl.hpp"
#include <llvm/IR/Module.h>

using namespace llvm;


BEGIN_NAMESPACE_SCI


Function* GetFunctionDecl(Function *orig, Module *module)
{
    assert(orig->hasName() && "Declaration must have a name!");
    Function *func = module->getFunction(orig->getName());
    if (func == nullptr)
    {
        func = Function::Create(orig->getFunctionType(), GlobalValue::ExternalLinkage, orig->getName(), module);
        func->copyAttributesFrom(orig);

        if (!orig->hasExternalLinkage())
        {
            orig->setLinkage(GlobalValue::ExternalLinkage);
        }
    }
    return func;
}

GlobalVariable* GetGlobalVariableDecl(GlobalVariable *orig, Module *module)
{
    assert(orig->hasName() && "Declaration must have a name!");
    GlobalVariable *var = module->getGlobalVariable(orig->getName(), true);
    if (var == nullptr)
    {
        var = new GlobalVariable(*module,
                                 orig->getType()->getElementType(),
                                 orig->isConstant(),
                                 GlobalValue::ExternalLinkage,
                                 nullptr,
                                 orig->getName(),
                                 nullptr,
                                 orig->getThreadLocalMode(),
                                 orig->getType()->getAddressSpace());
        var->copyAttributesFrom(orig);
        var->setAlignment(orig->getAlignment());

        if (!orig->hasExternalLinkage())
        {
            orig->setLinkage(GlobalValue::ExternalLinkage);
        }
    }
    return var;
}


END_NAMESPACE_SCI
