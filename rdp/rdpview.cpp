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
    if (m_hostPreferences->scaleToSize() && parentWidget()) {
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
            // Starts before connecting, so it is no evidence of reaching the server.
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
        setCanceledByUser();
        return;
    }

    m_user = dialog->username();
    m_domain = dialog->domain();
    m_password = dialog->password();

    // update m_url so it gets saved correctly
    // A null username removes the user info; an empty one leaves an '@' in the URL.
    m_url.setUserName(m_user.isEmpty() ? QString() : m_user);
    QUrlQuery query(m_url);
    const bool hasDomainItem = query.hasQueryItem(QStringLiteral("domain"));
    query.removeQueryItem(QStringLiteral("domain"));
    if (!m_domain.isEmpty()) {
        query.addQueryItem(QStringLiteral("domain"), m_domain);
    } else if (hasDomainItem) {
        // Keep an empty domain= query item: the presence of the query is what
        // disables the legacy DOMAIN\user username splitting when this URL is
        // reloaded from a bookmark.
        query.addQueryItem(QStringLiteral("domain"), QStringLiteral(""));
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
        setCanceledByUser();
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
        setCanceledByUser();
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
    using Stage = ConnectionError::Stage;

    switch (error) {
    case FREERDP_ERROR_BASE:
        return; // no error, no need to show an error message
    case FREERDP_ERROR_CONNECT_CANCELLED:
        return; // user canceled connection, no need to show an error message
    default:
        break;
    }

    const QString details = QStringLiteral("%1 (0x%2): %3")
                                .arg(QString::fromUtf8(freerdp_get_last_error_name(error)))
                                .arg(error, 8, 16, QLatin1Char('0'))
                                .arg(QString::fromUtf8(freerdp_get_last_error_string(error)));
    ConnectionError report;

    switch (error) {
    case FREERDP_ERROR_AUTHENTICATION_FAILED:
    case FREERDP_ERROR_CONNECT_LOGON_FAILURE:
    case FREERDP_ERROR_CONNECT_WRONG_PASSWORD:
    case FREERDP_ERROR_CONNECT_NO_OR_MISSING_CREDENTIALS:
        report = ConnectionError::create(Stage::Authentication,
                                         m_user.isEmpty() ? QString() : i18nc("@label", "The credentials for “%1” were not accepted.", m_user),
                                         details);
        if (!m_hostPreferences->gatewayServer().isEmpty()) {
            report.steps << i18nc("@info", "Check the RD Gateway credentials in the host settings.");
        }
        if (m_password.isEmpty()) {
            deleteWalletPassword();
        }
        break;
    case FREERDP_ERROR_CONNECT_ACCOUNT_LOCKED_OUT:
    case FREERDP_ERROR_CONNECT_ACCOUNT_EXPIRED:
    case FREERDP_ERROR_CONNECT_ACCOUNT_DISABLED:
    case FREERDP_ERROR_CONNECT_ACCOUNT_RESTRICTION:
    case FREERDP_ERROR_CONNECT_LOGON_TYPE_NOT_GRANTED:
    case FREERDP_ERROR_CONNECT_ACCESS_DENIED:
    case FREERDP_ERROR_CONNECT_CLIENT_REVOKED:
    case FREERDP_ERROR_INSUFFICIENT_PRIVILEGES:
    case FREERDP_ERROR_SERVER_INSUFFICIENT_PRIVILEGES:
        report = ConnectionError::create(Stage::Authentication,
                                         m_user.isEmpty() ? i18nc("@label", "This account may not log in remotely.")
                                                          : i18nc("@label", "“%1” may not log in remotely.", m_user),
                                         details);
        report.title = i18nc("@title connection error", "Access denied");
        report.steps = {
            i18nc("@info", "Check whether the account is locked, disabled or expired."),
            i18nc("@info",
                  "Ask the administrator of the remote computer to add the account to the Remote Desktop Users group, or log in with a different account."),
        };
        break;
    case FREERDP_ERROR_CONNECT_PASSWORD_EXPIRED:
    case FREERDP_ERROR_CONNECT_PASSWORD_CERTAINLY_EXPIRED:
    case FREERDP_ERROR_CONNECT_PASSWORD_MUST_CHANGE:
    case FREERDP_ERROR_SERVER_FRESH_CREDENTIALS_REQUIRED:
        report =
            ConnectionError::create(Stage::Authentication, i18nc("@label", "The password must be changed before it can be used to log in remotely."), details);
        report.title = i18nc("@title connection error", "Password expired");
        report.steps = {
            i18nc("@info", "Change the password by logging in locally on the remote computer, or ask its administrator to reset it."),
        };
        report.actions = ConnectionError::Retry;
        break;
    case FREERDP_ERROR_CONNECT_KDC_UNREACHABLE:
        report = ConnectionError::create(Stage::Authentication, i18nc("@label", "The Kerberos domain controller could not be reached."), details);
        report.steps = {
            i18nc("@info", "Check the domain you entered when logging in."),
            i18nc("@info", "Connect to the network or VPN of the domain."),
        };
        break;
    case FREERDP_ERROR_DNS_ERROR:
    case FREERDP_ERROR_DNS_NAME_NOT_FOUND:
        report = ConnectionError::create(Stage::NameResolution, i18nc("@label", "No IP address was found for this name."), details);
        break;
    case FREERDP_ERROR_TLS_CONNECT_FAILED:
    case FREERDP_ERROR_SECURITY_NEGO_CONNECT_FAILED:
#ifdef FREERDP_ERROR_CONNECT_HYBRID_REQUIRED_BY_SERVER // FreeRDP >= 3.31
    case FREERDP_ERROR_CONNECT_HYBRID_REQUIRED_BY_SERVER:
#endif
        if (status() == Connected) {
            report = ConnectionError::create(Stage::ConnectionLost, QString(), details);
            break;
        }
        report = ConnectionError::create(Stage::Security, i18nc("@label", "The server and KRDC could not agree on a security protocol."), details);
        if (m_hostPreferences->tlsSecLevel() != RdpHostPreferences::TlsSecLevel::Any) {
            report.steps.prepend(i18nc("@info", "Set the TLS security level in the host settings to “Any”. The server may not support the level selected."));
        }
#ifdef FREERDP_ERROR_CONNECT_HYBRID_REQUIRED_BY_SERVER
        if (error == FREERDP_ERROR_CONNECT_HYBRID_REQUIRED_BY_SERVER) {
            report.message = i18nc("@label", "The server requires Network Level Authentication (NLA).");
            report.steps.prepend(i18nc("@info", "Enable NLA in the security settings for this host."));
        }
#endif
        break;
    case FREERDP_ERROR_CONNECT_FAILED:
    case FREERDP_ERROR_CONNECT_TRANSPORT_FAILED:
    case FREERDP_ERROR_MCS_CONNECT_INITIAL_ERROR:
        if (status() == Connected) {
            report = ConnectionError::create(Stage::ConnectionLost, QString(), details);
        } else if (error == FREERDP_ERROR_CONNECT_FAILED) {
            // refused, timed out or unreachable
            report = ConnectionError::networkFailure(ConnectionError::NetworkFailure::Unknown, details);
        } else {
            // opened, then dropped by the server during the handshake
            report = ConnectionError::serverClosedDuringSetup(details);
        }
        if (!m_hostPreferences->gatewayServer().isEmpty()) {
            report.steps << i18nc("@info",
                                  "Check the RD Gateway settings for this host. The gateway “%1” may be unreachable or reject the connection.",
                                  m_hostPreferences->gatewayServer());
        }
        break;
    case FREERDP_ERROR_CONNECT_ACTIVATION_TIMEOUT:
    case FREERDP_ERROR_CONNECT_TARGET_BOOTING:
    case FREERDP_ERROR_LOGON_TIMEOUT:
        report = ConnectionError::create(Stage::Session, i18nc("@label", "The remote computer did not finish starting the session in time."), details);
        report.steps = {i18nc("@info", "The remote computer may still be starting up or be under heavy load. Wait a minute, then connect again.")};
        report.actions = ConnectionError::Retry;
        break;
    case FREERDP_ERROR_SERVER_DENIED_CONNECTION:
        report = ConnectionError::create(Stage::Session, i18nc("@label", "It accepted the connection, then refused to start a session."), details);
        report.title = i18nc("@title connection error", "Session refused");
        break;
    case FREERDP_ERROR_PRE_CONNECT_FAILED:
    case FREERDP_ERROR_POST_CONNECT_FAILED:
        report = ConnectionError::create(Stage::Session, i18nc("@label", "The server did not accept the options selected for this host."), details);
        break;
    case FREERDP_ERROR_RPC_INITIATED_DISCONNECT:
        report = ConnectionError::create(Stage::SessionEnded, i18nc("@label", "An administrator or another program disconnected this session."), details);
        break;
    case FREERDP_ERROR_RPC_INITIATED_LOGOFF:
        report = ConnectionError::create(Stage::SessionEnded, i18nc("@label", "An administrator signed this session out."), details);
        break;
    case FREERDP_ERROR_RPC_INITIATED_DISCONNECT_BY_USER:
        report = ConnectionError::create(Stage::SessionEnded, i18nc("@label", "The session was disconnected from the remote computer."), details);
        break;
    case FREERDP_ERROR_LOGOFF_BY_USER:
        // The user signed out in the remote session: close quietly, as when disconnecting here.
        setCanceledByUser();
        return;
    case FREERDP_ERROR_DISCONNECTED_BY_OTHER_CONNECTION:
        report = ConnectionError::create(Stage::SessionEnded, i18nc("@label", "Disconnected because someone else connected to this session."), details);
        report.steps = {i18nc("@info", "Reconnect to take the session back. This disconnects the other user.")};
        break;
    case FREERDP_ERROR_IDLE_TIMEOUT:
        report = ConnectionError::create(Stage::SessionEnded, i18nc("@label", "The session was closed after being idle for too long."), details);
        report.steps = {i18nc("@info", "Reconnect to continue. The idle time limit is set by the administrator of the server.")};
        break;
    default:
        qCDebug(KRDC) << "Unhandled error" << error;
        report = ConnectionError::create(stageForCurrentStatus(), QString(), details);
        break;
    }

    reportError(report);
}

void RdpView::onLogonError(const QString &error)
{
    ConnectionError report =
        ConnectionError::create(ConnectionError::Stage::Authentication, i18nc("@label", "The remote computer reported a problem while logging in."), error);
    report.steps.prepend(i18nc("@info", "Check that this user may log in to the remote computer, and that the session is not busy."));
    report.fatal = false;
    reportError(report);
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
