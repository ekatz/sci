cmake_minimum_required(VERSION 3.4.3)

if(POLICY CMP0075)
  cmake_policy(SET CMP0075 NEW)
endif()

find_package(OpenGL REQUIRED)

# Check if sci is built as a standalone project.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(sci)

  set(CMAKE_INCLUDE_CURRENT_DIR ON)

  if(NOT LLVM_CONFIG_PATH)
    find_program(LLVM_CONFIG_PATH "llvm-config" DOC "Path to llvm-config binary")
    if(NOT LLVM_CONFIG_PATH)
      message(FATAL_ERROR "llvm-config not found: specify LLVM_CONFIG_PATH")
    endif()
  endif()

  execute_process(COMMAND "${LLVM_CONFIG_PATH}"
                          "--obj-root"
                          "--includedir"
                          "--cmakedir"
                          "--src-root"
                  RESULT_VARIABLE HAD_ERROR
                  OUTPUT_VARIABLE LLVM_CONFIG_OUTPUT
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(HAD_ERROR)
    message(FATAL_ERROR "llvm-config failed with status ${HAD_ERROR}")
  endif()

  string(REGEX REPLACE "[ \t]*[\r\n]+[ \t]*" ";" LLVM_CONFIG_OUTPUT "${LLVM_CONFIG_OUTPUT}")

  list(GET LLVM_CONFIG_OUTPUT 0 OBJ_ROOT)
  list(GET LLVM_CONFIG_OUTPUT 1 MAIN_INCLUDE_DIR)
  list(GET LLVM_CONFIG_OUTPUT 2 LLVM_CONFIG_CMAKE_PATH)
  list(GET LLVM_CONFIG_OUTPUT 3 MAIN_SRC_DIR)

  set(LLVM_OBJ_ROOT ${OBJ_ROOT} CACHE PATH "path to LLVM build tree")
  set(LLVM_MAIN_INCLUDE_DIR ${MAIN_INCLUDE_DIR} CACHE PATH "path to llvm/include")
  set(LLVM_MAIN_SRC_DIR ${MAIN_SRC_DIR} CACHE PATH "Path to LLVM source tree")

  # Normalize LLVM_CMAKE_PATH. --cmakedir might contain backslashes.
  # CMake assumes slashes as PATH.
  file(TO_CMAKE_PATH ${LLVM_CONFIG_CMAKE_PATH} LLVM_CMAKE_PATH)
  file(TO_CMAKE_PATH ${LLVM_OBJ_ROOT} LLVM_BINARY_DIR)

  if(NOT EXISTS "${LLVM_CMAKE_PATH}/LLVMConfig.cmake")
    message(FATAL_ERROR "LLVMConfig.cmake not found")
  endif()
  include("${LLVM_CMAKE_PATH}/LLVMConfig.cmake")

  list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_PATH}")

  set(PACKAGE_VERSION "${LLVM_PACKAGE_VERSION}")
  include_directories("${LLVM_BINARY_DIR}/include" ${LLVM_INCLUDE_DIRS})
  link_directories(${LLVM_LIBRARY_DIRS})

  set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib${LLVM_LIBDIR_SUFFIX})
  set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/bin)

  include(AddLLVM)
  include(HandleLLVMOptions)
endif()

set(SCI_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SCI_INCLUDE_DIR ${SCI_SOURCE_DIR}/include )
set(SCI_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR AND NOT MSVC_IDE)
  message(FATAL_ERROR "In-source builds are not allowed. CMake would overwrite "
"the makefiles distributed with LLVM. Please create a directory and run cmake "
"from there, passing the path to this source directory as the last argument. "
"This process created the file `CMakeCache.txt' and the directory "
"`CMakeFiles'. Please delete them.")
endif()

list (APPEND CMAKE_MODULE_PATH "${SCI_SOURCE_DIR}/cmake/modules")

include(AddSCI)

option(SCI_BUILD_TOOLS
  "Build the sci tools. If OFF, just generate build targets." ON)

if (MSVC)
  add_definitions(-wd4530) # Suppress 'warning C4530: C++ exception handler used, but unwind semantics are not enabled.'
  add_definitions(-wd4062) # Suppress 'warning C4062: enumerator X in switch of enum Y is not handled' from system header.
endif()

include_directories(BEFORE
  ${CMAKE_CURRENT_BINARY_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

if (NOT LLVM_INSTALL_TOOLCHAIN_ONLY)
  install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN ".svn" EXCLUDE
    )
endif()

add_subdirectory(utils/pmachine-llvm)
add_subdirectory(lib)
add_subdirectory(tools)
