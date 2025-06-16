/*
    SPDX-FileCopyrightText: 2007-2013 Urs Wolfer <uwolfer@kde.org>
    SPDX-FileCopyrightText: 2021 Rafał Lalik <rafallalik@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "vncview.h"
#include "krdc_debug.h"

#include <QApplication>
#include <QImage>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>

#ifdef QTONLY
#include <QInputDialog>
#define error(parent, message, caption) critical(parent, caption, message)
#else
#include "settings.h"
#include <KActionCollection>
#include <KMainWindow>
#include <KPasswordDialog>
#include <KXMLGUIClient>
#endif

// Definition of key modifier mask constants
#define KMOD_Alt_R 0x01
#define KMOD_Alt_L 0x02
#define KMOD_Meta_L 0x04
#define KMOD_Control_L 0x08
#define KMOD_Shift_L 0x10

VncView::VncView(QWidget *parent, const QUrl &url, KConfigGroup configGroup)
    : RemoteView(parent)
    , m_initDone(false)
    , m_buttonMask(0)
    , m_quitFlag(false)
    , m_firstPasswordTry(true)
    , m_horizontalFactor(1.0)
    , m_verticalFactor(1.0)
    , m_wheelRemainderV(0)
    , m_wheelRemainderH(0)
    , m_forceLocalCursor(false)
{
    m_url = url;
    m_host = url.host();
    m_port = url.port();

    if (m_port <= 0) // port is invalid or empty...
        m_port = 5900; // fallback: try an often used VNC port

    if (m_port < 100) // the user most likely used the short form (e.g. :1)
        m_port += 5900;

    // BlockingQueuedConnection can cause deadlocks when exiting, handled in startQuitting()
    connect(&vncThread, SIGNAL(imageUpdated(int, int, int, int)), this, SLOT(updateImage(int, int, int, int)), Qt::BlockingQueuedConnection);
    connect(&vncThread, SIGNAL(gotCut(QString)), this, SLOT(setCut(QString)), Qt::BlockingQueuedConnection);
    connect(&vncThread, SIGNAL(passwordRequest(bool)), this, SLOT(requestPassword(bool)), Qt::BlockingQueuedConnection);
    connect(&vncThread, SIGNAL(outputErrorMessage(QString)), this, SLOT(outputErrorMessage(QString)));
    connect(&vncThread, &VncClientThread::gotCursor, this, [this](QCursor cursor) {
        setCursor(cursor);
    });

#ifndef QTONLY
    m_hostPreferences = new VncHostPreferences(configGroup, this);
#else
    Q_UNUSED(configGroup);
#endif
}

VncView::~VncView()
{
    if (!m_quitFlag)
        startQuitting();
}

QSize VncView::framebufferSize()
{
    return m_frame.size() / devicePixelRatioF();
}

QSize VncView::sizeHint() const
{
    return size();
}

QSize VncView::minimumSizeHint() const
{
    return size();
}

void VncView::scaleResize(int w, int h)
{
    RemoteView::scaleResize(w, h);

    qCDebug(KRDC) << w << h;
    if (m_scale) {
        const QSize frameSize = m_frame.size() / m_frame.devicePixelRatio();

        m_verticalFactor = static_cast<qreal>(h) / frameSize.height() * m_factor;
        m_horizontalFactor = static_cast<qreal>(w) / frameSize.width() * m_factor;

#ifndef QTONLY
        if (Settings::keepAspectRatio()) {
            m_verticalFactor = m_horizontalFactor = qMin(m_verticalFactor, m_horizontalFactor);
        }
#else
        m_verticalFactor = m_horizontalFactor = qMin(m_verticalFactor, m_horizontalFactor);
#endif

        const qreal newW = frameSize.width() * m_horizontalFactor;
        const qreal newH = frameSize.height() * m_verticalFactor;
        setMaximumSize(newW, newH); // This is a hack to force Qt to center the view in the scroll area
        resize(newW, newH);
    }
}

void VncView::updateConfiguration()
{
    RemoteView::updateConfiguration();

    // Update the scaling mode in case KeepAspectRatio changed
    scaleResize(parentWidget()->width(), parentWidget()->height());
}

void VncView::startQuittingConnection()
{
    // Already quitted. No need to clean up again and also avoid triggering
    // `disconnected` signal below.
    if (m_quitFlag)
        return;

    qCDebug(KRDC) << "about to quit";

    setStatus(Disconnecting);

    m_quitFlag = true;

    vncThread.stop();

    unpressModifiers();

    // Disconnect all signals so that we don't get any more callbacks from the client thread
    vncThread.disconnect();

    vncThread.quit();

    const bool quitSuccess = vncThread.wait(500);
    if (!quitSuccess) {
        // happens when vncThread wants to call a slot via BlockingQueuedConnection,
        // needs an event loop in this thread so execution continues after 'emit'
        QEventLoop loop;
        if (!loop.processEvents()) {
            qCDebug(KRDC) << "BUG: deadlocked, but no events to deliver?";
        }
        vncThread.wait(500);
    }

    qCDebug(KRDC) << "Quit VNC thread success:" << quitSuccess;

    // emit the disconnect siginal only after all the events are handled.
    // Otherwise some error messages might be thrown away without
    // showing to the user.
    Q_EMIT disconnected();
    setStatus(Disconnected);
}

bool VncView::isQuitting()
{
    return m_quitFlag;
}

bool VncView::startConnection()
{
    // This flag is used to make sure `startQuitting` only run once.
    // This should not matter for now but it is an easy way to make sure
    // things works in case the object may get reused.
    m_quitFlag = false;

    vncThread.setHost(m_host);
    RemoteView::Quality quality;
#ifdef QTONLY
    quality = (RemoteView::Quality)((QCoreApplication::arguments().count() > 2) ? QCoreApplication::arguments().at(2).toInt() : 2);
#else
    quality = m_hostPreferences->quality();
#endif

    vncThread.setQuality(quality);
    vncThread.setDevicePixelRatio(devicePixelRatioF());

    // set local cursor on by default because low quality mostly means slow internet connection
    if (quality == RemoteView::Low) {
        showLocalCursor(RemoteView::CursorOn);
#ifndef QTONLY
        // KRDC does always just have one main window, so at(0) is safe
        KXMLGUIClient *mainWindow = dynamic_cast<KXMLGUIClient *>(KMainWindow::memberList().at(0));
        if (mainWindow)
            mainWindow->actionCollection()->action(QLatin1String("show_local_cursor"))->setChecked(true);
#endif
    }

    setStatus(Connecting);

    vncThread.setPort(m_port);
    vncThread.start();
    return true;
}

bool VncView::supportsScaling() const
{
    return true;
}

bool VncView::supportsLocalCursor() const
{
    return true;
}

bool VncView::supportsViewOnly() const
{
    return true;
}

bool VncView::supportsClipboardSharing() const
{
    return true;
}

void VncView::requestPassword(bool includingUsername)
{
    qCDebug(KRDC) << "request password";

    setStatus(Authenticating);

    if (m_firstPasswordTry && !m_url.userName().isNull()) {
        vncThread.setUsername(m_url.userName());
    }

#ifndef QTONLY
    // just try to get the password from the wallet the first time, otherwise it will loop (see issue #226283)
    if (m_firstPasswordTry && m_hostPreferences->walletSupport()) {
        QString walletPassword = readWalletPassword();

        if (!walletPassword.isNull()) {
            vncThread.setPassword(walletPassword);
            m_firstPasswordTry = false;
            return;
        }
    }
#endif

    if (m_firstPasswordTry && !m_url.password().isNull()) {
        vncThread.setPassword(m_url.password());
        m_firstPasswordTry = false;
        return;
    }

#ifdef QTONLY
    bool ok;
    if (includingUsername) {
        QString username = QInputDialog::getText(this, // krazy:exclude=qclasses (code not used in kde build)
                                                 tr("Username required"),
                                                 tr("Please enter the username for the remote desktop:"),
                                                 QLineEdit::Normal,
                                                 m_url.userName(),
                                                 &ok); // krazy:exclude=qclasses
        if (ok)
            vncThread.setUsername(username);
        else
            startQuitting();
    }

    QString password = QInputDialog::getText(this, // krazy:exclude=qclasses
                                             tr("Password required"),
                                             tr("Please enter the password for the remote desktop:"),
                                             QLineEdit::Password,
                                             QString(),
                                             &ok); // krazy:exclude=qclasses
    m_firstPasswordTry = false;
    if (ok)
        vncThread.setPassword(password);
    else
        startQuitting();
#else
    KPasswordDialog dialog(this, includingUsername ? KPasswordDialog::ShowUsernameLine : KPasswordDialog::NoFlags);
    dialog.setPrompt(m_firstPasswordTry ? i18n("Access to the system requires a password.") : i18n("Authentication failed. Please try again."));
    if (includingUsername)
        dialog.setUsername(m_url.userName());
    if (dialog.exec() == KPasswordDialog::Accepted) {
        m_firstPasswordTry = false;
        vncThread.setPassword(dialog.password());
        if (includingUsername)
            vncThread.setUsername(dialog.username());
    } else {
        qCDebug(KRDC) << "password dialog not accepted";
        startQuitting();
    }
#endif
}

void VncView::outputErrorMessage(const QString &message)
{
    qCritical(KRDC) << message;

    if (message == QLatin1String("INTERNAL:APPLE_VNC_COMPATIBILTY")) {
        setCursor(localDefaultCursor());
        m_forceLocalCursor = true;
        return;
    }

    startQuitting();

    Q_EMIT errorMessage(i18n("VNC failure"), message);
}

#ifndef QTONLY
HostPreferences *VncView::hostPreferences()
{
    return m_hostPreferences;
}
#endif

void VncView::updateImage(int x, int y, int w, int h)
{
    // qCDebug(KRDC) << "got update" << width() << height();

    m_frame = vncThread.image();

    if (!m_initDone) {
        if (!vncThread.username().isEmpty()) {
            m_url.setUserName(vncThread.username());
        }
        setAttribute(Qt::WA_StaticContents);
        setAttribute(Qt::WA_OpaquePaintEvent);

        setCursor(((m_localCursorState == CursorOn) || m_forceLocalCursor) ? localDefaultCursor() : Qt::BlankCursor);

        setFocusPolicy(Qt::WheelFocus);
        setStatus(Connected);
        Q_EMIT connected();

        if (m_scale) {
#ifndef QTONLY
            qCDebug(KRDC) << "Setting initial size w:" << m_hostPreferences->width() << " h:" << m_hostPreferences->height();
            QSize frameSize = QSize(m_hostPreferences->width(), m_hostPreferences->height()) / devicePixelRatioF();
            Q_EMIT framebufferSizeChanged(frameSize.width(), frameSize.height());
            scaleResize(frameSize.width(), frameSize.height());
            qCDebug(KRDC) << "m_frame.size():" << m_frame.size() << "size()" << size();
#else
// TODO: qtonly alternative
#endif
        }

        m_initDone = true;

#ifndef QTONLY
        if (m_hostPreferences->walletSupport()) {
            saveWalletPassword(vncThread.password());
        }
#endif
    }

    const QSize frameSize = m_frame.size() / m_frame.devicePixelRatio();
    if ((y == 0 && x == 0) && (frameSize != size())) {
        qCDebug(KRDC) << "Updating framebuffer size";
        if (m_scale) {
            setMaximumSize(QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));
            if (parentWidget())
                scaleResize(parentWidget()->width(), parentWidget()->height());
        } else {
            qCDebug(KRDC) << "Resizing: " << m_frame.width() << m_frame.height();
            resize(frameSize);
            setMaximumSize(frameSize); // This is a hack to force Qt to center the view in the scroll area
            setMinimumSize(frameSize);
            Q_EMIT framebufferSizeChanged(frameSize.width(), frameSize.height());
        }
    }

    const auto dpr = m_frame.devicePixelRatio();
    repaint(QRectF(x / dpr * m_horizontalFactor, y / dpr * m_verticalFactor, w / dpr * m_horizontalFactor, h / dpr * m_verticalFactor)
                .toAlignedRect()
                .adjusted(-1, -1, 1, 1));
}

void VncView::showLocalCursor(LocalCursorState state)
{
    RemoteView::showLocalCursor(state);

    if (state == CursorOn) {
        // show local cursor, hide remote one
        setCursor(localDefaultCursor());
        vncThread.setShowLocalCursor(true);
    } else {
        // hide local cursor, show remote one
        setCursor(Qt::BlankCursor);
        vncThread.setShowLocalCursor(false);
    }
}

void VncView::enableScaling(bool scale)
{
    RemoteView::enableScaling(scale);

    if (scale) {
        setMaximumSize(QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));
        setMinimumSize(1, 1);
        if (parentWidget())
            scaleResize(parentWidget()->width(), parentWidget()->height());
    } else {
        m_verticalFactor = 1.0;
        m_horizontalFactor = 1.0;

        const QSize frameSize = m_frame.size() / m_frame.devicePixelRatio();
        setMaximumSize(frameSize); // This is a hack to force Qt to center the view in the scroll area
        setMinimumSize(frameSize);
        resize(frameSize);
    }
}

void VncView::setCut(const QString &text)
{
    QMimeData *data = new QMimeData;
    data->setText(text);
    remoteClipboardChanged(data);
}

void VncView::paintEvent(QPaintEvent *event)
{
    // qCDebug(KRDC) << "paint event: x: " << m_x << ", y: " << m_y << ", w: " << m_w << ", h: " << m_h;
    if (m_frame.isNull() || m_frame.format() == QImage::Format_Invalid) {
        qCDebug(KRDC) << "no valid image to paint";
        RemoteView::paintEvent(event);
        return;
    }

    event->accept();

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const auto dpr = m_frame.devicePixelRatio();
    const QRectF dstRect = event->rect();
    const QRectF srcRect(dstRect.x() * dpr / m_horizontalFactor,
                         dstRect.y() * dpr / m_verticalFactor,
                         dstRect.width() * dpr / m_horizontalFactor,
                         dstRect.height() * dpr / m_verticalFactor);
    painter.drawImage(dstRect, m_frame, srcRect);

    RemoteView::paintEvent(event);
}

void VncView::resizeEvent(QResizeEvent *event)
{
    RemoteView::resizeEvent(event);
    update();
}

void VncView::handleMouseEvent(QMouseEvent *e)
{
    if (e->type() != QEvent::MouseMove) {
        if ((e->type() == QEvent::MouseButtonPress) || (e->type() == QEvent::MouseButtonDblClick)) {
            if (e->button() & Qt::LeftButton)
                m_buttonMask |= 0x01;
            if (e->button() & Qt::MiddleButton)
                m_buttonMask |= 0x02;
            if (e->button() & Qt::RightButton)
                m_buttonMask |= 0x04;
            if (e->button() & Qt::ExtraButton1)
                m_buttonMask |= 0x80;
        } else if (e->type() == QEvent::MouseButtonRelease) {
            if (e->button() & Qt::LeftButton)
                m_buttonMask &= 0xfe;
            if (e->button() & Qt::MiddleButton)
                m_buttonMask &= 0xfd;
            if (e->button() & Qt::RightButton)
                m_buttonMask &= 0xfb;
            if (e->button() & Qt::ExtraButton1)
                m_buttonMask &= ~0x80;
        }
    }

    const auto dpr = devicePixelRatioF();
    QPointF screenPos = e->globalPosition();
    // We need to restore mouse position in device coordinates.
    // QMouseEvent::localPos() can be rounded (bug in Qt), but QMouseEvent::screenPos() is not.
    QPointF pos = (e->pos() + (screenPos - screenPos.toPoint())) * dpr;
    vncThread.mouseEvent(qRound(pos.x() / m_horizontalFactor), qRound(pos.y() / m_verticalFactor), m_buttonMask);
}

void VncView::handleWheelEvent(QWheelEvent *event)
{
    const auto delta = event->angleDelta();
    // Reset accumulation if direction changed
    const int accV = (delta.y() < 0) == (m_wheelRemainderV < 0) ? m_wheelRemainderV : 0;
    const int accH = (delta.x() < 0) == (m_wheelRemainderH < 0) ? m_wheelRemainderH : 0;
    // A wheel tick is 15° or 120 eights of a degree
    const int verTicks = (delta.y() + accV) / 120;
    const int horTicks = (delta.x() + accH) / 120;
    m_wheelRemainderV = (delta.y() + accV) % 120;
    m_wheelRemainderH = (delta.x() + accH) % 120;

    const auto dpr = devicePixelRatioF();
    // We need to restore mouse position in device coordinates.
    const QPointF pos = event->position() * dpr;

    const int x = qRound(pos.x() / m_horizontalFactor);
    const int y = qRound(pos.y() / m_verticalFactor);

    // Fast movement might generate more than one tick, loop for each axis
    int eb = verTicks < 0 ? 0x10 : 0x08;
    for (int i = 0; i < std::abs(verTicks); i++) {
        vncThread.mouseEvent(x, y, eb | m_buttonMask);
        vncThread.mouseEvent(x, y, m_buttonMask);
    }

    eb = horTicks < 0 ? 0x40 : 0x20;
    for (int i = 0; i < std::abs(horTicks); i++) {
        vncThread.mouseEvent(x, y, eb | m_buttonMask);
        vncThread.mouseEvent(x, y, m_buttonMask);
    }

    event->accept();
}

void VncView::handleKeyEvent(QKeyEvent *e)
{
    // strip away autorepeating KeyRelease; see bug #206598
    if (e->isAutoRepeat() && (e->type() == QEvent::KeyRelease))
        return;

    // parts of this code are based on https://github.com/veyon/veyon/blob/master/core/src/VncView.cpp
    rfbKeySym k = e->nativeVirtualKey();

    // we do not handle Key_Backtab separately as the Shift-modifier
    // is already enabled
    if (e->key() == Qt::Key_Backtab) {
        k = XK_Tab;
    }

    const bool pressed = (e->type() == QEvent::KeyPress);
    if (k) {
        vncThread.keyEvent(k, pressed);
    }
}

void VncView::handleLocalClipboardChanged(const QMimeData *data)
{
#ifndef QTONLY
    if (m_hostPreferences->dontCopyPasswords()) {
        if (data->hasFormat(QLatin1String("x-kde-passwordManagerHint"))) {
            qCDebug(KRDC) << "VncView::clipboardDataChanged data hasFormat x-kde-passwordManagerHint -- ignoring";
            return;
        }
    }
#endif

    // TODO: VNC backend doesn't support other formats like  hasImage(), hasHtml()
    if (data->hasText()) {
        const QString text = data->text();
        vncThread.clientCut(text);
    }
}

#include "moc_vncview.cpp"
