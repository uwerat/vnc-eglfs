############################################################################
# VncEGLFS - Copyright (C) 2022 Uwe Rathmann
#            SPDX-License-Identifier: BSD-3-Clause
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

list(APPEND HEADERS
    va/VncJpeg.h
    va/VncVaJpegRenderer.h
    va/VncVaEncoder.h
)

list(APPEND SOURCES
    va/VncJpeg.cpp
    va/VncVaJpegRenderer.cpp
    va/VncVaEncoder.cpp
)

set(target qvnceglfs)

add_library(${target} SHARED ${SOURCES} ${HEADERS})

set_target_properties(${target} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER VncNamespace.h)

target_link_libraries(${target} PUBLIC
    Qt::Gui
    Qt::GuiPrivate
    Qt::Network
    ${OpenSSL_LIBRARIES}
    ${VideoAcceleration_LIBRARIES}
)

target_compile_definitions(${target} PRIVATE
    VNC_MAKEDLL)

target_compile_definitions(${target} PRIVATE
    -DVNC_VA_ENCODER
)

# configure_package_config_file TODO ...

install(TARGETS ${target}
        PUBLIC_HEADER
        DESTINATION include)
