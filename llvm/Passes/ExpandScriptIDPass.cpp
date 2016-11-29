#include "ExpandScriptIDPass.hpp"
#include "../World.hpp"
#include "../Script.hpp"
#include "../Decl.hpp"

using namespace llvm;


#define KORDINAL_ScriptID   2


BEGIN_NAMESPACE_SCI


ExpandScriptIDPass::ExpandScriptIDPass()
{
}


ExpandScriptIDPass::~ExpandScriptIDPass()
{
}


void ExpandScriptIDPass::run()
{
    World &world = GetWorld();

    Function *func = world.getIntrinsic(Intrinsic::callk);
    auto u = func->user_begin();
    while (u != func->user_end())
    {
        CallKernelInst *call = cast<CallKernelInst>(*u);
        ++u;

        if (static_cast<uint>(call->getKernelOrdinal()->getZExtValue()) == KORDINAL_ScriptID)
        {
            expand(call);
        }
    }
}


void ExpandScriptIDPass::expand(CallKernelInst *call)
{
    uint argc = static_cast<uint>(cast<ConstantInt>(call->getArgCount())->getZExtValue());
    assert((argc == 1 || argc == 2) && "Invalid KScriptID call!");

    uint entryIndex = (argc == 2) ? static_cast<uint>(cast<ConstantInt>(call->getArgOperand(1))->getZExtValue()) : 0;

    Value *val;
    World &world = GetWorld();
    Value *scriptIdVal = call->getArgOperand(0);
    if (isa<ConstantInt>(scriptIdVal))
    {
        uint scriptId = static_cast<uint>(cast<ConstantInt>(scriptIdVal)->getZExtValue());

        Script *script = world.getScript(scriptId);
        assert(script != nullptr && "Invalid script ID.");

        GlobalVariable *var = cast<GlobalVariable>(script->getExportedValue(entryIndex));
        var = GetGlobalVariableDecl(var, call->getModule());
        val = new PtrToIntInst(var, world.getSizeType(), "", call);
    }
    else
    {
        assert(entryIndex == 0 && "Non-constant Script ID is not supported without entry index 0!");
        Function *func = getOrCreateGetDispatchAddrFunction();
        func = GetFunctionDecl(func, call->getModule());
        val = CallInst::Create(func, scriptIdVal, "", call);
    }

    call->replaceAllUsesWith(val);
    call->eraseFromParent();
}


Function* ExpandScriptIDPass::getOrCreateGetDispatchAddrFunction() const
{
    StringRef name = "GetDispatchAddr";
    World &world = GetWorld();
    LLVMContext &ctx = world.getContext();
    IntegerType *sizeTy = world.getSizeType();
    Module *mainModule = world.getScript(0)->getModule();

    Function *func = mainModule->getFunction(name);
    if (func != nullptr)
    {
        return func;
    }

    FunctionType *funcTy = FunctionType::get(sizeTy, sizeTy, false);
    func = Function::Create(funcTy, GlobalValue::InternalLinkage, name, mainModule);

    BasicBlock *bbEntry = BasicBlock::Create(ctx, "entry", func);

    BasicBlock *bbUnknown = BasicBlock::Create(ctx, "case.unknown", func);
    ReturnInst::Create(ctx, Constant::getNullValue(sizeTy), bbUnknown);

    SwitchInst *switchScript = SwitchInst::Create(&*func->arg_begin(), bbUnknown, world.getScriptCount(), bbEntry);

    for (Script &script : world.scripts())
    {
        GlobalVariable *var = dyn_cast_or_null<GlobalVariable>(script.getExportedValue(0));
        if (var != nullptr)
        {
            var = GetGlobalVariableDecl(var, mainModule);
            BasicBlock *bbCase = BasicBlock::Create(ctx, "case." + script.getModule()->getName(), func, bbUnknown);
            Value *val = new PtrToIntInst(var, sizeTy, "", bbCase);
            ReturnInst::Create(ctx, val, bbCase);

            switchScript->addCase(ConstantInt::get(sizeTy, script.getId()), bbCase);
        }
    }
    return func;
}


END_NAMESPACE_SCI
