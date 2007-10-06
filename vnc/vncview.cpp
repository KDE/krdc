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

#include "vncview.h"

#include <KLocale>
#include <KMessageBox>
#include <KPasswordDialog>
#include <KDebug>

#include <QImage>
#include <QPainter>
#include <QMouseEvent>

VncView::VncView(QWidget *parent, const KUrl &url)
  : RemoteView(parent),
    m_initDone(false),
    m_buttonMask(0),
    m_repaint(false),
    m_quitFlag(false),
    m_firstPasswordTry(true)
{
    m_url = url;
    m_host = url.host();
    m_port = url.port();

    connect(&vncThread, SIGNAL(imageUpdated(int, int, int, int)), this, SLOT(updateImage(int, int, int, int)), Qt::BlockingQueuedConnection);
    connect(&vncThread, SIGNAL(passwordRequest()), this, SLOT(requestPassword()), Qt::BlockingQueuedConnection);
    connect(&vncThread, SIGNAL(outputErrorMessage(QString)), this, SLOT(outputErrorMessage(QString)));
}

VncView::~VncView()
{
    startQuitting();
}

bool VncView::eventFilter(QObject *obj, QEvent *event)
{
    if (m_viewOnly) {
        setCursor(Qt::ArrowCursor);

        if (event->type() == QEvent::KeyPress ||
            event->type() == QEvent::KeyRelease ||
            event->type() == QEvent::MouseButtonDblClick ||
            event->type() == QEvent::MouseButtonPress ||
            event->type() == QEvent::MouseButtonRelease ||
            event->type() == QEvent::Wheel ||
            event->type() == QEvent::MouseMove)
            return true;
    }

    setCursor(m_dotCursorState == CursorOn ? localDotCursor() : Qt::BlankCursor);

    return RemoteView::eventFilter(obj, event);
}

QSize VncView::framebufferSize()
{
    return size();
}

QSize VncView::sizeHint() const
{
    return size();
}

QSize VncView::minimumSizeHint() const
{
    return size();
}

void VncView::startQuitting()
{
    kDebug(5011) << "about to quit";

    bool connected = status() == RemoteView::Connected;

    setStatus(Disconnecting);

    m_quitFlag = true;

    if (connected) {
        vncThread.stop();
    } else {
        vncThread.quit();
    }

    vncThread.wait(500);

    setStatus(Disconnected);
}

bool VncView::isQuitting()
{
    return m_quitFlag;
}

bool VncView::start()
{
    m_hostPreferences = new VncHostPreferences(m_url.prettyUrl(KUrl::RemoveTrailingSlash), false, this);

    vncThread.setHost(m_host);
    vncThread.setPort(m_port);
    vncThread.setQuality(m_hostPreferences->quality());

    setStatus(Connecting);

    vncThread.start();
    return true;
}

bool VncView::supportsLocalCursor() const
{
    return true;
}

void VncView::requestPassword()
{
    kDebug(5011) << "request password";

    setStatus(Authenticating);

    if (m_hostPreferences->walletSupport()) {
        QString walletPassword = readWalletPassword();

        if (!walletPassword.isNull()) {
            vncThread.setPassword(walletPassword);
            return;
        }
    }

    if(!m_url.password().isNull()) {
        vncThread.setPassword(m_url.password());
        return;
    }

    KPasswordDialog dialog(this);
    dialog.setPixmap(KIcon("dialog-password").pixmap(48));
    dialog.setPrompt(m_firstPasswordTry ? i18n("Access to the system requires a password.")
                                        : i18n("Authentication failed. Please try again."));
    if (dialog.exec() == KPasswordDialog::Accepted) {
        m_firstPasswordTry = false;
        vncThread.setPassword(dialog.password());
    }
}

void VncView::outputErrorMessage(const QString &message)
{
    kDebug(5011) << message;

    startQuitting();

    KMessageBox::error(this, message, i18n("VNC failure"));
}

void VncView::updateImage(int x, int y, int w, int h)
{
//     kDebug(5011) << "got update";

    m_x = x;
    m_y = y;
    m_w = w;
    m_h = h;

    if (!m_initDone) {
        setAttribute(Qt::WA_StaticContents);
        setAttribute(Qt::WA_OpaquePaintEvent);
        installEventFilter(this);

        setCursor(m_dotCursorState == CursorOn ? localDotCursor() : Qt::BlankCursor);

        setMouseTracking(true); // get mouse events even when there is no mousebutton pressed
        setFocusPolicy(Qt::WheelFocus);
        QImage frame = vncThread.image();
        setFixedSize(frame.width(), frame.height());
        setStatus(Connected);
        emit changeSize(frame.width(), frame.height());
        emit connected();
        m_initDone = true;

        if (m_hostPreferences->walletSupport()) {
            saveWalletPassword(vncThread.password());
        }
    }

    m_repaint = true;
    repaint(x, y, w, h);
    m_repaint = false;
}

void VncView::paintEvent(QPaintEvent *event)
{
//     kDebug(5011) << "paint event: x: " << m_x << ", y: " << m_y << ", w: " << m_w << ", h: " << m_h;

    event->accept();

    QPainter painter(this);


    if (m_repaint) {
//         kDebug(5011) << "normal repaint";
        painter.drawImage(QRect(m_x, m_y, m_w, m_h), vncThread.image(m_x, m_y, m_w, m_h));
    } else {
//         kDebug(5011) << "resize repaint";
        QImage frame = vncThread.image();
        painter.drawImage(frame.rect(), frame);
    }

    QWidget::paintEvent(event);
}

void VncView::focusOutEvent(QFocusEvent *event)
{
//     kDebug(5011) << "focusOutEvent";

    if (event->reason() == Qt::TabFocusReason) {
//         kDebug(5011) << "event->reason() == Qt::TabFocusReason";
        event->ignore();
        setFocus(); // get focus back and send tab key event to remote desktop
        keyEvent(new QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier));
        keyEvent(new QKeyEvent(QEvent::KeyRelease, Qt::Key_Tab, Qt::NoModifier));
    }

    event->accept();
}

void VncView::mouseMoveEvent(QMouseEvent *event)
{
//     kDebug(5011) << "mouse move";

    mouseEvent(event);

    event->accept();
}

void VncView::mousePressEvent(QMouseEvent *event)
{
//     kDebug(5011) << "mouse press";

    mouseEvent(event);

    event->accept();
}

void VncView::mouseDoubleClickEvent(QMouseEvent *event)
{
//     kDebug(5011) << "mouse double click";

    mouseEvent(event);

    event->accept();
}

void VncView::mouseReleaseEvent(QMouseEvent *event)
{
//     kDebug(5011) << "mouse release";

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

void VncView::keyEvent(QKeyEvent *e)
{
    rfbKeySym k = 0;
    switch (e->key()) {
    case Qt::Key_Backspace: k = XK_BackSpace; break;
    case Qt::Key_Tab: k = XK_Tab; break;
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

    if (k < 26) // workaround for modified keys by pressing CTRL
        k += 96;

    vncThread.keyEvent(k, (e->type() == QEvent::KeyPress) ? true : false);
}

void VncView::keyPressEvent(QKeyEvent *event)
{
//     kDebug(5011) << "key press" << event->key();

    keyEvent(event);

    event->accept();
}

void VncView::keyReleaseEvent(QKeyEvent *event)
{
//     kDebug(5011) << "key release" << event->key();

    keyEvent(event);

    event->accept();
}

#include "vncview.moc"
