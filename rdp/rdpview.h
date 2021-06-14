/*
    SPDX-FileCopyrightText: 2002 Arend van Beelen jr. <arend@auton.nl>
    SPDX-FileCopyrightText: 2007 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
    explicit RdpView(QWidget *parent = nullptr,
                     const QUrl &url = QUrl(),
                     KConfigGroup configGroup = KConfigGroup(),
                     const QString &user = QString(),
                     const QString &password = QString());

    ~RdpView() override;

    // functions regarding the window
    QSize framebufferSize() override;         // returns the size of the remote view
    QSize sizeHint() const override;                  // returns the suggested size

    // functions regarding the connection
    void startQuitting() override;            // start closing the connection
    bool isQuitting() override;               // are we currently closing the connection?
    bool start() override;                    // open a connection
    void setGrabAllKeys(bool grabAllKeys) override;
    
    HostPreferences* hostPreferences() override;
    
    QPixmap takeScreenshot() override;

public Q_SLOTS:
    void switchFullscreen(bool on) override;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    // Marks if connectionClosed should close the connection if m_quitFlag is true.
    enum CloseType {
        NormalClose,
        ForceClose,
    };

    void connectionError(const QString &text,
                         const QString &caption); // called if xfreerdp quits with error
    void connectionClosed(CloseType closeType); // Signals the connection closed if not quitting or it is forced

    QString keymapToXfreerdp(const QString &keyboadLayout);
    QHash<QString, QString> initKeymapToXfreerdp();

    QString m_name;
    QString m_user;
    QString m_password;

    bool m_quitFlag;
    QWindow *m_container;   // container for the xfreerdp window
    QWidget *m_containerWidget; // Widget to contain the xfreerdp window.
    QProcess *m_process;               // xfreerdp process

    RdpHostPreferences *m_hostPreferences;

private Q_SLOTS:
    void connectionOpened();           // called if xfreerdp started
    void connectionClosed();           // called if xfreerdp quits
    void processError(QProcess::ProcessError error); // called if xfreerdp dies
    void receivedStandardError();      // catches xfreerdp debug output
    void receivedStandardOutput();     // catches xfreerdp output
};

static QHash<QString, QString> keymapToXfreerdpHash;

#endif
