#include "World.hpp"
#include <llvm/IR/LLVMContext.h>

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

cl::opt<bool> opt_ptrSize64("ptr-64",
                            cl::desc("Use 64-bit pointers or not"),
                            cl::init(true));

int main(int argc, char *argv[])
{
    cl::ParseCommandLineOptions(argc, argv);
    
    if (opt_ptrSize64)
    {
        sci::GetWorld().setDataLayout(DataLayout("p:64:64"));
    }
    else
    {
        sci::GetWorld().setDataLayout(DataLayout("p:32:32"));
    }
    /*
    Triple triple = Triple(Triple::normalize(opt_targetTriple));
    if (triple.getTriple().empty())
    {
        triple.setTriple(sys::getDefaultTargetTriple());
    }

    std::string err;
    const Target *target = TargetRegistry::lookupTarget(opt_march, triple, err);
    if (target == nullptr)
    {
        errs() << argv[0] << ": " << err;
        return 1;
    }

    if (opt_mcpu == "native")
    {
        opt_mcpu = sys::getHostCPUName();
    }

    auto y = getFeaturesStr();
    auto z = InitTargetOptionsFromCodeGenFlags();
    std::unique_ptr<TargetMachine> tm(
        target->createTargetMachine(triple.getTriple(), opt_mcpu, "", InitTargetOptionsFromCodeGenFlags(), Optional<Reloc::Model>()));
    errs() << target->getName() << '\n';
    errs() << triple.getTriple() << '\n';
    errs() << tm->getPointerSize() << '\n';
    assert(tm && "Could not allocate target machine!");*/

    g_isExternal = true;
    InitResource(NULL);

    //sci::GetWorld().setDataLayout(tm->createDataLayout());
    sci::GetWorld().load();
    return 0;
}
