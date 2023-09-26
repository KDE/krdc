/*
    SPDX-FileCopyrightText: 2002 Arend van Beelen jr. <arend@auton.nl>
    SPDX-FileCopyrightText: 2007 Urs Wolfer <uwolfer@kde.org>
    SPDX-FileCopyrightText: 2023 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef RDPVIEW_H
#define RDPVIEW_H

#include "remoteview.h"

#include "rdphostpreferences.h"

// #include <QProcess>
#include <QUrl>

#define TCP_PORT_RDP 3389

class RdpSession;

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

    void switchFullscreen(bool on) override;

    void savePassword(const QString &password);

protected:
    void paintEvent(QPaintEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

    void wheelEvent(QWheelEvent * event) override;

private:
    void onRectangleUpdated(const QRect &rect);

    QString m_name;
    QString m_user;
    QString m_password;
    //
    bool m_quitting = false;

    std::unique_ptr<RdpHostPreferences> m_hostPreferences;
    std::unique_ptr<RdpSession> m_session;

    QRect m_pendingRectangle;
    QImage m_pendingData;
};

#endif
