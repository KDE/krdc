/*
 * SPDX-FileCopyrightText: 2023 Arjen Hiemstra <ahiemstra@heimr.nl>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "rdpsession.h"

#include <memory>

#include <QKeyEvent>
#include <QMouseEvent>

#include <KLocalizedString>
#include <KMessageBox>
#include <KMessageDialog>
#include <KPasswordDialog>

#include <freerdp/addin.h>
#include <freerdp/channels/rdpgfx.h>
#include <freerdp/client.h>
#include <freerdp/client/channels.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/disp.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/event.h>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/gdi/gfx.h>
#include <freerdp/input.h>
#include <freerdp/utils/signal.h>
#include <winpr/synch.h>

#include "rdpview.h"

#include "krdc_debug.h"
#include "rdpaadview.h"

BOOL RdpSession::preConnect(freerdp *rdp)
{
    WINPR_ASSERT(rdp);
    auto ctx = rdp->context;
    WINPR_ASSERT(ctx);

    auto settings = ctx->settings;
    WINPR_ASSERT(settings);

    settings->OsMajorType = OSMAJORTYPE_UNIX;
    settings->OsMinorType = OSMINORTYPE_UNSPECIFIED;

    PubSub_SubscribeChannelConnected(ctx->pubSub, channelConnected);
    PubSub_SubscribeChannelDisconnected(ctx->pubSub, channelDisconnected);

    return TRUE;
}

BOOL RdpSession::postConnect(freerdp *rdp)
{
    WINPR_ASSERT(rdp);

    auto ctx = rdp->context;
    WINPR_ASSERT(ctx);

    auto rctx = reinterpret_cast<RdpContext *>(ctx);
    WINPR_ASSERT(rctx);

    auto session = rctx->session;
    WINPR_ASSERT(session);

    session->setState(State::Connected);

    auto settings = ctx->settings;
    WINPR_ASSERT(settings);

    auto &buffer = session->m_videoBuffer;
    buffer = QImage(settings->DesktopWidth, settings->DesktopHeight, QImage::Format_RGBA8888);

    if (!gdi_init_ex(rdp, PIXEL_FORMAT_RGBA32, buffer.bytesPerLine(), buffer.bits(), nullptr)) {
        qCWarning(KRDC) << "Could not initialize GDI subsystem";
        return false;
    }

    auto gdi = ctx->gdi;
    if (!gdi || gdi->width < 0 || gdi->height < 0) {
        return false;
    }

    session->m_size = QSize(gdi->width, gdi->height);
    Q_EMIT session->sizeChanged();

    ctx->update->EndPaint = endPaint;
    ctx->update->DesktopResize = resizeDisplay;

    // TODO: Custom QKeyEvent to scancode mapping! freerdp_keyboard_init_ex(settings->KeyboardLayout, settings->KeyboardRemappingList);

    return TRUE;
}

void RdpSession::postDisconnect(freerdp *rdp)
{
    WINPR_ASSERT(rdp);

    auto ctx = rdp->context;
    WINPR_ASSERT(ctx);

    auto session = reinterpret_cast<RdpContext *>(ctx)->session;
    WINPR_ASSERT(session);

    session->setState(State::Closed);
    gdi_free(rdp);
}

void RdpSession::postFinalDisconnect(freerdp *)
{
}

BOOL RdpSession::authenticateEx(freerdp *instance, char **username, char **password, char **domain, rdp_auth_reason reason)
{
    auto session = reinterpret_cast<RdpContext *>(instance->context)->session;
    // TODO: this needs to handle:
    // gateway
    // user
    // smartcard
    // AAD
    // needs new settings
    if (session->onAuthenticate(username, password, domain)) {
        return TRUE;
    }

    return FALSE;
}

DWORD RdpSession::verifyChangedCertificateEx(freerdp *rdp,
                                             const char *host,
                                             UINT16 port,
                                             const char *common_name,
                                             const char *subject,
                                             const char *issuer,
                                             const char *new_fingerprint,
                                             const char *old_subject,
                                             const char *old_issuer,
                                             const char *old_fingerprint,
                                             DWORD flags)
{
    auto session = reinterpret_cast<RdpContext *>(rdp->context)->session;

    // TODO: Update use or replace by whole custom cert handling
    // TODO: This reuses FreeRDP internal certificate store
    // TODO: Use VerifyX509Certificate for that and store the certificates with KRDC data
    Certificate oldCertificate;
    oldCertificate.host = QString::fromUtf8(host);
    oldCertificate.port = port;
    oldCertificate.commonName = QString::fromUtf8(common_name);
    oldCertificate.subject = QString::fromUtf8(old_subject);
    oldCertificate.issuer = QString::fromUtf8(old_issuer);
    oldCertificate.fingerprint = QString::fromUtf8(old_fingerprint);
    oldCertificate.flags = flags;

    Certificate newCertificate;
    newCertificate.host = oldCertificate.host;
    newCertificate.port = oldCertificate.port;
    newCertificate.commonName = oldCertificate.commonName;
    newCertificate.subject = QString::fromUtf8(subject);
    newCertificate.issuer = QString::fromUtf8(issuer);
    newCertificate.fingerprint = QString::fromUtf8(new_fingerprint);
    newCertificate.flags = flags;

    switch (session->onVerifyChangedCertificate(oldCertificate, newCertificate)) {
    case RdpSession::CertificateResult::DoNotAccept:
        return 0;
    case RdpSession::CertificateResult::AcceptTemporarily:
        return 2;
    case RdpSession::CertificateResult::AcceptPermanently:
        return 1;
    }

    return 0;
}

DWORD RdpSession::verifyCertificateEx(freerdp *rdp,
                                      const char *host,
                                      UINT16 port,
                                      const char *common_name,
                                      const char *subject,
                                      const char *issuer,
                                      const char *fingerprint,
                                      DWORD flags)
{
    auto session = reinterpret_cast<RdpContext *>(rdp->context)->session;
    // TODO: Update use or replace by whole custom cert handling
    // TODO: This reuses FreeRDP internal certificate store
    // TODO: Use VerifyX509Certificate for that and store the certificates with KRDC data
    Certificate certificate;
    certificate.host = QString::fromUtf8(host);
    certificate.port = port;
    certificate.commonName = QString::fromUtf8(common_name);
    certificate.subject = QString::fromUtf8(subject);
    certificate.issuer = QString::fromUtf8(issuer);
    certificate.fingerprint = QString::fromUtf8(fingerprint);
    certificate.flags = flags;

    switch (session->onVerifyCertificate(certificate)) {
    case RdpSession::CertificateResult::DoNotAccept:
        return 0;
    case RdpSession::CertificateResult::AcceptTemporarily:
        return 2;
    case RdpSession::CertificateResult::AcceptPermanently:
        return 1;
    }

    return 0;
}

int RdpSession::logonErrorInfo(freerdp *rdp, UINT32 data, UINT32 type)
{
    auto dataString = QString::fromUtf8(freerdp_get_logon_error_info_data(data));
    auto typeString = QString::fromUtf8(freerdp_get_logon_error_info_type(type));

    if (!rdp || !rdp->context)
        return -1;

    qCDebug(KRDC) << "Logon Error" << type;
    /* ignore LOGON_MSG_SESSION_CONTINUE message */
    if (type == LOGON_MSG_SESSION_CONTINUE)
        return 0;

    // TODO: Move this to UI thread
    // TODO: Block for result as this might be just a informative message
    KMessageBox::error(nullptr, typeString + QStringLiteral(" ") + dataString, i18nc("@title:dialog", "Logon Error"));

    return 1;
}

BOOL RdpSession::presentGatewayMessage(freerdp *instance, UINT32 type, BOOL isDisplayMandatory, BOOL isConsentMandatory, size_t length, const WCHAR *message)
{
    // TODO: Implement
    return FALSE;
}

BOOL RdpSession::chooseSmartcard(freerdp *instance, SmartcardCertInfo **cert_list, DWORD count, DWORD *choice, BOOL gateway)
{
    // TODO: Implement
    // TODO: Move this to UI thread
    // TODO: Block for result as this might be just a informative message
    return FALSE;
}

SSIZE_T RdpSession::retryDialog(freerdp *instance, const char *what, size_t current, void *userarg)
{
    // TODO: Implement
    // TODO: Move this to UI thread
    // TODO: Block for result as this might be just a informative message
    return -1;
}

BOOL RdpSession::client_global_init()
{
#if defined(_WIN32)
    WSADATA wsaData = {0};
    const DWORD wVersionRequested = MAKEWORD(1, 1);
    const int rc = WSAStartup(wVersionRequested, &wsaData);
    if (rc != 0) {
        WLog_ERR(SDL_TAG, "WSAStartup failed with %s [%d]", gai_strerrorA(rc), rc);
        return FALSE;
    }
#endif

    if (freerdp_handle_signals() != 0)
        return FALSE;

    return TRUE;
}

void RdpSession::client_global_uninit()
{
#if defined(_WIN32)
    WSACleanup();
#endif
}

BOOL RdpSession::client_context_new(freerdp *instance, rdpContext *context)
{
    auto kctx = reinterpret_cast<RdpContext *>(context);

    if (!instance || !context)
        return FALSE;

    instance->PreConnect = preConnect;
    instance->PostConnect = postConnect;
    instance->PostDisconnect = postDisconnect;
    instance->PostFinalDisconnect = postFinalDisconnect;
    instance->AuthenticateEx = authenticateEx;
    instance->VerifyCertificateEx = verifyCertificateEx;
    instance->VerifyChangedCertificateEx = verifyChangedCertificateEx;
    instance->LogonErrorInfo = logonErrorInfo;
    instance->PresentGatewayMessage = presentGatewayMessage;
    instance->ChooseSmartcard = chooseSmartcard;
    instance->RetryDialog = retryDialog;
    instance->GetAccessToken = RdpAADView::getAccessToken;

    return TRUE;
}

void RdpSession::client_context_free(freerdp *instance, rdpContext *context)
{
    auto ctx = reinterpret_cast<RdpContext *>(context);

    if (!ctx)
        return;
    ctx->session = nullptr;
}

int RdpSession::client_context_start(rdpContext *context)
{
    auto kcontext = reinterpret_cast<RdpContext *>(context);
    WINPR_ASSERT(kcontext);

    auto session = kcontext->session;
    WINPR_ASSERT(session);

    auto settings = context->settings;
    WINPR_ASSERT(settings);

    session->setState(State::Starting);

    qCInfo(KRDC) << "Starting RDP session";

    // TODO: Migrate to freerdp_settings_set_* API
    settings->ServerHostname = qstrdup(session->m_host.toUtf8().data());
    settings->ServerPort = session->m_port;

    settings->Username = qstrdup(session->m_user.toUtf8().data());
    settings->Password = qstrdup(session->m_password.toUtf8().data());

    if (session->m_size.width() > 0 && session->m_size.height() > 0) {
        settings->DesktopWidth = session->m_size.width();
        settings->DesktopHeight = session->m_size.height();
    }

    switch (session->m_preferences->acceleration()) {
    case RdpHostPreferences::Acceleration::ForceGraphicsPipeline:
        settings->SupportGraphicsPipeline = true;
        settings->GfxAVC444 = true;
        settings->GfxAVC444v2 = true;
        settings->GfxH264 = true;
        settings->RemoteFxCodec = false;
        settings->ColorDepth = 32;
        break;
    case RdpHostPreferences::Acceleration::ForceRemoteFx:
        settings->SupportGraphicsPipeline = false;
        settings->GfxAVC444 = false;
        settings->GfxAVC444v2 = false;
        settings->GfxH264 = false;
        settings->RemoteFxCodec = true;
        settings->ColorDepth = 32;
        break;
    case RdpHostPreferences::Acceleration::Disabled:
        settings->SupportGraphicsPipeline = false;
        settings->GfxAVC444 = false;
        settings->GfxAVC444v2 = false;
        settings->GfxH264 = false;
        settings->RemoteFxCodec = false;
        break;
    case RdpHostPreferences::Acceleration::Auto:
        settings->SupportGraphicsPipeline = true;
        settings->GfxAVC444 = true;
        settings->GfxAVC444v2 = true;
        settings->GfxH264 = true;
        settings->RemoteFxCodec = true;
        settings->ColorDepth = 32;
        break;
    }

    switch (session->m_preferences->colorDepth()) {
    case RdpHostPreferences::ColorDepth::Auto:
    case RdpHostPreferences::ColorDepth::Depth32:
        settings->ColorDepth = 32;
        break;
    case RdpHostPreferences::ColorDepth::Depth24:
        settings->ColorDepth = 24;
        break;
    case RdpHostPreferences::ColorDepth::Depth16:
        settings->ColorDepth = 16;
        break;
    case RdpHostPreferences::ColorDepth::Depth8:
        settings->ColorDepth = 8;
    }

    settings->FastPathOutput = true;
    settings->FastPathInput = true;
    settings->FrameMarkerCommandEnabled = true;

    settings->SupportDynamicChannels = true;

    switch (session->m_preferences->sound()) {
    case RdpHostPreferences::Sound::Local:
        settings->AudioPlayback = true;
        settings->AudioCapture = true;
        break;
    case RdpHostPreferences::Sound::Remote:
        settings->RemoteConsoleAudio = true;
        break;
    case RdpHostPreferences::Sound::Disabled:
        settings->AudioPlayback = false;
        settings->AudioCapture = false;
        break;
    }

    if (!session->m_preferences->shareMedia().isEmpty()) {
        const char *params[2] = {strdup("drive"), session->m_preferences->shareMedia().toUtf8().data()};
        freerdp_client_add_device_channel(settings, 1, params);
    }

    settings->KeyboardLayout = session->m_preferences->rdpKeyboardLayout();

    switch (session->m_preferences->tlsSecLevel()) {
    case RdpHostPreferences::TlsSecLevel::Bit80:
        settings->TlsSecLevel = 1;
        break;
    case RdpHostPreferences::TlsSecLevel::Bit112:
        settings->TlsSecLevel = 2;
        break;
    case RdpHostPreferences::TlsSecLevel::Bit128:
        settings->TlsSecLevel = 3;
        break;
    case RdpHostPreferences::TlsSecLevel::Bit192:
        settings->TlsSecLevel = 4;
        break;
    case RdpHostPreferences::TlsSecLevel::Bit256:
        settings->TlsSecLevel = 5;
        break;
    case RdpHostPreferences::TlsSecLevel::Any:
    default:
        settings->TlsSecLevel = 0;
        break;
    }

    session->m_thread = std::thread(std::bind(&RdpSession::run, session));
    pthread_setname_np(session->m_thread.native_handle(), "rdp_session");
    return 0;
}

int RdpSession::client_context_stop(rdpContext *context)
{
    auto kcontext = reinterpret_cast<RdpContext *>(context);
    WINPR_ASSERT(kcontext);

    /* We do not want to use freerdp_abort_connect_context here.
     * It would change the exit code and we do not want that. */
    HANDLE event = freerdp_abort_event(context);
    if (!SetEvent(event))
        return -1;

    WINPR_ASSERT(kcontext->session);
    if (kcontext->session->m_thread.joinable()) {
        kcontext->session->m_thread.join();
    }

    return 0;
}

RDP_CLIENT_ENTRY_POINTS RdpSession::RdpClientEntry()
{
    RDP_CLIENT_ENTRY_POINTS entry = {};

    entry.Version = RDP_CLIENT_INTERFACE_VERSION;
    entry.Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
    entry.GlobalInit = client_global_init;
    entry.GlobalUninit = client_global_uninit;
    entry.ContextSize = sizeof(RdpContext);
    entry.ClientNew = client_context_new;
    entry.ClientFree = client_context_free;
    entry.ClientStart = client_context_start;
    entry.ClientStop = client_context_stop;

    return entry;
}

BOOL RdpSession::endPaint(rdpContext *context)
{
    WINPR_ASSERT(context);

    auto session = reinterpret_cast<RdpContext *>(context)->session;
    WINPR_ASSERT(session);

    auto gdi = context->gdi;

    if (!gdi || !gdi->primary) {
        return FALSE;
    }

    auto invalid = gdi->primary->hdc->hwnd->invalid;
    if (invalid->null) {
        return TRUE;
    }

    auto rect = QRect{invalid->x, invalid->y, invalid->w, invalid->h};
    Q_EMIT session->rectangleUpdated(rect);
    return TRUE;
}

BOOL RdpSession::resizeDisplay(rdpContext *context)
{
    WINPR_ASSERT(context);

    auto session = reinterpret_cast<RdpContext *>(context)->session;
    WINPR_ASSERT(session);

    auto gdi = context->gdi;
    WINPR_ASSERT(gdi);

    auto settings = context->settings;
    WINPR_ASSERT(settings);

    auto &buffer = session->m_videoBuffer;
    buffer = QImage(settings->DesktopWidth, settings->DesktopHeight, QImage::Format_RGBA8888);

    if (!gdi_resize_ex(gdi, settings->DesktopWidth, settings->DesktopHeight, buffer.bytesPerLine(), PIXEL_FORMAT_RGBA32, buffer.bits(), nullptr)) {
        qCWarning(KRDC) << "Failed resizing GDI subsystem";
        return false;
    }

    session->m_size = QSize(settings->DesktopWidth, settings->DesktopHeight);
    Q_EMIT session->sizeChanged();

    return TRUE;
}

void RdpSession::channelConnected(void *context, const ChannelConnectedEventArgs *e)
{
    if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0) {
        CliprdrClientContext *clip = (CliprdrClientContext *)e->pInterface;
        clip->custom = context;
        // TODO: Implement clipboard
    } else if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0) {
        auto disp = reinterpret_cast<DispClientContext *>(e->pInterface);
        WINPR_ASSERT(disp);
        // TODO: Implement display channel
    } else
        freerdp_client_OnChannelConnectedEventHandler(context, e);
}

void RdpSession::channelDisconnected(void *context, const ChannelDisconnectedEventArgs *e)
{
    if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0) {
        CliprdrClientContext *clip = (CliprdrClientContext *)e->pInterface;
        clip->custom = nullptr;
        // TODO: Implement clipboard
    } else if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0) {
        auto disp = reinterpret_cast<DispClientContext *>(e->pInterface);
        WINPR_ASSERT(disp);
        // TODO: Implement display channel
    } else
        freerdp_client_OnChannelDisconnectedEventHandler(context, e);
}

QString Certificate::toString() const
{
    return i18nc("@label", "Host: %1:%2\nCommon Name: %3\nSubject: %4\nIssuer: %5\nFingerprint: %6\n", host, port, commonName, subject, issuer, fingerprint);
}

RdpSession::RdpSession(RdpView *view)
    : QObject(nullptr)
    , m_view(view)
{
    auto entry = RdpClientEntry();
    m_context.rdp = freerdp_client_context_new(&entry);
    m_context.krdp->session = this;
}

RdpSession::~RdpSession()
{
    stop();
    freerdp_client_context_free(m_context.rdp);
}

RdpSession::State RdpSession::state() const
{
    return m_state;
}

QString RdpSession::host() const
{
    return m_host;
}

void RdpSession::setHost(const QString &newHost)
{
    m_host = newHost;
}

QString RdpSession::user() const
{
    return m_user;
}

void RdpSession::setUser(const QString &newUser)
{
    m_user = newUser;
}

QString RdpSession::domain() const
{
    return m_domain;
}

void RdpSession::setDomain(const QString &newDomain)
{
    m_domain = newDomain;
}

QString RdpSession::password() const
{
    return m_password;
}

void RdpSession::setPassword(const QString &newPassword)
{
    m_password = newPassword;
}

int RdpSession::port() const
{
    return m_port;
}

void RdpSession::setPort(int port)
{
    m_port = port;
}

RdpHostPreferences *RdpSession::preferences() const
{
    return m_preferences;
}

void RdpSession::setHostPreferences(RdpHostPreferences *preferences)
{
    m_preferences = preferences;
}

QSize RdpSession::size() const
{
    return m_size;
}

void RdpSession::setSize(QSize size)
{
    m_size = size;
}

bool RdpSession::start()
{
    return freerdp_client_start(m_context.rdp);
}

void RdpSession::stop()
{
    freerdp_client_stop(m_context.rdp);
}

const QImage *RdpSession::videoBuffer() const
{
    return &m_videoBuffer;
}

bool RdpSession::sendEvent(QEvent *event, QWidget *source)
{
    auto input = m_context.rdp->input;

    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::KeyRelease: {
        auto keyEvent = static_cast<QKeyEvent *>(event);
        // TODO: Proper scancode to RDP scancode mapping
        // auto code = freerdp_keyboard_get_rdp_scancode_from_x11_keycode(keyEvent->nativeScanCode());
        auto code = keyEvent->nativeScanCode();
        freerdp_input_send_keyboard_event_ex(input, keyEvent->type() == QEvent::KeyPress, keyEvent->isAutoRepeat(), code);
        return true;
    }
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseMove: {
        auto mouseEvent = static_cast<QMouseEvent *>(event);
        auto position = mouseEvent->localPos();
        auto sourceSize = QSizeF{source->size()};

        auto x = (position.x() / sourceSize.width()) * m_size.width();
        auto y = (position.y() / sourceSize.height()) * m_size.height();

        bool extendedEvent = false;
        UINT16 flags = 0;

        switch (mouseEvent->button()) {
        case Qt::LeftButton:
            flags |= PTR_FLAGS_BUTTON1;
            break;
        case Qt::RightButton:
            flags |= PTR_FLAGS_BUTTON2;
            break;
        case Qt::MiddleButton:
            flags |= PTR_FLAGS_BUTTON3;
            break;
        case Qt::BackButton:
            flags |= PTR_XFLAGS_BUTTON1;
            extendedEvent = true;
            break;
        case Qt::ForwardButton:
            flags |= PTR_XFLAGS_BUTTON2;
            extendedEvent = true;
            break;
        default:
            break;
        }

        if (mouseEvent->type() == QEvent::MouseButtonPress || mouseEvent->type() == QEvent::MouseButtonDblClick) {
            if (extendedEvent) {
                flags |= PTR_XFLAGS_DOWN;
            } else {
                flags |= PTR_FLAGS_DOWN;
            }
        } else if (mouseEvent->type() == QEvent::MouseMove) {
            flags |= PTR_FLAGS_MOVE;
        }

        if (extendedEvent) {
            freerdp_input_send_extended_mouse_event(input, flags, uint16_t(x), uint16_t(y));
        } else {
            freerdp_input_send_mouse_event(input, flags, uint16_t(x), uint16_t(y));
        }

        return true;
    }
    case QEvent::Wheel: {
        auto wheelEvent = static_cast<QWheelEvent *>(event);
        auto delta = wheelEvent->angleDelta();

        uint16_t flags = 0;
        uint16_t value = 0;
        if (delta.y() != 0) {
            value = std::clamp(std::abs(delta.y()), 0, 0xFF);
            flags |= PTR_FLAGS_WHEEL;
            if (wheelEvent->angleDelta().y() < 0) {
                flags |= PTR_FLAGS_WHEEL_NEGATIVE;
                flags = (flags & 0xFF00) | (0x100 - value);
            } else {
                flags |= value;
            }
        } else if (wheelEvent->angleDelta().x() != 0) {
            value = std::clamp(std::abs(delta.x()), 0, 0xFF);
            flags |= PTR_FLAGS_HWHEEL;
            if (wheelEvent->angleDelta().x() < 0) {
                flags |= PTR_FLAGS_WHEEL_NEGATIVE;
                flags = (flags & 0xFF00) | (0x100 - value);
            } else {
                flags |= value;
            }
        }

        auto position = wheelEvent->position();
        auto sourceSize = QSizeF{source->size()};

        auto x = (position.x() / sourceSize.width()) * m_size.width();
        auto y = (position.y() / sourceSize.height()) * m_size.height();

        freerdp_input_send_mouse_event(input, flags, uint16_t(x), uint16_t(y));
    }
    default:
        break;
    }

    return QObject::event(event);
}

void RdpSession::setState(RdpSession::State newState)
{
    if (newState == m_state) {
        return;
    }

    m_state = newState;
    Q_EMIT stateChanged();
}

bool RdpSession::onAuthenticate(char **username, char **password, char **domain)
{
    Q_UNUSED(domain);

    std::unique_ptr<KPasswordDialog> dialog;
    bool hasUsername = qstrlen(*username) != 0;
    if (hasUsername) {
        dialog = std::make_unique<KPasswordDialog>(nullptr, KPasswordDialog::ShowKeepPassword);
        dialog->setPrompt(i18nc("@label", "Access to this system requires a password."));
    } else {
        dialog = std::make_unique<KPasswordDialog>(nullptr, KPasswordDialog::ShowUsernameLine | KPasswordDialog::ShowKeepPassword);
        dialog->setPrompt(i18nc("@label", "Access to this system requires a username and password."));
    }

    if (!dialog->exec()) {
        return false;
    }

    *password = qstrdup(dialog->password().toUtf8().data());

    if (!hasUsername) {
        *username = qstrdup(dialog->username().toUtf8().data());
    }

    if (dialog->keepPassword()) {
        m_view->savePassword(dialog->password());
    }

    return true;
}

RdpSession::CertificateResult RdpSession::onVerifyCertificate(const Certificate &certificate)
{
    KMessageDialog dialog{KMessageDialog::QuestionTwoActions, i18nc("@label", "The certificate for this system is unknown. Do you wish to continue?")};
    dialog.setCaption(i18nc("@title:dialog", "Verify Certificate"));
    dialog.setIcon(QIcon::fromTheme(QStringLiteral("view-certficate")));

    dialog.setDetails(certificate.toString());

    dialog.setDontAskAgainText(i18nc("@label", "Remember this certificate"));

    dialog.setButtons(KStandardGuiItem::cont(), KStandardGuiItem::cancel());

    if (!dialog.exec()) {
        return CertificateResult::DoNotAccept;
    }

    if (dialog.isDontAskAgainChecked()) {
        return CertificateResult::AcceptPermanently;
    } else {
        return CertificateResult::AcceptTemporarily;
    }
}

RdpSession::CertificateResult RdpSession::onVerifyChangedCertificate(const Certificate &oldCertificate, const Certificate &newCertificate)
{
    KMessageDialog dialog{KMessageDialog::QuestionTwoActions, i18nc("@label", "The certificate for this system has changed. Do you wish to continue?")};
    dialog.setCaption(i18nc("@title:dialog", "Certificate has Changed"));
    dialog.setIcon(QIcon::fromTheme(QStringLiteral("view-certficate")));

    dialog.setDetails(i18nc("@label", "Previous certificate:\n%1\nNew Certificate:\n%2", oldCertificate.toString(), newCertificate.toString()));

    dialog.setDontAskAgainText(i18nc("@label", "Remember this certificate"));

    dialog.setButtons(KStandardGuiItem::cont(), KStandardGuiItem::cancel());

    if (!dialog.exec()) {
        return CertificateResult::DoNotAccept;
    }

    if (dialog.isDontAskAgainChecked()) {
        return CertificateResult::AcceptPermanently;
    } else {
        return CertificateResult::AcceptTemporarily;
    }
}

/* RDP main thread.
 * run all stuff on this thread to avoid issues with race conditions
 */
void RdpSession::run()
{
    auto timer = CreateWaitableTimerA(nullptr, FALSE, "rdp-session-timer");
    if (!timer) {
        return;
    }

    LARGE_INTEGER due;
    due.QuadPart = 0;
    if (!SetWaitableTimer(timer, &due, 1, nullptr, nullptr, false)) {
        return;
    }

    auto instance = m_context.rdp->instance;
    if (!freerdp_connect(instance)) {
        qWarning(KRDC) << "Unable to connect";
        emitErrorMessage();
        return;
    }

    setState(State::Running);

    HANDLE handles[MAXIMUM_WAIT_OBJECTS] = {};
    while (!freerdp_shall_disconnect_context(m_context.rdp)) {
        handles[0] = timer;
        auto count = freerdp_get_event_handles(m_context.rdp, &handles[1], ARRAYSIZE(handles) - 1);

        auto status = WaitForMultipleObjects(count, handles, FALSE, INFINITE);
        if (status == WAIT_FAILED) {
            emitErrorMessage();
            break;
        }

        if (freerdp_check_event_handles(m_context.rdp) != TRUE) {
            emitErrorMessage();
            break;
        }
    }

    freerdp_disconnect(instance);
}

void RdpSession::emitErrorMessage()
{
    const unsigned int error = freerdp_get_last_error(m_context.rdp);

    if (error == FREERDP_ERROR_CONNECT_CANCELLED) {
        return;
    }

    auto name = freerdp_get_last_error_name(error);
    auto description = freerdp_get_last_error_string(error);
    qCDebug(KRDC) << name << description;

    Q_EMIT errorMessage(error);
}
