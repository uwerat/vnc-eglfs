/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "RfbInputEventHandler.h"
#include <qpa/qwindowsysteminterface.h>
#include <qguiapplication.h>
#include <unordered_map>
#include <cctype>
#include <QDebug>

namespace Rfb
{
    static const std::unordered_map< quint32, Qt::Key > keyTable(

        {
            { 0xff61, Qt::Key_Print },
            { 0xff13, Qt::Key_Pause },
            { 0xff14, Qt::Key_ScrollLock },
            { 0xff67, Qt::Key_Menu },

            { 0xff08, Qt::Key_Backspace },
            { 0xff09, Qt::Key_Tab       },
            { 0xff0d, Qt::Key_Return    },
            { 0xff1b, Qt::Key_Escape    },
            { 0xff63, Qt::Key_Insert    },
            { 0xffff, Qt::Key_Delete    },
            { 0xff50, Qt::Key_Home      },
            { 0xff57, Qt::Key_End       },
            { 0xff55, Qt::Key_PageUp    },
            { 0xff56, Qt::Key_PageDown  },
            { 0xff51, Qt::Key_Left      },
            { 0xff52, Qt::Key_Up        },
            { 0xff53, Qt::Key_Right     },
            { 0xff54, Qt::Key_Down      },

            { 0xffe1, Qt::Key_Shift     },
            { 0xffe2, Qt::Key_Shift     },
            { 0xffe3, Qt::Key_Control   },
            { 0xffe4, Qt::Key_Control   },
            { 0xffe7, Qt::Key_Meta      },
            { 0xffe8, Qt::Key_Meta      },
            { 0xffe9, Qt::Key_Alt       },
            { 0xffea, Qt::Key_Alt       },

            { 0xff8d, Qt::Key_Return    },
            { 0xffaa, Qt::Key_Asterisk  },
            { 0xffab, Qt::Key_Plus      },
            { 0xffad, Qt::Key_Minus     },
            { 0xffae, Qt::Key_Period    },
            { 0xffaf, Qt::Key_Slash     },

            { 0xff95, Qt::Key_Home      },
            { 0xff96, Qt::Key_Left      },
            { 0xff97, Qt::Key_Up        },
            { 0xff98, Qt::Key_Right     },
            { 0xff99, Qt::Key_Down      },
            { 0xff9a, Qt::Key_PageUp    },
            { 0xff9b, Qt::Key_PageDown  },
            { 0xff9c, Qt::Key_End       },
            { 0xff9e, Qt::Key_Insert    },
            { 0xff9f, Qt::Key_Delete    }

            // to be completed ...
        }
    );
}

void Rfb::handlePointerEvent( const QPointF& pos, quint8 buttonMask, QWindow* window )
{
    enum
    {
        ButtonLeft   = 1 << 0,
        ButtonMiddle = 1 << 1,
        ButtonRight  = 1 << 2,

        WheelUp      = 1 << 3,
        WheelDown    = 1 << 4,
        WheelLeft    = 1 << 5,
        WheelRight   = 1 << 6,

        ButtonMask   = ( ButtonLeft | ButtonMiddle | ButtonRight ),
        WheelMask    = ( WheelUp | WheelDown | WheelLeft | WheelRight )
    };

    const auto keyboardModifiers = QGuiApplication::keyboardModifiers();

    if ( buttonMask & WheelMask )
    {
        QPoint delta;

        if ( buttonMask & WheelUp )
        {
            delta.setY( 1 );
        }
        else if ( buttonMask & WheelDown )
        {
            delta.setY( -1 );
        }

        if ( buttonMask & WheelLeft )
        {
            delta.setX( -1 );
        }
        else if ( buttonMask & WheelRight )
        {
            delta.setX( 1 );
        }

        delta *= QWheelEvent::DefaultDeltasPerStep;

        QWindowSystemInterface::handleWheelEvent(
            window, pos, pos, QPoint(), delta, keyboardModifiers );
    }
    else
    {
        Qt::MouseButtons buttons;

        if ( buttonMask & ButtonLeft )
            buttons |= Qt::LeftButton;

        if ( buttonMask & ButtonMiddle )
            buttons |= Qt::MiddleButton;

        if ( buttonMask & ButtonRight )
            buttons |= Qt::RightButton;

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)

        // what to do, when having inputs from different clients ???
        auto oldButtons = QGuiApplication::mouseButtons();
        oldButtons &= ( Qt::LeftButton | Qt::MiddleButton | Qt::RightButton );

        QEvent::Type eventType;
        Qt::MouseButton button;

        if ( buttons == oldButtons )
        {
            eventType = QEvent::MouseMove;
            button = Qt::NoButton;
        }
        else
        {
            eventType = ( buttons > oldButtons ) ?
                QEvent::MouseButtonPress : QEvent::MouseButtonRelease;

            const quint32 modified = buttons ^ oldButtons;
            // Q_ASSERT( qPopulationCount( modified ) == 1 );
            button = static_cast< Qt::MouseButton >( modified );
        }

        QWindowSystemInterface::handleMouseEvent(
            window, pos, pos, buttons, button, eventType, keyboardModifiers );
#else
        QWindowSystemInterface::handleMouseEvent(
            window, pos, pos, buttons, keyboardModifiers );
#endif
    }
}

static QString keyText( quint32 qtkey, bool isLower, Qt::KeyboardModifiers modifiers )
{
    if ( modifiers == Qt::ControlModifier )
    {
        switch( qtkey )
        {
            case Qt::Key_G:
                return QStringLiteral( "\a" );

            case Qt::Key_H:
                return QStringLiteral( "\b" );

            case Qt::Key_I:
                return QStringLiteral( "\t" );

            case Qt::Key_J:
                return QStringLiteral( "\n" );

            case Qt::Key_K:
                return QStringLiteral( "\v" );

            case Qt::Key_L:
                return QStringLiteral( "\f" );

            case Qt::Key_M:
                return QStringLiteral( "\r" );
        }
    }

    switch( qtkey )
    {
        case Qt::Key_Backspace:
            return QStringLiteral( "\b" );

        case Qt::Key_Return:
            return QStringLiteral( "\r" );
    }

    if ( qtkey >= Qt::Key_Space && qtkey <= Qt::Key_ydiaeresis )
    {
        if ( isLower )
            qtkey -= 'A' - 'a';

        return QString( QLatin1Char( static_cast< char >( qtkey ) ) );
    }

    return QString();
}

void Rfb::handleKeyEvent( quint32 keysym, bool down, QWindow* window )
{
    // see https://cgit.freedesktop.org/xorg/proto/x11proto/tree/keysymdef.h

    quint32 qtkey = 0; // corresponding to Qt::Key
    bool isLower = false;

    // isprint and friends depend on the locale, TODO ...

    if ( keysym <= 0x00ff )
    {
        if ( isprint( keysym ) )
        {
            qtkey = toupper( keysym );
            isLower = qtkey != keysym;
        }
    }
    else
    {
        if ( keysym >= 0xfe50 && keysym <= 0xfe6f )
        {
            // dead keys
            qtkey = Qt::Key_Dead_Grave + ( keysym - 0xfe50 );
        }
        else if ( keysym >= 0xffbe && keysym <= 0xffe0 )
        {
            // function keys
            qtkey = Qt::Key_F1 + ( keysym - 0xffbe );
        }
        else if ( keysym >= 0xffb0 && keysym <= 0xffb9 )
        {
            // Num pad
            qtkey = Qt::Key_0 + ( keysym - 0xffb0 );
        }
        else
        {
            auto it = Rfb::keyTable.find( keysym );
            if ( it != Rfb::keyTable.end() )
                qtkey = it->second;
        }
    }

#if 0
    qDebug() << QString( "0x" )
        + QString::number( keysym ) << Qt::Key( qtkey ) << unicode;
#endif
    if ( qtkey )
    {
        auto modifiers = QGuiApplication::keyboardModifiers();
        const auto text = keyText( qtkey, isLower, modifiers );

        if ( qtkey == Qt::Key_Shift )
        {
            modifiers.setFlag( Qt::ShiftModifier, down );
        }
        else if ( qtkey == Qt::Key_Control )
        {
            modifiers.setFlag( Qt::ControlModifier, down );
        }
        else if ( qtkey == Qt::Key_Alt )
        {
            modifiers.setFlag( Qt::AltModifier, down );
        }

        const auto eventType = down ? QEvent::KeyPress : QEvent::KeyRelease;

        QWindowSystemInterface::handleKeyEvent( window,
            eventType, qtkey, modifiers, text );
    }
}
