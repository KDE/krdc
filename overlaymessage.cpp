/*
    SPDX-FileCopyrightText: 2026 Tony Murray <murraytony@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "overlaymessage.h"

#include <QEvent>
#include <QPainter>

static constexpr int s_margin = 16;
static constexpr qreal s_radius = 6;

OverlayMessage::OverlayMessage(QWidget *parent)
    : QLabel(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAlignment(Qt::AlignCenter);
    setContentsMargins(12, 8, 12, 8);
    setForegroundRole(QPalette::ToolTipText);
    hide();

    m_hideTimer.setSingleShot(true);
    connect(&m_hideTimer, &QTimer::timeout, this, &QWidget::hide);

    parent->installEventFilter(this);
}

void OverlayMessage::showMessage(const QString &text, int timeout)
{
    if (!isVisible() || text != this->text()) {
        setText(text);
        reposition();
        show();
        raise();
    }

    if (timeout > 0) {
        m_hideTimer.start(timeout);
    } else {
        m_hideTimer.stop();
    }
}

bool OverlayMessage::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parent() && event->type() == QEvent::Resize && isVisible()) {
        reposition();
    }
    return QLabel::eventFilter(watched, event);
}

void OverlayMessage::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor border = palette().color(QPalette::ToolTipText);
    border.setAlphaF(0.3);
    painter.setPen(border);
    painter.setBrush(palette().color(QPalette::ToolTipBase));
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), s_radius, s_radius);
    painter.end();

    QLabel::paintEvent(event);
}

void OverlayMessage::reposition()
{
    const QWidget *parent = parentWidget();
    const int maximumWidth = qMax(100, parent->width() - 2 * s_margin);

    // Only wrap when needed: the size hint of a wrapping label is narrower than necessary
    setWordWrap(false);
    QSize size = sizeHint();
    if (size.width() > maximumWidth) {
        setWordWrap(true);
        size = QSize(maximumWidth, heightForWidth(maximumWidth));
    }

    setGeometry((parent->width() - size.width()) / 2, parent->height() - size.height() - 2 * s_margin, size.width(), size.height());
}
