#pragma once
#ifndef _UnifyOverloadsPass_HPP_
#define _UnifyOverloadsPass_HPP_

#include "../Types.hpp"
#include <llvm/IR/Instructions.h>
#include <set>

BEGIN_NAMESPACE_SCI

class UnifyOverloadsPass
{
public:
    UnifyOverloadsPass();
    ~UnifyOverloadsPass();

    void run();

private:
};

END_NAMESPACE_SCI

#endif // !_UnifyOverloadsPass_HPP_
