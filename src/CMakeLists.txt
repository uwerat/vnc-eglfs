cmake_minimum_required(VERSION 3.16)

project(vncgl VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")

if (NOT DEFINED CMAKE_CXX_VISIBLITY_PRESET AND
    NOT DEFINED CMAKE_VISIBILITY_INLINES_HIDDEN)
  set(CMAKE_CXX_VISIBLITY_PRESET hidden)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)
endif ()

find_package(QT NAMES Qt5 Qt6)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui Network )

find_package(PkgConfig REQUIRED)
pkg_check_modules(OpenSSL REQUIRED openssl)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic-errors -Wpedantic -Wsuggest-override -Wsuggest-final-types -Wsuggest-final-methods")
endif()

add_library(${PROJECT_NAME} SHARED
    RfbSocket.h
    RfbPixelStreamer.h
    RfbEncoder.h
    RfbInputEventHandler.h
    VncServer.h
    VncClient.h
    VncNamespace.h

    RfbSocket.cpp
    RfbPixelStreamer.cpp
    RfbEncoder.cpp
    RfbInputEventHandler.cpp
    VncServer.cpp
    VncClient.cpp
    VncNamespace.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER VncNamespace.h)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::GuiPrivate
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Network

    ${OpenSSL_LIBRARIES}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    #QT_DISABLE_DEPRECATED_BEFORE=0x060500
    VNC_MAKEDLL)

install(TARGETS ${PROJECT_NAME}
        PUBLIC_HEADER
        DESTINATION include)
