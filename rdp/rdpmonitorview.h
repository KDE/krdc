/*
 * SPDX-FileCopyrightText: 2026 Fabio Bas <ctrlaltca@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QCursor>
#include <QWidget>

class RdpSession;
class QFocusEvent;
class QKeyEvent;
class QResizeEvent;

class RdpMonitorView : public QWidget
{
    Q_OBJECT

public:
    RdpMonitorView(RdpSession *session, const QRect &remoteRect, QWidget *parent = nullptr);
    ~RdpMonitorView() override = default;

    void showLocalCursor(bool show);
    void setRemoteCursor(const QCursor &cursor);

Q_SIGNALS:
    void keyEventReceived(QKeyEvent *event);
    void focusLost();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private Q_SLOTS:
    void onRectangleUpdated(const QRect &remoteUpdate, const QSize &remoteSize);

private:
    void forwardInput(QEvent *event);
    void updateRemoteCursor();

    RdpSession *m_session;
    QRect m_remoteRect;
    QCursor m_remoteCursor;
    bool m_showLocalCursor = false;
};
