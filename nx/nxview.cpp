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

#include <KInputDialog>
#include <KMessageBox>
#include <KPasswordDialog>

#include <QEvent>

NxView::NxView(QWidget *parent, const KUrl &url)
        : RemoteView(parent),
        m_quitFlag(false)
{
    m_url = url;
    m_host = url.host();
    m_port = url.port();

    if (m_port <= 0 || m_port >= 65536) {
        m_port = TCP_PORT_NX;
    }

    m_container = new QX11EmbedContainer(this);
    m_container->installEventFilter(this);
}

NxView::~NxView()
{
    startQuitting();
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

    bool connected = status() == RemoteView::Connected;
    setStatus(Disconnecting);
    m_quitFlag = true;

    if (connected) {
        nxClientThread.stop();
    } else {
        nxClientThread.quit();
    }

    nxClientThread.wait(500);
    setStatus(Disconnected);
    m_container->discardClient();
}

bool NxView::isQuitting()
{
    return m_quitFlag;
}

bool NxView::start()
{
    m_hostPreferences = new NxHostPreferences(m_url.prettyUrl(KUrl::RemoveTrailingSlash), false, this);

    nxClientThread.setResolution(m_hostPreferences->width(), m_hostPreferences->height());
    nxClientThread.setDesktopType(m_hostPreferences->desktopType());
    nxClientThread.setKeyboardLayout(m_hostPreferences->keyboardLayout());
    nxClientThread.setPrivateKey(m_hostPreferences->privateKey());
    
    m_container->show();

    if (m_hostPreferences->walletSupport()) {
        if (m_url.userName().isEmpty()) {
            QString userName;
            bool ok = true;

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

    nxClientThread.setHost(m_host);
    nxClientThread.setPort(m_port);
    nxClientThread.setUserName(m_url.userName());
    nxClientThread.setPassword(m_url.password());
    nxClientThread.setResolution(m_hostPreferences->width(), m_hostPreferences->height());

    setStatus(Connecting);
    nxClientThread.start();

    connect(m_container, SIGNAL(clientIsEmbedded()), SLOT(connectionOpened()));
    connect(m_container, SIGNAL(clientClosed()), SLOT(connectionClosed()));
    
    return true;
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
    if (on == true) {
        m_container->grabKeyboard();
    }
}

void NxView::connectionOpened()
{
    kDebug(5013) << "Connection opened";
    QSize size = m_container->minimumSizeHint();
    kDebug(5013) << "Size hint: " << size.width() << " " << size.height();
    setStatus(Connected);
    setFixedSize(size);
    resize(size);
    m_container->setFixedSize(size);
    emit changeSize(size.width(), size.height());
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
