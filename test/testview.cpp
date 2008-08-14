/****************************************************************************
**
** Copyright (C) 2008 Urs Wolfer <uwolfer @ kde.org>
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

#include "testview.h"

#include <QEvent>

TestView::TestView(QWidget *parent, const KUrl &url)
        : RemoteView(parent)
{
    Q_UNUSED(url);

    setAutoFillBackground(true);

    QPalette pal = palette();
    pal.setColor(QPalette::Dark, Qt::yellow);
    setPalette(pal);

    QSize size = QSize(640, 480);
    setStatus(Connected);
    setFixedSize(size);
    setFixedSize(size);
    emit changeSize(size.width(), size.height());
    emit connected();
}

TestView::~TestView()
{
    emit disconnected();
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
    return true;
}

void TestView::switchFullscreen(bool on)
{
    Q_UNUSED(on);
}

#include "testview.moc"
