cmake_minimum_required(VERSION 3.5)

project(vnc-eglfs LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Core Gui Network REQUIRED)
find_package(OpenSSL REQUIRED)

add_library(vnc-eglfs STATIC
  src/RfbSocket.cpp
  src/RfbPixelStreamer.cpp
  src/RfbEncoder.cpp
  src/RfbInputEventHandler.cpp
  src/VncServer.cpp
  src/VncClient.cpp
  src/VncNamespace.cpp
  # untitled1.json
)

target_compile_definitions(vnc-eglfs PRIVATE
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>
)

target_compile_definitions(vnc-eglfs PUBLIC
    VNC_MAKESTATIC
)

target_link_libraries(vnc-eglfs PRIVATE
    Qt5::Core Qt5::Gui Qt5::Network Qt5::GuiPrivate
    OpenSSL::SSL
    $<$<PLATFORM_ID:Windows>:ws2_32>
)

target_include_directories(vnc-eglfs PUBLIC src)
#install(TARGETS vnc-eglfs)

#install(TARGETS vnc-eglfs
#    RUNTIME DESTINATION "/usr/lib"
#    #${CMAKE_INSTALL_PREFIX}
#)

# target_compile_definitions(vnc-eglfs PRIVATE UNTITLED1_LIBRARY)
