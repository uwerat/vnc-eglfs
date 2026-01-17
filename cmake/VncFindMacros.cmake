############################################################################
# QSkinny - Copyright (C) The authors
#           SPDX-License-Identifier: BSD-3-Clause
############################################################################

macro(vnc_setup_Qt)

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

endmacro()

macro(vnc_find_packages)

    vnc_setup_Qt()

    find_package(PkgConfig REQUIRED)

    pkg_check_modules(OpenSSL REQUIRED openssl)
    
    if(BUILD_VIDEO_ACCELERATION)

        pkg_check_modules(VideoAcceleration QUIET libva)

        if ( VideoAcceleration_FOUND )
            pkg_check_modules(VideoAccelerationDrm REQUIRED libva-drm)
            set(ENABLE_VA ON)
        else()
            message(STATUS "libva not found: disabling GPU encoding")
        endif()

    endif()

endmacro()
