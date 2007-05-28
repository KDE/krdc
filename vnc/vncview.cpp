/* This file is part of the KDE project
   Copyright (C) 2007 Urs Wolfer <uwolfer @ kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING. If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "vncview.h"

#include <QImage>
#include <QPainter>
#include <QMouseEvent>
#include <QInputDialog>

VncView::VncView()
{
    m_initDone = false;

    m_buttonMask = 0;

    bool ok;
    QString text = QInputDialog::getText(this, "Address Dialog",
                                        "Please enter the VNC server you would like to connect to (e.g. 192.168.1.2:5901):", QLineEdit::Normal,
                                        "", &ok);
    if (ok) {
        int pos = text.indexOf(':');
        vncThread.setHost(text.left(pos));
        vncThread.setPort(text.right(text.length() - pos - 1).toInt());
    } else
        close();

    vncThread.start();

    connect(&vncThread, SIGNAL(imageUpdated(int, int)), this, SLOT(updateImage(int, int)), Qt::BlockingQueuedConnection);
    connect(&vncThread, SIGNAL(passwordRequest()), this, SLOT(requestPassword()), Qt::BlockingQueuedConnection);
}

VncView::~VncView()
{
}

void VncView::requestPassword()
{
//     qDebug("request password");

    bool ok;
    QString text = QInputDialog::getText(this, "Password Dialog",
                                        "Please enter the VNC password:", QLineEdit::Password,
                                        "", &ok);
    if (ok)
        vncThread.setPassword(text);
}

void VncView::updateImage(int x, int y)
{
//     qDebug("got update");

    m_x = x;
    m_y = y;

    if (!m_initDone) {
        setCursor(Qt::BlankCursor);
        setMouseTracking(true); // get mouse events even when there is no mousebutton pressed
        setAttribute(Qt::WA_OpaquePaintEvent); // keep the old background, just overwrite new parts
        setMinimumSize(vncThread.image().width(), vncThread.image().height());
        m_initDone = true;
    }

    repaint();
}

void VncView::paintEvent(QPaintEvent *event)
{
//     qDebug("paint event: x: %d, y: %d, h: %d, w: %d", m_x, m_y, m_w, m_h);

    event->accept();

    QPainter painter(this);
    painter.drawImage(QRect(m_x, m_y, vncThread.image().width(), vncThread.image().height()), vncThread.image());

    QWidget::paintEvent(event);
}

void VncView::mouseMoveEvent(QMouseEvent *event)
{
//     qDebug("mouse move");

    mouseEvent(event);

    event->accept();
}

void VncView::mousePressEvent(QMouseEvent *event)
{
//     qDebug("mouse press");

    mouseEvent(event);

    event->accept();
}

void VncView::mouseDoubleClickEvent(QMouseEvent *event)
{
//     qDebug("mouse double click");

    mouseEvent(event);

    event->accept();
}

void VncView::mouseReleaseEvent(QMouseEvent *event)
{
//     qDebug("mouse release");

    mouseEvent(event);

    event->accept();
}

void VncView::mouseEvent(QMouseEvent *e)
{
    if (e->type() != QEvent::MouseMove) {
        if ((e->type() == QEvent::MouseButtonPress) ||
            (e->type() == QEvent::MouseButtonDblClick)) {
            if (e->button() & Qt::LeftButton)
                m_buttonMask |= 0x01;
            if (e->button() & Qt::MidButton)
                m_buttonMask |= 0x02;
            if (e->button() & Qt::RightButton)
                m_buttonMask |= 0x04;
        }
        else if (e->type() == QEvent::MouseButtonRelease) {
            if (e->button() & Qt::LeftButton)
                m_buttonMask &= 0xfe;
            if (e->button() & Qt::MidButton)
                m_buttonMask &= 0xfd;
            if (e->button() & Qt::RightButton)
                m_buttonMask &= 0xfb;
        }
    }

    vncThread.mouseEvent(e->x(), e->y(), m_buttonMask);
}

void VncView::wheelEvent(QWheelEvent *event)
{
    int eb = 0;
    if (event->delta() < 0)
        eb |= 0x10;
    else
        eb |= 0x8;

    int x = event->x();
    int y = event->y();

    vncThread.mouseEvent(x, y, eb|m_buttonMask);
    vncThread.mouseEvent(x, y, m_buttonMask);
    event->accept();
}

void VncView::keyEvent(QKeyEvent *e, bool pressed)
{
    rfbKeySym k = 0;
    switch (e->key()) {
    case Qt::Key_Backspace: k = XK_BackSpace; break;
    case Qt::Key_Tab: k = XK_ISO_Left_Tab; break;
    case Qt::Key_Clear: k = XK_Clear; break;
    case Qt::Key_Return: k = XK_Return; break;
    case Qt::Key_Pause: k = XK_Pause; break;
    case Qt::Key_Escape: k = XK_Escape; break;
    case Qt::Key_Space: k = XK_space; break;
    case Qt::Key_Delete: k = XK_Delete; break;
    case Qt::Key_Enter: k = XK_KP_Enter; break;
    case Qt::Key_Equal: k = XK_KP_Equal; break;
    case Qt::Key_Up: k = XK_Up; break;
    case Qt::Key_Down: k = XK_Down; break;
    case Qt::Key_Right: k = XK_Right; break;
    case Qt::Key_Left: k = XK_Left; break;
    case Qt::Key_Insert: k = XK_Insert; break;
    case Qt::Key_Home: k = XK_Home; break;
    case Qt::Key_End: k = XK_End; break;
    case Qt::Key_PageUp: k = XK_Page_Up; break;
    case Qt::Key_PageDown: k = XK_Page_Down; break;
    case Qt::Key_F1: k = XK_F1; break;
    case Qt::Key_F2: k = XK_F2; break;
    case Qt::Key_F3: k = XK_F3; break;
    case Qt::Key_F4: k = XK_F4; break;
    case Qt::Key_F5: k = XK_F5; break;
    case Qt::Key_F6: k = XK_F6; break;
    case Qt::Key_F7: k = XK_F7; break;
    case Qt::Key_F8: k = XK_F8; break;
    case Qt::Key_F9: k = XK_F9; break;
    case Qt::Key_F10: k = XK_F10; break;
    case Qt::Key_F11: k = XK_F11; break;
    case Qt::Key_F12: k = XK_F12; break;
    case Qt::Key_F13: k = XK_F13; break;
    case Qt::Key_F14: k = XK_F14; break;
    case Qt::Key_F15: k = XK_F15; break;
    case Qt::Key_NumLock: k = XK_Num_Lock; break;
    case Qt::Key_CapsLock: k = XK_Caps_Lock; break;
    case Qt::Key_ScrollLock: k = XK_Scroll_Lock; break;
    case Qt::Key_Shift: k = XK_Shift_L; break;
    case Qt::Key_Control: k = XK_Control_L; break;
    case Qt::Key_AltGr: k = XK_Alt_R; break;
    case Qt::Key_Alt: k = XK_Alt_L; break;
    case Qt::Key_Meta: k = XK_Meta_L; break;
    case Qt::Key_Mode_switch: k = XK_Mode_switch; break;
    case Qt::Key_Help: k = XK_Help; break;
    case Qt::Key_Print: k = XK_Print; break;
    case Qt::Key_SysReq: k = XK_Sys_Req; break;
    default: break;
    }

    if (k == 0) {
        if (e->key() < 0x100)
            k = QChar(e->text().at(0)).unicode(); //respect upper- / lowercase
        else
            rfbClientLog("Unknown keysym: %d\n", e->key());
    }

    vncThread.keyEvent(k, pressed);
}

void VncView::keyPressEvent(QKeyEvent *event)
{
//     qDebug("key press");

    keyEvent(event, true);

    event->accept();
}

void VncView::keyReleaseEvent(QKeyEvent *event)
{
//     qDebug("key release");

    keyEvent(event, false);

    event->accept();
}

void VncView::closeEvent(QCloseEvent *event)
{
//     qDebug("close event");

    vncThread.cleanup();

    vncThread.stop();
    vncThread.wait();

    event->accept();
}

#include "vncview.moc"
