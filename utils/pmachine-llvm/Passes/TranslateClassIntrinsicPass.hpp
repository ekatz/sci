#pragma once
#ifndef _TranslateClassIntrinsicPass_HPP_
#define _TranslateClassIntrinsicPass_HPP_

#include "../Types.hpp"
#include "../Intrinsics.hpp"

BEGIN_NAMESPACE_SCI

class TranslateClassIntrinsicPass
{
public:
    TranslateClassIntrinsicPass();
    ~TranslateClassIntrinsicPass();

    void run();

private:
    void translate(ClassInst *call);

    llvm::IntegerType *m_sizeTy;
};

END_NAMESPACE_SCI

#endif // !_TranslateClassIntrinsicPass_HPP_
