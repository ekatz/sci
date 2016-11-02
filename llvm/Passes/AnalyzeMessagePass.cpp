#include "AnalyzeMessagePass.hpp"
#include "../World.hpp"
#include "../Script.hpp"
#include "../Object.hpp"
#include "../Method.hpp"
#include "../Property.hpp"
#include "../Resource.hpp"
#include <llvm/IR/CFG.h>

using namespace llvm;


BEGIN_NAMESPACE_SCI


static bool EqualGEPs(GetElementPtrInst *gep1, GetElementPtrInst *gep2)
{
    auto i1 = gep1->idx_begin(), e1 = gep1->idx_end();
    auto i2 = gep2->idx_begin(), e2 = gep2->idx_end();
    while (true)
    {
        // If one reached the end but the other not, then exit.
        if ((i1 == e1) ^ (i2 == e2))
        {
            return false;
        }
        if (i1 == e1)
        {
            break;
        }

        ConstantInt *int1 = dyn_cast<ConstantInt>(i1->get());
        ConstantInt *int2 = dyn_cast<ConstantInt>(i2->get());
        if ((int1 != nullptr) ^ (int2 != nullptr))
        {
            return false;
        }
        if (int1 != nullptr)
        {
            if (int1->getZExtValue() != int2->getZExtValue())
            {
                return false;
            }
        }

        ++i1;
        ++i2;
    }
    return true;
}


static bool ExtractClass(CallInst *objCall, Class *&cls)
{
    cls = nullptr;
    bool ret = false;
    World &world = GetWorld();
    Function *calledFunc = objCall->getCalledFunction();
    if (calledFunc == world.getStub(Stub::objc))
    {
        ret = true;
        ConstantInt *c = cast<ConstantInt>(objCall->getArgOperand(1));
        if (!c->isAllOnesValue())
        {
            uintptr_t clsAddr = static_cast<uintptr_t>(c->getZExtValue());
            cls = reinterpret_cast<Class *>(clsAddr);
        }
    }
    else if (calledFunc == world.getStub(Stub::clss))
    {
        ret = true;
        ConstantInt *c = cast<ConstantInt>(objCall->getArgOperand(0));
        uint classId = static_cast<uint>(c->getZExtValue());
        cls = world.getClass(classId);
    }
    return ret;
}


static Class* ValueToClass(ConstantInt *val)
{
    uintptr_t classId = static_cast<uintptr_t>(val->getZExtValue());
    Class *cls = GetWorld().getClass(static_cast<uint>(classId));
    if (cls == nullptr)
    {
        if (classId != (uintptr_t)-1)
        {
            cls = reinterpret_cast<Class *>(classId);
        }
    }
    return cls;
}


static Class* ValueToClass(Constant *val)
{
    if (isa<GlobalVariable>(val))
    {
        return GetWorld().lookupObject(*cast<GlobalVariable>(val));
    }
    else if (isa<ConstantInt>(val))
    {
        return ValueToClass(cast<ConstantInt>(val));
    }
    return nullptr;
}


static ConstantInt* ClassToValue(Class *cls)
{
    uintptr_t classId = cls->isObject() ? reinterpret_cast<uintptr_t>(cls) : cls->getId();
    return ConstantInt::get(GetWorld().getSizeType(), classId);
}


static bool Inherits(Class *super, Class *derived)
{
    uint depth = super->getDepth();
    while (depth < derived->getDepth())
    {
        derived = derived->getSuper();
        assert(derived != nullptr);
    }

    return (super == derived);
}


static Class* FindCommonBaseClass(Class *cls1, Class *cls2, uint selector)
{
    if (cls1 != cls2)
    {
        if (cls2->getDepth() < cls1->getDepth())
        {
            std::swap(cls2, cls1);
        }

        uint depth = cls1->getDepth();
        while (depth < cls2->getDepth())
        {
            cls2 = cls2->getSuper();
            assert(cls2 != nullptr);
        }

        while (cls1 != cls2)
        {
            cls1 = cls1->getSuper();
            cls2 = cls2->getSuper();
            assert(cls1 != nullptr && cls2 != nullptr);
        }
    }

    if (selector != selector_cast<uint>(-1) && cls1->getMethod(selector) == nullptr && cls1->getProperty(selector) == nullptr)
    {
        return nullptr;
    }
    return cls1;
}


static Constant* FindCommonBaseClass(Constant *cls1, Constant *cls2, uint selector)
{
    if (cls1 == cls2)
    {
        Class *cls = ValueToClass(cls1);
        if (selector != selector_cast<uint>(-1) && cls->getMethod(selector) == nullptr && cls->getProperty(selector) == nullptr)
        {
            return nullptr;
        }
        return cls1;
    }

    Class *cls = FindCommonBaseClass(ValueToClass(cls1), ValueToClass(cls2), selector);
    return ClassToValue(cls);
}


static Constant* UpdateCommonBaseClass(Constant *clsOrig, Constant *clsNew, uint selector)
{
    Constant *c;
    if (clsOrig == nullptr || clsNew == nullptr)
    {
        if (clsOrig == nullptr && clsNew == nullptr)
        {
            return nullptr;
        }

        c = (clsOrig != nullptr) ? clsOrig : clsNew;
        Class *cls = ValueToClass(c);
        if (cls->getMethod(selector) == nullptr && cls->getProperty(selector) == nullptr)
        {
            return nullptr;
        }
        return c;
    }

    c = FindCommonBaseClass(clsOrig, clsNew, selector);
    return (c != nullptr) ? c : clsOrig;
}


static Constant* UpdateCommonBaseClass(Constant *clsOrig, Constant *clsNew)
{
    if (clsOrig == nullptr)
    {
        return clsNew;
    }

    if (clsNew == nullptr)
    {
        return clsOrig;
    }

    return FindCommonBaseClass(clsOrig, clsNew, selector_cast<uint>(-1));
}


static bool IsPotentialPropertySendCall(const SendMessageInst *sendMsg)
{
    ConstantInt *c;

    c = dyn_cast<ConstantInt>(sendMsg->getSelector());
    if (c != nullptr)
    {
        uint sel = static_cast<uint>(c->getSExtValue());
        if (GetWorld().getSelectorTable().getProperties(sel).empty())
        {
            return false;
        }
    }

    c = dyn_cast<ConstantInt>(sendMsg->getArgCount());
    if (c != nullptr)
    {
        uint argc = static_cast<uint>(c->getZExtValue());
        switch (argc)
        {
        case 0:
            if (sendMsg->user_empty())
            {
                return false;
            }
            break;

        case 1:
            if (!sendMsg->user_empty())
            {
                if (sendMsg->hasOneUse())
                {
                    const Instruction *user = sendMsg->user_back();
                    const StoreInst *store = dyn_cast<StoreInst>(user);
                    if (store != nullptr)
                    {
                        const AllocaInst *alloc = dyn_cast<AllocaInst>(store->getPointerOperand());
                        if (alloc != nullptr)
                        {
                            if (alloc->getName().equals("acc.addr"))
                            {
                                break;
                            }
                        }
                    }
                    else if (isa<ReturnInst>(user))
                    {
                        break;
                    }
                }
                return false;
            }
            break;

        default:
            return false;
        }
    }
    return true;
}


static const Property* GetPropertyInfo(CallInst *call, Class *cls, uint &selector, uint &index)
{
    World &world = GetWorld();
    Function *calledFunc = call->getCalledFunction();
    if (calledFunc == world.getStub(Stub::send))
    {
        selector = static_cast<uint>(cast<ConstantInt>(call->getArgOperand(1))->getSExtValue());
        return cls->getProperty(selector, reinterpret_cast<int&>(index));
    }
    else if (calledFunc == world.getStub(Stub::prop))
    {
        ArrayRef<Property> props = cls->getProperties();
        index = static_cast<uint>(cast<ConstantInt>(call->getArgOperand(0))->getZExtValue());
        if (index < props.size())
        {
            const Property &p = props[index];
            selector = p.getSelector();
            return &p;
        }
    }
    selector = index = (uint)-1;
    return nullptr;
}


static const Property* GetProperty(CallInst *call, Class *cls)
{
    uint selector, index;
    return GetPropertyInfo(call, cls, selector, index);
}


static Class* GetPropertyClass(CallInst *call)
{
    Procedure *proc = GetWorld().getProcedure(*call->getFunction());
    assert(proc != nullptr && proc->isMethod());
    Class *cls = &static_cast<Method *>(proc)->getClass();

    assert(GetProperty(call, cls) != nullptr);
    return cls;
}


static bool IsFirstArgument(Argument *arg)
{
    return (arg == &*arg->getParent()->arg_begin());
}


AnalyzeMessagePass::AnalyzeMessagePass() :
    m_varRefCount(0),
    m_selector(selector_cast<uint>(-1))
{
}


AnalyzeMessagePass::~AnalyzeMessagePass()
{
}


void AnalyzeMessagePass::run()
{
    initializeReferences();
    ValueTracer tracer;

    World &world = GetWorld();
    ConstantInt *allOnes = cast<ConstantInt>(Constant::getAllOnesValue(world.getSizeType()));
    Function *sendFunc = world.getStub(Stub::send);
    Function *objcFunc = world.getStub(Stub::objc);
    for (User *user : sendFunc->users())
    {
        CallInst *call = cast<CallInst>(user);
        Value *obj = call->getArgOperand(0);
        
        Value *orig = tracer.backtrace(obj);
        assert(orig != nullptr);

        Value *args[] = {
            obj,
            allOnes,
            orig,
            nullptr
        };

        uint argCount = 3;
        if (isa<GetElementPtrInst>(orig))
        {
            args[argCount++] = cast<GetElementPtrInst>(orig)->getPointerOperand();
        }

        CallInst *callObjc = CallInst::Create(objcFunc, makeArrayRef(args, argCount), "", call);
        call->setArgOperand(0, callObjc);

   //     MessageAnalyzer(*this).analyze(call);
//         Value *v = backtraceValue(call->getArgOperand(0));
//         v = v;
//         if (isa<ConstantInt>(call->getArgOperand(0)))
//         {
//             mutateSuper(call);
//         }
    }


    for (User *user : objcFunc->users())
    {
        CallInst *call = cast<CallInst>(user);
        gatherObjectCalls(call);
    }
}


Class* ObjectAnalyzer::analyze(ObjCastInst *objc)
{
    Class *cls;
    ConstantInt *classIdVal = objc->getClassID();
    if (!classIdVal->isAllOnesValue())
    {
        uint classId = static_cast<uint>(classIdVal->getZExtValue());
        cls = GetWorld().getClass(classId);
        assert(cls != nullptr && "Unknown class ID.");
    }
    else
    {
        Value *anchor = objc->getAnchor();
        gatherObjectCalls(anchor);
        cls = analyzeValue(anchor);
        assert(cls != nullptr && "Failed to analyze object.");

        classIdVal = ConstantInt::get(classIdVal->getType(), cls->getId());
        for (ObjCastInst *objc : m_calls)
        {
            objc->setClassID(classIdVal);
        }
    }
    return cls;
}


Class* ObjectAnalyzer::analyzeValue(Value *val)
{
    assert(!m_classes.empty() && "No potential classes available.");
    if (m_classes.size() == 1)
    {
        return m_classes.front();
    }

    switch (val->getValueID())
    {
    case Value::ArgumentVal:
    case Value::GlobalVariableVal:
        if (!val->getName().empty() && val->getName().front() != '?')
        {
            assert(m_gep == nullptr && "Cannot index an object directly.");
            Class *cls = GetWorld().lookupObject(*cast<GlobalVariable>(val));
            chooseClass(cls);
            return cls;
        }
        return analyzeStoredGlobal(cast<GlobalVariable>(val));

    default:
        assert(false && "Unsupported value type for analysis.");
        return nullptr;

    case Value::InstructionVal + Instruction::Call:
        Intrinsic::send;
        Intrinsic::prop;
        Intrinsic::clss;
        Intrinsic::callk;

    case Value::InstructionVal + Instruction::GetElementPtr:



    case Value::GlobalVariableVal:

    case Value::ConstantIntVal:
        return nullptr;
    {
        This is not good!!!

//         ConstantInt *classIdVal = cast<ConstantInt>(val);
//         uint classId = static_cast<uint>(classIdVal->getZExtValue());
//         return GetWorld().getClass(classId);
    }

    case Value::ArgumentVal:
        return analyzeArgument(cast<Argument>(val));

    case Value::InstructionVal + Instruction::Call:
        return analyzeReturnValue(cast<CallInst>(val));

    case Value::InstructionVal + Instruction::GetElementPtr:
        assert(m_gep == nullptr);
        m_gep = cast<GetElementPtrInst>(val);
        val = m_gep->getPointerOperand();
        break;

    default:
        assert(0);
        return nullptr;
    }


    if (isa<ConstantInt>(val))
    {
        call->setArgOperand(1, val);
        continue;
    }

    if (!isa<Instruction>(val))
    {
        for (User *user : val->users())
        {
            CallInst *call = dyn_cast<CallInst>(user);
            if (call == nullptr)
            {
                continue;
            }

            if (call->getCalledFunction() != objcFunc)
            {
                continue;
            }

            if (val != call->getArgOperand(2))
            {
                continue;
            }
        }
    }

    if (isa<GetElementPtrInst>(val))
    {
        GetElementPtrInst *gep = cast<GetElementPtrInst>(val);
        val = gep->getPointerOperand();
        for (User *user : val->users())
        {
            CallInst *call = dyn_cast<CallInst>(user);
            if (call == nullptr)
            {
                continue;
            }

            if (call->getCalledFunction() != objcFunc)
            {
                continue;
            }

            GetElementPtrInst *gep2 = dyn_cast<GetElementPtrInst>(call->getArgOperand(2));
            if (gep2 == nullptr)
            {
                continue;
            }

            assert(gep->getPointerOperand() == gep2->getPointerOperand());
            if (!EqualGEPs(gep, gep2))
            {
                continue;
            }
        }
    }
}


Class* ObjectAnalyzer::analyzeArgument(Argument *arg)
{
    Procedure *proc = GetWorld().getProcedure(*arg->getParent());
    if (proc->isMethod())
    {
        return analyzeMethodArgument(static_cast<Method *>(proc), arg);
    }
    else
    {
        return analyzeProcedureArgument(proc, arg);
    }
}


Class* ObjectAnalyzer::analyzeMethodArgument(Method *method, Argument *arg)
{
    World &world = GetWorld();
    Class *clsMethod = &method->getClass();

    // If this is the "self" param.
    if (IsFirstArgument(arg))
    {
        return clsMethod;
    }
}


Class* ObjectAnalyzer::analyzeProcedureArgument(Procedure *proc, Argument *arg)
{
    uint argIndex = arg->getArgNo() + 1;
    if (!proc->hasArgc())
    {
        // All "call" calls have "argc".
        argIndex++;
    }

    if (proc->getFunction()->hasInternalLinkage())
    {
        return analyzeInternalProcedureArgument(proc, argIndex);
    }
    else
    {
        return analyzeExternalProcedureArgument(proc, argIndex);
    }
}


Class* ObjectAnalyzer::analyzeInternalProcedureArgument(Procedure *proc, uint argIndex)
{
    Class *cls = nullptr;
    World &world = GetWorld();
    Script &scriptProc = proc->getScript();
    Module *moduleProc = scriptProc.getModule();
    uint offsetProc = proc->getOffset();

    Function *callFunc = world.getStub(Stub::call);
    for (User *user : callFunc->users())
    {
        CallInst *call = cast<CallInst>(user);
        if (call->getModule() != moduleProc)
        {
            continue;
        }

        uint offset = static_cast<uint>(cast<ConstantInt>(call->getArgOperand(0))->getZExtValue());
        if (offset != offsetProc)
        {
            continue;
        }

        Value *arg;
        if (argIndex < call->getNumArgOperands())
        {
            arg = call->getArgOperand(argIndex);

            // This is not a rest call, so just analyze this argument.
            if (!(isa<CallInst>(arg) && cast<CallInst>(arg)->getCalledFunction() == world.getStub(Stub::rest)))
            {
                cls = analyzeValue(arg);
                if (cls != nullptr)
                {
                    break;
                }
                continue;
            }
        }
        else
        {
            arg = (call->arg_end() - 1)->get();
            assert(cast<CallInst>(arg)->getCalledFunction() == world.getStub(Stub::rest));
        }

        // This is a rest call, so delegate it to the previous caller.
// 
//         if (isa<CallInst>(arg) && cast<CallInst>(arg)->getCalledFunction() == world.getStub(Stub::rest))
//         {
//             assert(m_gep != nullptr);
//         }
//         else
//         {
//             assert(m_gep == nullptr);
//         }
    }
}


Class* ObjectAnalyzer::analyzeExternalProcedureArgument(Procedure *proc, uint argIndex)
{
    World &world = GetWorld();
    Script &scriptProc = proc->getScript();

    for (Script &script : world.scripts())
    {
        ;
    }
}


void ObjectAnalyzer::chooseClass(Class *cls)
{
    for (Class *base : m_classes)
    {
        if (Inherits(base, cls))
        {
            m_classes.clear();
            m_classes.push_back(cls);
            return;
        }
    }
    assert(false && "Requested class does not inherit from any class on the list.");
}


void ObjectAnalyzer::gatherObjectCalls(Value *anchor)
{
    World &world = GetWorld();
    SelectorTable &sels = world.getSelectorTable();

    for (User *user : anchor->users())
    {
        ObjCastInst *objc = dyn_cast<ObjCastInst>(user);
        if (objc == nullptr)
        {
            continue;
        }

        m_calls.push_back(objc);
        addSelectorInfo(objc->getSendMessage());
    }

    intersectClasses();
}


uint* ObjectAnalyzer::lookupSelectorInfo(uint sel)
{
    for (uint &selInfo : m_selInfos)
    {
        if (sel == (selInfo & ~((uint)SEL_TYPE_MASK)))
        {
            return &selInfo;
        }
    }
    return nullptr;
}


void ObjectAnalyzer::addSelectorInfo(SendMessageInst *sendMsg)
{
    ConstantInt *selVal = dyn_cast<ConstantInt>(sendMsg->getSelector());
    if (selVal == nullptr)
    {
        return;
    }

    uint sel = static_cast<uint>(selVal->getSExtValue());
    uint *selInfo = lookupSelectorInfo(sel);
    if (selInfo == nullptr)
    {
        m_selInfos.push_back(sel);
        selInfo = &m_selInfos.back();

        if (!GetWorld().getSelectorTable().getMethods(sel).empty())
        {
            *selInfo |= SEL_TYPE_METHOD;
        }

        if (IsPotentialPropertySendCall(sendMsg))
        {
            *selInfo |= SEL_TYPE_PROP;
        }
    }
    else
    {
        if ((*selInfo & SEL_TYPE_PROP) != 0 && !IsPotentialPropertySendCall(sendMsg))
        {
            *selInfo &= ~((uint)SEL_TYPE_PROP);
        }
    }
}


void ObjectAnalyzer::intersectClasses()
{
    SelectorTable &sels = GetWorld().getSelectorTable();
    for (uint selInfo : m_selInfos)
    {
        uint sel = selInfo & ~((uint)SEL_TYPE_MASK);
        bool isProperty = (selInfo & SEL_TYPE_PROP) != 0;
        bool isMethod = (selInfo & SEL_TYPE_METHOD) != 0;

        if (isProperty)
        {
            ArrayRef<Property *> props = sels.getProperties(sel);
            for (Property *prop : props)
            {
                Class *cls = traverseBaseClass(&prop->getClass());
                if (cls != nullptr && find(m_classes, cls) == m_classes.end())
                {
                    m_classes.push_back(cls);
                }
            }
        }

        if (isMethod)
        {
            ArrayRef<Method *> methods = sels.getMethods(sel);
            for (Method *method : methods)
            {
                Class *cls = traverseBaseClass(&method->getClass());
                if (cls != nullptr && find(m_classes, cls) == m_classes.end())
                {
                    m_classes.push_back(cls);
                }
            }
        }
    }
}


Class* ObjectAnalyzer::traverseBaseClass(Class *cls)
{
    int depthBase = -1;
    Class *clsBase = nullptr;
    for (uint selInfo : m_selInfos)
    {
        uint sel = selInfo & ~((uint)SEL_TYPE_MASK);
        bool isProperty = (selInfo & SEL_TYPE_PROP) != 0;
        bool isMethod = (selInfo & SEL_TYPE_METHOD) != 0;

        Class *clsTrav = TraverseBaseClass(cls, sel, isProperty, isMethod);
        if (clsTrav == nullptr)
        {
            return nullptr;
        }

        int depth = static_cast<int>(clsTrav->getDepth());
        if (depthBase < depth)
        {
            depthBase = depth;
            clsBase = clsTrav;
        }
    }
    return clsBase;
}


Class* ObjectAnalyzer::TraverseBaseClass(Class *cls, uint sel, bool isProperty, bool isMethod)
{
    if (isMethod)
    {
        int index;
        if (cls->getMethod(sel, index) != nullptr)
        {
            while (true)
            {
                Class *super = cls->getSuper();
                if (super == nullptr || static_cast<uint>(index) < super->getMethodCount())
                {
                    return cls;
                }
                cls = super;
            }
        }
    }

    if (isProperty)
    {
        int index;
        if (cls->getProperty(sel, index) != nullptr)
        {
            while (true)
            {
                Class *super = cls->getSuper();
                if (super == nullptr || static_cast<uint>(index) < super->getPropertyCount())
                {
                    return cls;
                }
                cls = super;
            }
        }
    }
    return nullptr;
}


MessageAnalyzer::MessageAnalyzer(AnalyzeMessagePass &pass) :
    m_pass(pass),
    m_gep(nullptr),
    m_klist(nullptr)
{
}


int MessageAnalyzer::KernelOrdinalToRefIndex(uint ordinal)
{
    switch (ordinal)
    {
#if 0
    case KORDINAL_NewNode:
        return KListInit_NewNode;

    case KORDINAL_AddAfter:
        return KListInit_AddAfter;

    case KORDINAL_AddToFront:
        return KListInit_AddToFront;

    case KORDINAL_AddToEnd:
        return KListInit_AddToEnd;
#endif
    default:
        return -1;
    }
}


void AnalyzeMessagePass::initializeReferences()
{
    World &world = GetWorld();
    uint tableSize = world.getSelectorTable().size();
    std::unique_ptr<uint[]> refCounts(new uint[tableSize]);
    memset(refCounts.get(), 0, tableSize * sizeof(uint));
    uint varRefCount = 0;

    Function *sendFunc = world.getStub(Stub::send);
    for (User *user : sendFunc->users())
    {
        CallInst *call = cast<CallInst>(user);
        ConstantInt *c = dyn_cast<ConstantInt>(call->getArgOperand(1));
        if (c != nullptr)
        {
            uint selector = static_cast<uint>(c->getZExtValue());
            refCounts[selector]++;
        }
        else
        {
            varRefCount++;
        }
    }
    
    m_refs.reset(new std::pair<std::unique_ptr<CallInst*[]>, uint>[tableSize]);
    m_varRefs.reset(new CallInst*[varRefCount]);
    m_varRefCount = varRefCount;

    varRefCount = 0;
    for (User *user : sendFunc->users())
    {
        CallInst *call = cast<CallInst>(user);
        ConstantInt *c = dyn_cast<ConstantInt>(call->getArgOperand(1));
        if (c != nullptr)
        {
            uint selector = static_cast<uint>(c->getZExtValue());
            if (!m_refs[selector].first)
            {
                uint count = refCounts[selector];
                m_refs[selector].first.reset(new CallInst*[count]);
                m_refs[selector].second = count;
            }

            uint idx = --refCounts[selector];
            m_refs[selector].first[idx] = call;
            assert(idx < (uint)m_refs[selector].second);
        }
        else
        {
            m_varRefs[varRefCount++] = call;
        }
    }


    memset(refCounts.get(), 0, tableSize * sizeof(uint));
    uint maxIndex = 0;
    Function *propFunc = world.getStub(Stub::prop);
    for (User *user : propFunc->users())
    {
        CallInst *call = cast<CallInst>(user);
        ConstantInt *c = cast<ConstantInt>(call->getArgOperand(0));
        uint index = static_cast<uint>(c->getZExtValue());
        if (maxIndex < index)
        {
            maxIndex = index;
        }

        refCounts[index]++;
    }

    m_propRefs.reset(new std::pair<std::unique_ptr<CallInst*[]>, uint>[maxIndex + 1]);
    for (User *user : propFunc->users())
    {
        CallInst *call = cast<CallInst>(user);
        ConstantInt *c = cast<ConstantInt>(call->getArgOperand(0));
        uint index = static_cast<uint>(c->getZExtValue());

        if (!m_propRefs[index].first)
        {
            uint count = refCounts[index];
            m_propRefs[index].first.reset(new CallInst*[count]);
            m_propRefs[index].second = count;
        }

        uint idx = --refCounts[index];
        m_propRefs[index].first[idx] = call;
        assert(idx < (uint)m_propRefs[index].second);
    }


    memset(refCounts.get(), 0, ARRAYSIZE(m_klistInitRefs) * sizeof(uint));
    Function *callkFunc = world.getStub(Stub::callk);
    for (User *user : callkFunc->users())
    {
        CallInst *call = cast<CallInst>(user);
        ConstantInt *c = cast<ConstantInt>(call->getArgOperand(0));
        uint kernelOrdinal = static_cast<uint>(c->getZExtValue());
        switch (kernelOrdinal)
        {
        case KORDINAL_NewNode:
            if (static_cast<uint>(cast<ConstantInt>(call->getArgOperand(1))->getZExtValue()) >= 1)
            {
                refCounts[KListInit_NewNode]++;
            }
            break;

        case KORDINAL_AddAfter:
            if (static_cast<uint>(cast<ConstantInt>(call->getArgOperand(1))->getZExtValue()) >= 4)
            {
                refCounts[KListInit_AddAfter]++;
            }
            break;

        case KORDINAL_AddToFront:
            if (static_cast<uint>(cast<ConstantInt>(call->getArgOperand(1))->getZExtValue()) >= 3)
            {
                refCounts[KListInit_AddToFront]++;
            }
            break;

        case KORDINAL_AddToEnd:
            if (static_cast<uint>(cast<ConstantInt>(call->getArgOperand(1))->getZExtValue()) >= 3)
            {
                refCounts[KListInit_AddToEnd]++;
            }
            break;

        default:
            break;
        }
    }

    for (User *user : callkFunc->users())
    {
        CallInst *call = cast<CallInst>(user);
        ConstantInt *c = cast<ConstantInt>(call->getArgOperand(0));
        uint kernelOrdinal = static_cast<uint>(c->getZExtValue());

        int index = -1;
        switch (kernelOrdinal)
        {
        case KORDINAL_NewNode:
            if (static_cast<uint>(cast<ConstantInt>(call->getArgOperand(1))->getZExtValue()) >= 1)
            {
                index = KListInit_NewNode;
            }
            break;

        case KORDINAL_AddAfter:
            if (static_cast<uint>(cast<ConstantInt>(call->getArgOperand(1))->getZExtValue()) >= 4)
            {
                index = KListInit_AddAfter;
            }
            break;

        case KORDINAL_AddToFront:
            if (static_cast<uint>(cast<ConstantInt>(call->getArgOperand(1))->getZExtValue()) >= 3)
            {
                index = KListInit_AddToFront;
            }
            break;

        case KORDINAL_AddToEnd:
            if (static_cast<uint>(cast<ConstantInt>(call->getArgOperand(1))->getZExtValue()) >= 3)
            {
                index = KListInit_AddToEnd;
            }
            break;

        default:
            break;
        }

        if (index < 0)
        {
            continue;
        }

        if (!m_klistInitRefs[index].first)
        {
            uint count = refCounts[index];
            m_klistInitRefs[index].first.reset(new CallInst*[count]);
            m_klistInitRefs[index].second = count;
        }

        uint idx = --refCounts[index];
        m_klistInitRefs[index].first[idx] = call;
        assert(idx < (uint)m_klistInitRefs[index].second);
    }
}


ArrayRef<CallInst*> AnalyzeMessagePass::getReferences(uint selector) const
{
    return makeArrayRef(m_refs[selector].first.get(), m_refs[selector].second);
}


ArrayRef<CallInst*> AnalyzeMessagePass::getPropReferences(uint index) const
{
    return makeArrayRef(m_propRefs[index].first.get(), m_propRefs[index].second);
}


Class* MessageAnalyzer::analyze(CallInst *call)
{
    Class *cls;
    World &world = GetWorld();
    assert(call->getCalledFunction() == world.getStub(Stub::send));

    Value *obj = call->getArgOperand(0);
    if (isa<CallInst>(obj))
    {
        if (ExtractClass(cast<CallInst>(obj), cls))
        {
            return cls;
        }
    }

    if (call->getFunction()->getName().equals("doit@UniversityDIcon"))
    {
        if (call->getName().equals("res@init10"))
        call = call;
    }

    gatherCallIntersectionBaseClasses(obj);

    Function *objcFunc = world.getStub(Stub::objc);
    if (m_classes.size() == 1)
    {
        cls = m_classes.front();
        Value *val = ClassToValue(cls);
        for (CallInst *callSend : m_calls)
        {
            Value *args[] = {
                callSend->getArgOperand(0),
                val
            };
            CallInst *callObjc = CallInst::Create(objcFunc, args, "", callSend);
            callSend->setArgOperand(0, callObjc);
        }
    }
    else
    {
        ConstantInt *allOnes = cast<ConstantInt>(Constant::getAllOnesValue(world.getSizeType()));
        for (CallInst *callSend : m_calls)
        {
            Value *args[] = {
                callSend->getArgOperand(0),
                allOnes
            };
            CallInst *callObjc = CallInst::Create(objcFunc, args, "", callSend);
            callSend->setArgOperand(0, callObjc);
        }

        if (!analyzeValue(obj))
        {
            return nullptr;
        }

        assert(m_classes.size() == 1);
        cls = m_classes.front();
        Value *val = ClassToValue(cls);
        for (CallInst *callSend : m_calls)
        {
            CallInst *callObjc = cast<CallInst>(callSend->getArgOperand(0));
            assert(callObjc->getCalledFunction() == objcFunc);
            callObjc->setArgOperand(1, val);
        }
    }

    

    uint sel = selector_cast<uint>(-1);
    ConstantInt *selVal = dyn_cast<ConstantInt>(call->getArgOperand(1));
    if (selVal != nullptr)
    {
        sel = static_cast<uint>(selVal->getSExtValue());
    }
    assert(sel == selector_cast<uint>(-1) || cls->getMethod(sel) != nullptr || cls->getProperty(sel) != nullptr);
    return cls;
}


Class* MessageAnalyzer::analyzeMessage(CallInst *call)
{
    return MessageAnalyzer(m_pass).analyze(call);
}


bool MessageAnalyzer::analyzeValue(Value *val)
{
    while (true)
    {
        switch (val->getValueID())
        {
        case Value::GlobalVariableVal:
            if (!val->getName().empty() && val->getName().front() != '?')
            {
                assert(m_gep == nullptr);
                Class *cls = GetWorld().lookupObject(*cast<GlobalVariable>(val));
                if (cls == nullptr)
                {
                    return false;
                }
                return updatePotentialClasses(cls);
            }
            return analyzeStoredGlobal(cast<GlobalVariable>(val));

        case Value::ConstantIntVal: {
            uint classId = static_cast<uint>(cast<ConstantInt>(val)->getZExtValue());
            Class *cls = GetWorld().getClass(classId);
            if (cls == nullptr)
            {
                return false;
            }
            return updatePotentialClasses(cls);
        }

        case Value::ArgumentVal:
            return analyzeArgument(cast<Argument>(val));

        case Value::InstructionVal + Instruction::Alloca:
            return analyzeStoredLocal(val);

        case Value::InstructionVal + Instruction::Call:
            return analyzeReturnValue(cast<CallInst>(val));

        case Value::InstructionVal + Instruction::PtrToInt:
            val = cast<PtrToIntInst>(val)->getPointerOperand();
            break;

        case Value::InstructionVal + Instruction::Load:
            val = cast<LoadInst>(val)->getPointerOperand();
            break;

       // case Value::InstructionVal + Instruction::Store:
        case Value::InstructionVal + Instruction::GetElementPtr:
            assert(m_gep == nullptr);
            m_gep = cast<GetElementPtrInst>(val);
            val = m_gep->getPointerOperand();
            break;

        default:
            assert(0);
            return false;
        }
    }
    return false;
}


bool MessageAnalyzer::analyzeReturnValue(CallInst *call)
{
    World &world = GetWorld();
    Function *calledFunc = call->getCalledFunction();
    if (calledFunc == world.getStub(Stub::send))
    {
        Class *cls = analyzeMessage(call);
        Value *selVal = call->getArgOperand(1);
        if (isa<ConstantInt>(selVal))
        {
            uint sel = static_cast<uint>(cast<ConstantInt>(selVal)->getSExtValue());
            Method *method = cls->getMethod(sel);
            if (method != nullptr)
            {
                return analyzeMethodReturnValue(cls, method);
            }
            else
            {
                return analyzeStoredProperty(cls, call);
            }

            //TODO: handle method pointer as a candidate.
            //backtraceValue - for method selector (stop on constant int)
        }
        else
        {
            //TODO: handle method pointer as a candidate.
        }
    }
    else if (calledFunc == world.getStub(Stub::prop))
    {
        Class *clsProp = GetPropertyClass(call);
        return analyzeStoredProperty(clsProp, call);
    }
    else if (calledFunc == world.getStub(Stub::callk))
    {
        uint kernelOrdinal = static_cast<uint>(cast<ConstantInt>(call->getArgOperand(0))->getZExtValue());
        switch (kernelOrdinal)
        {
        case KORDINAL_ScriptID: {
            uint scriptId = static_cast<uint>(cast<ConstantInt>(call->getArgOperand(2))->getZExtValue());
            uint entryIndex = 0;
            if (cast<ConstantInt>(call->getArgOperand(1))->getZExtValue() != 1)
            {
                entryIndex = static_cast<uint>(cast<ConstantInt>(call->getArgOperand(3))->getZExtValue());
            }
            Script *script = world.getScript(scriptId);
            GlobalObject *gobj = script->getExportedValue(entryIndex);
            assert(gobj != nullptr);
            return analyzeValue(gobj);
        }

        case KORDINAL_DisposeScript:
            return analyzeValue(call->getArgOperand(3));

        case KORDINAL_Clone:
            return analyzeValue(call->getArgOperand(2));

        case KORDINAL_MapKeyToDir:
            break;

        case KORDINAL_NodeValue:
            return analyzeKListNodeValue(call);

        //////////////////////////////////////////////////////////////////////////
        case KORDINAL_NextNode:
        case KORDINAL_PrevNode:
            if (m_klistCalls.insert(call).second)
            {
                return analyzeValue(call->getArgOperand(2));
            }
            break;

        case KORDINAL_NewNode:
            assert(0);
            break;

        case KORDINAL_FirstNode:
        case KORDINAL_LastNode:
        case KORDINAL_AddAfter:
        case KORDINAL_AddToFront:
        case KORDINAL_AddToEnd:
        case KORDINAL_FindKey:
            if (m_klist == nullptr)
            {
                m_klist = call->getArgOperand(2);
            }
            else if (m_klist != call->getArgOperand(2))
            {
                m_klist = m_klist;
            }
            break;
        //////////////////////////////////////////////////////////////////////////

        default:
            assert(0);
            break;
        }
    }
    else if (calledFunc == world.getStub(Stub::call))
    {
        uint offset = static_cast<uint>(cast<ConstantInt>(call->getArgOperand(0))->getZExtValue());
        Script *script = world.getScript(*call->getModule());
        assert(script != nullptr);
        Procedure *proc = script->getProcedure(offset);
        assert(proc != nullptr && !proc->isMethod());
        return analyzeProcedureReturnValue(proc);
    }
    else if (calledFunc == world.getStub(Stub::calle))
    {
        uint scriptId = static_cast<uint>(cast<ConstantInt>(call->getArgOperand(0))->getZExtValue());
        uint entryIndex = static_cast<uint>(cast<ConstantInt>(call->getArgOperand(1))->getZExtValue());
        Script *script = world.getScript(scriptId);
        GlobalObject *gobj = script->getExportedValue(entryIndex);
        assert(gobj != nullptr);
        return analyzeValue(gobj);
    }
    else
    {
        Class *cls;
        ExtractClass(call, cls);
        if (cls != nullptr)
        {
            return updatePotentialClasses(cls);
        }
    }
    return false;
}


bool MessageAnalyzer::analyzeProcedureReturnValue(Procedure *proc)
{
    // We are only interested in methods that return a pointer sized value.
    Function *func = proc->getFunction();
    if (func->getReturnType() != GetWorld().getSizeType())
    {
        return false;
    }

    for (const BasicBlock &bb : *func)
    {
        const ReturnInst *ret = dyn_cast<ReturnInst>(bb.getTerminator());
        if (ret == nullptr)
        {
            continue;
        }

        Value *retVal = ret->getReturnValue();
        assert(retVal != nullptr);
        if (analyzeValue(retVal))
        {
            return true;
        }
    }
    return false;
}


bool MessageAnalyzer::analyzeMethodReturnValue(Class *cls, Method *method)
{
    World &world = GetWorld();
    uint sel = method->getSelector();

    // The 'new' method is a special case.
    if (sel == world.getSelectorTable().getNewMethodSelector())
    {
        return updatePotentialClasses(cls);
    }

    Class *clsMethod = &method->getClass();
    IntegerType *sizeTy = world.getSizeType();
    ArrayRef<Method *> methods = world.getSelectorTable().getMethods(sel);
    for (Method *m : methods)
    {
        // Verify that the method overloads the current.
        if (!Inherits(clsMethod, &m->getClass()))
        {
            continue;
        }

        if (analyzeProcedureReturnValue(m))
        {
            return true;
        }
    }
    return false;
}


bool MessageAnalyzer::analyzeStoredProperty(Class *cls, CallInst *call)
{
    World &world = GetWorld();
    ArrayRef<CallInst *> refs;

    uint sel, index;
    GetPropertyInfo(call, cls, sel, index);
    assert(index != (uint)-1);

    // We only analyze a property 'getter'.
    assert(call->getCalledFunction() == world.getStub(Stub::prop) || cast<ConstantInt>(call->getArgOperand(2))->isNullValue());

    // Check for properties accessed by a selector.
    refs = m_pass.getReferences(sel);
    for (CallInst *callSend : refs)
    {
        // Property 'setter' must have 1 clear parameter.
        Value *val = callSend->getArgOperand(2);
        if (!isa<ConstantInt>(val))
        {
            continue;
        }

        // Property 'setter' must have only 1 parameter.
        if (static_cast<uint>(cast<ConstantInt>(val)->getZExtValue()) != 1)
        {
            continue;
        }

        // Property 'setter' do not return a value, so the 'send's return value cannot be used.
        if (!callSend->user_empty())
        {
            continue;
        }

        // Property 'setter' cannot have a 'rest' argument.
        val = callSend->getArgOperand(3);
        if (isa<CallInst>(val))
        {
            if (cast<CallInst>(val)->getCalledFunction() == world.getStub(Stub::rest))
            {
                continue;
            }
        }

        Class *clsProp = analyzeMessage(callSend);

        // Verify that this is actually a property.
        if (clsProp->getProperty(sel) == nullptr)
        {
            continue;
        }

        // Verify that the class is from the same inheritance path.
        if (!Inherits(cls, clsProp) && !Inherits(clsProp, cls))
        {
            continue;
        }

        if (analyzeValue(val))
        {
            return true;
        }
    }

    // Check for indexed properties.
    refs = m_pass.getPropReferences(index);
    for (CallInst *callProp : refs)
    {
        Class *clsProp = GetPropertyClass(callProp);

        // Verify that this is actually a property.
        if (clsProp->getProperty(sel) == nullptr)
        {
            continue;
        }

        // Verify that the class is from the same inheritance path.
        if (!Inherits(cls, clsProp) && !Inherits(clsProp, cls))
        {
            continue;
        }

        if (analyzeStoredLocal(callProp))
        {
            return true;
        }
    }
    return false;
}


bool MessageAnalyzer::analyzeKListNodeValue(CallInst *call)
{
    m_klistCalls.insert(call);
    analyzeValue(call->getArgOperand(2));
    m_klistCalls.clear();

    const Property *prop = nullptr;
    if (isa<CallInst>(m_klist))
    {
        CallInst *callSend = cast<CallInst>(m_klist);
        assert(callSend->getCalledFunction() == GetWorld().getStub(Stub::send));
        ConstantInt *selVal = cast<ConstantInt>(callSend->getArgOperand(1));
        uint sel = static_cast<uint>(selVal->getSExtValue());
        Class *cls = analyzeMessage(callSend);
        prop = cls->getProperty(sel);
    }
    else if (isa<LoadInst>(m_klist))
    {
        Value *ptrVal = cast<LoadInst>(m_klist)->getPointerOperand();
        CallInst *callProp = cast<CallInst>(ptrVal);
        assert(callProp->getCalledFunction() == GetWorld().getStub(Stub::prop));
        Class *cls = GetPropertyClass(callProp);
        prop = GetProperty(callProp, cls);
    }
    else
    {
        // We currently do not allow any other representation of the klist.
        assert(0);
    }
    assert(prop != nullptr);

    m_klist = nullptr;
    return false;// analyzeKList(prop);
}

/*
bool MessageAnalyzer::analyzeKList(const Property *prop)
{
    ;
}*/


bool MessageAnalyzer::analyzeStoredLocal(Value *ptrVal)
{
    assert(ptrVal->getType()->isPointerTy());

    if (m_gep != nullptr)
    {
        assert(m_gep->getPointerOperand() == ptrVal);
        GetElementPtrInst *gep = m_gep;
        m_gep = nullptr;

        for (User *user : ptrVal->users())
        {
            if (isa<GetElementPtrInst>(user))
            {
                GetElementPtrInst *otherGep = cast<GetElementPtrInst>(user);
                if (gep != otherGep && EqualGEPs(gep, otherGep))
                {
                    if (analyzeStoredLocal(otherGep))
                    {
                        return true;
                    }
                }
            }
        }
    }
    else
    {
        for (User *user : ptrVal->users())
        {
            if (isa<StoreInst>(user))
            {
                StoreInst *store = cast<StoreInst>(user);
                if (analyzeValue(store->getValueOperand()))
                {
                    return true;
                }
            }
        }
    }
    return false;
}


bool MessageAnalyzer::analyzeArgument(Argument *arg)
{
    World &world = GetWorld();
    Procedure *proc = world.getProcedure(*arg->getParent());
    if (proc->isMethod())
    {
        Method *method = static_cast<Method *>(proc);
        Class *clsMethod = &method->getClass();

        // If this is the "self" param.
        if (IsFirstArgument(arg))
        {
            bool updated = updatePotentialClasses(clsMethod);
            assert(updated);
            return updated;
        }

        uint argIndex = arg->getArgNo();
        if (proc->hasArgc())
        {
            argIndex--;
        }
        argIndex--; // "self" is always the first param.

        uint methodId = method->getSelector();
        ArrayRef<CallInst*> refs = m_pass.getReferences(methodId);
        for (CallInst *call : refs)
        {
            Class *cls = analyzeMessage(call);
            if (Inherits(clsMethod, cls))
            {
                Value *arg = call->getArgOperand(argIndex + 3);
                assert(!(isa<CallInst>(arg) && cast<CallInst>(arg)->getCalledFunction() == world.getStub(Stub::rest)));

                if (analyzeValue(arg))
                {
                    return true;
                }
            }
        }

        //TODO: handle method pointer as a candidate.
        //backtraceValue - for method selector (stop on constant int)
    }
    else if (proc->getFunction()->hasInternalLinkage())
    {
        ;
    }
    else
    {
        for (Script &script : world.scripts())
        {
            ;
        }
    }
    return false;
}


bool MessageAnalyzer::analyzeStoredGlobal(GlobalVariable *var)
{
    assert(m_gep != nullptr && m_gep->getNumIndices() == 2 && m_gep->hasAllConstantIndices());
    GetElementPtrInst *gep = m_gep;
    m_gep = nullptr;

    if (!var->getName().equals("?globals"))
    {
        return analyzeStoredScriptVariable(var, gep);
    }

    for (Script &script : GetWorld().scripts())
    {
        var = script.getModule()->getGlobalVariable("?globals");
        if (var != nullptr)
        {
            if (analyzeStoredScriptVariable(var, gep))
            {
                return true;
            }
        }
    }
    return false;
}


bool MessageAnalyzer::analyzeStoredScriptVariable(GlobalVariable *var, GetElementPtrInst *gep)
{
    for (User *user : var->users())
    {
        if (isa<GetElementPtrInst>(user))
        {
            GetElementPtrInst *otherGep = cast<GetElementPtrInst>(user);
            if (gep != otherGep && EqualGEPs(gep, otherGep))
            {
                if (analyzeStoredLocal(otherGep))
                {
                    return true;
                }
            }
        }
    }
    return false;
}


bool MessageAnalyzer::updatePotentialClasses(Class *cls)
{
    for (Class *clsPotential : m_classes)
    {
        if (Inherits(clsPotential, cls))
        {
            m_classes.clear();
            m_classes.push_back(cls);
            break;
        }
    }
    return (m_classes.size() == 1);
}


Class* MessageAnalyzer::TraverseBaseClass(Class *cls, uint sel, bool isProperty, bool isMethod)
{
    if (isMethod)
    {
        int index;
        if (cls->getMethod(sel, index) != nullptr)
        {
            while (true)
            {
                Class *super = cls->getSuper();
                if (super == nullptr || static_cast<uint>(index) < super->getMethodCount())
                {
                    return cls;
                }
                cls = super;
            }
        }
    }

    if (isProperty)
    {
        int index;
        if (cls->getProperty(sel, index) != nullptr)
        {
            while (true)
            {
                Class *super = cls->getSuper();
                if (super == nullptr || static_cast<uint>(index) < super->getPropertyCount())
                {
                    return cls;
                }
                cls = super;
            }
        }
    }
    return nullptr;
}


Class* MessageAnalyzer::TraverseBaseClass(Class *cls, const SmallDenseMap<uint, PotentialSelector> &potentialSels)
{
    int depthBase = -1;
    Class *clsBase = nullptr;
    for (const auto &p : potentialSels)
    {
        uint sel = p.first;
        PotentialSelector pi = p.second;

        Class *clsTrav = TraverseBaseClass(cls, sel, pi.isProperty, pi.isMethod);
        if (clsTrav == nullptr)
        {
            return nullptr;
        }

        int depth = static_cast<int>(clsTrav->getDepth());
        if (depthBase < depth)
        {
            depthBase = depth;
            clsBase = clsTrav;
        }
    }
    return clsBase;
}


void MessageAnalyzer::addPotentialSelector(CallInst *call, SmallDenseMap<uint, PotentialSelector> &potentialSels)
{
    ConstantInt *selVal = cast<ConstantInt>(call->getArgOperand(1));
    uint sel = static_cast<uint>(selVal->getSExtValue());
    auto &p = potentialSels[sel];
    if (p.isProperty && !IsPotentialPropertySendCall(call))
    {
        p.isProperty = false;
    }

    if (p.isMethod && GetWorld().getSelectorTable().getMethods(sel).empty())
    {
        p.isMethod = false;
    }

    m_calls.push_back(call);
}


void MessageAnalyzer::intersectClasses(const SmallDenseMap<uint, PotentialSelector> &potentialSels)
{
    SelectorTable &sels = GetWorld().getSelectorTable();
    for (const auto &p : potentialSels)
    {
        uint sel = p.first;
        PotentialSelector pi = p.second;
        if (pi.isProperty)
        {
            ArrayRef<Property *> props = sels.getProperties(sel);
            for (Property *prop : props)
            {
                Class *cls = TraverseBaseClass(&prop->getClass(), potentialSels);
                if (cls != nullptr && find(m_classes, cls) == m_classes.end())
                {
                    m_classes.push_back(cls);
                }
            }
        }

        if (pi.isMethod)
        {
            ArrayRef<Method *> methods = sels.getMethods(sel);
            for (Method *method : methods)
            {
                Class *cls = TraverseBaseClass(&method->getClass(), potentialSels);
                if (cls != nullptr && find(m_classes, cls) == m_classes.end())
                {
                    m_classes.push_back(cls);
                }
            }
        }
    }
}


//////////////////////////////////////////////////////////////////////////

static StoreInst* FindLastStoreInst(Value *ptrVal, BasicBlock *bb)
{
    for (auto i = bb->rbegin(), e = bb->rend(); i != e; ++i)
    {
        StoreInst *store = dyn_cast<StoreInst>(&*i);
        if (store != nullptr)
        {
            if (store->getPointerOperand() == ptrVal)
            {
                return store;
            }
        }
    }
    return nullptr;
}



void ValueTracer::clear()
{
    m_blocks.clear();
    m_gep = nullptr;
}


bool ValueTracer::addBasicBlock(BasicBlock *bb)
{
    bool ret = (find(m_blocks, bb) == m_blocks.end());
    if (ret)
    {
        m_blocks.push_back(bb);
    }
    return ret;
}


Value* ValueTracer::backtraceBasicBlockTransition(AllocaInst *accAddr, BasicBlock *bb)
{
    Value *res = nullptr;
    for (BasicBlock *pred : predecessors(bb))
    {
        if (!addBasicBlock(pred))
        {
            continue;
        }

        StoreInst *store = FindLastStoreInst(accAddr, pred);
        if (store == nullptr)
        {
            continue;
        }

        Value *val = backtraceValue(store->getValueOperand());
        if (val == nullptr)
        {
            continue;
        }

        res = val;
        if (!(isa<Constant>(val) && cast<Constant>(val)->isNullValue()))
        {
            break;
        }
    }
    return res;
}


Value* ValueTracer::backtraceValue(Value *val)
{
    while (true)
    {
        switch (val->getValueID())
        {
        case Value::ArgumentVal:
        case Value::GlobalVariableVal:
        case Value::ConstantIntVal:
            return val;

        case Value::InstructionVal + Instruction::Call:
            return backtraceReturnValue(cast<CallInst>(val));

        case Value::InstructionVal + Instruction::PtrToInt:
            val = cast<PtrToIntInst>(val)->getPointerOperand();
            break;

        case Value::InstructionVal + Instruction::Load:
            m_load = cast<LoadInst>(val);
            val = m_load->getPointerOperand();
//             if (isa<AllocaInst>(val) && val->getName().equals("acc.addr"))
//             {
//                 BasicBlock *bb = load->getParent();
//                 return backtraceBasicBlockTransition(cast<AllocaInst>(val), bb);
//             }
            break;

        case Value::InstructionVal + Instruction::Alloca:
            return backtraceStoredValue(val);

        case Value::InstructionVal + Instruction::GetElementPtr:
            assert(m_gep == nullptr && "Cannot backtrace more than one GEP instruction.");
            do
            {
                m_gep = cast<GetElementPtrInst>(val);
                val = m_gep->getPointerOperand();
            }
            while (isa<GetElementPtrInst>(val));
            break;

        default:
            assert(false && "Cannot backtrace from unsupported instruction");
            return nullptr;
        }
    }
    return nullptr;
}


Value* ValueTracer::backtraceStoredValue(Value *ptrVal)
{
    assert(ptrVal->getType()->isPointerTy() && "Destination value is not a pointer.");

    Value *res = nullptr;
    if (m_gep != nullptr)
    {
        assert(m_gep->getPointerOperand() == ptrVal && "Mismatch GEP instruction.");
        GetElementPtrInst *gep = m_gep;
        m_gep = nullptr;

        for (User *user : ptrVal->users())
        {
            if (isa<GetElementPtrInst>(user))
            {
                GetElementPtrInst *otherGep = cast<GetElementPtrInst>(user);
                if (gep != otherGep && EqualGEPs(gep, otherGep))
                {
                    Value *val = backtraceStoredValue(otherGep);
                    if (val != nullptr)
                    {
                        res = val;
                        if (!(isa<Constant>(val) && cast<Constant>(val)->isNullValue()))
                        {
                            break;
                        }
                    }
                }
            }
        }
    }
    else
    {
        for (User *user : ptrVal->users())
        {
            if (isa<StoreInst>(user))
            {
                StoreInst *store = cast<StoreInst>(user);
                Value *val = backtraceValue(store->getValueOperand());
                if (val != nullptr)
                {
                    res = val;
                    if (!(isa<Constant>(val) && cast<Constant>(val)->isNullValue()))
                    {
                        break;
                    }
                }
            }
        }
    }
    return res;
}


Value* ValueTracer::backtraceReturnValue(CallInst *call)
{
    Value *val = call;
    World &world = GetWorld();
    switch (cast<IntrinsicInst>(call)->getIntrinsicID())
    {
    case Intrinsic::objc:
        val = call->getArgOperand(2);
        break;

    case Intrinsic::send: {
        Value *selVal = call->getArgOperand(1);
        if (isa<ConstantInt>(selVal))
        {
            uint sel = static_cast<uint>(cast<ConstantInt>(selVal)->getSExtValue());
            if (sel == world.getSelectorTable().getNewMethodSelector())
            {
                val = call->getArgOperand(0);
                val = backtraceValue(val);
            }
        }
        break;
    }

    case Intrinsic::call: {
        uint offset = static_cast<uint>(cast<ConstantInt>(call->getArgOperand(0))->getZExtValue());
        Script *script = world.getScript(*call->getModule());
        assert(script != nullptr && "Module is not owned by a script!");
        Procedure *proc = script->getProcedure(offset);
        assert(proc != nullptr && !proc->isMethod() && "Procedure not found!");
        val = backtraceFunctionReturnValue(proc->getFunction());
        break;
    }

    case Intrinsic::calle: {
        uint scriptId = static_cast<uint>(cast<ConstantInt>(call->getArgOperand(0))->getZExtValue());
        uint entryIndex = static_cast<uint>(cast<ConstantInt>(call->getArgOperand(1))->getZExtValue());
        Script *script = world.getScript(scriptId);
        GlobalObject *func = script->getExportedValue(entryIndex);
        val = backtraceFunctionReturnValue(cast<Function>(func));
        break;
    }

    default:
        break;
    }
    return val;
}


Value* ValueTracer::backtraceFunctionReturnValue(Function *func)
{
    assert(func->getReturnType() == GetWorld().getSizeType() &&
           "Function does not return an object size value!");

    Value *val = nullptr;
    for (BasicBlock &bb : *func)
    {
        ReturnInst *ret = dyn_cast<ReturnInst>(bb.getTerminator());
        if (ret == nullptr)
        {
            continue;
        }

        Value *retVal = ret->getReturnValue();
        val = backtraceValue(retVal);
        if (!(isa<Constant>(val) && cast<Constant>(val)->isNullValue()))
        {
            break;
        }
    }
    return val;
}


Value* ValueTracer::backtrace(Value *val)
{
    clear();
    val = backtraceValue(val);
    assert(m_gep == nullptr || m_gep->getPointerOperand() == val);
    return (m_gep == nullptr) ? val : m_gep;
}

//////////////////////////////////////////////////////////////////////////


void MessageAnalyzer::gatherCallIntersectionBaseClasses(Value *objVal)
{
    SmallVector<Instruction *, 16> instructions;

    Value *val = objVal;
    while (true)
    {
        switch (val->getValueID())
        {
        case Value::GlobalVariableVal:
            if (!val->getName().empty() && val->getName().front() != '?')
            {
                assert(m_gep == nullptr);
                Class *cls = GetWorld().lookupObject(*cast<GlobalVariable>(val));
                if (cls != nullptr)
                {
                    m_classes.push_back(cls);
                }
            }

        case Value::InstructionVal + Instruction::Alloca:
        case Value::InstructionVal + Instruction::Call:
            gatherValueBaseClasses(val, instructions);
            return;

        case Value::ArgumentVal:
            gatherArgumentBaseClasses(cast<Argument>(val), instructions);
            return;

        case Value::InstructionVal + Instruction::PtrToInt:
            instructions.push_back(cast<Instruction>(val));
            val = cast<PtrToIntInst>(val)->getPointerOperand();
            break;

        case Value::InstructionVal + Instruction::Load:
            instructions.push_back(cast<Instruction>(val));
            val = cast<LoadInst>(val)->getPointerOperand();
            break;

            // case Value::InstructionVal + Instruction::Store:
        case Value::InstructionVal + Instruction::GetElementPtr:
            instructions.push_back(cast<Instruction>(val));
            val = cast<GetElementPtrInst>(val)->getPointerOperand();
            break;

        default:
            assert(0);
            return;
        }
    }

    if (m_classes.empty())
    {
        gatherValueBaseClasses(objVal, ArrayRef<Instruction *>());
        assert(!m_classes.empty());
    }
}


void MessageAnalyzer::gatherValueBaseClasses(Value *objVal, ArrayRef<Instruction *> insts, SmallDenseMap<uint, PotentialSelector> &potentialSels)
{
    if (!insts.empty())
    {
        Instruction *inst = insts.back();
        for (User *user : objVal->users())
        {
            if (user->getValueID() != inst->getValueID())
            {
                continue;
            }

            bool equal;
            switch (user->getValueID())
            {
            case Value::InstructionVal + Instruction::GetElementPtr:
                equal = EqualGEPs(cast<GetElementPtrInst>(user), cast<GetElementPtrInst>(inst));
                break;

            case Value::InstructionVal + Instruction::PtrToInt:
            case Value::InstructionVal + Instruction::Load:
                equal = true;
                break;

            default:
                equal = (user == inst);
                break;
            }

            if (equal)
            {
                gatherValueBaseClasses(user, makeArrayRef(insts.data(), insts.size() - 1), potentialSels);
            }
        }
    }
    else
    {
        World &world = GetWorld();
        Function *sendFunc = world.getStub(Stub::send);

        if (isa<CallInst>(objVal))
        {
            CallInst *call = cast<CallInst>(objVal);
            Class *cls;
            ExtractClass(call, cls);
            if (cls != nullptr)
            {
                m_classes.push_back(cls);
            }
            else if (call->getCalledFunction() == sendFunc)
            {
                Value *selVal = call->getArgOperand(1);
                if (isa<ConstantInt>(selVal))
                {
                    uint sel = static_cast<uint>(cast<ConstantInt>(selVal)->getSExtValue());
                    if (sel == world.getSelectorTable().getNewMethodSelector())
                    {
                        gatherCallIntersectionBaseClasses(call->getArgOperand(0));
                    }
                }
            }
        }

        for (User *user : objVal->users())
        {
            CallInst *call = dyn_cast<CallInst>(user);
            if (call != nullptr && call->getCalledFunction() == sendFunc)
            {
                if (call->getArgOperand(0) == objVal)
                {
                    if (isa<ConstantInt>(call->getArgOperand(1)))
                    {
                        addPotentialSelector(call, potentialSels);
                    }
                }
            }
        }
    }
}


void MessageAnalyzer::gatherValueBaseClasses(Value *objVal, ArrayRef<Instruction *> insts)
{
    SmallDenseMap<uint, PotentialSelector> potentialSels;
    gatherValueBaseClasses(objVal, insts, potentialSels);

    // Make sure the gathered classes respond to the selectors.
    if (!m_classes.empty())
    {
        for (int i = 0; i < static_cast<int>(m_classes.size()); ++i)
        {
            Class *cls = m_classes[i];
            for (const auto &p : potentialSels)
            {
                uint sel = p.first;
                PotentialSelector pi = p.second;
                if (pi.isProperty)
                {
                    if (cls->getProperty(sel) != nullptr)
                    {
                        continue;
                    }
                }

                if (pi.isMethod)
                {
                    if (cls->getMethod(sel) != nullptr)
                    {
                        continue;
                    }
                }

                m_classes.erase(m_classes.begin() + i--);
                break;
            }
        }
    }

    if (m_classes.empty())
    {
        intersectClasses(potentialSels);
    }
}


void MessageAnalyzer::gatherArgumentBaseClasses(Argument *arg, ArrayRef<Instruction *> insts)
{
    // If this is the "self" param.
    if (IsFirstArgument(arg))
    {
        Method *method = GetWorld().getProcedure(*arg->getParent())->asMethod();
        if (method != nullptr)
        {
            m_classes.push_back(&method->getClass());
        }
    }

    gatherValueBaseClasses(arg, insts);
}


void AnalyzeMessagePass::mutateSuper(CallInst *call)
{
    World &world = GetWorld();
    Function *callvFunc = world.getStub(Stub::callv);

    uint classId = static_cast<uint>(cast<ConstantInt>(call->getArgOperand(0))->getZExtValue());
    Class *super = Class::Get(classId);
    assert(super != nullptr);

    uint methodId = static_cast<uint>(cast<ConstantInt>(call->getArgOperand(1))->getZExtValue());
    Method *superMethod = super->getMethod(methodId);
    assert(superMethod->getFunction() != nullptr);

    Method *selfMethod = static_cast<Method *>(world.getProcedure(*call->getFunction()));
    assert(selfMethod->isMethod());

    Script &selfScript = selfMethod->getScript();
    Script &superScript = superMethod->getScript();

    Function *superFunc = superMethod->getFunction();
    if (selfScript.getId() != superScript.getId())
    {
        superFunc = cast<Function>(selfScript.getModule()->getOrInsertFunction(superFunc->getName(), superFunc->getFunctionType()));
    }

    SmallVector<Value *, 16> args;
    args.push_back(&*selfMethod->getFunction()->arg_begin()); // self

    if (superMethod->hasArgc())
    {
        args.push_back(call->getArgOperand(2));
    }
    for (auto i = call->arg_begin() + 3, e = call->arg_end(); i != e; ++i)
    {
        args.push_back(*i);
    }

    CallInst *newCall = CallInst::Create(superFunc, args, "", call);

    

    //TODO: handle "rest"
//     uint paramCount = static_cast<uint>(cast<ConstantInt>(call->getArgOperand(1))->getZExtValue());
//     args.resize(paramCount + 1);
//     args[0] = call->getArgOperand(0);
//     for (; paramCount != 0; --paramCount)
//     {
//         args[paramCount] = CallInst::Create(popFunc, "", call);
//     }
// 
//     CallInst *newCall = CallInst::Create(world.getStub(Stub::callv), args, "", call);
    call->replaceAllUsesWith(newCall);
    call->eraseFromParent();
}


END_NAMESPACE_SCI
