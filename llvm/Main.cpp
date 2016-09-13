#include "World.hpp"
#include <llvm/ADT/Triple.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/CodeGen/CommandFlags.h>
#include <llvm/Support/TargetRegistry.h>

extern "C" {
#include "../SCI/src/ErrMsg.c"
#include "../SCI/src/Log.c"
#include "../SCI/src/List.c"
#include "../SCI/src/FileIO.c"
#include "../SCI/src/Resource.c"
#include "../SCI/src/ResName.c"
#include "../SCI/src/VolLoad.c"
#include "../SCI/src/Crypt.c"
#include "../SCI/src/FarData.c"
} // extern "C"

using namespace llvm;


static cl::opt<std::string>
opt_targetTriple("target", cl::desc("Generate code for the given target"));


int main(int argc, char *argv[])
{
#if 0
    std::string triple = Triple::normalize(opt_targetTriple);
    if (triple.empty())
    {
        triple = sys::getDefaultTargetTriple();
    }

    std::string err;
    const Target *target = TargetRegistry::lookupTarget(triple, err);
    if (target == nullptr)
    {
        errs() << argv[0] << ": " << err;
        return 1;
    }

    std::unique_ptr<TargetMachine> tm(
        target->createTargetMachine(triple, getCPUStr(), getFeaturesStr(), InitTargetOptionsFromCodeGenFlags()));

    assert(tm && "Could not allocate target machine!");
#endif
    g_isExternal = true;
    InitResource(NULL);

    //sci::GetWorld().setDataLayout(tm->createDataLayout());
    sci::GetWorld().load();
    return 0;
}
