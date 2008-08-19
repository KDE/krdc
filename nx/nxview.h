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

#ifndef NXVIEW_H
#define NXVIEW_H

#include "remoteview.h"
#include "nxcallbacks.h"
#include "nxclientthread.h"
#include "nxhostpreferences.h"
#include "nxresumesessions.h"

#include <QX11EmbedContainer>

#define TCP_PORT_NX 22

class NxView : public RemoteView
{
    Q_OBJECT

public:
    explicit NxView(QWidget *parent = 0, const KUrl &url = KUrl());
    virtual ~NxView();

    // Start closing the connection
    virtual void startQuitting();
    // If we are currently closing the connection
    virtual bool isQuitting();
    // Open a connection
    virtual bool start();

    // Returns the size of the remote view
    virtual QSize framebufferSize();
    // Returns the suggested size of the remote view
    QSize sizeHint() const;
    virtual void setGrabAllKeys(bool grabAllKeys);

public slots:
    void switchFullscreen(bool on);
    void hasXid(int xid);
    void handleProgress(int id, QString msg);
    void handleSuspendedSessions(QList<nxcl::NXResumeData> sessions);
    void handleNoSessions();
    void handleAtCapacity();
    void handleNewSession();
    void handleResumeSession(QString id);
    void connectionOpened();
    void connectionClosed();

protected:
    bool eventFilter(QObject *obj, QEvent *event);

private:
    // Thread that manage NX connection
    NxClientThread m_clientThread;
    // NX Callbacks
    NxCallbacks m_callbacks;
    // If we are currently closing the connection
    bool m_quitFlag;
    // Widget which contains the NX Window
    QX11EmbedContainer *m_container;   
    // Dialog which allows user to choose NX preferences.
    NxHostPreferences *m_hostPreferences;
    // Dialog which allows user to resume NX sessions.
    NxResumeSessions m_resumeSessions;
};

#endif
