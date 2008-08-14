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

#include <kglobal.h>
#include <kstandarddirs.h>

#include <QDesktopWidget>
#include <QApplication>
#include <QFile>

#include <sstream>

NxClientThread::NxClientThread(QObject *parent)
        : QThread(parent),
	  m_host(std::string()),
	  m_port(0),
	  m_privateKey(std::string()),
	  m_xid(0),
	  m_stopped(false)
{
    m_nxClient.setSessionData(&m_nxData);
    m_nxClient.setExternalCallbacks(&m_nxClientCallbacks);
    
    QDesktopWidget *desktop = QApplication::desktop();
    int currentScreen = desktop->screenNumber();
    QRect rect = desktop->screenGeometry(currentScreen);
    m_nxClient.setResolution(rect.width(), rect.height());
    m_nxClient.setDepth(24);
    m_nxClient.setRender(true);
				 
    m_nxData.sessionName = "krdcSession";
    m_nxData.cache = 8;
    m_nxData.images = 32;
    m_nxData.linkType = "adsl";
    m_nxData.render = true;
    m_nxData.backingstore = "when_requested";
    m_nxData.imageCompressionMethod = 2;
    m_nxData.keyboard = "defkeymap";
    m_nxData.media = false;
    m_nxData.agentServer = "";
    m_nxData.agentUser = "";
    m_nxData.agentPass = "";
    m_nxData.cups = 0;
    m_nxData.suspended = false;
    m_nxData.fullscreen = false;
    m_nxData.encryption = true;
    m_nxData.terminate = false;
}

NxClientThread::~NxClientThread()
{
    stop();
    wait(500);
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
    QByteArray tmp = userName.toAscii();
    std::string strUserName = tmp.data();
    m_nxClient.setUsername(strUserName);
}

void NxClientThread::setPassword(const QString &password)
{
    QMutexLocker locker(&m_mutex);
    QByteArray tmp = password.toAscii();
    std::string strPassword = tmp.data();
    m_nxClient.setPassword(strPassword);
}

void NxClientThread::setResolution(int width, int height)
{
    QMutexLocker locker(&m_mutex);
    stringstream ss;
    ss << width << "x" << height << "+0+0";
    m_nxData.geometry = ss.str();
}

void NxClientThread::setDesktopType(const QString &desktopType)
{
    QMutexLocker locker(&m_mutex);
    QByteArray tmp = desktopType.toAscii();
    m_nxData.sessionType = tmp.data();
}

void NxClientThread::setKeyboardLayout(const QString &keyboardLayout)
{
    QMutexLocker locker(&m_mutex);
    QByteArray tmp = keyboardLayout.toAscii();
    m_nxData.kbtype = tmp.data();
}

void NxClientThread::setPrivateKey(const QString &privateKey)
{
    QMutexLocker locker(&m_mutex);
    QByteArray tmp = privateKey.toAscii();
    m_privateKey = tmp.data();
}

void NxClientThread::stop()
{
    QMutexLocker locker(&m_mutex);
    m_stopped = true;
}

void NxClientThread::run()
{
    if(m_privateKey.compare("default") == 0) {
        QString keyfilename = QString("default.dsa.key");
    	QString keyfilepath = KGlobal::dirs()->findResource("appdata", keyfilename);

        QFile file(keyfilepath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return;

        QByteArray key;
        while (!file.atEnd())
            key += file.readLine();

        m_nxClient.invokeNXSSH("supplied", m_host, true, key.data(), m_port);
    } else 
        m_nxClient.invokeNXSSH("supplied", m_host, true, m_privateKey, m_port);

    m_nxClient.runSession();

    nxcl::notQProcess* p;
    while ((m_nxClient.getIsFinished()) == false && !m_stopped) {
        if (m_nxClient.getReadyForProxy() == false) {
            p = m_nxClient.getNXSSHProcess();
            p->probeProcess();
        } else {
            p = m_nxClient.getNXSSHProcess();
            p->probeProcess();
            p = m_nxClient.getNXProxyProcess();
            p->probeProcess();
        }

	if (!this->m_xid) {
	    this->m_xid = m_nxClient.getXID();

	    if(this->m_xid) 
	        emit hasXid(this->m_xid);
        }

        usleep (1000);
    }
}

#include "moc_nxclientthread.cpp"
