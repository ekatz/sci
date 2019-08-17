#pragma once
#ifndef _SplitSendPass_HPP_
#define _SplitSendPass_HPP_

#include "../Types.hpp"
#include <llvm/IR/Instructions.h>

BEGIN_NAMESPACE_SCI

class SplitSendPass
{
public:
    SplitSendPass();
    ~SplitSendPass();

    void run();

private:
    bool splitSend(llvm::CallInst *callSend);

    std::unique_ptr<llvm::Function> m_fnStubSend;
};

END_NAMESPACE_SCI

#endif // !_SplitSendPass_HPP_
