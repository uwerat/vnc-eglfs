############################################################################
# VncEGLFS - Copyright (C) 2022 Uwe Rathmann
#           SPDX-License-Identifier: BSD-3-Clause
############################################################################

cmake_minimum_required(VERSION 3.16)

list(APPEND HEADERS
    RfbSocket.h
    RfbPixelStreamer.h
    RfbEncoder.h
    RfbInputEventHandler.h
    VncServer.h
    VncClient.h
    VncNamespace.h
)

list(APPEND SOURCES
    RfbSocket.cpp
    RfbPixelStreamer.cpp
    RfbEncoder.cpp
    RfbInputEventHandler.cpp
    VncServer.cpp
    VncClient.cpp
    VncNamespace.cpp
)

set(target qvnceglfs)

if (NOT DEFINED CMAKE_CXX_VISIBLITY_PRESET AND
    NOT DEFINED CMAKE_VISIBILITY_INLINES_HIDDEN)
  set(CMAKE_CXX_VISIBLITY_PRESET hidden)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)
endif ()

add_library(${target} SHARED ${SOURCES} ${HEADERS})

set_target_properties(${target} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER VncNamespace.h)

target_link_libraries(${target} PUBLIC
    Qt::Gui Qt::GuiPrivate Qt::Network ${OpenSSL_LIBRARIES}
)

target_compile_definitions(${target} PRIVATE
    VNC_MAKEDLL)

install(TARGETS ${target}
        PUBLIC_HEADER
        DESTINATION include)
