/****************************************************************************
**
** Copyright (C) 2007 Urs Wolfer <uwolfer @ kde.org>
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

#ifndef VNCVIEW_H
#define VNCVIEW_H

#include "remoteview.h"
#include "vnchostpreferences.h"
#include "vncclientthread.h"

extern "C" {
#include <rfb/rfbclient.h>
}

class QCursor;

class VncView: public RemoteView
{
    Q_OBJECT

public:
    explicit VncView(QWidget *parent = 0,
                     const KUrl &url = KUrl(),
                     RemoteView::Quality quality = Medium);
    ~VncView();

    QSize framebufferSize();
    QSize sizeHint() const;
    QSize minimumSizeHint() const;
    void startQuitting();
    bool isQuitting();
    bool start();
    bool supportsLocalCursor() const;

protected:
    void paintEvent(QPaintEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);
    void keyPressEvent(QKeyEvent *event);
    void keyReleaseEvent(QKeyEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);

private:
    VncClientThread vncThread;
    bool m_initDone;
    int m_buttonMask;
    int m_x, m_y, m_w, m_h;
    bool m_repaint;
    bool m_quitFlag;
    RemoteView::Quality m_quality;
    VncHostPreferences *m_hostPreferences;
    QCursor m_cursor;

private slots:
    void updateImage(int x, int y, int w, int h);
    void requestPassword();
    void mouseEvent(QMouseEvent *event);
    void keyEvent(QKeyEvent *e, bool pressed);
};

#endif
