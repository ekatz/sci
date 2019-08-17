#pragma once
#ifndef _ExpandScriptIDPass_HPP_
#define _ExpandScriptIDPass_HPP_

#include "../Intrinsics.hpp"

BEGIN_NAMESPACE_SCI

class Script;

class EmitScriptUtilitiesPass
{
public:
    EmitScriptUtilitiesPass();
    ~EmitScriptUtilitiesPass();

    void run();

private:
    void expandScriptID(CallKernelInst *call);
    void createDisposeScriptFunctions();

    llvm::Function* getOrCreateDisposeScriptNumFunction(Script *script);
    llvm::Function* getOrCreateGetDispatchAddrFunction() const;
};

END_NAMESPACE_SCI

#endif // !_ExpandScriptIDPass_HPP_
