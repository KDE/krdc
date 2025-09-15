/*
    SPDX-FileCopyrightText: 2007-2013 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef VNCVIEW_H
#define VNCVIEW_H

#include "remoteview.h"
#include "vncclientthread.h"
#include "vnchostpreferences.h"

#include <QMap>

extern "C" {
#include <rfb/rfbclient.h>
}

class VncView : public RemoteView
{
    Q_OBJECT

public:
    explicit VncView(QWidget *parent = nullptr, const QUrl &url = QUrl(), KConfigGroup configGroup = KConfigGroup());
    ~VncView() override;

    QSize framebufferSize() override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    bool isQuitting() override;
    bool supportsScaling() const override;
    bool supportsLocalCursor() const override;
    bool supportsViewOnly() const override;
    bool supportsClipboardSharing() const override;

    HostPreferences *hostPreferences() override;

    void showLocalCursor(LocalCursorState state) override;
    void enableScaling(bool scale) override;

    void updateConfiguration() override;

public Q_SLOTS:
    void scaleResize(int w, int h) override;

protected:
    bool startConnection() override;
    void startQuittingConnection() override;

    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void handleKeyEvent(QKeyEvent *event) override;
    void handleWheelEvent(QWheelEvent *event) override;
    void handleMouseEvent(QMouseEvent *event) override;
    void handleLocalClipboardChanged(const QMimeData *data) override;
    void handleDevicePixelRatioChange() override;

private:
    VncClientThread vncThread;
    bool m_initDone;
    int m_buttonMask;
    bool m_quitFlag;
    bool m_firstPasswordTry;
    qreal m_horizontalFactor;
    qreal m_verticalFactor;
    int m_wheelRemainderV;
    int m_wheelRemainderH;
    VncHostPreferences *m_hostPreferences;
    QImage m_frame;
    bool m_forceLocalCursor;

private Q_SLOTS:
    void updateImage(int x, int y, int w, int h);
    void setCut(const QString &text);
    void requestPassword(bool includingUsername);
    void outputErrorMessage(const QString &message);
};

#endif
