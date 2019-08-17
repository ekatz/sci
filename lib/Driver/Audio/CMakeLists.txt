add_sci_library(sciAudioDriver
  LPcmDriver.c
  MidiDriver.c

  LINK_LIBS
  sciInputDriver
  sciLogger
  sciUtils
  )
