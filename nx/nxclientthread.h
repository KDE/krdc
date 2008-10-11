/****************************************************************************
**
** Copyright (C) 2008 David Gross <gdavid.devel@gmail.com>
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

#ifndef NXCLIENTTHREAD_H
#define NXCLIENTTHREAD_H

#include <KDebug>
#include <KLocale>
#include <KUrl>

#include "remoteview.h"

#include <QThread>
#include <QMutex>
#include <QString>

#include <string>

#include <nxcl/nxclientlib.h>
#include <nxcl/nxdata.h>

class NxCallbacks;

class NxClientThread: public QThread
{
    Q_OBJECT

public:
    explicit NxClientThread(QObject *parent = 0);
    ~NxClientThread();

    void setCallbacks(NxCallbacks *callbacks);
    void setHost(const QString &host);
    void setPort(int port);
    void setUserName(const QString &userName);
    void setPassword(const QString &password);
    void setResolution(int width, int height);
    void setDesktopType(const QString &desktopType);
    void setKeyboardLayout(const QString &keyboardLayout);
    void setPrivateKey(const QString &privateKey);
    void setSuspended(bool suspended);
    void setId(const QString &id);
    void stop();
    void startSession();

protected:
    void run();

signals:
    /**
     * Emitted when the X Window ID of the main NX
     * window is received.
     * @param xid the X Window ID of the main NX window
     */
    void hasXid(int xid);

private:
    nxcl::NXClientLib m_client;
    nxcl::NXSessionData m_data;

    std::string m_host;
    int m_port;
    std::string m_privateKey;
    int m_xid;

    bool m_stopped;
    QMutex m_mutex;
};

#endif
