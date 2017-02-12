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
    Method *method;
    Function *func, *newFunc;
    Module *module;

    method = obj->getMethod(s_isKindOf);
    func = method->getFunction();
    module = func->getParent();

    newFunc = createObjMethodFunction(module, createIsKindOfFunctionPrototype(module));

    newFunc->takeName(func);
    func->replaceAllUsesWith(newFunc);
    func->eraseFromParent();


    method = obj->getMethod(s_isMemberOf);
    func = method->getFunction();
    module = func->getParent();

    newFunc = createObjMethodFunction(module, createIsMemberOfFunctionPrototype(module));

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


Function* FixCodePass::createIsMemberOfFunctionPrototype(Module *module) const
{
    Type *params[] = { m_sizeTy, m_sizeTy };
    FunctionType *funcTy = FunctionType::get(Type::getInt1Ty(m_sizeTy->getContext()), params, false);

    return Function::Create(funcTy, GlobalValue::ExternalLinkage, "IsMemberOf", module);
}


Function* FixCodePass::createObjMethodFunction(Module *module, Function *externFunc) const
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

    Value *args[] = { obj1Var, obj2Var };
    Value *val = CallInst::Create(externFunc, args, "", bb);
    val = new ZExtInst(val, m_sizeTy, "", bb);
    ReturnInst::Create(ctx, val, bb);
    return func;
}


END_NAMESPACE_SCI
