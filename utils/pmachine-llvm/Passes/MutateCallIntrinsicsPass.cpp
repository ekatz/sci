//===- Passes/MutateCallIntrinsicsPass.cpp --------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "MutateCallIntrinsicsPass.hpp"
#include "../Decl.hpp"
#include "../Method.hpp"
#include "../Object.hpp"
#include "../Script.hpp"
#include "../World.hpp"
#include "llvm/IR/IRBuilder.h"

using namespace sci;
using namespace llvm;

#define MSG_TYPE_METHOD (1U << 0)
#define MSG_TYPE_PROP_GET (1U << 1)
#define MSG_TYPE_PROP_SET (1U << 2)
#define MSG_TYPE_PROP (MSG_TYPE_PROP_GET | MSG_TYPE_PROP_SET)

const char *MutateCallIntrinsicsPass::KernelNames[] = {"KLoad",
                                                       "KUnLoad",
                                                       "KScriptID",
                                                       "KDisposeScript",
                                                       "KClone",
                                                       "KDisposeClone",
                                                       "KIsObject",
                                                       "KRespondsTo",
                                                       "KDrawPic",
                                                       "KShow",
                                                       "KPicNotValid",
                                                       "KAnimate",
                                                       "KSetNowSeen",
                                                       "KNumLoops",
                                                       "KNumCels",
                                                       "KCelWide",
                                                       "KCelHigh",
                                                       "KDrawCel",
                                                       "KAddToPic",
                                                       "KNewWindow",
                                                       "KGetPort",
                                                       "KSetPort",
                                                       "KDisposeWindow",
                                                       "KDrawControl",
                                                       "KHiliteControl",
                                                       "KEditControl",
                                                       "KTextSize",
                                                       "KDisplay",
                                                       "KGetEvent",
                                                       "KGlobalToLocal",
                                                       "KLocalToGlobal",
                                                       "KMapKeyToDir",
                                                       "KDrawMenuBar",
                                                       "KMenuSelect",
                                                       "KAddMenu",
                                                       "KDrawStatus",
                                                       "KParse",
                                                       "KSaid",
                                                       "KSetSynonyms",
                                                       "KHaveMouse",
                                                       "KSetCursor",
                                                       "KSaveGame",
                                                       "KRestoreGame",
                                                       "KRestartGame",
                                                       "KGameIsRestarting",
                                                       "KDoSound",
                                                       "KNewList",
                                                       "KDisposeList",
                                                       "KNewNode",
                                                       "KFirstNode",
                                                       "KLastNode",
                                                       "KEmptyList",
                                                       "KNextNode",
                                                       "KPrevNode",
                                                       "KNodeValue",
                                                       "KAddAfter",
                                                       "KAddToFront",
                                                       "KAddToEnd",
                                                       "KFindKey",
                                                       "KDeleteKey",
                                                       "KRandom",
                                                       "KAbs",
                                                       "KSqrt",
                                                       "KGetAngle",
                                                       "KGetDistance",
                                                       "KWait",
                                                       "KGetTime",
                                                       "KStrEnd",
                                                       "KStrCat",
                                                       "KStrCmp",
                                                       "KStrLen",
                                                       "KStrCpy",
                                                       "KFormat",
                                                       "KGetFarText",
                                                       "KReadNumber",
                                                       "KBaseSetter",
                                                       "KDirLoop",
                                                       "KCantBeHere",
                                                       "KOnControl",
                                                       "KInitBresen",
                                                       "KDoBresen",
                                                       "KDoAvoider",
                                                       "KSetJump",
                                                       "KSetDebug",
                                                       "KInspectObj",
                                                       "KShowSends",
                                                       "KShowObjs",
                                                       "KShowFree",
                                                       "KMemoryInfo",
                                                       "KStackUsage",
                                                       "KProfiler",
                                                       "KGetMenu",
                                                       "KSetMenu",
                                                       "KGetSaveFiles",
                                                       "KGetCWD",
                                                       "KCheckFreeSpace",
                                                       "KValidPath",
                                                       "KCoordPri",
                                                       "KStrAt",
                                                       "KDeviceInfo",
                                                       "KGetSaveDir",
                                                       "KCheckSaveGame",
                                                       "KShakeScreen",
                                                       "KFlushResources",
                                                       "KSinMult",
                                                       "KCosMult",
                                                       "KSinDiv",
                                                       "KCosDiv",
                                                       "KGraph",
                                                       "KJoystick",
                                                       "KShiftScreen",
                                                       "KPalette",
                                                       "KMemorySegment",
                                                       "KIntersections",
                                                       "KMemory",
                                                       "KListOps",
                                                       "KFileIO",
                                                       "KDoAudio",
                                                       "KDoSync",
                                                       "KAvoidPath",
                                                       "KSort",
                                                       "KATan",
                                                       "KLock",
                                                       "KStrSplit",
                                                       "KMessage",
                                                       "KIsItSkip"};

static unsigned getPotentialMessageType(const SendMessageInst *SendMsg) {
  unsigned MsgType = MSG_TYPE_METHOD;
  ConstantInt *C;

  C = dyn_cast<ConstantInt>(SendMsg->getSelector());
  if (C) {
    unsigned Sel = static_cast<unsigned>(C->getSExtValue());
    if (!(Sel == 0 || Sel == 1) &&
        GetWorld().getSelectorTable().getProperties(Sel).empty())
      return MSG_TYPE_METHOD;

    if (GetWorld().getSelectorTable().getMethods(Sel).empty())
      MsgType = 0;
  }

  C = cast<ConstantInt>(SendMsg->getArgCount());
  unsigned Argc = static_cast<unsigned>(C->getZExtValue());
  switch (Argc) {
  case 0:
    MsgType |= MSG_TYPE_PROP_GET;
    if (SendMsg->user_empty()) {
      assert((MsgType & MSG_TYPE_METHOD) != 0 &&
             "Unknown message destination!");
      return MSG_TYPE_METHOD;
    }
    break;

  case 1:
    MsgType |= MSG_TYPE_PROP_SET;
    if (!SendMsg->user_empty()) {
      if (SendMsg->hasOneUse()) {
        const Instruction *U = SendMsg->user_back();
        if (const StoreInst *Store = dyn_cast<StoreInst>(U)) {
          const AllocaInst *Alloc =
              dyn_cast<AllocaInst>(Store->getPointerOperand());
          if (Alloc && Alloc->getName().equals("acc.addr"))
            break;
        } else if (isa<ReturnInst>(U))
          break;
      }
      assert((MsgType & MSG_TYPE_METHOD) != 0 &&
             "Unknown message destination!");
      return MSG_TYPE_METHOD;
    }
    break;

  default:
    assert((MsgType & MSG_TYPE_METHOD) != 0 && "Unknown message destination!");
    return MSG_TYPE_METHOD;
  }
  return MsgType;
}

MutateCallIntrinsicsPass::MutateCallIntrinsicsPass()
    : SizeTy(GetWorld().getSizeType()),
      SizeBytesVal(ConstantInt::get(SizeTy, SizeTy->getBitWidth() / 8)),
      SizeAlign(GetWorld().getSizeTypeAlignment()),
      Int8PtrTy(Type::getInt8PtrTy(GetWorld().getContext())),
      Zero(ConstantInt::get(SizeTy, 0)), One(ConstantInt::get(SizeTy, 1)),
      AllOnes(cast<ConstantInt>(Constant::getAllOnesValue(SizeTy))),
      NullPtr(ConstantPointerNull::get(SizeTy->getPointerTo())) {}

MutateCallIntrinsicsPass::~MutateCallIntrinsicsPass() {}

void MutateCallIntrinsicsPass::run() {
  World &W = GetWorld();
  Function *Func;

  Func = W.getIntrinsic(Intrinsic::send);
  while (!Func->user_empty()) {
    SendMessageInst *Call = cast<SendMessageInst>(Func->user_back());
    mutateSendMessage(Call);
  }

  Func = W.getIntrinsic(Intrinsic::call);
  while (!Func->user_empty()) {
    CallInternalInst *call = cast<CallInternalInst>(Func->user_back());
    mutateCallInternal(call);
  }

  Func = W.getIntrinsic(Intrinsic::calle);
  while (!Func->user_empty()) {
    CallExternalInst *call = cast<CallExternalInst>(Func->user_back());
    mutateCallExternal(call);
  }

  Func = W.getIntrinsic(Intrinsic::callk);
  while (!Func->user_empty()) {
    CallKernelInst *call = cast<CallKernelInst>(Func->user_back());
    mutateCallKernel(call);
  }
}

void MutateCallIntrinsicsPass::mutateCallKernel(CallKernelInst *Callk) {
  unsigned KernelOrdinal =
      static_cast<unsigned>(Callk->getKernelOrdinal()->getZExtValue());
  Function *Func = getOrCreateKernelFunction(KernelOrdinal, Callk->getModule());

  Value *Restc, *Restv;
  extractRest(Callk, Restc, Restv);

  ConstantInt *ArgcVal = cast<ConstantInt>(Callk->getArgCount());
  CallInst *Call = delegateCall(Callk, Func, 0, Zero, AllOnes, Restc, Restv,
                                ArgcVal, Callk->arg_begin());

  Callk->replaceAllUsesWith(Call);
  Callk->eraseFromParent();
}

void MutateCallIntrinsicsPass::mutateCallInternal(CallInternalInst *Calli) {
  unsigned Offset = static_cast<unsigned>(Calli->getOffset()->getZExtValue());
  Script *S = GetWorld().getScript(*Calli->getModule());
  assert(S != nullptr && "Not a script module.");
  Procedure *Proc = S->getProcedure(Offset);
  assert(Proc != nullptr && "No procedure in the offset.");
  Function *Func = getFunctionDecl(Proc->getFunction(), Calli->getModule());

  Value *Restc, *Restv;
  extractRest(Calli, Restc, Restv);

  ConstantInt *ArgcVal = cast<ConstantInt>(Calli->getArgCount());
  CallInst *Call = delegateCall(Calli, Func, 0, Zero, AllOnes, Restc, Restv,
                                ArgcVal, Calli->arg_begin());

  Calli->replaceAllUsesWith(Call);
  Calli->eraseFromParent();
}

void MutateCallIntrinsicsPass::mutateCallExternal(CallExternalInst *Calle) {
  unsigned ScriptID =
      static_cast<unsigned>(Calle->getScriptID()->getZExtValue());
  unsigned EntryIndex =
      static_cast<unsigned>(Calle->getEntryIndex()->getZExtValue());
  Script *S = GetWorld().getScript(ScriptID);
  assert(S != nullptr && "Invalid script ID.");
  Function *Func = getFunctionDecl(
      cast<Function>(S->getExportedValue(EntryIndex)), Calle->getModule());

  Value *Restc, *Restv;
  extractRest(Calle, Restc, Restv);

  ConstantInt *ArgcVal = cast<ConstantInt>(Calle->getArgCount());
  CallInst *Call = delegateCall(Calle, Func, 0, Zero, AllOnes, Restc, Restv,
                                ArgcVal, Calle->arg_begin());

  Calle->replaceAllUsesWith(Call);
  Calle->eraseFromParent();
}

void MutateCallIntrinsicsPass::mutateSendMessage(SendMessageInst *SendMsg) {
  Value *Restc, *Restv;
  extractRest(SendMsg, Restc, Restv);

  CallInst *Call;
  unsigned MsgType = getPotentialMessageType(SendMsg);
  if ((MsgType & MSG_TYPE_METHOD) != 0) {
    Call =
        createMethodCall(SendMsg, (MsgType == MSG_TYPE_METHOD), Restc, Restv);
  } else {
    assert(Restc == Zero && Restv == NullPtr &&
           "Property cannot have a 'rest' param!");
    Call = createPropertyCall(SendMsg, (MsgType == MSG_TYPE_PROP_GET));
  }

  SendMsg->replaceAllUsesWith(Call);
  SendMsg->eraseFromParent();
}

void MutateCallIntrinsicsPass::extractRest(CallInst *Call, Value *&Restc,
                                           Value *&Restv) const {
  auto ILast = Call->arg_end() - 1;
  RestInst *Rest = dyn_cast<RestInst>(*ILast);
  if (Rest) {
    Function *ParentFunc = Call->getFunction();
    auto A = ParentFunc->arg_begin();
    if (ParentFunc->arg_size() != 1) {
      assert(ParentFunc->arg_begin()->getName().equals("self") &&
             "Function with more than one parameter that is not 'self'.");
      ++A;
    }

    Value *ParentArgs = &*A;
    Value *ParentArgc =
        GetElementPtrInst::CreateInBounds(ParentArgs, Zero, "", Rest);
    ParentArgc =
        new LoadInst(ParentArgc, "argc", false,
                     GetWorld().getElementTypeAlignment(ParentArgc), Rest);

    ConstantInt *Idx = ConstantInt::get(
        SizeTy, Rest->getParamIndex()->getZExtValue() - (uint64_t)1);
    Restc = BinaryOperator::CreateSub(ParentArgc, Idx, "rest.count", Rest);

    Restv = GetElementPtrInst::CreateInBounds(ParentArgs, Rest->getParamIndex(),
                                              "rest.args", Rest);

    Rest->replaceAllUsesWith(Restc);
    Rest->eraseFromParent();
  } else {
    Restc = Zero;
    Restv = NullPtr;
  }
}

CallInst *MutateCallIntrinsicsPass::createPropertyCall(SendMessageInst *SendMsg,
                                                       bool IsGetter) {
  assert(!isa<ConstantInt>(SendMsg->getObject()) &&
         "Property cannot be called for super!");

  Module *M = SendMsg->getModule();
  StringRef OldName = SendMsg->getName();
  std::string Name;
  if (OldName.startswith("res@")) {
    Name = "prop";
    Name += OldName.substr(3);
  } else
    Name = OldName;

  Function *Func;
  unsigned Argc = 2;
  Value *Args[3] = {SendMsg->getObject(), SendMsg->getSelector()};

  if (IsGetter) {
    Func = getGetPropertyFunction(M);
  } else {
    Args[Argc++] = SendMsg->getArgOperand(0);
    Func = getSetPropertyFunction(M);
  }

  return CallInst::Create(Func, makeArrayRef(Args, Argc), Name, SendMsg);
}

CallInst *MutateCallIntrinsicsPass::createMethodCall(SendMessageInst *SendMsg,
                                                     bool IsMethod,
                                                     Value *Restc,
                                                     Value *Restv) {
  if (isa<ConstantInt>(SendMsg->getObject()))
    return createSuperMethodCall(SendMsg, Restc, Restv);

  Module *M = SendMsg->getModule();
  ConstantInt *ArgcVal = cast<ConstantInt>(SendMsg->getArgCount());
  Function *Func =
      IsMethod ? getCallMethodFunction(M) : getSendMessageFunction(M);
  return delegateCall(SendMsg, Func, 2, SendMsg->getObject(),
                      SendMsg->getSelector(), Restc, Restv, ArgcVal,
                      SendMsg->arg_begin());
}

CallInst *
MutateCallIntrinsicsPass::createSuperMethodCall(SendMessageInst *SendMsg,
                                                Value *Restc, Value *Restv) {
  Module *M = SendMsg->getModule();
  ConstantInt *SuperIDVal = cast<ConstantInt>(SendMsg->getObject());
  ConstantInt *SelVal = cast<ConstantInt>(SendMsg->getSelector());
  unsigned SuperID = static_cast<unsigned>(SuperIDVal->getZExtValue());
  unsigned Sel = static_cast<unsigned>(SelVal->getSExtValue());

  Class *Super = GetWorld().getClass(SuperID);
  assert(Super != nullptr && "Invalid class ID.");
  Method *Mtd = Super->getMethod(Sel);
  assert(Mtd != nullptr && "Method selector not found!");
  Function *Func = getFunctionDecl(Mtd->getFunction(), M);

  Argument *Self = &*SendMsg->getFunction()->arg_begin();
  assert(Self->getType() == SizeTy &&
         "Call to 'super' is not within a method!");
  ConstantInt *ArgcVal = cast<ConstantInt>(SendMsg->getArgCount());
  return delegateCall(SendMsg, Func, 1, Self, SelVal, Restc, Restv, ArgcVal,
                      SendMsg->arg_begin());
}

CallInst *MutateCallIntrinsicsPass::delegateCall(
    CallInst *StubCall, Function *Func, unsigned Prefixc, Value *Self,
    Value *Sel, Value *Restc, Value *Restv, ConstantInt *ArgcVal,
    CallInst::op_iterator ArgI) {
  Module *M = StubCall->getModule();
  Function *DelegFunc = getOrCreateDelegator(ArgcVal, M);
  unsigned Argc = static_cast<unsigned>(ArgcVal->getZExtValue());
  unsigned ParamsCount = 1 /*func*/ + 1 /*prefixc*/ + 1 /*self*/ +
                         1 /*selector*/ + 1 /*restc*/ + 1 /*restv*/ + Argc;

  unsigned I = 0;
  Value **Args = static_cast<Value **>(alloca(ParamsCount * sizeof(Value *)));

  Value *FuncCast =
      CastInst::Create(Instruction::BitCast, Func, Int8PtrTy, "", StubCall);
  Args[I++] = FuncCast;
  Args[I++] = ConstantInt::get(SizeTy, Prefixc);
  Args[I++] = Self;
  Args[I++] = Sel;
  Args[I++] = Restc;
  Args[I++] = Restv;

  for (; I < ParamsCount; ++I, ++ArgI)
    Args[I] = *ArgI;

  CallInst *Call = CallInst::Create(DelegFunc, makeArrayRef(Args, ParamsCount),
                                    "", StubCall);
  Call->takeName(StubCall);
  return Call;
}

Function *MutateCallIntrinsicsPass::getOrCreateDelegator(ConstantInt *ArgcVal,
                                                         Module *M) {
  //
  // uintptr_t delegate@SCI@argc(uint8_t *func, uintptr_t prefixc, uintptr_t
  // self, uintptr_t sel, uintptr_t restc, uintptr_t *restv, ...)
  //

  unsigned Argc = static_cast<unsigned>(ArgcVal->getZExtValue());

  std::string DelegFuncName = "delegate@SCI@" + utostr(Argc);
  Function *DelegFunc = M->getFunction(DelegFuncName);
  if (DelegFunc != nullptr) {
    return DelegFunc;
  }

  World &W = GetWorld();
  LLVMContext &Ctx = W.getContext();
  unsigned ParamsCount = 1 /*func*/ + 1 /*prefixc*/ + 1 /*self*/ +
                         1 /*selector*/ + 1 /*restc*/ + 1 /*restv*/ + Argc;
  unsigned I = 0;
  Type **ParamTypes =
      static_cast<Type **>(alloca(ParamsCount * sizeof(Type *)));

  ParamTypes[I++] = Int8PtrTy;              // func
  ParamTypes[I++] = SizeTy;                 // prefixc
  ParamTypes[I++] = SizeTy;                 // self
  ParamTypes[I++] = SizeTy;                 // selector
  ParamTypes[I++] = SizeTy;                 // restc
  ParamTypes[I++] = SizeTy->getPointerTo(); // restv
  for (; I < ParamsCount; ++I)
    ParamTypes[I] = SizeTy;

  FunctionType *FuncTy =
      FunctionType::get(SizeTy, makeArrayRef(ParamTypes, ParamsCount), false);
  DelegFunc =
      Function::Create(FuncTy, GlobalValue::PrivateLinkage, DelegFuncName, M);
  DelegFunc->addFnAttr(Attribute::AlwaysInline);

  auto Params = DelegFunc->arg_begin();
  Argument *Func = &*Params;
  ++Params;
  Argument *Prefixc = &*Params;
  ++Params;
  Argument *Self = &*Params;
  ++Params;
  Argument *Sel = &*Params;
  ++Params;
  Argument *Restc = &*Params;
  ++Params;
  Argument *Restv = &*Params;
  ++Params;

  BasicBlock *BB = BasicBlock::Create(Ctx, "entry", DelegFunc);

  // Add code to check if 'restc' is less than 0, then set to 0.
  ICmpInst *Cmp = new ICmpInst(*BB, CmpInst::ICMP_SGT, Restc, Zero);
  SelectInst::Create(Cmp, Restc, Zero, "", BB);

  ConstantInt *ArgcTotal = ConstantInt::get(SizeTy, Argc + 1);
  Value *ArgsLen = BinaryOperator::CreateAdd(Restc, ArgcTotal, "argsLen", BB);

  Value *Args = new AllocaInst(SizeTy, 0, ArgsLen, SizeAlign, "args", BB);

  Value *Total = BinaryOperator::CreateAdd(Restc, ArgcVal, "argc", BB);
  new StoreInst(Total, Args, false, SizeAlign, BB);

  for (I = 1; I <= Argc; ++I) {
    Value *param = &*Params;
    ++Params;

    ConstantInt *C = ConstantInt::get(SizeTy, I);
    Value *ArgSlot = GetElementPtrInst::CreateInBounds(Args, C, "", BB);
    new StoreInst(param, ArgSlot, false, SizeAlign, BB);
  }

  Value *ArgRest = GetElementPtrInst::CreateInBounds(Args, ArgcTotal, "", BB);
  Value *RestcBytes =
      BinaryOperator::CreateMul(Restc, SizeBytesVal, "restcBytes", BB);

  IRBuilder<>(BB).CreateMemCpy(ArgRest, SizeAlign, Restv, SizeAlign,
                               RestcBytes);

  BasicBlock *SwitchBBs[3];
  SwitchBBs[2] = BasicBlock::Create(Ctx, "case.method", DelegFunc);
  SwitchBBs[1] = BasicBlock::Create(Ctx, "case.super", DelegFunc);
  SwitchBBs[0] = BasicBlock::Create(Ctx, "case.proc", DelegFunc);

  SwitchInst *switchPrefix = SwitchInst::Create(Prefixc, SwitchBBs[2], 2, BB);
  switchPrefix->addCase(One, SwitchBBs[1]);
  switchPrefix->addCase(Zero, SwitchBBs[0]);

  Value *CallArgs[3];
  for (I = 0; I < 3; ++I) {
    if (I >= 1) {
      ParamTypes[0] = SizeTy;
      CallArgs[0] = Self;

      if (I >= 2) {
        ParamTypes[1] = SizeTy;
        CallArgs[1] = Sel;
      }
    }

    ParamTypes[I] = SizeTy->getPointerTo();
    CallArgs[I] = Args;

    BasicBlock *SwitchBB = SwitchBBs[I];
    FuncTy = FunctionType::get(SizeTy, makeArrayRef(ParamTypes, I + 1), false);
    Value *FuncCast = CastInst::Create(Instruction::BitCast, Func,
                                       FuncTy->getPointerTo(), "", SwitchBB);

    CallInst *Call =
        CallInst::Create(FuncCast, makeArrayRef(CallArgs, I + 1), "", SwitchBB);
    ReturnInst::Create(Ctx, Call, SwitchBB);
  }

  return DelegFunc;
}

Function *MutateCallIntrinsicsPass::getSendMessageFunction(Module *M) {
  //
  // uintptr_t SendMessage(uintptr_t self, uintptr_t sel, uintptr_t *args)
  //

  StringRef Name = "SendMessage";
  Function *Func = M->getFunction(Name);
  if (Func == nullptr) {
    Type *Params[] = {
        SizeTy,                // self
        SizeTy,                // sel
        SizeTy->getPointerTo() // args
    };
    FunctionType *FTy = FunctionType::get(SizeTy, Params, false);
    Func = Function::Create(FTy, GlobalValue::ExternalLinkage, Name, M);
  }
  return Func;
}

Function *MutateCallIntrinsicsPass::getGetPropertyFunction(Module *M) {
  //
  // uintptr_t GetProperty(uintptr_t self, uintptr_t sel)
  //

  StringRef Name = "GetProperty";
  Function *Func = M->getFunction(Name);
  if (Func == nullptr) {
    Type *Params[] = {
        SizeTy, // self
        SizeTy  // sel
    };
    FunctionType *FTy = FunctionType::get(SizeTy, Params, false);
    Func = Function::Create(FTy, GlobalValue::ExternalLinkage, Name, M);
  }
  return Func;
}

Function *MutateCallIntrinsicsPass::getSetPropertyFunction(Module *M) {
  //
  // uintptr_t SetProperty(uintptr_t self, uintptr_t sel, uintptr_t val)
  //

  StringRef Name = "SetProperty";
  Function *Func = M->getFunction(Name);
  if (Func == nullptr) {
    Type *Params[] = {
        SizeTy, // self
        SizeTy, // sel
        SizeTy  // val
    };
    FunctionType *FTy = FunctionType::get(SizeTy, Params, false);
    Func = Function::Create(FTy, GlobalValue::ExternalLinkage, Name, M);
  }
  return Func;
}

Function *MutateCallIntrinsicsPass::getCallMethodFunction(Module *M) {
  //
  // uintptr_t CallMethod(uintptr_t self, uintptr_t sel, uintptr_t *args)
  //

  StringRef Name = "CallMethod";
  Function *Func = M->getFunction(Name);
  if (Func == nullptr) {
    Type *Params[] = {
        SizeTy,                // self
        SizeTy,                // sel
        SizeTy->getPointerTo() // args
    };
    FunctionType *FTy = FunctionType::get(SizeTy, Params, false);
    Func = Function::Create(FTy, GlobalValue::ExternalLinkage, Name, M);
  }
  return Func;
}

Function *MutateCallIntrinsicsPass::getOrCreateKernelFunction(unsigned ID,
                                                              Module *M) {
  if (ID >= array_lengthof(KernelNames)) {
    return nullptr;
  }

  Function *Func = M->getFunction(KernelNames[ID]);
  if (Func == nullptr) {
    FunctionType *FTy =
        FunctionType::get(SizeTy, SizeTy->getPointerTo(), false);
    Func =
        Function::Create(FTy, GlobalValue::ExternalLinkage, KernelNames[ID], M);
    Func->arg_begin()->setName("args");
  }
  return Func;
}
