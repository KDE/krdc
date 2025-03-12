/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "testview.h"

#include <QEvent>
#include <QTimer>

TestView::TestView(QWidget *parent, const QUrl &url, KConfigGroup configGroup)
    : RemoteView(parent)
{
    m_hostPreferences = new TestHostPreferences(configGroup, this);

    Q_UNUSED(url);
}

TestView::~TestView()
{
    Q_EMIT disconnected();
    setStatus(Disconnected);
}

void TestView::handleKeyEvent(QKeyEvent *event)
{
    Q_UNUSED(event);
}

void TestView::handleMouseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
}

void TestView::handleWheelEvent(QWheelEvent *event)
{
    Q_UNUSED(event);
}

void TestView::asyncConnect()
{
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::yellow);
    setPalette(pal);
    setAutoFillBackground(true);

    const QSize size = QSize(640, 480);
    setFixedSize(size);
    resize(size);
    setStatus(Connected);
    Q_EMIT framebufferSizeChanged(size.width(), size.height());
    Q_EMIT connected();
    setFocus();
}

QSize TestView::framebufferSize()
{
    return minimumSizeHint();
}

QSize TestView::sizeHint() const
{
    return maximumSize();
}

bool TestView::isQuitting()
{
    return false;
}

bool TestView::startConnection()
{
    setStatus(Connecting);
    // call it async in order to simulate real world behavior
    QTimer::singleShot(1000, this, SLOT(asyncConnect()));
    return true;
}

HostPreferences *TestView::hostPreferences()
{
    return m_hostPreferences;
}

void TestView::startQuittingConnection()
{
}

void TestView::handleLocalClipboardChanged(const QMimeData *data)
{
    Q_UNUSED(data);
}
