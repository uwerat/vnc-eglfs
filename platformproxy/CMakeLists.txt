############################################################################
# VncEGLFS - Copyright (C) 2022 Uwe Rathmann
#            SPDX-License-Identifier: BSD-3-Clause
############################################################################

cmake_minimum_required(VERSION 3.16)

set(target vncproxy)

# target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/src)

add_library(${target} MODULE
    VncProxyPlugin.cpp
    metadata.json
)

target_link_libraries(${target} PRIVATE qvnceglfs )

install(TARGETS ${target} DESTINATION plugins/platforms)
