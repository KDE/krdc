/*
    SPDX-FileCopyrightText: 2007-2013 Urs Wolfer <uwolfer@kde.org>
    SPDX-FileCopyrightText: 2021 Rafał Lalik <rafallalik@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "vncview.h"
#include "krdc_debug.h"

#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QMouseEvent>
#include <QTimer>
#include <QMimeData>

#ifdef QTONLY
    #include <QMessageBox>
    #include <QInputDialog>
    #define KMessageBox QMessageBox
    #define error(parent, message, caption) \
        critical(parent, caption, message)
#else
    #include "settings.h"
    #include <KActionCollection>
    #include <KXmlGui/KMainWindow>
    #include <KWidgetsAddons/KMessageBox>
    #include <KWidgetsAddons/KPasswordDialog>
    #include <KXmlGui/KXMLGUIClient>
#endif

// Definition of key modifier mask constants
#define KMOD_Alt_R 	0x01
#define KMOD_Alt_L 	0x02
#define KMOD_Meta_L 	0x04
#define KMOD_Control_L 	0x08
#define KMOD_Shift_L	0x10

VncView::VncView(QWidget *parent, const QUrl &url, KConfigGroup configGroup)
        : RemoteView(parent),
        m_initDone(false),
        m_buttonMask(0),
        m_quitFlag(false),
        m_firstPasswordTry(true),
        m_dontSendClipboard(false),
        m_horizontalFactor(1.0),
        m_verticalFactor(1.0),
        m_forceLocalCursor(false)
#ifdef LIBSSH_FOUND
        , m_sshTunnelThread(nullptr)
#endif
{
    m_url = url;
    m_host = url.host();
    m_port = url.port();

    if (m_port <= 0) // port is invalid or empty...
        m_port = 5900; // fallback: try an often used VNC port

    if (m_port < 100) // the user most likely used the short form (e.g. :1)
        m_port += 5900;

    // BlockingQueuedConnection can cause deadlocks when exiting, handled in startQuitting()
    connect(&vncThread, SIGNAL(imageUpdated(int,int,int,int)), this, SLOT(updateImage(int,int,int,int)), Qt::BlockingQueuedConnection);
    connect(&vncThread, SIGNAL(gotCut(QString)), this, SLOT(setCut(QString)), Qt::BlockingQueuedConnection);
    connect(&vncThread, SIGNAL(passwordRequest(bool)), this, SLOT(requestPassword(bool)), Qt::BlockingQueuedConnection);
    connect(&vncThread, SIGNAL(outputErrorMessage(QString)), this, SLOT(outputErrorMessage(QString)));
    connect(&vncThread, &VncClientThread::gotCursor, this, [this](QCursor cursor){ setCursor(cursor); });

    m_clipboard = QApplication::clipboard();
    connect(m_clipboard, SIGNAL(dataChanged()), this, SLOT(clipboardDataChanged()));

#ifndef QTONLY
    m_hostPreferences = new VncHostPreferences(configGroup, this);
#else
    Q_UNUSED(configGroup);
#endif
}

VncView::~VncView()
{
    if (!m_quitFlag) startQuitting();
}

bool VncView::eventFilter(QObject *obj, QEvent *event)
{
    if (m_viewOnly) {
        if (event->type() == QEvent::KeyPress ||
                event->type() == QEvent::KeyRelease ||
                event->type() == QEvent::MouseButtonDblClick ||
                event->type() == QEvent::MouseButtonPress ||
                event->type() == QEvent::MouseButtonRelease ||
                event->type() == QEvent::Wheel ||
                event->type() == QEvent::MouseMove)
            return true;
    }

    return RemoteView::eventFilter(obj, event);
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
        const qreal _newW = (frameSize.width() - w) * m_factor + w;
        const qreal _newH = (frameSize.height() - h) * m_factor + h;

        m_verticalFactor = _newH / frameSize.height();
        m_horizontalFactor = _newW / frameSize.width();

#ifndef QTONLY
        if (Settings::keepAspectRatio()) {
            m_verticalFactor = m_horizontalFactor = qMin(m_verticalFactor, m_horizontalFactor);
        }
#else
        m_verticalFactor = m_horizontalFactor = qMin(m_verticalFactor, m_horizontalFactor);
#endif

        const qreal newW = frameSize.width() * m_horizontalFactor;
        const qreal newH = frameSize.height() * m_verticalFactor;
        setMaximumSize(newW, newH); //This is a hack to force Qt to center the view in the scroll area
        resize(newW, newH);
    }
}

void VncView::updateConfiguration()
{
    RemoteView::updateConfiguration();

    // Update the scaling mode in case KeepAspectRatio changed
    scaleResize(parentWidget()->width(), parentWidget()->height());
}

void VncView::startQuitting()
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

#ifdef LIBSSH_FOUND
    if (m_sshTunnelThread) {
        delete m_sshTunnelThread;
        m_sshTunnelThread = nullptr;
    }
#endif

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

bool VncView::start()
{
    // This flag is used to make sure `startQuitting` only run once.
    // This should not matter for now but it is an easy way to make sure
    // things works in case the object may get reused.
    m_quitFlag = false;

    QString vncHost = m_host;
    int vncPort = m_port;
#ifdef LIBSSH_FOUND
    if (m_hostPreferences->useSshTunnel()) {
        Q_ASSERT(!m_sshTunnelThread);

        const int tunnelPort = 58219; // Just a random port

        m_sshTunnelThread = new VncSshTunnelThread(m_host.toUtf8(),
                                                   m_port,
                                                   tunnelPort,
                                                   m_hostPreferences->sshTunnelPort(),
                                                   m_hostPreferences->sshTunnelUserName().toUtf8(),
                                                   m_hostPreferences->useSshTunnelLoopback());
        connect(m_sshTunnelThread, &VncSshTunnelThread::passwordRequest, this, &VncView::sshRequestPassword, Qt::BlockingQueuedConnection);
        connect(m_sshTunnelThread, &VncSshTunnelThread::errorMessage, this, &VncView::sshErrorMessage);
        m_sshTunnelThread->start();

        if (m_hostPreferences->useSshTunnelLoopback()) {
            vncHost = QStringLiteral("127.0.0.1");
        }
        vncPort = tunnelPort;
    }
#endif

    vncThread.setHost(vncHost);
    vncThread.setPort(vncPort);
    RemoteView::Quality quality;
#ifdef QTONLY
    quality = (RemoteView::Quality)((QCoreApplication::arguments().count() > 2) ?
        QCoreApplication::arguments().at(2).toInt() : 2);
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
        KXMLGUIClient *mainWindow = dynamic_cast<KXMLGUIClient*>(KMainWindow::memberList().at(0));
        if (mainWindow)
            mainWindow->actionCollection()->action(QLatin1String("show_local_cursor"))->setChecked(true);
#endif
    }

    setStatus(Connecting);

#ifdef LIBSSH_FOUND
    if (m_hostPreferences->useSshTunnel()) {
        connect(m_sshTunnelThread, &VncSshTunnelThread::listenReady, this, [this] { vncThread.start(); });
    }
    else
#endif
    {
        vncThread.start();
    }
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
        QString username = QInputDialog::getText(this, //krazy:exclude=qclasses (code not used in kde build)
                                                 tr("Username required"),
                                                 tr("Please enter the username for the remote desktop:"),
                                                 QLineEdit::Normal, m_url.userName(), &ok); //krazy:exclude=qclasses
        if (ok)
            vncThread.setUsername(username);
        else
            startQuitting();
    }

    QString password = QInputDialog::getText(this, //krazy:exclude=qclasses
                                             tr("Password required"),
                                             tr("Please enter the password for the remote desktop:"),
                                             QLineEdit::Password, QString(), &ok); //krazy:exclude=qclasses
    m_firstPasswordTry = false;
    if (ok)
        vncThread.setPassword(password);
    else
        startQuitting();
#else
    KPasswordDialog dialog(this, includingUsername ? KPasswordDialog::ShowUsernameLine : KPasswordDialog::NoFlags);
    dialog.setPrompt(m_firstPasswordTry ? i18n("Access to the system requires a password.")
                                        : i18n("Authentication failed. Please try again."));
    if (includingUsername) dialog.setUsername(m_url.userName());
    if (dialog.exec() == KPasswordDialog::Accepted) {
        m_firstPasswordTry = false;
        vncThread.setPassword(dialog.password());
        if (includingUsername) vncThread.setUsername(dialog.username());
    } else {
        qCDebug(KRDC) << "password dialog not accepted";
        startQuitting();
    }
#endif
}

#ifdef LIBSSH_FOUND
void VncView::sshRequestPassword(VncSshTunnelThread::PasswordRequestFlags flags)
{
    qCDebug(KRDC) << "request ssh password";

    if (m_hostPreferences->walletSupport() && ((flags & VncSshTunnelThread::IgnoreWallet) != VncSshTunnelThread::IgnoreWallet)) {
        const QString walletPassword = readWalletSshPassword();

        if (!walletPassword.isNull()) {
            m_sshTunnelThread->setPassword(walletPassword, VncSshTunnelThread::PasswordFromWallet);
            return;
        }
    }

    KPasswordDialog dialog(this);
    dialog.setPrompt(i18n("Please enter the SSH password."));
    if (dialog.exec() == KPasswordDialog::Accepted) {
        m_sshTunnelThread->setPassword(dialog.password(), VncSshTunnelThread::PasswordFromDialog);
    } else {
        qCDebug(KRDC) << "ssh password dialog not accepted";
        m_sshTunnelThread->userCanceledPasswordRequest();
        // We need to use a single shot because otherwise startQuitting deletes the thread
        // but we're here from a blocked queued connection and thus we deadlock
        QTimer::singleShot(0, this, &VncView::startQuitting);
    }
}
#endif

void VncView::outputErrorMessage(const QString &message)
{
    qCritical(KRDC) << message;

    if (message == QLatin1String("INTERNAL:APPLE_VNC_COMPATIBILTY")) {
        setCursor(localDefaultCursor());
        m_forceLocalCursor = true;
        return;
    }

    startQuitting();

    KMessageBox::error(this, message, i18n("VNC failure"));

    Q_EMIT errorMessage(i18n("VNC failure"), message);
}

void VncView::sshErrorMessage(const QString &message)
{
    qCritical(KRDC) << message;

    startQuitting();

    KMessageBox::error(this, message, i18n("SSH Tunnel failure"));

    Q_EMIT errorMessage(i18n("SSH Tunnel failure"), message);
}

#ifndef QTONLY
HostPreferences* VncView::hostPreferences()
{
    return m_hostPreferences;
}
#endif

void VncView::updateImage(int x, int y, int w, int h)
{
//     qCDebug(KRDC) << "got update" << width() << height();

    m_frame = vncThread.image();

    if (!m_initDone) {
        if (!vncThread.username().isEmpty()) {
            m_url.setUserName(vncThread.username());
        }
        setAttribute(Qt::WA_StaticContents);
        setAttribute(Qt::WA_OpaquePaintEvent);
        installEventFilter(this);

        setCursor(((m_localCursorState == CursorOn) || m_forceLocalCursor) ? localDefaultCursor() : Qt::BlankCursor);

        setMouseTracking(true); // get mouse events even when there is no mousebutton pressed
        setFocusPolicy(Qt::WheelFocus);
        setStatus(Connected);
        Q_EMIT connected();

        if (m_scale) {
#ifndef QTONLY
            qCDebug(KRDC) << "Setting initial size w:" <<m_hostPreferences->width() << " h:" << m_hostPreferences->height();
            QSize frameSize = QSize(m_hostPreferences->width(), m_hostPreferences->height()) / devicePixelRatioF();
            Q_EMIT framebufferSizeChanged(frameSize.width(), frameSize.height());
            scaleResize(frameSize.width(), frameSize.height());
            qCDebug(KRDC) << "m_frame.size():" << m_frame.size() << "size()" << size();
#else
//TODO: qtonly alternative
#endif
        }

        m_initDone = true;

#ifndef QTONLY
        if (m_hostPreferences->walletSupport()) {
            saveWalletPassword(vncThread.password());
#ifdef LIBSSH_FOUND
            if (m_hostPreferences->useSshTunnel()) {
                saveWalletSshPassword();
            }
#endif
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
            setMaximumSize(frameSize); //This is a hack to force Qt to center the view in the scroll area
            setMinimumSize(frameSize);
            Q_EMIT framebufferSizeChanged(frameSize.width(), frameSize.height());
        }
    }

    const auto dpr = m_frame.devicePixelRatio();
    repaint(QRectF(x / dpr * m_horizontalFactor, y / dpr * m_verticalFactor, w / dpr * m_horizontalFactor, h / dpr * m_verticalFactor).toAlignedRect().adjusted(-1,-1,1,1));
}

void VncView::setViewOnly(bool viewOnly)
{
    RemoteView::setViewOnly(viewOnly);

    m_dontSendClipboard = viewOnly;

    if (viewOnly)
        setCursor(Qt::ArrowCursor);
    else
        setCursor(m_localCursorState == CursorOn ? localDefaultCursor() : Qt::BlankCursor);
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
        setMaximumSize(frameSize); //This is a hack to force Qt to center the view in the scroll area
        setMinimumSize(frameSize);
        resize(frameSize);
    }
}

void VncView::setCut(const QString &text)
{
    m_dontSendClipboard = true;
    m_clipboard->setText(text, QClipboard::Clipboard);
    m_dontSendClipboard = false;
}

void VncView::paintEvent(QPaintEvent *event)
{
//     qCDebug(KRDC) << "paint event: x: " << m_x << ", y: " << m_y << ", w: " << m_w << ", h: " << m_h;
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
    const QRectF srcRect(dstRect.x() * dpr / m_horizontalFactor, dstRect.y() * dpr / m_verticalFactor,
                         dstRect.width() * dpr / m_horizontalFactor, dstRect.height() * dpr / m_verticalFactor);
    painter.drawImage(dstRect, m_frame, srcRect);

    RemoteView::paintEvent(event);
}

void VncView::resizeEvent(QResizeEvent *event)
{
    RemoteView::resizeEvent(event);
    update();
}

bool VncView::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
//         qCDebug(KRDC) << "keyEvent";
        keyEventHandler(static_cast<QKeyEvent*>(event));
        return true;
        break;
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
//         qCDebug(KRDC) << "mouseEvent";
        mouseEventHandler(static_cast<QMouseEvent*>(event));
        return true;
        break;
    case QEvent::Wheel:
//         qCDebug(KRDC) << "wheelEvent";
        wheelEventHandler(static_cast<QWheelEvent*>(event));
        return true;
        break;
    default:
        return RemoteView::event(event);
    }
}

void VncView::mouseEventHandler(QMouseEvent *e)
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
        } else if (e->type() == QEvent::MouseButtonRelease) {
            if (e->button() & Qt::LeftButton)
                m_buttonMask &= 0xfe;
            if (e->button() & Qt::MidButton)
                m_buttonMask &= 0xfd;
            if (e->button() & Qt::RightButton)
                m_buttonMask &= 0xfb;
        }
    }

    const auto dpr = devicePixelRatioF();
    QPointF screenPos = e->screenPos();
    // We need to restore mouse position in device coordinates.
    // QMouseEvent::localPos() can be rounded (bug in Qt), but QMouseEvent::screenPos() is not.
    QPointF pos = (e->pos() + (screenPos - screenPos.toPoint())) * dpr;
    vncThread.mouseEvent(qRound(pos.x() / m_horizontalFactor), qRound(pos.y() / m_verticalFactor), m_buttonMask);
}

void VncView::wheelEventHandler(QWheelEvent *event)
{
    int eb = 0;
    if (event->angleDelta().y() < 0)
        eb |= 0x10;
    else
        eb |= 0x8;

    const auto dpr = devicePixelRatioF();
    // We need to restore mouse position in device coordinates.
    const QPointF pos = event->position() * dpr;

    const int x = qRound(pos.x() / m_horizontalFactor);
    const int y = qRound(pos.y() / m_verticalFactor);

    vncThread.mouseEvent(x, y, eb | m_buttonMask);
    vncThread.mouseEvent(x, y, m_buttonMask);

    event->accept();
}

#ifdef LIBSSH_FOUND
QString VncView::readWalletSshPassword()
{
    return readWalletPasswordForKey(QStringLiteral("SSHTUNNEL") + m_url.toDisplayString(QUrl::StripTrailingSlash));
}

void VncView::saveWalletSshPassword()
{
    saveWalletPasswordForKey(QStringLiteral("SSHTUNNEL") + m_url.toDisplayString(QUrl::StripTrailingSlash), m_sshTunnelThread->password());
}
#endif

void VncView::keyEventHandler(QKeyEvent *e)
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

    // handle modifiers
    if (k == XK_Shift_L || k == XK_Control_L || k == XK_Meta_L || k == XK_Alt_L || XK_Super_L || XK_Hyper_L ||
        k == XK_Shift_R || k == XK_Control_R || k == XK_Meta_R || k == XK_Alt_R || XK_Super_R || XK_Hyper_R) {
        if (pressed) {
            m_mods[k] = true;
        } else if (m_mods.contains(k)) {
            m_mods.remove(k);
        } else {
            unpressModifiers();
        }
    }

    if (k) {
        vncThread.keyEvent(k, pressed);
    }
}

void VncView::unpressModifiers()
{
    const QList<unsigned int> keys = m_mods.keys();
    QList<unsigned int>::const_iterator it = keys.constBegin();
    while (it != keys.end()) {
        qCDebug(KRDC) << "VncView::unpressModifiers key=" << (*it);
        vncThread.keyEvent(*it, false);
        it++;
    }
    m_mods.clear();
}

void VncView::clipboardDataChanged()
{
    if (m_status != Connected)
        return;

    if (m_clipboard->ownsClipboard() || m_dontSendClipboard)
        return;

    if (m_hostPreferences->dontCopyPasswords()) {
        const QMimeData* data = m_clipboard->mimeData();
        if (data && data->hasFormat(QLatin1String("x-kde-passwordManagerHint"))) {
            qCDebug(KRDC) << "VncView::clipboardDataChanged data hasFormat x-kde-passwordManagerHint -- ignoring";
            return;
        }
    }

    const QString text = m_clipboard->text(QClipboard::Clipboard);

    vncThread.clientCut(text);
}

void VncView::focusOutEvent(QFocusEvent *event)
{
    qCDebug(KRDC) << "VncView::focusOutEvent";
    unpressModifiers();

    RemoteView::focusOutEvent(event);
}

#include "moc_vncview.cpp"
