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
** You should have received a copy of the GNU Library General Public License
** along with this library; see the file COPYING.LIB. If not, write to
** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
** Boston, MA 02110-1301, USA.
**
****************************************************************************/

#include "kremoteview.h"

KRemoteView::KRemoteView(QWidget *parent)
  : QWidget(parent),
    m_status(Disconnected),
    m_host(QString()),
    m_port(0)
{
}

KRemoteView::RemoteStatus KRemoteView::status()
{
    return m_status;
}

void KRemoteView::setStatus(KRemoteView::RemoteStatus s)
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

KRemoteView::~KRemoteView()
{
}

bool KRemoteView::supportsScaling() const
{
    return false;
}

bool KRemoteView::supportsLocalCursor() const
{
    return false;
}

QString KRemoteView::host()
{
    return m_host;
}

QSize KRemoteView::framebufferSize()
{
    return QSize(0, 0);
}

void KRemoteView::startQuitting()
{
}

bool KRemoteView::isQuitting()
{
    return false;
}

int KRemoteView::port()
{
    return m_port;
}

void KRemoteView::pressKey(XEvent *)
{
}

bool KRemoteView::viewOnly()
{
    return false;
}

void KRemoteView::setViewOnly(bool)
{
}

void KRemoteView::showDotCursor(DotCursorState)
{
}

KRemoteView::DotCursorState KRemoteView::dotCursorState() const
{
    return CursorOff;
}

bool KRemoteView::scaling() const
{
    return false;
}

void KRemoteView::enableScaling(bool)
{
}

void KRemoteView::switchFullscreen(bool)
{
}

#include "kremoteview.moc"
