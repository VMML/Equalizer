# Copyright (c) 2010 Daniel Pfeifer <daniel@pfeifer-mail.de>

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/examples/include)

macro(ADD_TOOL NAME)
  add_executable(${NAME} ${ARGN})
  list(APPEND TOOLS ${NAME})
endmacro(ADD_TOOL NAME)

set(TOOLS)

if(WIN32)
  add_tool(affinityCheck affinityCheck/affinityCheck.cpp)
  target_link_libraries(affinityCheck ${OPENGL_LIBRARIES} ${GLEW_LIBRARY})
endif(WIN32)

add_tool(configTool
  configTool/configTool.cpp
  configTool/writeFromFile.cpp
  )
target_link_libraries(configTool Equalizer EqualizerServer)

if(NOT WIN32) # TODO: make it compatible!
  add_tool(eVolveConverter
    eVolveConverter/eVolveConverter.cpp
    eVolveConverter/ddsbase.cpp
    )
endif(NOT WIN32)

add_tool(netproxy netproxy/netproxy.cpp)
target_link_libraries(netproxy Equalizer)

add_tool(windowAdmin windowAdmin/main.cpp)
target_link_libraries(windowAdmin Equalizer EqualizerAdmin)

install(TARGETS ${TOOLS} DESTINATION bin COMPONENT tools)