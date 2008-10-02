/****************************************************************************
**
** Copyright (C) 2002 Arend van Beelen jr. <arend@auton.nl>
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

#ifndef RDPVIEW_H
#define RDPVIEW_H

#include "remoteview.h"

#include "rdphostpreferences.h"

#include <QProcess>

#define TCP_PORT_RDP 3389
#define RDP_LOGON_NORMAL 0x33

class RdpView;

class QX11EmbedContainer;

class RdpView : public RemoteView
{
    Q_OBJECT

public:
    explicit RdpView(QWidget *parent = 0,
                     const KUrl &url = KUrl(),
                     KConfigGroup configGroup = KConfigGroup(),
                     const QString &user = QString(), const QString &password = QString(),
                     int flags = RDP_LOGON_NORMAL, const QString &domain = QString(),
                     const QString &shell = QString(), const QString &directory = QString());

    virtual ~RdpView();

    // functions regarding the window
    virtual QSize framebufferSize();         // returns the size of the remote view
    QSize sizeHint() const;                  // returns the suggested size

    // functions regarding the connection
    virtual void startQuitting();            // start closing the connection
    virtual bool isQuitting();               // are we currently closing the connection?
    virtual bool start();                    // open a connection
    void setGrabAllKeys(bool grabAllKeys);
    
    HostPreferences* hostPreferences();

public slots:
    virtual void switchFullscreen(bool on);

protected:
    bool eventFilter(QObject *obj, QEvent *event);

private:
    // properties used for setting up the connection
    QString m_name;       // name of the connection
    QString m_user;       // the user to use to log in
    QString m_password;   // the password to use
    int m_flags;      // flags which determine how the connection is set up
    QString m_domain;     // the domain where the host is on
    QString m_shell;      // the shell to use
    QString m_directory;  // the working directory on the server

    // other properties
    bool m_quitFlag;                // if set: die
    QString m_clientVersion;           // version number returned by rdesktop
    QX11EmbedContainer *m_container;   // container for the rdesktop window
    QProcess *m_process;              // rdesktop process

    RdpHostPreferences *m_hostPreferences;

private slots:
    void connectionOpened();           // called if rdesktop started
    void connectionClosed();           // called if rdesktop quits
    void processError(QProcess::ProcessError error); // called if rdesktop dies
    void receivedStandardError();      // catches rdesktop debug output
};

#endif
