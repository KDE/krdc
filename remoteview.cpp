/****************************************************************************
**
** Copyright (C) 2002-2003 Tim Jansen <tim@tjansen.de>
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

#include "remoteview.h"

#include <QBitmap>
#include <KDebug>
#include <KStandardDirs>

RemoteView::RemoteView(QWidget *parent)
  : QWidget(parent),
    m_status(Disconnected),
    m_host(QString()),
    m_port(0),
    m_viewOnly(false),
    m_wallet(0),
    m_dotCursorState(CursorOff)
{
}

RemoteView::RemoteStatus RemoteView::status()
{
    return m_status;
}

void RemoteView::setStatus(RemoteView::RemoteStatus s)
{
    if (m_status == s)
        return;

    if (((1+(int)m_status) != (int)s) && (s != Disconnected)) {
        // follow state transition rules

        if (s == Disconnecting) {
            if (m_status == Disconnected)
                return;
        } else {
            Q_ASSERT(((int) s) >= 0);
            if (((int)m_status) > ((int)s) ) {
                m_status = Disconnected;
                emit statusChanged(Disconnected);
            }
            // smooth state transition
            int origState = (int)m_status;
            for (int i = origState; i < (int)s; i++) {
                m_status = (RemoteStatus) i;
                emit statusChanged((RemoteStatus) i);
            }
        }
    }
    m_status = s;
    emit statusChanged(m_status);
}

RemoteView::~RemoteView()
{
}

bool RemoteView::supportsScaling() const
{
    return false;
}

bool RemoteView::supportsLocalCursor() const
{
    return false;
}

QString RemoteView::host()
{
    return m_host;
}

QSize RemoteView::framebufferSize()
{
    return QSize(0, 0);
}

void RemoteView::startQuitting()
{
}

bool RemoteView::isQuitting()
{
    return false;
}

int RemoteView::port()
{
    return m_port;
}

void RemoteView::keyEvent(QKeyEvent *)
{
}

bool RemoteView::viewOnly()
{
    return m_viewOnly;
}

void RemoteView::setViewOnly(bool viewOnly)
{
    m_viewOnly = viewOnly;
}

void RemoteView::showDotCursor(DotCursorState state)
{
    m_dotCursorState = state;
}

RemoteView::DotCursorState RemoteView::dotCursorState() const
{
    return m_dotCursorState;
}

bool RemoteView::scaling() const
{
    return false;
}

void RemoteView::enableScaling(bool)
{
}

void RemoteView::switchFullscreen(bool)
{
}

KUrl RemoteView::url()
{
    return m_url;
}

QString RemoteView::readWalletPassword()
{
    QString krdc_folder = "KRDC";

    m_wallet = KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(), window()->winId());

    if (m_wallet) {
        bool walletOK = m_wallet->hasFolder(krdc_folder);
        if (!walletOK)
            walletOK = m_wallet->createFolder(krdc_folder);
            kDebug(5010) << "Wallet folder created";
        if (walletOK) {
            kDebug(5010) << "Wallet OK";
            m_wallet->setFolder(krdc_folder);
            QString password;

            if (m_wallet->hasEntry(m_url.prettyUrl(KUrl::RemoveTrailingSlash)) &&
                !m_wallet->readPassword(m_url.prettyUrl(KUrl::RemoveTrailingSlash), password)) {
                kDebug(5010) << "Password read OK";

                return password;
            }
        }
    }
    return QString();
}

void RemoteView::saveWalletPassword(const QString &password)
{
    if (m_wallet && m_wallet->isOpen() && !m_wallet->hasEntry(m_url.prettyUrl(KUrl::RemoveTrailingSlash))) {
        kDebug(5010) << "Write wallet password";
        m_wallet->writePassword(m_url.prettyUrl(KUrl::RemoveTrailingSlash), password);
    }
}

QCursor RemoteView::localDotCursor() const
{
    QBitmap cursorBitmap(KGlobal::dirs()->findResource("appdata",
                                                        "pics/pointcursor.png"));
    QBitmap cursorMask(KGlobal::dirs()->findResource("appdata",
                                                        "pics/pointcursormask.png"));
    return QCursor(cursorBitmap, cursorMask);
}

#include "remoteview.moc"
