#pragma once
#ifndef _MutateCallvPass_HPP_
#define _MutateCallvPass_HPP_

#include "../Types.hpp"
#include <llvm/IR/Instructions.h>

BEGIN_NAMESPACE_SCI

class MutateCallvPass
{
public:
    MutateCallvPass();
    ~MutateCallvPass();

    void run();

private:
    void mutateSuper(llvm::CallInst *call);
};

END_NAMESPACE_SCI

#endif // !_MutateCallvPass_HPP_
