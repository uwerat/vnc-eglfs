############################################################################
# VncEGLFS - Copyright (C) 2022 Uwe Rathmann
#            SPDX-License-Identifier: BSD-3-Clause
############################################################################

cmake_minimum_required(VERSION 3.16)

macro(find_packages)

    # relying on cmake heuristics to select a specific Qt version is no good idea.
    # using -DCMAKE_PREFIX_PATH="..." is highly recommended

    set( QT_NO_PRIVATE_MODULE_WARNING ON )

    find_package(QT "5.6" NAMES Qt6 Qt5)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui Network )

    if(QT_VERSION_MAJOR VERSION_GREATER_EQUAL 6)
        if(QT_VERSION_MINOR VERSION_GREATER_EQUAL 9)
            find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
        endif()
    endif()

    if ( QT_FOUND )
            
        # Would like to have a status message about where the Qt installation
        # has been found without having the mess of CMAKE_FIND_DEBUG_MODE
        # With Qt6 there seems to be: _qt_cmake_dir
                
        message(STATUS "Found Qt ${QT_VERSION} ${_qt_cmake_dir}")
    else()
        message(FATAL_ERROR "Couldn't find any Qt package !")
    endif()

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(OpenSSL REQUIRED openssl)

endmacro()

macro(setup)

    message( STATUS "Build Type: ${CMAKE_BUILD_TYPE}" )

    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC OFF)
    set(CMAKE_AUTOUIC OFF)

    set(CMAKE_GLOBAL_AUTOGEN_TARGET OFF)
    set(AUTOGEN_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/autogen")

    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

    #################
    # compiler flags
    #################

    add_compile_definitions(QT_NO_KEYWORDS)
    # add_compile_definitions(QT_DISABLE_DEPRECATED_BEFORE=0x000000)

    if(QT_VERSION_MAJOR VERSION_EQUAL 5)
        add_compile_definitions(QT_STRICT_ITERATORS)
    endif()

    if( (CMAKE_CXX_COMPILER_ID MATCHES "Clang") OR (CMAKE_CXX_COMPILER_ID MATCHES "GNU") )

        if (CMAKE_BUILD_TYPE MATCHES Debug)
            add_compile_options(-O0)
        else()
            add_compile_options(-O3)
        endif()

        add_compile_options(-ffast-math)

        add_compile_options(-Wall -Wextra)
        add_compile_options(-pedantic-errors)
        add_compile_options(-Wpedantic)
        add_compile_options(-Wsuggest-override)
        add_compile_options(-Wsuggest-final-types)
        add_compile_options(-Wsuggest-final-methods)
            
        if ( BUILD_PEDANTIC )

            message( STATUS "Setting pedantic compiler flags" )

            add_compile_options(-pedantic)
            add_compile_options(-pedantic-errors)

            add_compile_options(-Wformat -Werror=format-security)
            add_compile_options(-Wsuggest-override)
        
            add_compile_options(-Wextra-semi)
            add_compile_options(-Winline)

            add_compile_options(-Wmissing-declarations)
            add_compile_options(-Wredundant-decls)

            add_compile_options(-Wnon-virtual-dtor)
            add_compile_options(-Woverloaded-virtual)
            # add_compile_options(-Wfloat-equal)

            if ( CMAKE_CXX_COMPILER_ID MATCHES "GNU" )
    
                add_compile_options( -Wsuggest-attribute=pure)
                add_compile_options( -Wsuggest-attribute=const)
                add_compile_options( -Wsuggest-attribute=noreturn)
                add_compile_options( -Wsuggest-attribute=malloc)
                add_compile_options( -Wsuggest-final-types)
                add_compile_options( -Wsuggest-final-methods)
        
                add_compile_options( -Wduplicated-branches )
                add_compile_options( -Wduplicated-cond )
                add_compile_options( -Wshadow=local)
    
                # we have a lot of useless casts, when qreal is a double,
                # that are needed when qreal is a float. 

                # add_compile_options( -Wuseless-cast )

                add_compile_options( -Wlogical-op)
                add_compile_options( -Wclass-memaccess )
    
            else()

                add_compile_options( -Weverything)
                add_compile_options( -Wno-c++98-compat)
                add_compile_options( -Wno-c++98-compat-pedantic )
                add_compile_options( -Wno-global-constructors )

                # We have a lot of dummy destructors with the intention to
                #     - a breakpoint for the debugger
                #     - being able to add some debug statements without recompiling
                #     - preparation for binary compatible changes
                # Needs to be reconsidered: TODO ...
                add_compile_options( -Wno-deprecated-copy-with-user-provided-dtor )

                add_compile_options( -Wno-signed-enum-bitfield )
                add_compile_options( -Wno-padded )

                # since Qt 6.3 Q_GLOBAL_STATIC seems to use what is not supported for < c++20 
                add_compile_options( -Wno-gnu-zero-variadic-macro-arguments )
            endif()

        endif()

    endif()

endmacro()

project(vnceglfs
    LANGUAGES CXX
    HOMEPAGE_URL "https://github.com/uwerat/vnc-eglfs"
    VERSION 1.0.0)

option(BUILD_PEDANTIC       "Enable pedantic compile flags ( only GNU/CLANG )" OFF)
option(BUILD_PLATFORM_PROXY "Build the platformproxy plugin" ON)

find_packages()
setup()

add_subdirectory(src)

if(BUILD_PLATFORM_PROXY)
    add_subdirectory(platformproxy)
endif()
