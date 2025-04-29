cmake_minimum_required(VERSION 3.16)

project(vncproxy VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")

find_package(QT NAMES Qt5 Qt6)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Gui REQUIRED)

# Need better way to include library ?
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../src")

add_library(${PROJECT_NAME} MODULE
    VncProxyPlugin.cpp
    metadata.json
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::GuiPrivate
    Qt${QT_VERSION_MAJOR}::Gui
    vncgl )

install(TARGETS ${PROJECT_NAME}
        DESTINATION plugins/platforms)
