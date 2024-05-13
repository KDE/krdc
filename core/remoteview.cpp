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
    , m_dontSendClipboard(false)
#ifndef QTONLY
    , m_wallet(nullptr)
#endif
    , m_localCursorState(CursorOff)
{
    resize(0, 0);
    installEventFilter(this);
    setMouseTracking(true);

    m_clipboard = QApplication::clipboard();
    connect(m_clipboard, &QClipboard::dataChanged, this, &RemoteView::localClipboardChanged);
}

RemoteView::~RemoteView()
{
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
    if (m_status == s)
        return;

    if (((1 + m_status) != s) && (s != Disconnected)) {
        // follow state transition rules

        if (s == Disconnecting) {
            if (m_status == Disconnected)
                return;
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

void RemoteView::switchFullscreen(bool)
{
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

    QWidget::focusOutEvent(event);
}

bool RemoteView::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
        handleKeyEvent(static_cast<QKeyEvent *>(event));
        return true;
        break;
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
            || event->type() == QEvent::MouseMove)
            return true;
    }

    return QWidget::eventFilter(obj, event);
}

void RemoteView::localClipboardChanged()
{
    if (m_status != Connected)
        return;

    if (m_clipboard->ownsClipboard() || m_dontSendClipboard)
        return;

    const QMimeData *data = m_clipboard->mimeData(QClipboard::Clipboard);
    if (data) {
        handleLocalClipboardChanged(data);
    }
}

void RemoteView::remoteClipboardChanged(QMimeData *data)
{
    const bool saved_dontSendClipboard = m_dontSendClipboard;
    m_dontSendClipboard = true;
    m_clipboard->setMimeData(data, QClipboard::Clipboard);
    m_dontSendClipboard = saved_dontSendClipboard;
}

#include "moc_remoteview.cpp"
