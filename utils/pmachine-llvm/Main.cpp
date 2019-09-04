//===- Main.cpp -----------------------------------------------------------===//
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#include "World.hpp"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

extern "C" {
#include "sci/Kernel/VolLoad.h"
#include "sci/Utils/ErrMsg.h"
}

using namespace llvm;

static cl::OptionCategory PMachineCat("p-Machine Options");

static cl::opt<std::string> GameDir("source", cl::desc("Game directory"),
                                    cl::value_desc("directory"),
                                    cl::cat(PMachineCat));
static cl::alias GameDirA("s", cl::aliasopt(GameDir));

static cl::opt<std::string> OutputDir("output", cl::desc("Output directory"),
                                      cl::value_desc("directory"),
                                      cl::cat(PMachineCat));
static cl::alias OutputDirA("o", cl::aliasopt(OutputDir));

cl::opt<bool> PtrSize64("ptr-64", cl::desc("Use 64-bit pointers or not"),
                        cl::init(sizeof(void *) == 8), cl::cat(PMachineCat));

cl::opt<bool> EmitLLVM("emit-llvm", cl::desc("Use the LLVM representation"),
                       cl::init(false), cl::cat(PMachineCat));

int main(int argc, char *argv[]) {
  InitLLVM X(argc, argv);

  // Enable debug stream buffering.
  EnableDebugBuffering = true;
  cl::HideUnrelatedOptions(PMachineCat);
  cl::ParseCommandLineOptions(argc, argv, "p-Machine llvm translator\n");

  std::error_code EC;
  if (!GameDir.empty()) {
    EC = sys::fs::set_current_path(GameDir);
    if (EC) {
      errs() << "error: Failed to read game directory \"" << GameDir << "\" ("
             << EC.message() << ")\n";
      return EXIT_FAILURE;
    }
  }

  SmallString<256> OutputFile = OutputDir;
  sys::path::append(OutputFile, "sci.");
  OutputFile += EmitLLVM ? "ll" : "bc";
  raw_fd_ostream OS(OutputFile, EC,
                    EmitLLVM ? sys::fs::F_Text : sys::fs::F_None);
  if (EC) {
    errs() << "error: Failed to create output file \"" << OutputFile << "\" ("
           << EC.message() << ")\n";
    return EXIT_FAILURE;
  }

  if (PtrSize64)
    sci::GetWorld().setDataLayout(DataLayout("p:64:64"));
  else
    sci::GetWorld().setDataLayout(DataLayout("p:32:32"));

  SetPanicProc(NULL);
  InitResource();

  std::unique_ptr<Module> M = sci::GetWorld().load();

  if (EmitLLVM)
    M->print(OS, nullptr);
  else
    WriteBitcodeToFile(*M, OS);
  return EXIT_SUCCESS;
}
