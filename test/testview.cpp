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

bool TestView::eventFilter(QObject *obj, QEvent *event)
{
    if (m_viewOnly) {
        if (event->type() == QEvent::KeyPress ||
                event->type() == QEvent::KeyRelease ||
                event->type() == QEvent::MouseButtonDblClick ||
                event->type() == QEvent::MouseButtonPress ||
                event->type() == QEvent::MouseButtonRelease ||
                event->type() == QEvent::MouseMove)
            return true;
    }
    return RemoteView::eventFilter(obj, event);
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

bool TestView::start()
{
    setStatus(Connecting);
    // call it async in order to simulate real world behavior
    QTimer::singleShot(1000, this, SLOT(asyncConnect()));
    return true;
}

HostPreferences* TestView::hostPreferences()
{
    return m_hostPreferences;
}

void TestView::switchFullscreen(bool on)
{
    Q_UNUSED(on);
}

