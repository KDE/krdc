/****************************************************************************
**
** Copyright (C) 2007 Urs Wolfer <uwolfer @ kde.org>
**
** This file is part of KDE.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU Library General Public License
** along with this library; see the file COPYING.LIB. If not, write to
** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
** Boston, MA 02110-1301, USA.
**
****************************************************************************/

#ifndef VNCCLIENTTHREAD_H
#define VNCCLIENTTHREAD_H

#include <QThread>
#include <QImage>
#include <QMutex>

extern "C" {
#include <rfb/rfbclient.h>
}

class VncClientThread: public QThread
{
    Q_OBJECT

public:
    explicit VncClientThread();
    ~VncClientThread();
    const QImage image(int x = 0, int y = 0, int w = 0, int h = 0);
    void setImage(const QImage &img);
    void emitUpdated(int x, int y, int w, int h);
    void emitPasswordRequest();
    void stop();
    void setHost(const QString &host);
    void setPort(int port);
    void setPassword(const QString &password);
    const QString password() const;

signals:
    void imageUpdated(int x, int y, int w, int h);
    void passwordRequest();

public slots:
    void mouseEvent(int x, int y, int buttonMask);
    void keyEvent(int key, bool pressed);
    void cleanup();

protected:
    void run();

private:
    QImage m_image;
    rfbClient *cl;
    volatile bool m_stopped;
    volatile bool m_cleanup;
    QString m_host;
    QString m_password;
    int m_port;
    QMutex mutex;
};

#endif
