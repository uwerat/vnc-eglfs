/******************************************************************************
 * VncEGLFS - Copyright (C) 2022 Uwe Rathmann
 *            SPDX-License-Identifier: BSD-3-Clause
 *****************************************************************************/

#pragma once

#include <qobject.h>
#include <qimage.h>
#include <qvector.h>
#include <memory>

class VncFrame;
class QWindow;
class QReadWriteLock;

class VncCursor
{
  public:
    QImage image;
    QPoint hotspot;
};

class VncServer final : public QObject
{
    Q_OBJECT

  public:
    VncServer( int port, QWindow* );
    ~VncServer() override;

    QWindow* window() const;
    int port() const;

    void setTimerInterval( int ms );

    QSize windowBufferSize() const;

    QVector< VncFrame > grabFrames(
        const QVector< QRect >&, int qualityLevel ) const;

    VncCursor cursor() const;

  private Q_SLOTS:
    void copyWindowBuffer();
    void pauseServer();

  private:
    void addClient( qintptr fd );
    void removeClient();

    class PrivateData;
    std::unique_ptr< PrivateData > m_data;
};

