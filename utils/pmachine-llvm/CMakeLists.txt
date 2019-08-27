set(LLVM_LINK_COMPONENTS
  Core
  Support
  )
  
file(GLOB_RECURSE headers
  *.hpp
  Passes/*.hpp
  )

add_sci_executable(pmachine-llvm
  Class.cpp
  Decl.cpp
  Intrinsics.cpp
  Main.cpp
  Method.cpp
  Object.cpp
  PMachine.cpp
  Procedure.cpp
  Property.cpp
  Script.cpp
  SelectorTable.cpp
  World.cpp
  Passes/EmitScriptUtilitiesPass.cpp
  Passes/FixCodePass.cpp
  Passes/MutateCallIntrinsicsPass.cpp
  Passes/SplitSendPass.cpp
  Passes/StackReconstructionPass.cpp
  Passes/TranslateClassIntrinsicPass.cpp
  
  ADDITIONAL_HEADERS
  ${headers}
  )
set_target_properties(pmachine-llvm PROPERTIES FOLDER "SCI utils")

target_link_libraries(pmachine-llvm
  PRIVATE
  sciKernel
  )

# Effective pmachine-llvm executable to be used:
set(PMACHINE_LLVM_EXE pmachine-llvm)
set(PMACHINE_LLVM_TARGET pmachine-llvm)
