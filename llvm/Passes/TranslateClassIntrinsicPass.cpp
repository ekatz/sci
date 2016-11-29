#include "TranslateClassIntrinsicPass.hpp"
#include "../World.hpp"
#include "../Object.hpp"
#include "../Decl.hpp"
#include <llvm/IR/GlobalVariable.h>

using namespace llvm;


BEGIN_NAMESPACE_SCI


TranslateClassIntrinsicPass::TranslateClassIntrinsicPass() :
    m_sizeTy(GetWorld().getSizeType())
{
}


TranslateClassIntrinsicPass::~TranslateClassIntrinsicPass()
{
}


void TranslateClassIntrinsicPass::run()
{
    Function *func = GetWorld().getIntrinsic(Intrinsic::clss);
    while (!func->user_empty())
    {
        ClassInst *call = cast<ClassInst>(func->user_back());
        translate(call);
    }
}


void TranslateClassIntrinsicPass::translate(ClassInst *call)
{
    uint classId = static_cast<uint>(call->getClassID()->getZExtValue());
    Object *cls = GetWorld().getClass(classId);
    assert(cls != nullptr && "Invalid class ID.");

    GlobalVariable *var = GetGlobalVariableDecl(cls->getGlobal(), call->getModule());
    Value *val = new PtrToIntInst(var, m_sizeTy, "", call);

    call->replaceAllUsesWith(val);
    call->eraseFromParent();
}


END_NAMESPACE_SCI
