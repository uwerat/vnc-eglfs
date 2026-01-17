############################################################################
# VncEGLFS - Copyright (C) 2022 Uwe Rathmann
#            SPDX-License-Identifier: BSD-3-Clause
############################################################################

cmake_minimum_required(VERSION 3.16)

project(vnceglfs
    LANGUAGES CXX
    HOMEPAGE_URL "https://github.com/uwerat/vnc-eglfs"
    VERSION 1.0.0)

set(VNC_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(VNC_CMAKE_DIR ${VNC_SOURCE_DIR}/cmake)

option(BUILD_PEDANTIC "Enable pedantic compile flags ( only GNU/CLANG )" OFF)
option(BUILD_PLATFORM_PROXY "Build the platformproxy plugin" ON)
option(BUILD_VIDEO_ACCELERATION "Enable GPU encoding" ON)
option(BUILD_AUTHENTIFICATION "Enable authentification" ON)

include(GNUInstallDirs)

# loading helper macros
list(APPEND CMAKE_MODULE_PATH ${VNC_CMAKE_DIR})

include("VncConfigMacros")
include("VncFindMacros")

vnc_find_packages()
vnc_configure_flags()

add_subdirectory(src)

if(BUILD_PLATFORM_PROXY)
    add_subdirectory(platformproxy)
endif()
