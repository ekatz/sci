#pragma once
#ifndef _StackReconstructionPass_HPP_
#define _StackReconstructionPass_HPP_

#include "../Types.hpp"
#include <llvm/IR/Instructions.h>

BEGIN_NAMESPACE_SCI

class StackReconstructionPass
{
public:
    StackReconstructionPass();

    void run();

private:
    void runOnPop(llvm::CallInst *callPop);

    llvm::Instruction *m_insertPt;
};

END_NAMESPACE_SCI

#endif // !_StackReconstructionPass_HPP_
