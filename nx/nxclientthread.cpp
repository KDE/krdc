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

#include "nxclientthread.h"
#include "nxcallbacks.h"

#include <kglobal.h>
#include <kstandarddirs.h>

#include <QDesktopWidget>
#include <QApplication>
#include <QFile>

NxClientThread::NxClientThread(QObject *parent)
        : QThread(parent),
        m_host(std::string()),
        m_port(0),
        m_privateKey(std::string()),
        m_xid(0),
        m_stopped(false)
{
    m_client.setSessionData(&m_data);

    QDesktopWidget *desktop = QApplication::desktop();
    int currentScreen = desktop->screenNumber();
    QRect rect = desktop->screenGeometry(currentScreen);
    m_client.setResolution(rect.width(), rect.height());
    m_client.setDepth(24);
    m_client.setRender(true);

    m_data.sessionName = "krdcSession";
    m_data.cache = 8;
    m_data.images = 32;
    m_data.linkType = "adsl";
    m_data.render = true;
    m_data.backingstore = "when_requested";
    m_data.imageCompressionMethod = 2;
    m_data.keyboard = "defkeymap";
    m_data.media = false;
    m_data.agentServer = "";
    m_data.agentUser = "";
    m_data.agentPass = "";
    m_data.cups = 0;
    m_data.suspended = false;
    m_data.fullscreen = false;
    m_data.encryption = true;
    m_data.terminate = false;
}

NxClientThread::~NxClientThread()
{
    stop();
    wait(500);
}

void NxClientThread::setCallbacks(NxCallbacks *callbacks)
{
    m_client.setExternalCallbacks(callbacks);
}

void NxClientThread::setHost(const QString &host)
{
    QMutexLocker locker(&m_mutex);
    QByteArray tmp = host.toAscii();
    m_host = tmp.data();
}

void NxClientThread::setPort(int port)
{
    QMutexLocker locker(&m_mutex);
    m_port = port;
}

void NxClientThread::setUserName(const QString &userName)
{
    QMutexLocker locker(&m_mutex);
    std::string userNameStr = std::string(userName.toAscii().data());
    m_client.setUsername(userNameStr);
}

void NxClientThread::setPassword(const QString &password)
{
    QMutexLocker locker(&m_mutex);
    std::string passwordStr = std::string(password.toAscii());
    m_client.setPassword(passwordStr);
}

void NxClientThread::setResolution(int width, int height)
{
    QMutexLocker locker(&m_mutex);
    m_data.geometry = width + 'x' + height + "+0+0";
}

void NxClientThread::setDesktopType(const QString &desktopType)
{
    QMutexLocker locker(&m_mutex);
    QByteArray tmp = desktopType.toAscii();
    m_data.sessionType = tmp.data();
}

void NxClientThread::setKeyboardLayout(const QString &keyboardLayout)
{
    QMutexLocker locker(&m_mutex);
    QByteArray tmp = keyboardLayout.toAscii();
    m_data.kbtype = tmp.data();
}

void NxClientThread::setPrivateKey(const QString &privateKey)
{
    QMutexLocker locker(&m_mutex);
    QByteArray tmp = privateKey.toAscii();
    m_privateKey = tmp.data();
}

void NxClientThread::setSuspended(bool suspended)
{
    QMutexLocker locker(&m_mutex);
    m_data.suspended = suspended;
}

void NxClientThread::setId(const QString &id)
{
    QMutexLocker locker(&m_mutex);
    QByteArray tmp = id.toAscii();
    m_data.id = tmp.data();
}

void NxClientThread::stop()
{
    QMutexLocker locker(&m_mutex);
    m_stopped = true;
}

void NxClientThread::run()
{
    if (m_privateKey.compare("default") == 0) {
        const QString keyfilename = QString("default.dsa.key");
        const QString keyfilepath = KGlobal::dirs()->findResource("appdata", keyfilename);

        QFile file(keyfilepath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return;

        QByteArray key;
        while (!file.atEnd())
            key += file.readLine();

        m_client.invokeNXSSH("supplied", m_host, true, key.data(), m_port);
    } else
        m_client.invokeNXSSH("supplied", m_host, true, m_privateKey, m_port);

    nxcl::notQProcess* p;
    while (!m_client.getIsFinished() && !m_stopped) {
        if (!m_client.getReadyForProxy()) {
            p = m_client.getNXSSHProcess();
            p->probeProcess();
        } else {
            p = m_client.getNXSSHProcess();
            p->probeProcess();
            p = m_client.getNXProxyProcess();
            p->probeProcess();
        }

        if (!this->m_xid) {
            this->m_xid = m_client.getXID();

            if (this->m_xid)
                emit hasXid(this->m_xid);
        }

        usleep(1000);
    }
}

void NxClientThread::startSession()
{
    m_client.runSession();
}

#include "moc_nxclientthread.cpp"
