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
** You should have received a copy of the GNU General Public License
** along with this program; see the file COPYING. If not, write to
** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
** Boston, MA 02110-1301, USA.
**
****************************************************************************/

#ifndef VNCCLIENTTHREAD_H
#define VNCCLIENTTHREAD_H

#include "remoteview.h"

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
    void stop();
    void setHost(const QString &host);
    void setPort(int port);
    void setQuality(RemoteView::Quality quality);
    void setPassword(const QString &password) { m_password = password; }
    const QString password() const { return m_password; }

    RemoteView::Quality quality() const;

signals:
    void imageUpdated(int x, int y, int w, int h);
    void passwordRequest();
    void outputMessage(const QString &message);

public slots:
    void mouseEvent(int x, int y, int buttonMask);
    void keyEvent(int key, bool pressed);

protected:
    void run();

private:
    static char* passwdHandler(rfbClient *cl);
    static void outputHandler(const char *format, ...);

    QImage m_image;
    rfbClient *cl;
    QString m_host;
    QString m_password;
    int m_port;
    QMutex mutex;
    RemoteView::Quality m_quality;

    volatile bool m_stopped;
    volatile bool m_passwordError;

private slots:
    void checkOutputMessage();
};

#endif
