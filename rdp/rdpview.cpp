/*
    SPDX-FileCopyrightText: 2002 Arend van Beelen jr. <arend@auton.nl>
    SPDX-FileCopyrightText: 2007-2012 Urs Wolfer <uwolfer@kde.org>
    SPDX-FileCopyrightText: 2012 AceLan Kao <acelan@acelan.idv.tw>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rdpview.h"

#include "krdc_debug.h"
#include "rdpcursor.h"

#include <algorithm>

#include <KMessageDialog>
#include <KPasswordDialog>
#include <KShell>
#include <KWindowSystem>

#include <QChar>
#include <QDir>
#include <QEvent>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWindow>

#include "rdpmonitorview.h"

RdpView::RdpView(QWidget *parent, const QUrl &url, KConfigGroup configGroup, const QString &user, const QString &domain, const QString &password)
    : RemoteView(parent)
    , m_user(user)
    , m_domain(domain)
    , m_password(password)
{
    m_url = url;
    m_host = url.host();
    m_port = url.port();

    if (m_user.isEmpty() && !m_url.userName().isEmpty()) {
        m_user = m_url.userName();
    }

    if (m_domain.isEmpty()) {
        if (m_url.hasQuery()) {
            QUrlQuery query(m_url);
            QString queryDomain = query.queryItemValue(QStringLiteral("domain"));
            if (!queryDomain.isEmpty()) {
                m_domain = queryDomain;
            }
        } else {
            // convert legacy DOMAIN\\user@host URLs
            QStringList splitted = m_user.split(QChar::fromLatin1('\\'));
            if (splitted.size() == 2) {
                m_domain = splitted[0];
                m_user = splitted[1];
            }
        }
    }

    if (m_password.isEmpty() && !m_url.password().isEmpty()) {
        m_password = m_url.password();
    }

    if (m_port <= 0) {
        m_port = TCP_PORT_RDP;
    }

    m_hostPreferences = std::make_unique<RdpHostPreferences>(configGroup);
}

RdpView::~RdpView()
{
    startQuitting();
}

QSize RdpView::framebufferSize()
{
    if (m_session) {
        return m_session->size();
    }

    return QSize{};
}

void RdpView::scaleResize(int w, int h)
{
    RemoteView::scaleResize(w, h);

    // handle window resizes
    resize(sizeHint());

    if (m_session) {
        m_session->sendResizeEvent(QSize(w, h) * m_session->outputScale());
    }
}

QSize RdpView::sizeHint() const
{
    if (!m_session) {
        return QSize{};
    }

    const QSize remoteSize = m_fullscreenMonitorRect.isEmpty() ? m_session->size() : m_fullscreenMonitorRect.size();

    // when parent is resized and scaling is enabled, resize the view, preserving aspect ratio
    if (m_hostPreferences->scaleToSize()) {
        return remoteSize.scaled(parentWidget()->size(), Qt::KeepAspectRatio);
    }

    return remoteSize / m_session->outputScale();
}

void RdpView::startQuittingConnection()
{
    if (m_quitting) {
        return; // ignore repeated triggers
    }

    qCDebug(KRDC) << "Stopping RDP session";
    m_quitting = true;

    unpressModifiers();

    destroyMonitorWindows();

    if (m_session) {
        m_session->stop();
    }

    qCDebug(KRDC) << "RDP session stopped";
    Q_EMIT disconnected();
    setStatus(Disconnected);
}

bool RdpView::isQuitting()
{
    return m_quitting;
}

bool RdpView::startConnection()
{
    m_session = std::make_unique<RdpSession>(this);
    m_session->setHostPreferences(m_hostPreferences.get());
    m_session->setHost(m_host);
    m_session->setPort(m_port);
    m_session->setUser(m_user);
    m_session->setDomain(m_domain);

    m_session->setSize(initialSize());

    if (m_password.isEmpty()) {
        m_session->setPassword(readWalletPassword());
    } else {
        m_session->setPassword(m_password);
    }

    connect(m_session.get(), &RdpSession::sizeChanged, this, [this]() {
        resize(sizeHint());
        qCDebug(KRDC) << "freerdp resized rdp view" << sizeHint();
        Q_EMIT framebufferSizeChanged(width(), height());
    });
    connect(m_session.get(), &RdpSession::rectangleUpdated, this, &RdpView::onRectangleUpdated);
    connect(m_session.get(), &RdpSession::stateChanged, this, [this]() {
        switch (m_session->state()) {
        case RdpSession::State::Starting:
            setStatus(Authenticating);
            break;
        case RdpSession::State::Connected:
            setStatus(Preparing);
            break;
        case RdpSession::State::Running:
            if (m_hostPreferences->resolution() == RdpHostPreferences::Resolution::AllScreens) {
                QScreen *primaryScreen = nullptr;
                const auto screens = QGuiApplication::screens();
                for (const auto &monitor : m_session->monitors()) {
                    if (monitor.primary && monitor.id >= 0 && monitor.id < screens.size()) {
                        primaryScreen = screens.at(monitor.id);
                        break;
                    }
                }
                m_primaryScreen = primaryScreen;
                Q_EMIT fullScreenRequested(primaryScreen);

                // The main window may already have entered full screen while
                // the connection was being established. In that case the
                // monitor geometry was not available to switchFullscreen().
                if (window()->isFullScreen() && m_fullscreenMonitorRect.isEmpty()) {
                    switchFullscreen(true);
                }
            }
            setStatus(Connected);
            break;
        case RdpSession::State::Closed:
            Q_EMIT disconnected();
            setStatus(Disconnected);
            break;
        default:
            break;
        }
    });
    connect(m_session.get(), &RdpSession::errorMessage, this, &RdpView::handleError);
    connect(m_session.get(), &RdpSession::onLogonError, this, &RdpView::onLogonError);
    connect(m_session.get(), &RdpSession::onAuthRequested, this, &RdpView::onAuthRequested, Qt::BlockingQueuedConnection);
    connect(m_session.get(), &RdpSession::onVerifyCertificate, this, &RdpView::onVerifyCertificate, Qt::BlockingQueuedConnection);
    connect(m_session.get(), &RdpSession::onVerifyChangedCertificate, this, &RdpView::onVerifyChangedCertificate, Qt::BlockingQueuedConnection);

    connect(m_session.get(), &RdpSession::cursorChanged, this, &RdpView::setRemoteCursor);

    setStatus(RdpView::Connecting);
    if (!m_session->start()) {
        Q_EMIT disconnected();
        return false;
    }

    setFocus();

    return true;
}

void RdpView::onAuthRequested()
{
    std::unique_ptr<KPasswordDialog> dialog;
    dialog =
        std::make_unique<KPasswordDialog>(nullptr, KPasswordDialog::ShowUsernameLine | KPasswordDialog::ShowKeepPassword | KPasswordDialog::ShowDomainLine);
    dialog->setPrompt(i18nc("@label", "Access to this system requires a username and password."));
    dialog->setUsername(m_user);
    dialog->setDomain(m_domain);
    dialog->setPassword(m_password);

    if (!dialog->exec()) {
        return;
    }

    m_user = dialog->username();
    m_domain = dialog->domain();
    m_password = dialog->password();

    // update m_url so it gets saved correctly
    m_url.setUserName(m_user);
    QUrlQuery query(m_url);
    query.removeQueryItem(QStringLiteral("domain"));
    if (!m_domain.isEmpty()) {
        query.addQueryItem(QStringLiteral("domain"), m_domain);
    }
    m_url.setQuery(query);

    if (dialog->keepPassword()) {
        savePassword(m_password);
    }

    m_session->setUser(m_user);
    m_session->setDomain(m_domain);
    m_session->setPassword(m_password);
}

void RdpView::onVerifyCertificate(RdpSession::CertificateResult *ret, const QString &certificate)
{
    KMessageDialog dialog{KMessageDialog::WarningContinueCancel, i18nc("@label", "The certificate for this system is unknown. Do you wish to continue?")};
    dialog.setCaption(i18nc("@title:dialog", "Verify Certificate"));
    dialog.setIcon(QIcon::fromTheme(QStringLiteral("view-certficate")));

    dialog.setDetails(certificate);

    dialog.setDontAskAgainText(i18nc("@label", "Remember this certificate"));

    dialog.setButtons(KStandardGuiItem::cont(), KGuiItem(), KStandardGuiItem::cancel());

    const auto result = static_cast<KMessageDialog::ButtonType>(dialog.exec());
    if (result == KMessageDialog::Cancel) {
        *ret = RdpSession::CertificateResult::DoNotAccept;
        return;
    }

    if (dialog.isDontAskAgainChecked()) {
        *ret = RdpSession::CertificateResult::AcceptPermanently;
    } else {
        *ret = RdpSession::CertificateResult::AcceptTemporarily;
    }
}

void RdpView::onVerifyChangedCertificate(RdpSession::CertificateResult *ret, const QString &oldCertificate, const QString &newCertificate)
{
    KMessageDialog dialog{KMessageDialog::WarningContinueCancel, i18nc("@label", "The certificate for this system has changed. Do you wish to continue?")};
    dialog.setCaption(i18nc("@title:dialog", "Certificate has Changed"));
    dialog.setIcon(QIcon::fromTheme(QStringLiteral("view-certficate")));

    dialog.setDetails(i18nc("@label", "Previous certificate:\n%1\nNew Certificate:\n%2", oldCertificate, newCertificate));

    dialog.setDontAskAgainText(i18nc("@label", "Remember this certificate"));

    dialog.setButtons(KStandardGuiItem::cont(), KGuiItem(), KStandardGuiItem::cancel());

    const auto result = static_cast<KMessageDialog::ButtonType>(dialog.exec());
    if (result == KMessageDialog::Cancel) {
        *ret = RdpSession::CertificateResult::DoNotAccept;
        return;
    }

    if (dialog.isDontAskAgainChecked()) {
        *ret = RdpSession::CertificateResult::AcceptPermanently;
    } else {
        *ret = RdpSession::CertificateResult::AcceptTemporarily;
    }
}

void RdpView::handleError(const unsigned int error)
{
    QString title;
    QString message;

    switch (error) {
    case FREERDP_ERROR_BASE:
        return; // no error, no need to show an error message
    case FREERDP_ERROR_CONNECT_CANCELLED:
        return; // user canceled connection, no need to show an error message
    case FREERDP_ERROR_AUTHENTICATION_FAILED:
    case FREERDP_ERROR_CONNECT_LOGON_FAILURE:
    case FREERDP_ERROR_CONNECT_WRONG_PASSWORD:
        title = i18nc("@title:dialog", "Login Failure");
        message = i18nc("@label", "Unable to login with the provided credentials. Please double check the user and password.");
        if (m_password.isEmpty()) {
            deleteWalletPassword();
        }
        break;
    case FREERDP_ERROR_CONNECT_ACCOUNT_LOCKED_OUT:
    case FREERDP_ERROR_CONNECT_ACCOUNT_EXPIRED:
    case FREERDP_ERROR_CONNECT_ACCOUNT_DISABLED:
    case FREERDP_ERROR_SERVER_INSUFFICIENT_PRIVILEGES:
        title = i18nc("@title:dialog", "Account Problems");
        message = i18nc("@label", "The provided account is not allowed to log in to this machine. Please contact your system administrator.");
        break;
    case FREERDP_ERROR_CONNECT_PASSWORD_EXPIRED:
    case FREERDP_ERROR_CONNECT_PASSWORD_CERTAINLY_EXPIRED:
    case FREERDP_ERROR_CONNECT_PASSWORD_MUST_CHANGE:
        title = i18nc("@title:dialog", "Password Problems");
        message = i18nc("@label", "Unable to login with the provided password. Please contact your system administrator to change it.");
        break;
    case FREERDP_ERROR_CONNECT_FAILED:
    case FREERDP_ERROR_TLS_CONNECT_FAILED:
    case FREERDP_ERROR_CONNECT_TRANSPORT_FAILED:
        if (status() == Connected) {
            title = i18nc("@title:dialog", "Connection Lost");
            message = i18nc("@label", "Lost connection to the server.");
        } else {
            title = i18nc("@title:dialog", "Could not Connect");
            message = i18nc("@label", "Could not connect to the server.");
        }
        break;
    case FREERDP_ERROR_DNS_ERROR:
    case FREERDP_ERROR_DNS_NAME_NOT_FOUND:
        title = i18nc("@title:dialog", "Server not Found");
        message = i18nc("@label", "Could not find the server.");
        break;
    case FREERDP_ERROR_SERVER_DENIED_CONNECTION:
        title = i18nc("@title:dialog", "Connection Refused");
        message = i18nc("@label", "The server refused the connection request.");

        break;
    case FREERDP_ERROR_RPC_INITIATED_DISCONNECT:
    case FREERDP_ERROR_RPC_INITIATED_LOGOFF:
    case FREERDP_ERROR_RPC_INITIATED_DISCONNECT_BY_USER:
    case FREERDP_ERROR_LOGOFF_BY_USER:
        // user or admin initiated action, quit without error
        return;
    case FREERDP_ERROR_DISCONNECTED_BY_OTHER_CONNECTION:
        title = i18nc("@title:dialog", "Connection Closed");
        message = QStringLiteral("Disconnected by other session");
        break;
    default:
        qCDebug(KRDC) << "Unhandled error" << error;
        title = i18nc("@title:dialog", "Connection Failed");
        message = i18nc("@label", "An unknown error occurred");
        break;
    }

    qCDebug(KRDC) << "error message" << title << message;
    // TODO offer reconnect if appropriate
    Q_EMIT errorMessage(title, message);
}

void RdpView::onLogonError(const QString &error)
{
    Q_EMIT errorMessage(i18nc("@title:dialog", "Logon Error"), error);
}

HostPreferences *RdpView::hostPreferences()
{
    return m_hostPreferences.get();
}

QPixmap RdpView::takeScreenshot()
{
    if (m_session && !m_session->videoBuffer()->isNull()) {
        return QPixmap::fromImage(*m_session->videoBuffer());
    }
    return QPixmap{};
}

bool RdpView::supportsScaling() const
{
    return true;
}

bool RdpView::supportsLocalCursor() const
{
    return true;
}

bool RdpView::supportsViewOnly() const
{
    return true;
}

bool RdpView::supportsClipboardSharing() const
{
    return true;
}

void RdpView::showLocalCursor(LocalCursorState state)
{
    RemoteView::showLocalCursor(state);

    for (QWidget *window : std::as_const(m_monitorWindows)) {
        if (auto *monitorView = window->findChild<RdpMonitorView *>()) {
            monitorView->showLocalCursor(state == CursorOn);
        }
    }

    if (state == CursorOn) {
        // show local cursor, hide remote one
        setCursor(localDefaultCursor());
    } else {
        // hide local cursor, show remote one
        updateRemoteCursor();
    }
}

void RdpView::setRemoteCursor(const QCursor cursor)
{
    m_remoteCursor = cursor;
    if (m_localCursorState != CursorOn) {
        updateRemoteCursor();
    }
}

void RdpView::updateRemoteCursor()
{
    if (!m_session) {
        setCursor(m_remoteCursor);
        return;
    }

    const QSize remoteSize = m_fullscreenMonitorRect.isEmpty() ? m_session->size() : m_fullscreenMonitorRect.size();
    if (remoteSize.isEmpty()) {
        setCursor(m_remoteCursor);
        return;
    }

    const qreal sx = width() / static_cast<qreal>(remoteSize.width());
    const qreal sy = height() / static_cast<qreal>(remoteSize.height());
    setCursor(scaledRemoteCursor(m_remoteCursor, qMin(sx, sy)));
}

bool RdpView::scaling() const
{
    return m_hostPreferences->scaleToSize();
}

void RdpView::enableScaling(bool scale)
{
    m_hostPreferences->setScaleToSize(scale);
    qCDebug(KRDC) << "Scaling changed" << scale;
    resize(sizeHint());
    update();
}

void RdpView::switchFullscreen(bool on)
{
    if (m_hostPreferences->resolution() != RdpHostPreferences::Resolution::AllScreens) {
        RemoteView::switchFullscreen(on);
        return;
    }

    if (on) {
        m_fullscreenMonitorRect = primaryMonitorRect();
        if (m_monitorWindows.isEmpty()) {
            createMonitorWindows();
        }
    } else {
        destroyMonitorWindows();
        m_fullscreenMonitorRect = {};
    }

    resize(sizeHint());
    update();

    // Recreate shortcut inhibition and keyboard grab only after the target
    // top-level window has changed.
    RemoteView::switchFullscreen(on);
}

void RdpView::setFullscreenMinimized(bool minimized)
{
    if (m_monitorWindowsMinimized == minimized) {
        return;
    }

    // Update this before changing the windows: showFullScreen() generates a
    // WindowStateChange for every monitor and those events must not trigger
    // another group restore.
    m_monitorWindowsMinimized = minimized;
    if (!minimized) {
        Q_EMIT fullScreenRequested(m_primaryScreen ? m_primaryScreen.data() : QGuiApplication::primaryScreen());
    }
    for (QWidget *window : std::as_const(m_monitorWindows)) {
        if (minimized) {
            window->showMinimized();
        } else {
            showMonitorWindowFullScreen(window);
        }
    }
}

QSize RdpView::initialSize()
{
    switch (m_hostPreferences->resolution()) {
    case RdpHostPreferences::Resolution::Small:
        return QSize{1280, 720};
    case RdpHostPreferences::Resolution::Medium:
        return QSize{1600, 900};
    case RdpHostPreferences::Resolution::Large:
        return QSize{1920, 1080};
    case RdpHostPreferences::Resolution::MatchScreen:
    case RdpHostPreferences::Resolution::AllScreens:
        return window()->windowHandle()->screen()->size();
    case RdpHostPreferences::Resolution::Custom:
        return QSize{m_hostPreferences->width(), m_hostPreferences->height()};
    case RdpHostPreferences::Resolution::MatchWindow:
    default:
        return parentWidget()->size() * devicePixelRatio();
    }
}

void RdpView::savePassword(const QString &password)
{
    saveWalletPassword(password);
}

void RdpView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateRemoteCursor();
}

void RdpView::paintEvent(QPaintEvent *event)
{
    if (!m_session || m_session->videoBuffer()->isNull()) {
        return;
    }

    QPainter painter;

    painter.begin(this);
    painter.setClipRect(event->rect());

    const auto &image = *m_session->videoBuffer();

    if (!m_fullscreenMonitorRect.isEmpty()) {
        const QRect sourceRect = m_fullscreenMonitorRect.intersected(image.rect());
        if (!sourceRect.isEmpty()) {
            painter.drawImage(rect(), image, sourceRect);
        }
        painter.end();
        return;
    }

    auto scaledImage = image;
    const qreal outputScale = m_session->outputScale();
    scaledImage.setDevicePixelRatio(outputScale);

    if (m_hostPreferences->scaleToSize()) {
        painter.drawImage(QPoint{0, 0}, scaledImage.scaled(size() * outputScale, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        painter.drawImage(QPoint{0, 0}, scaledImage);
    }
    painter.end();
}

void RdpView::handleKeyEvent(QKeyEvent *event)
{
    if (m_session) {
        m_session->sendEvent(event, this);
    }
}

void RdpView::handleMouseEvent(QMouseEvent *event)
{
    if (m_session) {
        if (m_fullscreenMonitorRect.isEmpty()) {
            m_session->sendEvent(event, this);
        } else {
            m_session->sendEvent(event, this, m_fullscreenMonitorRect);
        }
    }
}

void RdpView::handleWheelEvent(QWheelEvent *event)
{
    if (m_session) {
        if (m_fullscreenMonitorRect.isEmpty()) {
            m_session->sendEvent(event, this);
        } else {
            m_session->sendEvent(event, this, m_fullscreenMonitorRect);
        }
    }
}

void RdpView::onRectangleUpdated(const QRect &remoteRect, const QSize &remoteSize)
{
    if (!m_fullscreenMonitorRect.isEmpty()) {
        const QRect intersection = remoteRect.intersected(m_fullscreenMonitorRect);
        if (intersection.isEmpty()) {
            return;
        }

        const qreal sx = width() / static_cast<qreal>(m_fullscreenMonitorRect.width());
        const qreal sy = height() / static_cast<qreal>(m_fullscreenMonitorRect.height());
        const QRectF localUpdate{(intersection.x() - m_fullscreenMonitorRect.x()) * sx,
                                 (intersection.y() - m_fullscreenMonitorRect.y()) * sy,
                                 intersection.width() * sx,
                                 intersection.height() * sy};
        update(localUpdate.toAlignedRect());
        return;
    }

    // convert remoteRect based on remoteSize to local widget size()
    auto ratio = (qreal)size().width() / remoteSize.width();
    auto destRect =
        QRect{qFloor(remoteRect.x() * ratio), qFloor(remoteRect.y() * ratio), qCeil(remoteRect.width() * ratio), qCeil(remoteRect.height() * ratio)};
    update(destRect);
}

void RdpView::handleLocalClipboardChanged(const QMimeData *data)
{
    if (m_session) {
        m_session->sendClipboard(data);
    }
}

void RdpView::focusInEvent(QFocusEvent *event)
{
    if (m_session) {
        m_session->syncKeyState();
    }

    RemoteView::focusInEvent(event);
}

bool RdpView::eventFilter(QObject *watched, QEvent *event)
{
    auto *window = qobject_cast<QWidget *>(watched);
    const bool isMonitorWindow = window && m_monitorWindows.contains(window);

    bool scheduleGroupRestore = isMonitorWindow && m_monitorWindowsMinimized && event->type() == QEvent::WindowActivate;
    if (isMonitorWindow && m_monitorWindowsMinimized && event->type() == QEvent::WindowStateChange) {
        const auto *stateEvent = static_cast<QWindowStateChangeEvent *>(event);
        scheduleGroupRestore = stateEvent->oldState() & Qt::WindowMinimized;
    }
    if (scheduleGroupRestore) {
        // Event filters run before QWidget has applied the new window state.
        // Inspect it on the next event-loop iteration instead.
        QTimer::singleShot(0, this, &RdpView::restoreMonitorWindowsIfNeeded);
    }

    if (event->type() == QEvent::WindowActivate) {
        if (isMonitorWindow && grabAllKeys()) {
            // RemoteView releases the grab when its embedded widget loses
            // focus. Reapply it when one of our external monitor windows
            // becomes active.
            setGrabAllKeys(true);
        }
    }
    return RemoteView::eventFilter(watched, event);
}

void RdpView::restoreMonitorWindowsIfNeeded()
{
    if (!m_monitorWindowsMinimized) {
        return;
    }

    for (const QWidget *window : std::as_const(m_monitorWindows)) {
        if (!(window->windowState() & Qt::WindowMinimized)) {
            setFullscreenMinimized(false);
            return;
        }
    }
}

void RdpView::showMonitorWindowFullScreen(QWidget *window)
{
    if (!window) {
        return;
    }

    if (QScreen *screen = m_monitorScreens.value(window)) {
        window->setScreen(screen);
        window->setGeometry(screen->geometry());
    }
    window->showFullScreen();
}

QRect RdpView::primaryMonitorRect() const
{
    if (!m_session) {
        return {};
    }

    const auto &monitors = m_session->monitors();
    const auto primary = std::find_if(monitors.cbegin(), monitors.cend(), [](const RdpSession::MonitorGeometry &monitor) {
        return monitor.primary;
    });
    return primary == monitors.cend() ? QRect{} : primary->virtualRect;
}

void RdpView::createMonitorWindows()
{
    destroyMonitorWindows();
    m_monitorWindowsMinimized = false;

    if (!m_session) {
        return;
    }

    const auto &monitors = m_session->monitors();
    if (monitors.isEmpty()) {
        return;
    }

    const auto screens = QGuiApplication::screens();

    for (const auto &monitor : monitors) {
        // The primary monitor is rendered by the RdpView embedded in the
        // full-screen KRDC main window.
        if (monitor.primary) {
            continue;
        }

        if (monitor.id < 0 || monitor.id >= screens.size()) {
            continue;
        }

        QScreen *screen = screens.at(monitor.id);
        if (!screen) {
            continue;
        }

        QWidget *window = new QWidget(nullptr, Qt::Window);
        window->setAttribute(Qt::WA_DeleteOnClose);
        window->setWindowTitle(i18nc("@title:window", "KRDC – Screen %1", monitor.id + 1));
        window->installEventFilter(this);

        auto *layout = new QVBoxLayout(window);
        layout->setContentsMargins(0, 0, 0, 0);

        auto *view = new RdpMonitorView(m_session.get(), monitor.virtualRect, window);
        view->installEventFilter(this);
        connect(view, &RdpMonitorView::keyEventReceived, this, [this](QKeyEvent *event) {
            RemoteView::event(event);
        });
        connect(view, &RdpMonitorView::focusLost, this, [this]() {
            unpressModifiers();
        });
        view->setRemoteCursor(m_remoteCursor);
        view->showLocalCursor(m_localCursorState == CursorOn);
        layout->addWidget(view);

        window->setScreen(screen);
        window->setGeometry(screen->geometry());
        m_monitorWindows.push_back(window);
        m_monitorScreens.insert(window, screen);
        showMonitorWindowFullScreen(window);
    }
}

void RdpView::destroyMonitorWindows()
{
    m_monitorWindowsMinimized = false;
    for (QWidget *window : std::as_const(m_monitorWindows)) {
        if (!window) {
            continue;
        }
        window->close();
        window->deleteLater();
    }
    m_monitorWindows.clear();
    m_monitorScreens.clear();
}
