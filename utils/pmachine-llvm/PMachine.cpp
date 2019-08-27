//===- PMachine.cpp -------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "PMachine.hpp"
#include "Class.hpp"
#include "Property.hpp"
#include "Resource.hpp"
#include "World.hpp"

using namespace sci;
using namespace llvm;


#define PSTACKSIZE (10 * 1024)


#define OP_LDST     0x80  // load/store if set
#define OP_BYTE     0x01  // byte operation if set, word otw

#define OP_TYPE     0x60  // mask for operation type
#define OP_LOAD     0x00  // load
#define OP_STORE    0x20  // store
#define OP_INC      0x40  // increment operation
#define OP_DEC      0x60  // decrement operation

#define OP_INDEX    0x10  // indexed op if set, non-indexed otw

#define OP_STACK    0x08  // load to stack if set

#define OP_VAR      0x06  // mask for var type
#define OP_GLOBAL   0x00  // global var
#define OP_LOCAL    0x02  // local var
#define OP_TMP      0x04  // temporary var (on the stack)
#define OP_PARM     0x06  // parameter (different stack frame than tmp)



#define OP_bnot         0x00
//      BadOp           0x01
#define OP_add          0x02
//      BadOp           0x03
#define OP_sub          0x04
//      BadOp           0x05
#define OP_mul          0x06
//      BadOp           0x07
#define OP_div          0x08
//      BadOp           0x09
#define OP_mod          0x0A
//      BadOp           0x0B
#define OP_shr          0x0C
//      BadOp           0x0D
#define OP_shl          0x0E
//      BadOp           0x0F
#define OP_xor          0x10
//      BadOp           0x11
#define OP_and          0x12
//      BadOp           0x13
#define OP_or           0x14
//      BadOp           0x15
#define OP_neg          0x16
//      BadOp           0x17
#define OP_not          0x18
//      BadOp           0x19
#define OP_eq           0x1A
//      BadOp           0x1B
#define OP_ne           0x1C
//      BadOp           0x1D
#define OP_gt           0x1E
//      BadOp           0x1F
#define OP_ge           0x20
//      BadOp           0x21
#define OP_lt           0x22
//      BadOp           0x23
#define OP_le           0x24
//      BadOp           0x25
#define OP_ugt          0x26
//      BadOp           0x27
#define OP_uge          0x28
//      BadOp           0x29
#define OP_ult          0x2A
//      BadOp           0x2B
#define OP_ule          0x2C
//      BadOp           0x2D
#define OP_bt_TWO       0x2E
#define OP_bt_ONE       0x2F
#define OP_bnt_TWO      0x30
#define OP_bnt_ONE      0x31
#define OP_jmp_TWO      0x32
#define OP_jmp_ONE      0x33
#define OP_loadi_TWO    0x34
#define OP_loadi_ONE    0x35
#define OP_push         0x36
//      BadOp           0x37
#define OP_pushi_TWO    0x38
#define OP_pushi_ONE    0x39
#define OP_toss         0x3A
//      BadOp           0x3B
#define OP_dup          0x3C
//      BadOp           0x3D
#define OP_link_TWO     0x3E
#define OP_link_ONE     0x3F
#define OP_call_THREE   0x40
#define OP_call_TWO     0x41
#define OP_callk_THREE  0x42
#define OP_callk_TWO    0x43
#define OP_callb_THREE  0x44
#define OP_callb_TWO    0x45
#define OP_calle_FOUR   0x46
#define OP_calle_TWO    0x47
#define OP_ret          0x48
//      BadOp           0x49
#define OP_send_ONE     0x4A
//      BadOp           0x4B
//      BadOp           0x4C
//      BadOp           0x4D
//      BadOp           0x4E
//      BadOp           0x4F
#define OP_class_TWO    0x50
#define OP_class_ONE    0x51
//      BadOp           0x52
//      BadOp           0x53
#define OP_self_TWO     0x54
#define OP_self_ONE     0x55
#define OP_super_THREE  0x56
#define OP_super_TWO    0x57
//      BadOp           0x58
#define OP_rest_ONE     0x59
#define OP_lea_FOUR     0x5A
#define OP_lea_TWO      0x5B
#define OP_selfID       0x5C
//      BadOp           0x5D
//      BadOp           0x5E
//      BadOp           0x5F
#define OP_pprev        0x60
//      BadOp           0x61
#define OP_pToa_TWO     0x62
#define OP_pToa_ONE     0x63
#define OP_aTop_TWO     0x64
#define OP_aTop_ONE     0x65
#define OP_pTos_TWO     0x66
#define OP_pTos_ONE     0x67
#define OP_sTop_TWO     0x68
#define OP_sTop_ONE     0x69
#define OP_ipToa_TWO    0x6A
#define OP_ipToa_ONE    0x6B
#define OP_dpToa_TWO    0x6C
#define OP_dpToa_ONE    0x6D
#define OP_ipTos_TWO    0x6E
#define OP_ipTos_ONE    0x6F
#define OP_dpTos_TWO    0x70
#define OP_dpTos_ONE    0x71
#define OP_lofsa_TWO    0x72
//      BadOp           0x73
#define OP_lofss_TWO    0x74
//      BadOp           0x75
#define OP_push0        0x76
//      BadOp           0x77
#define OP_push1        0x78
//      BadOp           0x79
#define OP_push2        0x7A
//      BadOp           0x7B
#define OP_pushSelf     0x7C
//      BadOp           0x7B
//      BadOp           0x7B
//      BadOp           0x7F
#define OP_lag_TWO      0x80
#define OP_lag_ONE      0x81
#define OP_lal_TWO      0x82
#define OP_lal_ONE      0x83
#define OP_lat_TWO      0x84
#define OP_lat_ONE      0x85
#define OP_lap_TWO      0x86
#define OP_lap_ONE      0x87
#define OP_lsg_TWO      0x88
#define OP_lsg_ONE      0x89
#define OP_lsl_TWO      0x8A
#define OP_lsl_ONE      0x8B
#define OP_lst_TWO      0x8C
#define OP_lst_ONE      0x8D
#define OP_lsp_TWO      0x8E
#define OP_lsp_ONE      0x8F
#define OP_lagi_TWO     0x90
#define OP_lagi_ONE     0x91
#define OP_lali_TWO     0x92
#define OP_lali_ONE     0x93
#define OP_lati_TWO     0x94
#define OP_lati_ONE     0x95
#define OP_lapi_TWO     0x96
#define OP_lapi_ONE     0x97
#define OP_lsgi_TWO     0x98
#define OP_lsgi_ONE     0x99
#define OP_lsli_TWO     0x9A
#define OP_lsli_ONE     0x9B
#define OP_lsti_TWO     0x9C
#define OP_lsti_ONE     0x9D
#define OP_lspi_TWO     0x9E
#define OP_lspi_ONE     0x9F
#define OP_sag_TWO      0xA0
#define OP_sag_ONE      0xA1
#define OP_sal_TWO      0xA2
#define OP_sal_ONE      0xA3
#define OP_sat_TWO      0xA4
#define OP_sat_ONE      0xA5
#define OP_sap_TWO      0xA6
#define OP_sap_ONE      0xA7
#define OP_ssg_TWO      0xA8
#define OP_ssg_ONE      0xA9
#define OP_ssl_TWO      0xAA
#define OP_ssl_ONE      0xAB
#define OP_sst_TWO      0xAC
#define OP_sst_ONE      0xAD
#define OP_ssp_TWO      0xAE
#define OP_ssp_ONE      0xAF
#define OP_sagi_TWO     0xB0
#define OP_sagi_ONE     0xB1
#define OP_sali_TWO     0xB2
#define OP_sali_ONE     0xB3
#define OP_sati_TWO     0xB4
#define OP_sati_ONE     0xB5
#define OP_sapi_TWO     0xB6
#define OP_sapi_ONE     0xB7
#define OP_ssgi_TWO     0xB8
#define OP_ssgi_ONE     0xB9
#define OP_ssli_TWO     0xBA
#define OP_ssli_ONE     0xBB
#define OP_ssti_TWO     0xBC
#define OP_ssti_ONE     0xBD
#define OP_sspi_TWO     0xBE
#define OP_sspi_ONE     0xBF
#define OP_iag_TWO      0xC0
#define OP_iag_ONE      0xC1
#define OP_ial_TWO      0xC2
#define OP_ial_ONE      0xC3
#define OP_iat_TWO      0xC4
#define OP_iat_ONE      0xC5
#define OP_iap_TWO      0xC6
#define OP_iap_ONE      0xC7
#define OP_isg_TWO      0xC8
#define OP_isg_ONE      0xC9
#define OP_isl_TWO      0xCA
#define OP_isl_ONE      0xCB
#define OP_ist_TWO      0xCC
#define OP_ist_ONE      0xCD
#define OP_isp_TWO      0xCE
#define OP_isp_ONE      0xCF
#define OP_iagi_TWO     0xD0
#define OP_iagi_ONE     0xD1
#define OP_iali_TWO     0xD2
#define OP_iali_ONE     0xD3
#define OP_iati_TWO     0xD4
#define OP_iati_ONE     0xD5
#define OP_iapi_TWO     0xD6
#define OP_iapi_ONE     0xD7
#define OP_isgi_TWO     0xD8
#define OP_isgi_ONE     0xD9
#define OP_isli_TWO     0xDA
#define OP_isli_ONE     0xDB
#define OP_isti_TWO     0xDC
#define OP_isti_ONE     0xDD
#define OP_ispi_TWO     0xDE
#define OP_ispi_ONE     0xDF
#define OP_dag_TWO      0xE0
#define OP_dag_ONE      0xE1
#define OP_dal_TWO      0xE2
#define OP_dal_ONE      0xE3
#define OP_dat_TWO      0xE4
#define OP_dat_ONE      0xE5
#define OP_dap_TWO      0xE6
#define OP_dap_ONE      0xE7
#define OP_dsg_TWO      0xE8
#define OP_dsg_ONE      0xE9
#define OP_dsl_TWO      0xEA
#define OP_dsl_ONE      0xEB
#define OP_dst_TWO      0xEC
#define OP_dst_ONE      0xED
#define OP_dsp_TWO      0xEE
#define OP_dsp_ONE      0xEF
#define OP_dagi_TWO     0xF0
#define OP_dagi_ONE     0xF1
#define OP_dali_TWO     0xF2
#define OP_dali_ONE     0xF3
#define OP_dati_TWO     0xF4
#define OP_dati_ONE     0xF5
#define OP_dapi_TWO     0xF6
#define OP_dapi_ONE     0xF7
#define OP_dsgi_TWO     0xF8
#define OP_dsgi_ONE     0xF9
#define OP_dsli_TWO     0xFA
#define OP_dsli_ONE     0xFB
#define OP_dsti_TWO     0xFC
#define OP_dsti_ONE     0xFD
#define OP_dspi_TWO     0xFE
#define OP_dspi_ONE     0xFF


PMachine::PfnOp PMachine::OpTable[] = {
    &PMachine::bnotOp,
    &PMachine::addOp,
    &PMachine::subOp,
    &PMachine::mulOp,
    &PMachine::divOp,
    &PMachine::modOp,
    &PMachine::shrOp,
    &PMachine::shlOp,
    &PMachine::xorOp,
    &PMachine::andOp,
    &PMachine::orOp,
    &PMachine::negOp,
    &PMachine::notOp,
    &PMachine::cmpOp,
    &PMachine::cmpOp,
    &PMachine::cmpOp,
    &PMachine::cmpOp,
    &PMachine::cmpOp,
    &PMachine::cmpOp,
    &PMachine::cmpOp,
    &PMachine::cmpOp,
    &PMachine::cmpOp,
    &PMachine::cmpOp,
    &PMachine::btOp,
    &PMachine::btOp,
    &PMachine::jmpOp,
    &PMachine::loadiOp,
    &PMachine::pushOp,
    &PMachine::pushiOp,
    &PMachine::tossOp,
    &PMachine::dupOp,
    &PMachine::linkOp,
    &PMachine::callOp,
    &PMachine::callkOp,
    &PMachine::callbOp,
    &PMachine::calleOp,
    &PMachine::retOp,
    &PMachine::sendOp,
    &PMachine::badOp,
    &PMachine::badOp,
    &PMachine::classOp,
    &PMachine::badOp,
    &PMachine::selfOp,
    &PMachine::superOp,
    &PMachine::restOp,
    &PMachine::leaOp,
    &PMachine::selfIdOp,
    &PMachine::badOp,
    &PMachine::pprevOp,
    &PMachine::pToaOp,
    &PMachine::aTopOp,
    &PMachine::pTosOp,
    &PMachine::sTopOp,
    &PMachine::ipToaOp,
    &PMachine::dpToaOp,
    &PMachine::ipTosOp,
    &PMachine::dpTosOp,
    &PMachine::lofsaOp,
    &PMachine::lofssOp,
    &PMachine::push0Op,
    &PMachine::push1Op,
    &PMachine::push2Op,
    &PMachine::pushSelfOp,
    &PMachine::badOp
};

PMachine::PMachine(Script &S)
    : TheScript(S), Context(S.getModule()->getContext()),
      SizeTy(GetWorld().getSizeType()) {
  Intrinsic::Get(Intrinsic::push);
  Intrinsic::Get(Intrinsic::pop);
  Intrinsic::Get(Intrinsic::rest);
  Intrinsic::Get(Intrinsic::clss);
  Intrinsic::Get(Intrinsic::objc);
  Intrinsic::Get(Intrinsic::prop);
  Intrinsic::Get(Intrinsic::send);
  Intrinsic::Get(Intrinsic::call);
  Intrinsic::Get(Intrinsic::calle);
  Intrinsic::Get(Intrinsic::callk);
}

Function *PMachine::interpretFunction(const uint8_t *Code, StringRef Name,
                                      unsigned ID, Class *Cls, bool Debug) {
  ThisClass = Cls;
  Acc = nullptr;
  AccAddr = nullptr;
  PAccAddr = nullptr;
  Temp = nullptr;
  TempCount = 0;
  Self.release();
  Args.release();

  if (ID != -1U)
    Self.reset(new Argument(SizeTy, "self"));
  Args.reset(new Argument(SizeTy->getPointerTo(), "args"));

  Entry = getBasicBlock(Code, "entry");
  processBasicBlocks();

  if (!RetTy)
    RetTy = Type::getVoidTy(Context);

  if (AccAddr && AccAddr->use_empty())
    AccAddr->eraseFromParent();
  if (PAccAddr && PAccAddr->use_empty())
    PAccAddr->eraseFromParent();

  FunctionType *FTy;
  if (Self) {
    Type *params[] = {Self->getType(), Args->getType()};
    FTy = FunctionType::get(RetTy, params, false);
  } else {
    Type *params[] = {Args->getType()};
    FTy = FunctionType::get(RetTy, params, false);
  }
  Function *Func =
      Function::Create(FTy, GlobalValue::InternalLinkage, Name, getModule());

  auto AI = Func->arg_begin();
  if (Self) {
    Self->replaceAllUsesWith(&*AI);
    AI->takeName(Self.get());
    ++AI;
  }
  Args->replaceAllUsesWith(&*AI);
  AI->takeName(Args.get());

  for (auto I = Labels.begin(), E = Labels.end(); I != E; ++I)
    I->second->insertInto(Func);

  if (Debug)
    emitDebugLog();
  return Func;
}

void PMachine::processBasicBlocks() {
  assert(WorkList.size() == 1);

  auto Item = WorkList.back();
  WorkList.pop_back();
  PC = Item.first;
  CurrBlock = Item.second;

  Rest = nullptr;
  unsigned SizeAlign = GetWorld().getSizeTypeAlignment();
  AccAddr = new AllocaInst(SizeTy, 0, "acc.addr", CurrBlock);
  AccAddr->setAlignment(SizeAlign);
  PAccAddr = new AllocaInst(SizeTy, 0, "prev.addr", CurrBlock);
  PAccAddr->setAlignment(SizeAlign);
  while (processNextInstruction())
    ;
  assert(Rest == nullptr);

  while (!WorkList.empty()) {
    Item = WorkList.back();
    WorkList.pop_back();
    PC = Item.first;
    CurrBlock = Item.second;

    Rest = nullptr;
    Acc = loadAcc();
    while (processNextInstruction())
      ;
    assert(Rest == nullptr);
  }
}

BasicBlock *PMachine::getBasicBlock(const uint8_t *Label, StringRef Name) {
  unsigned Offset = TheScript.getOffsetOf(Label);
  BasicBlock *&BB = Labels[Offset];
  if (BB == nullptr) {
    std::string FullName = Name;
    FullName += '@';
    FullName += utohexstr(Offset, true);
    BB = BasicBlock::Create(Context, FullName);
    WorkList.push_back(std::make_pair(Label, BB));
  }
  return BB;
}

Function *PMachine::getCallIntrinsic() {
  if (FuncCallIntrin == nullptr) {
    Function *FuncCallIntrin = Intrinsic::Get(Intrinsic::call);
    FuncCallIntrin = cast<Function>(
        TheScript.getModule()
            ->getOrInsertFunction(FuncCallIntrin->getName(),
                                  FuncCallIntrin->getFunctionType())
            .getCallee());
  }
  return FuncCallIntrin;
}

void PMachine::emitSend(Value *Obj) {
  unsigned ParamCount = getByte() / sizeof(uint16_t);
  if (!ParamCount)
    return;

  SmallVector<Value *, 16> MsgArgs;

  unsigned restParam = Rest ? 1 : 0;
  MsgArgs.resize(ParamCount + 1 + restParam);
  MsgArgs[0] = Obj;
  for (; ParamCount != 0; --ParamCount)
    MsgArgs[ParamCount] = PopInst::Create(CurrBlock);

  if (Rest) {
    MsgArgs.back() = Rest;
    Rest = nullptr;
  }

  Acc = SendMessageInst::Create(MsgArgs, CurrBlock);
}

void PMachine::emitCall(Function *Func, ArrayRef<Constant *> Constants) {
  unsigned ParamCount = getByte() / sizeof(uint16_t);

  SmallVector<Value *, 16> CallArgs;

  unsigned RestParam = Rest ? 1 : 0;
  unsigned ConstCount = Constants.size();
  CallArgs.resize(ParamCount + 1 + ConstCount + RestParam);

  for (unsigned I = 0; I < ConstCount; ++I)
    CallArgs[I] = Constants[I];
  for (ParamCount += ConstCount; ParamCount >= ConstCount; --ParamCount)
    CallArgs[ParamCount] = PopInst::Create(CurrBlock);

  if (Rest) {
    CallArgs.back() = Rest;
    Rest = nullptr;
  }

  Acc = CallInst::Create(Func, CallArgs, "", CurrBlock);
}

Value *PMachine::getIndexedPropPtr(uint8_t Opcode) {
  unsigned Idx = getUInt(Opcode) / sizeof(uint16_t);
  Type *Int32Ty = Type::getInt32Ty(Context);

  // Create indices into the Class Abstract Type.
  unsigned Sel;
  unsigned IdxCount = 0;
  Value *Indices[3];
  Indices[IdxCount++] = ConstantInt::get(Int32Ty, 0);
  if (Idx == ObjRes::SPECIES_OFFSET) {
    Indices[IdxCount++] = ConstantInt::get(Int32Ty, 0);
    Sel = 0;
  } else {
    assert(Idx != ObjRes::SUPER_OFFSET);
    Idx -= ObjRes::INFO_OFFSET;
    Indices[IdxCount++] = ConstantInt::get(Int32Ty, 1);
    Indices[IdxCount++] = ConstantInt::get(SizeTy, Idx);
    Sel = ThisClass->getProperties()[Idx].getSelector();
  }

  Value *Ptr = new IntToPtrInst(
      Self.get(), Class::GetAbstractType()->getPointerTo(), "", CurrBlock);
  std::string Name = "prop";
  Name += '@';
  Name += GetWorld().getSelectorName(Sel);
  return GetElementPtrInst::CreateInBounds(Ptr, makeArrayRef(Indices, IdxCount),
                                           Name, CurrBlock);
}

Value *PMachine::getValueByOffset(uint8_t Opcode) {
#if defined __WINDOWS__ || 1
  unsigned Offset = getUInt(Opcode);
#else
#error Not implemented
#endif

  Value *Val = TheScript.getRelocatedValue(Offset);
  if (Val)
    if (Instruction *Inst = dyn_cast<Instruction>(Val)) {
      Inst = Inst->clone();
      CurrBlock->getInstList().push_back(Inst);
      Val = Inst;
    }
  return Val;
}

Value *PMachine::getIndexedVariablePtr(uint8_t Opcode, unsigned Idx) {
  if ((Opcode & OP_INDEX) != 0) {
    castAccToSizeType();
  }

  Value *Var;
  switch (Opcode & OP_VAR) {
  default:
  case OP_GLOBAL:
    Var = TheScript.getGlobalVariable(Idx);
    break;

  case OP_LOCAL:
    Var = TheScript.getLocalVariable(Idx);
    break;

  case OP_TMP: {
    assert(Idx < TempCount);
    Value *Indices[] = {ConstantInt::get(SizeTy, 0),
                        ConstantInt::get(SizeTy, Idx)};
    Var = GetElementPtrInst::CreateInBounds(Temp, Indices);
    break;
  }

  case OP_PARM: {
    ConstantInt *C = ConstantInt::get(SizeTy, Idx);
    Var = GetElementPtrInst::CreateInBounds(Args.get(), C);
    break;
  }
  }

  CurrBlock->getInstList().push_back(static_cast<Instruction *>(Var));

  // If the variable is indexed, add the index in acc to the offset.
  if ((Opcode & OP_INDEX) != 0)
    Var = GetElementPtrInst::CreateInBounds(Var, Acc, "", CurrBlock);
  return Var;
}

bool PMachine::processNextInstruction() {
  assert(PC != nullptr);
  assert(CurrBlock != nullptr);

  uint8_t Opcode = getByte();
  if ((Opcode & 0x80) != 0) {
    return ldstOp(Opcode);
  }
  return (this->*OpTable[Opcode >> 1])(Opcode);
}

Value *PMachine::castValueToSizeType(Value *Val, BasicBlock *BB) {
  if (Val) {
    if (Val->getType()->isIntegerTy()) {
      unsigned SrcBits = Val->getType()->getScalarSizeInBits();
      unsigned DstBits = SizeTy->getScalarSizeInBits();
      if (SrcBits != DstBits)
        Val = new ZExtInst(Val, SizeTy, "", BB);
    } else if (Val->getType()->isPointerTy())
      Val = new PtrToIntInst(Val, SizeTy, "", BB);
  }
  return Val;
}

Value *PMachine::castValueToSizeType(Value *Val) {
  return castValueToSizeType(Val, CurrBlock);
}

void PMachine::castAccToSizeType() { Acc = castValueToSizeType(Acc); }

void PMachine::fixAccConstantInt() {
  if (isa<ConstantInt>(Acc)) {
    int16_t V = static_cast<int16_t>(cast<ConstantInt>(Acc)->getZExtValue());
    if (V < 0) {
      Acc = ConstantInt::get(SizeTy, static_cast<uint16_t>(V));
    }
  }
}

void PMachine::storeAcc() {
  Value *Val = castValueToSizeType(Acc);
  new StoreInst(Val, AccAddr, false, GetWorld().getTypeAlignment(Val),
                CurrBlock);
}

Value *PMachine::loadAcc() {
  return new LoadInst(AccAddr, "", false,
                      GetWorld().getTypeAlignment(AccAddr->getAllocatedType()),
                      CurrBlock);
}

void PMachine::storePrevAcc() {
  Value *Val = castValueToSizeType(Acc);
  new StoreInst(Val, PAccAddr, false, GetWorld().getTypeAlignment(Val),
                CurrBlock);
}

Value *PMachine::loadPrevAcc() {
  return new LoadInst(PAccAddr, "", false,
                      GetWorld().getTypeAlignment(PAccAddr->getAllocatedType()),
                      CurrBlock);
}

Value *PMachine::callPop() { return PopInst::Create(CurrBlock); }

void PMachine::callPush(llvm::Value *Val) { PushInst::Create(Val, CurrBlock); }

bool PMachine::bnotOp(uint8_t Opcode) {
  castAccToSizeType();
  fixAccConstantInt();
  Acc = BinaryOperator::CreateNot(Acc, "", CurrBlock);
  return true;
}

bool PMachine::addOp(uint8_t Opcode) {
  castAccToSizeType();
  Value *Top = callPop();
  Acc = BinaryOperator::CreateAdd(Top, Acc, "", CurrBlock);
  return true;
}

bool PMachine::subOp(uint8_t Opcode) {
  castAccToSizeType();
  Value *Top = callPop();
  Acc = BinaryOperator::CreateSub(Top, Acc, "", CurrBlock);
  return true;
}

bool PMachine::mulOp(uint8_t Opcode) {
  castAccToSizeType();
  Value *Top = callPop();
  Acc = BinaryOperator::CreateMul(Top, Acc, "", CurrBlock);
  return true;
}

bool PMachine::divOp(uint8_t Opcode) {
  castAccToSizeType();
  Value *Top = callPop();
  Acc = BinaryOperator::CreateSDiv(Top, Acc, "", CurrBlock);
  return true;
}

bool PMachine::modOp(uint8_t Opcode) {
  castAccToSizeType();
  Value *Top = callPop();
  Acc = BinaryOperator::CreateSRem(Top, Acc, "", CurrBlock);
  return true;
}

bool PMachine::shrOp(uint8_t Opcode) {
  castAccToSizeType();
  fixAccConstantInt();
  Value *Top = callPop();
  Acc = BinaryOperator::CreateLShr(Top, Acc, "", CurrBlock);
  return true;
}

bool PMachine::shlOp(uint8_t Opcode) {
  castAccToSizeType();
  fixAccConstantInt();
  Value *Top = callPop();
  Acc = BinaryOperator::CreateShl(Top, Acc, "", CurrBlock);
  return true;
}

bool PMachine::xorOp(uint8_t Opcode) {
  castAccToSizeType();
  fixAccConstantInt();
  Value *Top = callPop();
  Acc = BinaryOperator::CreateXor(Top, Acc, "", CurrBlock);
  return true;
}

bool PMachine::andOp(uint8_t Opcode) {
  castAccToSizeType();
  fixAccConstantInt();
  Value *Top = callPop();
  Acc = BinaryOperator::CreateAnd(Top, Acc, "", CurrBlock);
  return true;
}

bool PMachine::orOp(uint8_t Opcode) {
  castAccToSizeType();
  fixAccConstantInt();
  Value *Top = callPop();
  Acc = BinaryOperator::CreateOr(Top, Acc, "", CurrBlock);
  return true;
}

bool PMachine::negOp(uint8_t Opcode) {
  castAccToSizeType();
  Acc = BinaryOperator::CreateNeg(Acc, "", CurrBlock);
  return true;
}

bool PMachine::notOp(uint8_t Opcode) {
  if (Acc->getType()->isIntegerTy(1)) {
    Acc = BinaryOperator::CreateNot(Acc, "not", CurrBlock);
  } else {
    Acc = new ICmpInst(*CurrBlock, CmpInst::ICMP_EQ, Acc,
                       ConstantInt::get(Acc->getType(), 0));
  }
  return true;
}

bool PMachine::cmpOp(uint8_t Opcode) {
  static CmpInst::Predicate CmpTable[] = {CmpInst::ICMP_EQ,  CmpInst::ICMP_NE,
                                          CmpInst::ICMP_SGT, CmpInst::ICMP_SGE,
                                          CmpInst::ICMP_SLT, CmpInst::ICMP_SLE,
                                          CmpInst::ICMP_UGT, CmpInst::ICMP_UGE,
                                          CmpInst::ICMP_ULT, CmpInst::ICMP_ULE};

  CmpInst::Predicate Pred = CmpTable[(Opcode >> 1) - (OP_eq >> 1)];
  switch (Pred) {
  case CmpInst::ICMP_UGT:
  case CmpInst::ICMP_UGE:
  case CmpInst::ICMP_ULT:
  case CmpInst::ICMP_ULE:
    fixAccConstantInt();
    break;

  default:
    break;
  }

  castAccToSizeType();
  storePrevAcc();
  Value *Top = callPop();
  Acc = new ICmpInst(*CurrBlock, Pred, Top, Acc);
  return true;
}

bool PMachine::btOp(uint8_t Opcode) {
  storeAcc();
  if (!Acc->getType()->isIntegerTy(1))
    Acc = new ICmpInst(*CurrBlock, CmpInst::ICMP_NE, Acc,
                       Constant::getNullValue(Acc->getType()));

  int offset = getSInt(Opcode);
  BasicBlock *IfTrue = getBasicBlock(PC + offset);
  BasicBlock *IfFalse = getBasicBlock(PC);

  if (Opcode <= OP_bt_ONE)
    BranchInst::Create(IfTrue, IfFalse, Acc, CurrBlock);
  else
    BranchInst::Create(IfFalse, IfTrue, Acc, CurrBlock);
  return false;
}

bool PMachine::jmpOp(uint8_t Opcode) {
  storeAcc();
  int Offset = getSInt(Opcode);
  BasicBlock *Dst = getBasicBlock(PC + Offset);
  BranchInst::Create(Dst, CurrBlock);
  return false;
}

bool PMachine::loadiOp(uint8_t Opcode) {
  Acc = ConstantInt::get(SizeTy, static_cast<uint64_t>(getSInt(Opcode)), true);
  return true;
}

bool PMachine::pushOp(uint8_t Opcode) {
  castAccToSizeType();
  callPush(Acc);
  return true;
}

bool PMachine::pushiOp(uint8_t Opcode) {
  Value *Imm =
      GetWorld().getConstantValue(static_cast<int16_t>(getSInt(Opcode)));
  callPush(Imm);
  return true;
}

bool PMachine::tossOp(uint8_t Opcode) {
  callPop();
  return true;
}

bool PMachine::dupOp(uint8_t Opcode) {
  // TODO: Maybe we can do this more efficient?
  Value *Top = callPop();
  callPush(Top);
  callPush(Top);
  return true;
}

bool PMachine::linkOp(uint8_t Opcode) {
  assert(!Temp && !TempCount);
  TempCount = getUInt(Opcode);
  Temp =
      new AllocaInst(ArrayType::get(SizeTy, TempCount), 0, "temp", CurrBlock);
  Temp->setAlignment(GetWorld().getTypeAlignment(SizeTy));
  return true;
}

bool PMachine::callOp(uint8_t Opcode) {
  int Offset = getSInt(Opcode);
  unsigned AbsOffset = TheScript.getOffsetOf((PC + 1) + Offset);
  Constant *Constants[] = {ConstantInt::get(SizeTy, AbsOffset)};
  emitCall(getCallIntrinsic(), Constants);
  return true;
}

bool PMachine::callkOp(uint8_t Opcode) {
  unsigned KernelOrdinal = getUInt(Opcode);

  Constant *Constants[] = {ConstantInt::get(SizeTy, KernelOrdinal)};
  emitCall(Intrinsic::Get(Intrinsic::callk), Constants);
  return true;
}

bool PMachine::callbOp(uint8_t Opcode) {
  unsigned EntryIndex = getUInt(Opcode);

  Constant *Constants[] = {ConstantInt::get(SizeTy, 0),
                           ConstantInt::get(SizeTy, EntryIndex)};
  emitCall(Intrinsic::Get(Intrinsic::calle), Constants);
  return true;
}

bool PMachine::calleOp(uint8_t Opcode) {
  unsigned ScriptID = getUInt(Opcode);
  unsigned EntryIndex = getUInt(Opcode);

  Constant *Constants[] = {ConstantInt::get(SizeTy, ScriptID),
                           ConstantInt::get(SizeTy, EntryIndex)};
  emitCall(Intrinsic::Get(Intrinsic::calle), Constants);
  return true;
}

bool PMachine::retOp(uint8_t Opcode) {
  if (Acc == nullptr) {
    assert(!RetTy || RetTy->isVoidTy());
    if (!RetTy)
      RetTy = Type::getVoidTy(Context);
    ReturnInst::Create(Context, CurrBlock);
  } else {
    if (!RetTy)
      RetTy = Acc->getType();
    else if (RetTy != Acc->getType()) {
      assert(!RetTy->isVoidTy());
      castAccToSizeType();
      RetTy = SizeTy;

      // Cast all other return values to the same type.
      for (auto I = Labels.begin(), E = Labels.end(); I != E; ++I) {
        BasicBlock *BB = I->second;
        ReturnInst *RetInst = dyn_cast_or_null<ReturnInst>(BB->getTerminator());
        if (RetInst != nullptr) {
          Value *Val = RetInst->getReturnValue();
          RetInst->eraseFromParent();
          Val = castValueToSizeType(Val, BB);

          ReturnInst::Create(Context, Val, BB);
        }
      }
    }
    ReturnInst::Create(Context, Acc, CurrBlock);
  }
  return false;
}

bool PMachine::sendOp(uint8_t Opcode) {
  assert(Acc != nullptr);
  castAccToSizeType();
  emitSend(Acc);
  return true;
}

bool PMachine::classOp(uint8_t Opcode) {
  unsigned ClassID = getUInt(Opcode);
  ConstantInt *C = ConstantInt::get(SizeTy, ClassID);
  Acc = ClassInst::Create(C, CurrBlock);
  return true;
}

bool PMachine::selfOp(uint8_t Opcode) {
  assert(Self != nullptr);
  emitSend(Self.get());
  return true;
}

bool PMachine::superOp(uint8_t Opcode) {
  unsigned ClassID = getUInt(Opcode);
  ConstantInt *Cls = ConstantInt::get(SizeTy, ClassID);
  emitSend(Cls);
  return true;
}

bool PMachine::restOp(uint8_t Opcode) {
  assert(Rest == nullptr);
  unsigned ParamIndex = getUInt(Opcode);
  ConstantInt *C = ConstantInt::get(SizeTy, ParamIndex);
  Rest = RestInst::Create(C, CurrBlock);
  return true;
}

bool PMachine::leaOp(uint8_t Opcode) {
  // Get the type of the variable.
  unsigned VarType = getUInt(Opcode);
  // Get the number of the variable.
  unsigned VarIdx = getUInt(Opcode);

  Acc = getIndexedVariablePtr(static_cast<uint8_t>(VarType), VarIdx);
  return true;
}

bool PMachine::selfIdOp(uint8_t Opcode) {
  assert(Self);
  Acc = Self.get();
  return true;
}

bool PMachine::pprevOp(uint8_t Opcode) {
  Value *Prev = loadPrevAcc();
  callPush(Prev);
  return true;
}

bool PMachine::pToaOp(uint8_t Opcode) {
  Value *Prop = getIndexedPropPtr(Opcode);
  Acc = new LoadInst(Prop, "", false, GetWorld().getElementTypeAlignment(Prop),
                     CurrBlock);
  castAccToSizeType();
  return true;
}

bool PMachine::aTopOp(uint8_t Opcode) {
  castAccToSizeType();
  Value *Prop = getIndexedPropPtr(Opcode);
  new StoreInst(Acc, Prop, false, GetWorld().getTypeAlignment(Acc), CurrBlock);
  return true;
}

bool PMachine::pTosOp(uint8_t Opcode) {
  Value *Prop = getIndexedPropPtr(Opcode);
  Prop = new LoadInst(Prop, "", false, GetWorld().getElementTypeAlignment(Prop),
                      CurrBlock);
  Prop = castValueToSizeType(Prop);
  callPush(Prop);
  return true;
}

bool PMachine::sTopOp(uint8_t Opcode) {
  Value *Val = callPop();
  Value *Prop = getIndexedPropPtr(Opcode);
  new StoreInst(Val, Prop, false, GetWorld().getTypeAlignment(Val), CurrBlock);
  return true;
}

bool PMachine::ipToaOp(uint8_t Opcode) {
  Value *Prop = getIndexedPropPtr(Opcode);
  Value *Val = new LoadInst(
      Prop, "", false, GetWorld().getElementTypeAlignment(Prop), CurrBlock);
  Acc = BinaryOperator::CreateAdd(Val, ConstantInt::get(Val->getType(), 1), "",
                                  CurrBlock);
  new StoreInst(Acc, Prop, false, GetWorld().getTypeAlignment(Acc), CurrBlock);
  castAccToSizeType();
  return true;
}

bool PMachine::dpToaOp(uint8_t Opcode) {
  Value *Prop = getIndexedPropPtr(Opcode);
  Value *Val = new LoadInst(
      Prop, "", false, GetWorld().getElementTypeAlignment(Prop), CurrBlock);
  Acc = BinaryOperator::CreateSub(Val, ConstantInt::get(Val->getType(), 1), "",
                                  CurrBlock);
  new StoreInst(Acc, Prop, false, GetWorld().getTypeAlignment(Acc), CurrBlock);
  castAccToSizeType();
  return true;
}

bool PMachine::ipTosOp(uint8_t Opcode) {
  Value *Prop = getIndexedPropPtr(Opcode);
  Value *Val = new LoadInst(
      Prop, "", false, GetWorld().getElementTypeAlignment(Prop), CurrBlock);
  Val = BinaryOperator::CreateAdd(Val, ConstantInt::get(Val->getType(), 1), "",
                                  CurrBlock);
  new StoreInst(Val, Prop, false, GetWorld().getTypeAlignment(Val), CurrBlock);
  Val = castValueToSizeType(Val);
  callPush(Val);
  return true;
}

bool PMachine::dpTosOp(uint8_t Opcode) {
  Value *Prop = getIndexedPropPtr(Opcode);
  Value *Val = new LoadInst(
      Prop, "", false, GetWorld().getElementTypeAlignment(Prop), CurrBlock);
  Val = BinaryOperator::CreateSub(Val, ConstantInt::get(SizeTy, 1), "",
                                  CurrBlock);
  new StoreInst(Val, Prop, false, GetWorld().getTypeAlignment(Val), CurrBlock);
  Val = castValueToSizeType(Val);
  callPush(Val);
  return true;
}

bool PMachine::lofsaOp(uint8_t Opcode) {
  Value *Val = getValueByOffset(Opcode);
  assert(Val != nullptr);

  Acc = Val;
  return true;
}

bool PMachine::lofssOp(uint8_t Opcode) {
  Value *Val = getValueByOffset(Opcode);
  assert(Val != nullptr);

  callPush(Val);
  return true;
}

bool PMachine::push0Op(uint8_t Opcode) {
  Value *Val = ConstantInt::get(SizeTy, 0);
  callPush(Val);
  return true;
}

bool PMachine::push1Op(uint8_t Opcode) {
  Value *Val = ConstantInt::get(SizeTy, 1);
  callPush(Val);
  return true;
}

bool PMachine::push2Op(uint8_t Opcode) {
  Value *Val = ConstantInt::get(SizeTy, 2);
  callPush(Val);
  return true;
}

bool PMachine::pushSelfOp(uint8_t Opcode) {
  callPush(Self.get());
  return true;
}

bool PMachine::ldstOp(uint8_t Opcode) {
  unsigned varIdx = getUInt(Opcode);

  Value *Var, *Val;
  Var = getIndexedVariablePtr(Opcode, varIdx);

  switch (Opcode & OP_TYPE) {
  default:
  case OP_LOAD:
    Val = new LoadInst(Var, "", false, GetWorld().getElementTypeAlignment(Var),
                       CurrBlock);
    break;

  case OP_INC:
    Val = new LoadInst(Var, "", false, GetWorld().getElementTypeAlignment(Var),
                       CurrBlock);
    Val = BinaryOperator::CreateAdd(Val, ConstantInt::get(SizeTy, 1), "",
                                    CurrBlock);
    new StoreInst(Val, Var, false, GetWorld().getTypeAlignment(Val), CurrBlock);
    break;

  case OP_DEC:
    Val = new LoadInst(Var, "", false, GetWorld().getElementTypeAlignment(Var),
                       CurrBlock);
    Val = BinaryOperator::CreateSub(Val, ConstantInt::get(SizeTy, 1), "",
                                    CurrBlock);
    new StoreInst(Val, Var, false, GetWorld().getTypeAlignment(Val), CurrBlock);
    break;

  case OP_STORE:
    if ((Opcode & OP_INDEX) != 0) {
      Val = callPop();
      if ((Opcode & OP_STACK) == 0) {
        Acc = Val;
      }
    } else {
      if ((Opcode & OP_STACK) == 0) {
        castAccToSizeType();
        Val = Acc;
      } else {
        Val = callPop();
      }
    }
    new StoreInst(Val, Var, false, GetWorld().getTypeAlignment(Val), CurrBlock);
    return true;
  }

  if ((Opcode & OP_STACK) == 0) {
    Acc = Val;
  } else {
    callPush(Val);
  }
  return true;
}

bool PMachine::badOp(uint8_t Opcode) {
  assert(0);
  return false;
}

void PMachine::emitDebugLog() {
  Function *Func = Entry->getParent();
  assert(Func && "Function is not ready!");

  Module *M = Func->getParent();
  LLVMContext &Ctx = M->getContext();
  PointerType *Int8PtrTy = Type::getInt8PtrTy(Ctx);

  Value *Self;
  auto AI = Func->arg_begin();
  if (!AI->getType()->isPointerTy()) {
    Self = &*AI;
    ++AI;
  } else
    Self = ConstantInt::get(SizeTy, 0);

  StringRef Name;
  Name = "DebugFunctionEntry";
  Function *FuncEntry = M->getFunction(Name);
  if (!FuncEntry) {
    Type *Params[] = {Int8PtrTy, SizeTy, SizeTy->getPointerTo()};
    FunctionType *FTy = FunctionType::get(Type::getVoidTy(Ctx), Params, false);
    FuncEntry = Function::Create(FTy, GlobalValue::ExternalLinkage, Name, M);
  }

  Name = "DebugFunctionExit";
  Function *FuncExit = M->getFunction(Name);
  if (!FuncExit) {
    FunctionType *FTy = FunctionType::get(Type::getVoidTy(Ctx), SizeTy, false);
    FuncExit = Function::Create(FTy, GlobalValue::ExternalLinkage, Name, M);
  }

  Constant *C = ConstantDataArray::getString(Ctx, Func->getName());
  GlobalVariable *Var = new GlobalVariable(*M, C->getType(), true,
                                           GlobalValue::PrivateLinkage, C);
  Var->setAlignment(GetWorld().getTypeAlignment(C->getType()));
  Var->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);

  Instruction *EntryPoint = &*Entry->begin();
  Value *StrCast =
      CastInst::Create(Instruction::BitCast, Var, Int8PtrTy, "", EntryPoint);
  Value *CallArgs[] = {StrCast, Self, &*AI};
  CallInst::Create(FuncEntry, CallArgs, "", EntryPoint);

  for (BasicBlock &BB : *Func) {
    ReturnInst *Ret = dyn_cast<ReturnInst>(BB.getTerminator());
    if (Ret)
      CallInst::Create(FuncExit, Self, "", Ret);
  }
}
