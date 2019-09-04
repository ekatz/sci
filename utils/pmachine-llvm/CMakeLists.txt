set(LLVM_LINK_COMPONENTS
  BitWriter
  Core
  )
  
file(GLOB headers *.hpp)
file(GLOB pass_headers Passes/*.hpp)
set(pass_src
  Passes/EmitScriptUtilitiesPass.cpp
  Passes/FixCodePass.cpp
  Passes/MutateCallIntrinsicsPass.cpp
  Passes/SplitSendPass.cpp
  Passes/StackReconstructionPass.cpp
  Passes/TranslateClassIntrinsicPass.cpp
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
  ${pass_src}
  
  ADDITIONAL_HEADERS
  ${headers}
  ${pass_headers}
  )

if(MSVC_IDE)
  source_group("Header Files\\Passes" FILES ${pass_headers})
  source_group("Source Files\\Passes" FILES ${pass_src})
endif()

set_target_properties(pmachine-llvm PROPERTIES FOLDER "SCI utils")

target_link_libraries(pmachine-llvm
  PRIVATE
  sciKernel
  )

# Effective pmachine-llvm executable to be used:
set(PMACHINE_LLVM_EXE pmachine-llvm)
set(PMACHINE_LLVM_TARGET pmachine-llvm)
