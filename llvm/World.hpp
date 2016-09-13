#pragma once
#ifndef _World_HPP_
#define _World_HPP_

#include "Types.hpp"
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <vector>

struct ObjRes;


BEGIN_NAMESPACE_SCI

class Class;
class Script;

struct Stub
{
    enum ID { push, pop, rest, super, send, call, callb, calle, callk };

    std::unique_ptr<llvm::Function> funcs[callk + 1];
};

class World
{
public:
    World();
    ~World();

    void setDataLayout(const llvm::DataLayout &dl);

    bool load();

    llvm::LLVMContext& getContext() const { return m_ctx; }
    llvm::IntegerType* getSizeType() const { return m_sizeTy; }
    llvm::ConstantInt* getConstantValue(int16_t val) const;

    Class* addClass(const ObjRes &res, Script &script);
    Class* getClass(uint id);
    llvm::StructType* getAbstractClassType() const { return m_absClassTy; }

    llvm::StringRef getSelectorName(uint id);
    llvm::FunctionType*& getMethodSignature(uint id);

    uint getGlobalVariablesCount() const;

    llvm::Function* getStub(Stub::ID id) const { return m_stubs.funcs[id].get(); }

private:
    Script* getScript(uint id);

    char* getSelectorData(uint id);

    void createStubFunctions();


    llvm::DataLayout m_dataLayout;
    llvm::LLVMContext &m_ctx;
    llvm::IntegerType *m_sizeTy;
    llvm::StructType *m_absClassTy;
    std::unique_ptr<Script> m_scripts[1000];
    Class *m_classes;
    uint m_classCount;

    std::vector<std::unique_ptr<char[]> > m_sels;

    Stub m_stubs;
};


World& GetWorld();

END_NAMESPACE_SCI

#endif // !_World_HPP_
