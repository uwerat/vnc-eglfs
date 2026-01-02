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

target_link_libraries(${target} PUBLIC
    ${OpenSSL_LIBRARIES}
)

target_compile_definitions(${target} PRIVATE
    VNC_MAKEDLL)

# configure_package_config_file TODO ...

install(TARGETS ${target}
        PUBLIC_HEADER
        DESTINATION include)
