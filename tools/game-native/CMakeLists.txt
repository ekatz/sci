set(SCI_GAME_DIRECTORY "" CACHE STRING "Specify SCI game directory.")
set(objdir ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR})

if (NOT "${SCI_GAME_DIRECTORY}" STREQUAL "")
  file(GLOB resource_files ${SCI_GAME_DIRECTORY}/resource.0*)
  set(sci_dependency ${resource_files})
  file(GLOB resource_files ${SCI_GAME_DIRECTORY}/RESOURCE.0*)
  set(sci_dependency ${sci_dependency} ${resource_files})
  list(REMOVE_DUPLICATES sci_dependency)
endif()

if(LLVM_CONFIG_PATH)
  if(NOT LLVM_LLC_PATH)
    find_program(LLVM_LLC_PATH "llc" DOC "Path to llc binary")
    if(NOT LLVM_LLC_PATH)
      message(FATAL_ERROR "llc not found: specify LLVM_LLC_PATH")
    endif()
  endif()
endif()

add_custom_command(OUTPUT ${objdir}/sci.bc
  DEPENDS pmachine-llvm ${sci_dependency}
  COMMAND pmachine-llvm -s ${SCI_GAME_DIRECTORY} -o ${objdir})

add_custom_command(OUTPUT ${objdir}/sci.obj
  DEPENDS ${objdir}/sci.bc
  COMMAND ${LLVM_LLC_PATH}
    --filetype=obj
	${objdir}/sci.bc
	-o=${objdir}/sci.obj)

set_source_files_properties(${objdir}/sci.obj
  PROPERTIES GENERATED TRUE)

add_sci_tool(game-native
  Main.c
  Object.c
  PMachine.c
  Script.c
  ${objdir}/sci.obj
  )

target_link_libraries(game-native
  PRIVATE
  sciKernel
  )

set_property(TARGET game-native APPEND_STRING PROPERTY
              LINK_FLAGS
              " /SUBSYSTEM:WINDOWS")
