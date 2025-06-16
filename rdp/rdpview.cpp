/*
    SPDX-FileCopyrightText: 2002 Arend van Beelen jr. <arend@auton.nl>
    SPDX-FileCopyrightText: 2007-2012 Urs Wolfer <uwolfer@kde.org>
    SPDX-FileCopyrightText: 2012 AceLan Kao <acelan@acelan.idv.tw>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rdpview.h"

#include "krdc_debug.h"
#include "rdphostpreferences.h"

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
#include <QUrlQuery>
#include <QWindow>

#include "rdpsession.h"

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
        m_session->sendResizeEvent(QSize(w, h) * devicePixelRatio());
    }
}

QSize RdpView::sizeHint() const
{
    if (!m_session) {
        return QSize{};
    }

    // when parent is resized and scaling is enabled, resize the view, preserving aspect ratio
    if (m_hostPreferences->scaleToSize()) {
        return m_session->size().scaled(parentWidget()->size(), Qt::KeepAspectRatio);
    }

    return m_session->size() / devicePixelRatio();
}

void RdpView::startQuitting()
{
    if (m_quitting) {
        return; // ignore repeated triggers
    }

    qCDebug(KRDC) << "Stopping RDP session";
    m_quitting = true;

    unpressModifiers();

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

bool RdpView::start()
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

    if (m_quitting)
        return;

    switch (error) {
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
        title = i18nc("@title:dialog", "Connection Lost");
        message = i18nc("@label", "Lost connection to the server.");
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
    case FREERDP_ERROR_TLS_CONNECT_FAILED:
    case FREERDP_ERROR_CONNECT_TRANSPORT_FAILED:
        title = i18nc("@title:dialog", "Could not Connect");
        message = i18nc("@label", "Could not connect to the server.");
        break;
    case FREERDP_ERROR_RPC_INITIATED_DISCONNECT:
    case FREERDP_ERROR_RPC_INITIATED_LOGOFF:
    case FREERDP_ERROR_RPC_INITIATED_DISCONNECT_BY_USER:
    case FREERDP_ERROR_LOGOFF_BY_USER:
        // user or admin initiated action, quit without error
        return;
    case FREERDP_ERROR_DISCONNECTED_BY_OTHER_CONNECTION:
        title = i18nc("@title:dialog", "Connection Closed");
        message = QStringLiteral("Diconnected by other sesion");
        break;
    case FREERDP_ERROR_BASE:
        title = i18nc("@title:dialog", "Connection Closed");
        message = i18nc("@label", "The connection to the server was closed.");
        break;
    default:
        qCDebug(KRDC) << "Unhandled error" << error;
        title = i18nc("@title:dialog", "Connection Failed");
        message = i18nc("@label", "An unknown error occurred");
        break;
    }

    qCDebug(KRDC) << "error message" << title << message;
    // TODO offer reconnect if approriate
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

void RdpView::showLocalCursor(LocalCursorState state)
{
    RemoteView::showLocalCursor(state);

    if (state == CursorOn) {
        // show local cursor, hide remote one
        setCursor(localDefaultCursor());
    } else {
        // hide local cursor, show remote one
        setCursor(m_remoteCursor);
    }
}

void RdpView::setRemoteCursor(const QCursor cursor)
{
    m_remoteCursor = cursor;
    if (m_localCursorState != CursorOn) {
        setCursor(m_remoteCursor);
    }
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

void RdpView::paintEvent(QPaintEvent *event)
{
    if (!m_session || m_session->videoBuffer()->isNull()) {
        return;
    }

    QPainter painter;

    painter.begin(this);
    painter.setClipRect(event->rect());

    auto image = *m_session->videoBuffer();
    image.setDevicePixelRatio(devicePixelRatio());

    if (m_hostPreferences->scaleToSize()) {
        painter.drawImage(QPoint{0, 0}, image.scaled(size() * devicePixelRatio(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        painter.drawImage(QPoint{0, 0}, image);
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
        m_session->sendEvent(event, this);
    }
}

void RdpView::handleWheelEvent(QWheelEvent *event)
{
    if (m_session) {
        m_session->sendEvent(event, this);
    }
}

void RdpView::onRectangleUpdated(const QRect &rect)
{
    m_pendingRectangle = rect;
    update();
}

void RdpView::handleLocalClipboardChanged(const QMimeData *data)
{
    if (m_session) {
        m_session->sendClipboard(data);
    }
}