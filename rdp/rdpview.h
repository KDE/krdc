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
#include <QUrl>

#define TCP_PORT_RDP 3389

class RdpView;

class QX11EmbedContainer;

class RdpView : public RemoteView
{
    Q_OBJECT

public:
    explicit RdpView(QWidget *parent = 0,
                     const QUrl &url = QUrl(),
                     KConfigGroup configGroup = KConfigGroup(),
                     const QString &user = QString(),
                     const QString &password = QString());

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
    
    virtual QPixmap takeScreenshot();

public Q_SLOTS:
    virtual void switchFullscreen(bool on);

protected:
    bool eventFilter(QObject *obj, QEvent *event);

private:
    QString keymapToXfreerdp(const QString &keyboadLayout);
    QHash<QString, QString> initKeymapToXfreerdp();

    QString m_name;
    QString m_user;
    QString m_password;

    bool m_quitFlag;
    QWindow *m_container;   // container for the xfreerdp window
    QProcess *m_process;               // xfreerdp process

    RdpHostPreferences *m_hostPreferences;

private Q_SLOTS:
    void connectionOpened();           // called if xfreerdp started
    void connectionClosed();           // called if xfreerdp quits
    void connectionError();            // called if xfreerdp quits with error
    void processError(QProcess::ProcessError error); // called if xfreerdp dies
    void receivedStandardError();      // catches xfreerdp debug output
    void receivedStandardOutput();     // catches xfreerdp output
};

static QHash<QString, QString> keymapToXfreerdpHash;

#endif
