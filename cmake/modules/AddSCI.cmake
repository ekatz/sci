macro(add_sci_library name)
  cmake_parse_arguments(ARG
    "SHARED"
    ""
    "ADDITIONAL_HEADERS"
    ${ARGN})
  set(srcs)
  if(MSVC_IDE OR XCODE)
    # Add public headers
    file(RELATIVE_PATH lib_path
      ${SCI_SOURCE_DIR}/lib/
      ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(NOT lib_path MATCHES "^[.][.]")
      file( GLOB_RECURSE headers
        ${SCI_SOURCE_DIR}/include/sci/${lib_path}/*.h
        ${SCI_SOURCE_DIR}/include/sci/${lib_path}/*.hpp
        ${SCI_SOURCE_DIR}/include/sci/${lib_path}/*.def
      )
      set_source_files_properties(${headers} PROPERTIES HEADER_FILE_ONLY ON)
      if(headers)
        set(srcs ${headers})
      endif()
    endif()
  endif(MSVC_IDE OR XCODE)
  if(srcs OR ARG_ADDITIONAL_HEADERS)
    set(srcs
      ADDITIONAL_HEADERS
      ${srcs}
      ${ARG_ADDITIONAL_HEADERS} # It may contain unparsed unknown args.
      )
  endif()
  if(ARG_SHARED)
    set(ARG_ENABLE_SHARED SHARED)
  endif()
  llvm_add_library(${name} ${ARG_ENABLE_SHARED} ${ARG_UNPARSED_ARGUMENTS} ${srcs})
  set_target_properties(${name} PROPERTIES FOLDER "SCI libraries")

  if (NOT LLVM_INSTALL_TOOLCHAIN_ONLY)
    if(${name} IN_LIST LLVM_DISTRIBUTION_COMPONENTS OR
        NOT LLVM_DISTRIBUTION_COMPONENTS)
      set(export_to_scitargets EXPORT sciTargets)
      set_property(GLOBAL PROPERTY SCI_HAS_EXPORTS True)
    endif()

    install(TARGETS ${name}
      COMPONENT ${name}
      ${export_to_scitargets}
      LIBRARY DESTINATION lib${LLVM_LIBDIR_SUFFIX}
      ARCHIVE DESTINATION lib${LLVM_LIBDIR_SUFFIX}
      RUNTIME DESTINATION bin)

    if (${ARG_SHARED} AND NOT CMAKE_CONFIGURATION_TYPES)
      add_custom_target(install-${name}
                        DEPENDS ${name}
                        COMMAND "${CMAKE_COMMAND}"
                                -DCMAKE_INSTALL_COMPONENT=${name}
                                -P "${CMAKE_BINARY_DIR}/cmake_install.cmake")
    endif()
    set_property(GLOBAL APPEND PROPERTY SCI_EXPORTS ${name})
  endif()
endmacro(add_sci_library)

macro(add_sci_executable name)
  add_llvm_executable( ${name} ${ARGN} )
  set_target_properties(${name} PROPERTIES FOLDER "SCI executables")
endmacro(add_sci_executable)

macro(add_sci_tool name)
  if (NOT SCI_BUILD_TOOLS)
    set(EXCLUDE_FROM_ALL ON)
  endif()

  add_sci_executable(${name} ${ARGN})

  if (SCI_BUILD_TOOLS)
    if(${name} IN_LIST LLVM_DISTRIBUTION_COMPONENTS OR
        NOT LLVM_DISTRIBUTION_COMPONENTS)
      set(export_to_scitargets EXPORT sciTargets)
      set_property(GLOBAL PROPERTY SCI_HAS_EXPORTS True)
    endif()

    install(TARGETS ${name}
      ${export_to_scitargets}
      RUNTIME DESTINATION bin
      COMPONENT ${name})

    if(NOT CMAKE_CONFIGURATION_TYPES)
      add_custom_target(install-${name}
        DEPENDS ${name}
        COMMAND "${CMAKE_COMMAND}"
        -DCMAKE_INSTALL_COMPONENT=${name}
        -P "${CMAKE_BINARY_DIR}/cmake_install.cmake")
    endif()
    set_property(GLOBAL APPEND PROPERTY SCI_EXPORTS ${name})
  endif()
endmacro()

macro(add_sci_symlink name dest)
  add_llvm_tool_symlink(${name} ${dest} ALWAYS_GENERATE)
  # Always generate install targets
  llvm_install_symlink(${name} ${dest} ALWAYS_GENERATE)
endmacro()
