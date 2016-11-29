#pragma once
#ifndef _ExpandScriptIDPass_HPP_
#define _ExpandScriptIDPass_HPP_

#include "../Intrinsics.hpp"

BEGIN_NAMESPACE_SCI

class ExpandScriptIDPass
{
public:
    ExpandScriptIDPass();
    ~ExpandScriptIDPass();

    void run();

private:
    void expand(CallKernelInst *call);

    llvm::Function* getOrCreateGetDispatchAddrFunction() const;
};

END_NAMESPACE_SCI

#endif // !_ExpandScriptIDPass_HPP_
