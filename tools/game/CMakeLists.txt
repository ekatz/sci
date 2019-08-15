add_sci_tool(game
  Main.c
  )

target_link_libraries(game
  PRIVATE
  sciAudioDriver
  sciDisplayDriver
  sciInputDriver
  sciKernel
  sciLogger
  sciPMachine
  sciUtils
  )

set_property(TARGET game APPEND_STRING PROPERTY
              LINK_FLAGS
              " /SUBSYSTEM:WINDOWS")
