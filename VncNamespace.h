/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#ifndef VNC_NAMESPACE
#define VNC_NAMESPACE

#include <qglobal.h>

#ifdef VNC_DLL
    #if defined( VNC_MAKEDLL )     // create a DLL library
        #define VNC_EXPORT Q_DECL_EXPORT
    #else                          // use a DLL library
        #define VNC_EXPORT Q_DECL_IMPORT
    #endif
#endif 
    
#ifndef VNC_EXPORT
    #define VNC_EXPORT
#endif
    
namespace Vnc
{
    VNC_EXPORT void setup();
}

#endif
