#include "FixCodePass.hpp"
#include "../World.hpp"
#include "../Script.hpp"
#include "../Object.hpp"
#include "../Method.hpp"
#include "../Resource.hpp"
#include "../../SCI/src/Selector.h"

using namespace llvm;


BEGIN_NAMESPACE_SCI


FixCodePass::FixCodePass() : m_sizeTy(GetWorld().getSizeType())
{
}


FixCodePass::~FixCodePass()
{
}


void FixCodePass::run()
{
    World &world = GetWorld();

    Object *obj = world.getClass(0);
    Method *method = obj->getMethod(s_isKindOf);
    Function *func = method->getFunction();
    Module *module = func->getParent();

    Function *newFunc = createIsKindOfMethodFunction(module);

    newFunc->takeName(func);
    func->replaceAllUsesWith(newFunc);
    func->eraseFromParent();
}


Function* FixCodePass::createIsKindOfFunctionPrototype(Module *module) const
{
    Type *params[] = { m_sizeTy, m_sizeTy };
    FunctionType *funcTy = FunctionType::get(Type::getInt1Ty(m_sizeTy->getContext()), params, false);

    return Function::Create(funcTy, GlobalValue::ExternalLinkage, "IsKindOf", module);
}


Function* FixCodePass::createIsKindOfMethodFunction(Module *module) const
{
    World &world = GetWorld();
    LLVMContext &ctx = world.getContext();

    Type *params[] = { m_sizeTy, m_sizeTy->getPointerTo() };
    FunctionType *funcTy = FunctionType::get(m_sizeTy, params, false);
    Function *func = Function::Create(funcTy, GlobalValue::LinkOnceODRLinkage, "", module);

    BasicBlock *bb = BasicBlock::Create(ctx, "entry", func);

    auto ai = func->arg_begin();
    Value *obj1Var = &*ai;
    ConstantInt *c = ConstantInt::get(m_sizeTy, 1);
    Value *obj2Var = GetElementPtrInst::CreateInBounds(&*++ai, c, "", bb);
    obj2Var = new LoadInst(obj2Var, "", false, world.getSizeTypeAlignment(), bb);

    Function *isKindOfFunc = createIsKindOfFunctionPrototype(module);
    Value *args[] = { obj1Var, obj2Var };
    Value *val = CallInst::Create(isKindOfFunc, args, "", bb);
    val = new ZExtInst(val, m_sizeTy, "", bb);
    ReturnInst::Create(ctx, val, bb);
    return func;
}


END_NAMESPACE_SCI
