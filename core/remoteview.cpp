/*
    SPDX-FileCopyrightText: 2002-2003 Tim Jansen <tim@tjansen.de>
    SPDX-FileCopyrightText: 2007-2008 Urs Wolfer <uwolfer@kde.org>
    SPDX-FileCopyrightText: 2021 Rafał Lalik <rafallalik@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "remoteview.h"
#include "krdc_debug.h"

#include <QApplication>
#include <QBitmap>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QStandardPaths>
#include <QWheelEvent>

#ifdef HAVE_WAYLAND
#include "waylandinhibition_p.h"
#endif

RemoteView::RemoteView(QWidget *parent)
    : QWidget(parent)
    , m_status(Disconnected)
    , m_host(QString())
    , m_port(0)
    , m_viewOnly(false)
    , m_grabAllKeys(false)
    , m_scale(false)
    , m_keyboardIsGrabbed(false)
    , m_factor(0.)
    , m_clipboard(nullptr)
#ifndef QTONLY
    , m_wallet(nullptr)
#endif
    , m_localCursorState(CursorOff)
{
    resize(0, 0);
    installEventFilter(this);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_clipboard = QApplication::clipboard();
    connect(m_clipboard, &QClipboard::dataChanged, this, &RemoteView::localClipboardChanged);

#ifdef HAVE_WAYLAND
    if (qGuiApp->platformName() == QLatin1String("wayland")) {
        m_inhibition.reset(new WaylandInhibition(window()->windowHandle()));
    }
#endif
}

RemoteView::~RemoteView()
{
#ifdef HAVE_WAYLAND
    if (qGuiApp->platformName() == QLatin1String("wayland")) {
        if (m_inhibition && m_inhibition->shortcutsAreInhibited()) {
            m_inhibition->disableInhibition();
        }
    }
#endif

#ifndef QTONLY
    delete m_wallet;
#endif
}

RemoteView::RemoteStatus RemoteView::status()
{
    return m_status;
}

void RemoteView::setStatus(RemoteView::RemoteStatus s)
{
    if (m_status == s) {
        return;
    }

    if (((1 + m_status) != s) && (s != Disconnected)) {
        // follow state transition rules

        if (s == Disconnecting) {
            if (m_status == Disconnected) {
                return;
            }
        } else {
            Q_ASSERT(((int)s) >= 0);
            if (m_status > s) {
                m_status = Disconnected;
                Q_EMIT statusChanged(Disconnected);
            }
            // smooth state transition
            RemoteStatus origState = m_status;
            for (int i = origState; i < s; ++i) {
                m_status = (RemoteStatus)i;
                Q_EMIT statusChanged((RemoteStatus)i);
            }
        }
    }
    m_status = s;
    Q_EMIT statusChanged(m_status);
}

bool RemoteView::supportsScaling() const
{
    return false;
}

bool RemoteView::supportsLocalCursor() const
{
    return false;
}

bool RemoteView::supportsViewOnly() const
{
    return false;
}

QString RemoteView::host()
{
    return m_host;
}

QSize RemoteView::framebufferSize()
{
    return QSize(0, 0);
}

void RemoteView::startQuitting()
{
}

bool RemoteView::isQuitting()
{
    return false;
}

int RemoteView::port()
{
    return m_port;
}

void RemoteView::updateConfiguration()
{
}

void RemoteView::keyEvent(QKeyEvent *)
{
}

bool RemoteView::viewOnly()
{
    return m_viewOnly;
}

void RemoteView::setViewOnly(bool viewOnly)
{
    m_viewOnly = viewOnly;
}

bool RemoteView::grabAllKeys()
{
    return m_grabAllKeys;
}

void RemoteView::setGrabAllKeys(bool grabAllKeys)
{
    m_grabAllKeys = grabAllKeys;

    if (grabAllKeys) {
        m_keyboardIsGrabbed = true;
        grabKeyboard();
    } else if (m_keyboardIsGrabbed) {
        releaseKeyboard();
    }
}

QPixmap RemoteView::takeScreenshot()
{
    return grab();
}

void RemoteView::showLocalCursor(LocalCursorState state)
{
    m_localCursorState = state;
}

RemoteView::LocalCursorState RemoteView::localCursorState() const
{
    return m_localCursorState;
}

bool RemoteView::scaling() const
{
    return m_scale;
}

void RemoteView::enableScaling(bool scale)
{
    m_scale = scale;
}

void RemoteView::setScaleFactor(float factor)
{
    m_factor = factor;
}

void RemoteView::switchFullscreen(bool on)
{
    Q_UNUSED(on);
#ifdef HAVE_WAYLAND
    // fullscreen mode moves the widget to a temporary window, reinitialize inhibitor
    if (qGuiApp->platformName() == QLatin1String("wayland")) {
        m_inhibition.reset(new WaylandInhibition(window()->windowHandle()));
    }
#endif
}

void RemoteView::scaleResize(int, int)
{
}

QUrl RemoteView::url()
{
    return m_url;
}

#ifndef QTONLY
QString RemoteView::readWalletPassword(bool fromUserNameOnly)
{
    return readWalletPasswordForKey(fromUserNameOnly ? m_url.userName() : m_url.toDisplayString(QUrl::StripTrailingSlash));
}

void RemoteView::saveWalletPassword(const QString &password, bool fromUserNameOnly)
{
    saveWalletPasswordForKey(fromUserNameOnly ? m_url.userName() : m_url.toDisplayString(QUrl::StripTrailingSlash), password);
}

void RemoteView::deleteWalletPassword(bool fromUserNameOnly)
{
    deleteWalletPasswordForKey(fromUserNameOnly ? m_url.userName() : m_url.toDisplayString(QUrl::StripTrailingSlash));
}

QString RemoteView::readWalletPasswordForKey(const QString &key)
{
    const QString KRDCFOLDER = QLatin1String("KRDC");

    window()->setDisabled(true); // WORKAROUND: disable inputs so users cannot close the current tab (see #181230)
    m_wallet = KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(), window()->winId(), KWallet::Wallet::OpenType::Synchronous);
    window()->setDisabled(false);

    if (m_wallet) {
        bool walletOK = m_wallet->hasFolder(KRDCFOLDER);
        if (!walletOK) {
            walletOK = m_wallet->createFolder(KRDCFOLDER);
            qCDebug(KRDC) << "Wallet folder created";
        }
        if (walletOK) {
            qCDebug(KRDC) << "Wallet OK";
            m_wallet->setFolder(KRDCFOLDER);
            QString password;

            if (m_wallet->hasEntry(key) && !m_wallet->readPassword(key, password)) {
                qCDebug(KRDC) << "Password read OK";

                return password;
            }
        }
    }
    return QString();
}

void RemoteView::saveWalletPasswordForKey(const QString &key, const QString &password)
{
    if (m_wallet && m_wallet->isOpen()) {
        qCDebug(KRDC) << "Write wallet password";
        m_wallet->writePassword(key, password);
    }
}

void RemoteView::deleteWalletPasswordForKey(const QString &key)
{
    if (m_wallet && m_wallet->isOpen()) {
        qCDebug(KRDC) << "Delete wallet password";
        m_wallet->removeEntry(key);
    }
}
#endif

QCursor RemoteView::localDefaultCursor() const
{
    return QCursor(Qt::ArrowCursor);
}

void RemoteView::focusInEvent(QFocusEvent *event)
{
    if (m_grabAllKeys) {
        m_keyboardIsGrabbed = true;
        grabKeyboard();
    }

    QWidget::focusInEvent(event);
}

void RemoteView::focusOutEvent(QFocusEvent *event)
{
    if (m_grabAllKeys || m_keyboardIsGrabbed) {
        m_keyboardIsGrabbed = false;
        releaseKeyboard();
    }

    unpressModifiers();

    QWidget::focusOutEvent(event);
}

bool RemoteView::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::KeyRelease: {
        auto keyEvent = static_cast<QKeyEvent *>(event);
        auto key = keyEvent->key();
        switch (key) {
        case Qt::Key_Shift:
        case Qt::Key_Control:
        case Qt::Key_Meta:
        case Qt::Key_Alt:
        case Qt::Key_AltGr:
        case Qt::Key_Super_L:
        case Qt::Key_Super_R:
        case Qt::Key_Hyper_L:
        case Qt::Key_Hyper_R:
            if (event->type() == QEvent::KeyPress) {
                ModifierKey modifierToAdd = {keyEvent->nativeScanCode(), keyEvent->nativeVirtualKey()};
                m_modifiers[key] = modifierToAdd;
            } else if (m_modifiers.contains(key)) {
                m_modifiers.remove(key);
            } else {
                unpressModifiers();
            }
            break;
        default:
            break;
        }

        handleKeyEvent(keyEvent);
        return true;
        break;
    }
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
        handleMouseEvent(static_cast<QMouseEvent *>(event));
        return true;
        break;
    case QEvent::Wheel:
        handleWheelEvent(static_cast<QWheelEvent *>(event));
        return true;
        break;
    default:
        return QWidget::event(event);
    }
}

bool RemoteView::eventFilter(QObject *obj, QEvent *event)
{
    if (m_viewOnly) {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease || event->type() == QEvent::MouseButtonDblClick
            || event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::Wheel
            || event->type() == QEvent::MouseMove) {
            return true;
        }
    } else {
        if (m_grabAllKeys && event->type() == QEvent::ShortcutOverride) {
            event->accept();
            return true;
        }
    }

    return QWidget::eventFilter(obj, event);
}

void RemoteView::localClipboardChanged()
{
    if (m_status != Connected) {
        return;
    }

    if (m_clipboard->ownsClipboard() || m_viewOnly) {
        return;
    }

    const QMimeData *data = m_clipboard->mimeData(QClipboard::Clipboard);
    if (data) {
        handleLocalClipboardChanged(data);
    }
}

void RemoteView::remoteClipboardChanged(QMimeData *data)
{
    if (m_viewOnly) {
        return;
    }
    m_clipboard->setMimeData(data, QClipboard::Clipboard);
}

void RemoteView::unpressModifiers()
{
    for (auto it = m_modifiers.begin(); it != m_modifiers.end(); ++it) {
        ModifierKey modifierToUnpress = it.value();
        QKeyEvent *event = new QKeyEvent(QEvent::KeyRelease,
                                         it.key(),
                                         Qt::NoModifier,
                                         modifierToUnpress.nativeScanCode,
                                         modifierToUnpress.nativeVirtualKey,
                                         0,
                                         QString(),
                                         false,
                                         1
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
                                         ,
                                         QInputDevice::primaryKeyboard()
#endif
        );
        handleKeyEvent(event);
    }
    m_modifiers.clear();
}

void RemoteView::grabKeyboard()
{
    QWidget::grabKeyboard();
#ifdef HAVE_WAYLAND
    if (qGuiApp->platformName() == QLatin1String("wayland")) {
        m_inhibition->enableInhibition();
    }
#endif
}

void RemoteView::releaseKeyboard()
{
#ifdef HAVE_WAYLAND
    if (qGuiApp->platformName() == QLatin1String("wayland")) {
        m_inhibition->disableInhibition();
    }
#endif
    QWidget::releaseKeyboard();
}

#include "moc_remoteview.cpp"
