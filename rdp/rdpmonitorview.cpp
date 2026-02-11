/*
 * SPDX-FileCopyrightText: 2026 Fabio Bas <ctrlaltca@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "rdpmonitorview.h"
#include "rdpcursor.h"

#include <QFocusEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include "rdpsession.h"

RdpMonitorView::RdpMonitorView(RdpSession *session, const QRect &remoteRect, QWidget *parent)
    : QWidget(parent)
    , m_session(session)
    , m_remoteRect(remoteRect)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    if (m_session) {
        connect(m_session, &RdpSession::rectangleUpdated, this, &RdpMonitorView::onRectangleUpdated);
        connect(m_session, &RdpSession::cursorChanged, this, &RdpMonitorView::setRemoteCursor);
    }
}

void RdpMonitorView::showLocalCursor(bool show)
{
    m_showLocalCursor = show;
    updateRemoteCursor();
}

void RdpMonitorView::paintEvent(QPaintEvent *event)
{
    if (!m_session) {
        return;
    }

    const QImage *buffer = m_session->videoBuffer();
    if (!buffer || buffer->isNull()) {
        return;
    }

    QPainter painter(this);
    painter.setClipRegion(event->region());
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    const QRect srcRect = m_remoteRect.intersected(QRect(QPoint(0, 0), buffer->size()));
    if (srcRect.isEmpty()) {
        return;
    }

    const QRect dstRect = rect();
    painter.drawImage(dstRect, *buffer, srcRect);
}

void RdpMonitorView::keyPressEvent(QKeyEvent *event)
{
    Q_EMIT keyEventReceived(event);
    event->accept();
}

void RdpMonitorView::keyReleaseEvent(QKeyEvent *event)
{
    Q_EMIT keyEventReceived(event);
    event->accept();
}

void RdpMonitorView::mousePressEvent(QMouseEvent *event)
{
    setFocus(Qt::MouseFocusReason);
    forwardInput(event);
}

void RdpMonitorView::mouseReleaseEvent(QMouseEvent *event)
{
    forwardInput(event);
}

void RdpMonitorView::mouseMoveEvent(QMouseEvent *event)
{
    forwardInput(event);
}

void RdpMonitorView::wheelEvent(QWheelEvent *event)
{
    forwardInput(event);
}

void RdpMonitorView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateRemoteCursor();
}

void RdpMonitorView::focusInEvent(QFocusEvent *event)
{
    if (m_session) {
        m_session->syncKeyState();
    }
    QWidget::focusInEvent(event);
}

void RdpMonitorView::focusOutEvent(QFocusEvent *event)
{
    Q_EMIT focusLost();
    QWidget::focusOutEvent(event);
}

void RdpMonitorView::onRectangleUpdated(const QRect &remoteUpdate, const QSize &remoteSize)
{
    Q_UNUSED(remoteSize);

    const QRect intersect = remoteUpdate.intersected(m_remoteRect);
    if (intersect.isEmpty()) {
        return;
    }

    const qreal sx = width() / static_cast<qreal>(m_remoteRect.width());
    const qreal sy = height() / static_cast<qreal>(m_remoteRect.height());

    const QRectF localUpdate{(intersect.x() - m_remoteRect.x()) * sx, (intersect.y() - m_remoteRect.y()) * sy, intersect.width() * sx, intersect.height() * sy};
    update(localUpdate.toAlignedRect());
}

void RdpMonitorView::setRemoteCursor(const QCursor &cursor)
{
    m_remoteCursor = cursor;
    updateRemoteCursor();
}

void RdpMonitorView::updateRemoteCursor()
{
    if (m_showLocalCursor) {
        setCursor(Qt::ArrowCursor);
        return;
    }

    const qreal sx = width() / static_cast<qreal>(m_remoteRect.width());
    const qreal sy = height() / static_cast<qreal>(m_remoteRect.height());
    setCursor(scaledRemoteCursor(m_remoteCursor, qMin(sx, sy)));
}

void RdpMonitorView::forwardInput(QEvent *event)
{
    if (!m_session) {
        return;
    }

    m_session->sendEvent(event, this, m_remoteRect);
}
