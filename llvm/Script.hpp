#pragma once
#ifndef _Script_HPP_
#define _Script_HPP_

#include "Types.hpp"
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <vector>

#ifdef __WINDOWS__
#pragma warning(push)
#pragma warning(disable: 4200) // zero-sized array in struct/union
#pragma warning(disable: 4201) // nonstandard extension used: nameless struct/union
#endif

namespace sci {


class ScriptResource;
class ObjectResource;

class Script;

class Class;
class Object;

class Procedure;
class Method;

} // namespace sci

struct ObjRes
{
    uint16_t magic;
    uint16_t localVarOffset;
    uint16_t funcSelOffset;
    uint16_t varSelNum;
    union {
        int16_t sels[0];

        struct {
            ObjID speciesSel;
            ObjID superSel; // This is the class selector for "OBJECT"
            ObjID infoSel;
            ObjID nameSel;
        };
    };

    //const char* getName(Handle hunk) const;

    llvm::ArrayRef<int16_t> getPropertyValues(uint offset = 0) const {
        return llvm::makeArrayRef(sels + offset, static_cast<uint>(varSelNum) - offset);
    }

    llvm::ArrayRef<ObjID> getPropertySelectors(uint offset = 0) const {
        return llvm::makeArrayRef(reinterpret_cast<const ObjID *>(sels) + varSelNum + offset, static_cast<uint>(varSelNum) - offset);
    }

    llvm::ArrayRef<ObjID> getMethodSelectors() const {
        const ObjID *funcs = reinterpret_cast<const ObjID *>(reinterpret_cast<const byte *>(sels) + funcSelOffset);
        return llvm::makeArrayRef(funcs, funcs[-1]);
    }

    llvm::ArrayRef<uint16_t> getMethodOffsets() const {
        const ObjID *funcs = reinterpret_cast<const ObjID *>(reinterpret_cast<const byte *>(sels) + funcSelOffset);
        return llvm::makeArrayRef(funcs + funcs[-1], funcs[-1]);
    }
};

#ifdef __WINDOWS__
#pragma warning(pop)
#endif


class Script;
class ScriptManager;


struct AnalyzedFunction
{
    uint8_t *addr;
    uint selector;
    uint numArgs;

    union {
        struct {
            uint32_t analyzed : 1;
            uint32_t argc : 1;
            uint32_t rest : 1;
            uint32_t leaParam : 1; // not supported!
            uint32_t indexParam : 1;
        };

        uint32_t value;
    } flags;
};


struct Object
{
    llvm::GlobalVariable *instance;
    llvm::GlobalVariable *vftbl;
    llvm::StructType *type;
    
    const int16_t *propVals;
    const ObjID *propSels;
    uint numProps;

    std::unique_ptr<AnalyzedFunction[]> methods;

    Object *super;
    Script *script;
};


class Script
{
public:
    Script(ScriptManager &mgr, uint id, Handle hunk);
    ~Script();

    ScriptManager &getManager() const { return m_mgr; }

    uint getId() const { return m_id; }
    llvm::Module* getModule() const { return m_module.get(); }
    llvm::GlobalVariable* getString(const llvm::StringRef &str);
    llvm::Function* getProcedure(const uint8_t *ptr);

    const char* getDataAt(uint offset) const {
        return (static_cast<int16_t>(offset) > 0) ? reinterpret_cast<const char *>(m_hunk) + offset : NULL;
    }

    bool load();

    llvm::Function* createConstructor(Object *obj);

    void initObject(Object *obj, const llvm::ArrayRef<ObjID> &methods, const llvm::StringRef &name);

    AnalyzedFunction* analyzeFunction(uint selector, uint offset);

private:
    Object* addObject(const ObjRes *res);
    void setLocals(llvm::ArrayRef<int16_t> vals);

    void addStubFunctions();
    void removeStubFunctions();

    llvm::Function* parseFunction(const uint8_t *code);

    ScriptManager &m_mgr;
    const uint m_id;
    Handle m_hunk;
    std::unique_ptr<llvm::Module> m_module;

    std::map<std::string, llvm::GlobalVariable *> m_strings;

    std::unique_ptr<Object[]> m_objects;
    uint m_numObjects;

    std::unique_ptr<llvm::GlobalVariable *[]> m_vars;
    uint m_numVars;

    std::unique_ptr<llvm::GlobalObject *[]> m_exports;
    uint m_numExports;

    std::map<const uint8_t *, llvm::Function *> m_procs;

    llvm::Function *m_funcPush;
    llvm::Function *m_funcPop;
};


class ScriptManager
{
public:
    ScriptManager(llvm::LLVMContext &ctx);
    ~ScriptManager();

    bool load();

    llvm::LLVMContext& getContext() const { return m_ctx; }

    Object* addClass(const ObjRes *res);
    Object* getClass(uint id);

    uint countMethods(const Object *obj, const llvm::ArrayRef<ObjID> &methods);

    void dumpClasses() const;

    llvm::StringRef getSelectorName(uint id);
    llvm::FunctionType*& getMethodSignature(uint id);

private:
    Script* getScript(uint id);

    char* getSelectorData(uint id);

    llvm::LLVMContext &m_ctx;
    std::unique_ptr<Script> m_scripts[1000];
    std::unique_ptr<Object[]> m_classes;
    uint m_numClasses;

    std::vector<std::unique_ptr<char[]> > m_sels;
};

#endif // !_Script_HPP_
