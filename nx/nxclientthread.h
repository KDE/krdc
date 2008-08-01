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

class NxClientThread: public QThread
{
    Q_OBJECT

public:
    explicit NxClientThread(QObject *parent = 0);
    ~NxClientThread();

    void setHost(const QString &host);
    void setPort(int port);
    void setUserName(const QString &userName);
    void setPassword(const QString &password);
    void setResolution(int width, int height);
    void setDesktopType(const QString &desktopType);
    void setKeyboardLayout(const QString &keyboardLayout);
    void setPrivateKey(const QString &privateKey);
    void stop();

protected:
    void run();

private:
    nxcl::NXClientLib m_nxClient;
    nxcl::NXClientLibExternalCallbacks m_nxClientCallbacks;
    nxcl::NXSessionData m_nxData;
    
    std::string m_host;
    int m_port;
    std::string m_privateKey;

    bool m_stopped;
    QMutex m_mutex;
};

#endif