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

#include "nxview.h"
#include "settings.h"

#include <nxcl/nxdata.h>

#include <KInputDialog>
#include <KMessageBox>
#include <KPasswordDialog>

#include <QEvent>
#include <QMetaType>

NxView::NxView(QWidget *parent, const KUrl &url, KConfigGroup configGroup)
        : RemoteView(parent),
        m_quitFlag(false),
	m_container(NULL),
	m_hostPreferences(NULL)
{
    m_url = url;
    m_host = url.host();
    m_port = url.port();

    if (m_port <= 0 || m_port >= 65536)
        m_port = TCP_PORT_NX;

    m_container = new QX11EmbedContainer(this);
    m_container->installEventFilter(this);

    qRegisterMetaType<QList<nxcl::NXResumeData> >("QList<nxcl::NXResumeData>");

    m_clientThread.setCallbacks(&m_callbacks);
    
    connect(&m_clientThread, SIGNAL(hasXid(int)), this, SLOT(hasXid(int)));
    connect(&m_callbacks, SIGNAL(progress(int, QString)), this, SLOT(handleProgress(int, QString)));
    connect(&m_callbacks, SIGNAL(suspendedSessions(QList<nxcl::NXResumeData>)), this, SLOT(handleSuspendedSessions(QList<nxcl::NXResumeData>)));
    connect(&m_callbacks, SIGNAL(noSessions()), this, SLOT(handleNoSessions()));
    connect(&m_callbacks, SIGNAL(atCapacity()), this, SLOT(handleAtCapacity()));
    connect(&m_resumeSessions, SIGNAL(newSession()), this, SLOT(handleNewSession()));
    connect(&m_resumeSessions, SIGNAL(resumeSession(QString)), this, SLOT(handleResumeSession(QString)));
    
    m_hostPreferences = new NxHostPreferences(configGroup, this);
}

NxView::~NxView()
{
}

// filter out key and mouse events to the container if we are view only
//FIXME: X11 events are passed to the app before getting caught in the Qt event processing
bool NxView::eventFilter(QObject *obj, QEvent *event)
{
    if (m_viewOnly) {
        if (event->type() == QEvent::KeyPress ||
                event->type() == QEvent::KeyRelease ||
                event->type() == QEvent::MouseButtonDblClick ||
                event->type() == QEvent::MouseButtonPress ||
                event->type() == QEvent::MouseButtonRelease ||
                event->type() == QEvent::MouseMove)
            return true;
    }

    return RemoteView::eventFilter(obj, event);
}

void NxView::startQuitting()
{
    kDebug(5013) << "about to quit";

    const bool connected = status() == RemoteView::Connected;
    setStatus(Disconnecting);
    m_quitFlag = true;

    if (connected)
        m_clientThread.stop();
    else
        m_clientThread.quit();

    m_clientThread.wait(500);
    setStatus(Disconnected);
    m_container->discardClient();
}

bool NxView::isQuitting()
{
    return m_quitFlag;
}

bool NxView::start()
{
    m_clientThread.setResolution(m_hostPreferences->width(), m_hostPreferences->height());
    m_clientThread.setDesktopType(m_hostPreferences->desktopType());
    m_clientThread.setKeyboardLayout(m_hostPreferences->keyboardLayout());
    m_clientThread.setPrivateKey(m_hostPreferences->privateKey());
    
    m_container->show();

    if (m_hostPreferences->walletSupport()) {
        if (m_url.userName().isEmpty()) {
            QString userName;
            bool ok = false;

            userName = KInputDialog::getText(i18n("Enter Username"),
                                             i18n("Please enter the username you would like to use for login."),
                                             QString(), &ok, this);

            if (ok)
                m_url.setUserName(userName);
        }

        if (!m_url.userName().isEmpty()) {
            QString walletPassword = readWalletPassword();

            if (!walletPassword.isNull())
                m_url.setPassword(walletPassword);
	    else {
                KPasswordDialog dialog(this);
                dialog.setPrompt(i18n("Access to the system requires a password."));
                if (dialog.exec() == KPasswordDialog::Accepted) {
                    m_url.setPassword(dialog.password());

                    if (m_hostPreferences->walletSupport())
                        saveWalletPassword(dialog.password());
                }
            }
        }
    }

    m_clientThread.setHost(m_host);
    m_clientThread.setPort(m_port);
    m_clientThread.setUserName(m_url.userName());
    m_clientThread.setPassword(m_url.password());
    m_clientThread.setResolution(m_hostPreferences->width(), m_hostPreferences->height());

    setStatus(Connecting);
    m_clientThread.start();

    connect(m_container, SIGNAL(clientIsEmbedded()), SLOT(connectionOpened()));
    connect(m_container, SIGNAL(clientClosed()), SLOT(connectionClosed()));
    
    return true;
}

HostPreferences* NxView::hostPreferences()
{
    return m_hostPreferences;
}

QSize NxView::framebufferSize()
{
    return m_container->minimumSizeHint();
}

QSize NxView::sizeHint() const
{
    return maximumSize();
}


void NxView::switchFullscreen(bool on)
{
    setGrabAllKeys(on);
}

void NxView::setGrabAllKeys(bool grabAllKeys)
{
    m_grabAllKeys = grabAllKeys;

    if (grabAllKeys) {
        m_keyboardIsGrabbed = true;
        m_container->grabKeyboard();
    } else if (m_keyboardIsGrabbed)
        m_container->releaseKeyboard();
}

void NxView::hasXid(int xid) 
{
    m_container->embedClient(xid);
}

void NxView::handleProgress(int id, QString msg)
{
    switch (id) {
        case NXCL_AUTH_FAILED:
            KMessageBox::error(this, i18n("The authentication key is invalid."), i18n("Invalid authentication key"), 0);
            break;
        case NXCL_LOGIN_FAILED:
            KMessageBox::error(this, i18n("The username or password that you have entered is not a valid."), i18n("Invalid username or password"), 0);
            break;
        case NXCL_HOST_KEY_VERIFAILED:
            KMessageBox::error(this, i18n("The host key verification has failed."), i18n("Host key verification failed"), 0);
            break;
        case NXCL_PROCESS_ERROR:
            KMessageBox::error(this, i18n("An error has occurred during the connection to the NX server."), i18n("Process error"), 0);
            break;
        default:
            break;
    }
}

void NxView::handleSuspendedSessions(QList<nxcl::NXResumeData> sessions)
{
    if (!m_resumeSessions.empty())
        m_resumeSessions.clear();

    m_resumeSessions.addSessions(sessions);
    m_resumeSessions.show();
}

void NxView::handleNoSessions()
{
    m_clientThread.setSuspended(false);
    m_clientThread.startSession();
}

void NxView::handleAtCapacity()
{
    KMessageBox::error(this, i18n("This NX server is running at capacity."), i18n("Server at capacity"), 0);
}

void NxView::handleNewSession() 
{
    m_clientThread.setSuspended(false);
    m_clientThread.startSession();
}

void NxView::handleResumeSession(QString id)
{
    m_clientThread.setId(id);
    m_clientThread.setSuspended(true);
    m_clientThread.startSession();
}

void NxView::connectionOpened()
{
    kDebug(5013) << "Connection opened";
    QSize size(m_hostPreferences->width(), m_hostPreferences->height());
    kDebug(5013) << "Size hint: " << size.width() << size.height();
    setStatus(Connected);
    setFixedSize(size);
    resize(size);
    m_container->setFixedSize(size);
    emit framebufferSizeChanged(size.width(), size.height());
    emit connected();
    setFocus();
}

void NxView::connectionClosed()
{
    emit disconnected();
    setStatus(Disconnected);
    m_quitFlag = true;
}

#include "nxview.moc"
