############################################################################
# VncEGLFS - Copyright (C) 2022 Uwe Rathmann
#            SPDX-License-Identifier: BSD-3-Clause
############################################################################

cmake_minimum_required(VERSION 3.16)

list(APPEND HEADERS
    RfbSocket.h
    RfbPixelStreamer.h
    RfbInputEventHandler.h
    VncServer.h
    VncClient.h
    VncFrame.h
    VncFrameGrabber.h
    VncNamespace.h
)

list(APPEND SOURCES
    RfbSocket.cpp
    RfbPixelStreamer.cpp
    RfbInputEventHandler.cpp
    VncServer.cpp
    VncClient.cpp
    VncFrame.cpp
    VncFrameGrabber.cpp
    VncNamespace.cpp
)

list(APPEND HEADERS
    va/VncEgl.h
    va/VncVa.h
    va/VncVaConverter.h
    va/VncVaEncoder.h
    va/VncVaApplication.h
)

list(APPEND SOURCES
    va/VncEgl.cpp
    va/VncVa.cpp
    va/VncVaConverter.cpp
    va/VncVaEncoder.cpp
    va/VncVaApplication.cpp
)

set(target qvnceglfs)

add_library(${target} SHARED ${SOURCES} ${HEADERS})

set_target_properties(${target} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER VncNamespace.h)

if(QT_VERSION_MAJOR LESS 6 AND QT_VERSION_MINOR LESS 7)
    target_link_libraries(${target} PRIVATE
        Qt5::Gui Qt5::Network
    )
    target_include_directories(${target} PRIVATE
        ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
    )
else()
    target_link_libraries(${target} PRIVATE
        Qt::Gui Qt::GuiPrivate Qt::Network
    )
endif()

target_compile_definitions(${target} PRIVATE VNC_MAKEDLL)

if ( BUILD_AUTHENTIFICATION )
    target_compile_definitions(${target} PRIVATE -DVNC_SSL)

    add_library(OpenSSL INTERFACE IMPORTED)
    target_link_libraries(${target} PRIVATE ${OpenSSL_LIBRARIES})
endif()

if ( BUILD_VIDEO_ACCELERATION )
    target_compile_definitions(${target} PRIVATE -DVNC_VA_ENCODER)

    add_library(VideoAcceleration INTERFACE IMPORTED)
    target_link_libraries(${target} PRIVATE ${VideoAcceleration_LIBRARIES})
endif()

# configure_package_config_file TODO ...

install(TARGETS ${target}
        PUBLIC_HEADER
        DESTINATION include)
