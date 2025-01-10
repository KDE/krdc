/*
    SPDX-FileCopyrightText: 2002 Arend van Beelen jr. <arend@auton.nl>
    SPDX-FileCopyrightText: 2007 Urs Wolfer <uwolfer@kde.org>
    SPDX-FileCopyrightText: 2023 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef RDPVIEW_H
#define RDPVIEW_H

#include "rdpsession.h"
#include "remoteview.h"

#include "rdphostpreferences.h"

#include <QCursor>
#include <QUrl>

#define TCP_PORT_RDP 3389

class QMimeData;

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
    QSize framebufferSize() override; // returns the size of the remote view
    QSize sizeHint() const override; // returns the suggested size

    // functions regarding the connection
    void startQuitting() override; // start closing the connection
    bool isQuitting() override; // are we currently closing the connection?
    bool start() override; // open a connection

    HostPreferences *hostPreferences() override;

    bool supportsScaling() const override;
    bool supportsLocalCursor() const override;
    bool supportsViewOnly() const override;

    void setRemoteCursor(QCursor cursor);
    void showLocalCursor(LocalCursorState state) override;
    bool scaling() const override;
    void enableScaling(bool scale) override;

    QPixmap takeScreenshot() override;

    void savePassword(const QString &password);

public Q_SLOTS:
    void scaleResize(int w, int h) override;
    void onAuthRequested();
    void onVerifyCertificate(RdpSession::CertificateResult *ret, const QString &certificate);
    void onVerifyChangedCertificate(RdpSession::CertificateResult *ret, const QString &oldCertificate, const QString &newCertificate);
    void onLogonError(const QString &error);

protected:
    QSize initialSize();

    void paintEvent(QPaintEvent *event) override;
    void handleKeyEvent(QKeyEvent *event) override;
    void handleWheelEvent(QWheelEvent *event) override;
    void handleMouseEvent(QMouseEvent *event) override;
    void handleLocalClipboardChanged(const QMimeData *data) override;

private:
    void onRectangleUpdated(const QRect &rect);
    void handleError(unsigned int error);

    QString m_name;
    QString m_user;
    QString m_password;
    //
    bool m_quitting = false;

    std::unique_ptr<RdpHostPreferences> m_hostPreferences;
    std::unique_ptr<RdpSession> m_session;

    QRect m_pendingRectangle;
    QImage m_pendingData;
    QCursor m_remoteCursor;
};

#endif
