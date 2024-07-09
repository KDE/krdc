/*
 * SPDX-FileCopyrightText: 2023 Arjen Hiemstra <ahiemstra@heimr.nl>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "rdpsession.h"
#include "rdpcliprdr.h"
#include "rdphostpreferences.h"

#include <memory>

#include <QApplication>
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
#include <freerdp/client/cmdline.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/event.h>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/gdi/gfx.h>
#include <freerdp/input.h>
#include <winpr/synch.h>
#ifdef Q_OS_UNIX
#include <freerdp/locale/keyboard.h>
#endif

#include "rdpview.h"

#include "krdc_debug.h"

BOOL preConnect(freerdp *rdp)
{
    auto session = reinterpret_cast<RdpContext *>(rdp->context)->session;
    if (session->onPreConnect()) {
        return TRUE;
    }
    return FALSE;
}

BOOL postConnect(freerdp *rdp)
{
    auto session = reinterpret_cast<RdpContext *>(rdp->context)->session;
    if (session->onPostConnect()) {
        return TRUE;
    }
    return FALSE;
}

void postDisconnect(freerdp *rdp)
{
    auto session = reinterpret_cast<RdpContext *>(rdp->context)->session;
    session->onPostDisconnect();
}

BOOL authenticate(freerdp *rdp, char **username, char **password, char **domain)
{
    auto session = reinterpret_cast<RdpContext *>(rdp->context)->session;
    if (session->onAuthenticate(username, password, domain)) {
        return TRUE;
    }

    return FALSE;
}

DWORD verifyChangedCertificate(freerdp *rdp,
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

DWORD verifyCertificate(freerdp *rdp,
                        const char *host,
                        UINT16 port,
                        const char *common_name,
                        const char *subject,
                        const char *issuer,
                        const char *fingerprint,
                        DWORD flags)
{
    auto session = reinterpret_cast<RdpContext *>(rdp->context)->session;

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

int logonErrorInfo(freerdp *rdp, UINT32 data, UINT32 type)
{
    auto dataString = QString::fromUtf8(freerdp_get_logon_error_info_data(data));
    auto typeString = QString::fromUtf8(freerdp_get_logon_error_info_type(type));

    if (!rdp || !rdp->context)
        return -1;

    qCDebug(KRDC) << "Logon Error" << type;
    /* ignore LOGON_MSG_SESSION_CONTINUE message */
    if (type == LOGON_MSG_SESSION_CONTINUE)
        return 0;

    KMessageBox::error(nullptr, typeString + QStringLiteral(" ") + dataString, i18nc("@title:dialog", "Logon Error"));

    return 1;
}

BOOL endPaint(rdpContext *context)
{
    auto session = reinterpret_cast<RdpContext *>(context)->session;
    if (session->onEndPaint()) {
        return TRUE;
    }
    return FALSE;
}

BOOL resizeDisplay(rdpContext *context)
{
    auto session = reinterpret_cast<RdpContext *>(context)->session;
    if (session->onResizeDisplay()) {
        return TRUE;
    }
    return FALSE;
}

BOOL playSound(rdpContext *context, const PLAY_SOUND_UPDATE *play_sound)
{
    Q_UNUSED(play_sound);
    auto session = reinterpret_cast<RdpContext *>(context)->session;
    if (session->onPlaySound()) {
        return TRUE;
    }
    return FALSE;
}

void channelConnected(void *context, ChannelConnectedEventArgs *e)
{
    if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0) {
        rdpContext *rdpC = reinterpret_cast<rdpContext *>(context);
        gdi_graphics_pipeline_init(rdpC->gdi, (RdpgfxClientContext *)e->pInterface);
    } else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0) {
        auto session = reinterpret_cast<RdpContext *>(context)->session;
        auto krdcContext = reinterpret_cast<RdpContext *>(context);
        auto cliprdr = reinterpret_cast<CliprdrClientContext *>(e->pInterface);
        session->initializeClipboard(krdcContext, cliprdr);
    }
}

void channelDisconnected(void *context, ChannelDisconnectedEventArgs *e)
{
    if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0) {
        rdpContext *rdpC = reinterpret_cast<rdpContext *>(context);
        gdi_graphics_pipeline_uninit(rdpC->gdi, (RdpgfxClientContext *)e->pInterface);
    } else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0) {
        auto session = reinterpret_cast<RdpContext *>(context)->session;
        session->destroyClipboard();
    }
}

QString Certificate::toString() const
{
    return i18nc("@label", "Host: %1:%2\nCommon Name: %3\nSubject: %4\nIssuer: %5\nFingerprint: %6\n", host, port, commonName, subject, issuer, fingerprint);
}

RdpSession::RdpSession(RdpView *view)
    : QObject(nullptr)
    , m_view(view)
{
}

RdpSession::~RdpSession()
{
    stop();
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
    setState(State::Starting);

    qCInfo(KRDC) << "Starting RDP session";

    m_freerdp = freerdp_new();

    m_freerdp->ContextSize = sizeof(RdpContext);
    m_freerdp->ContextNew = nullptr;
    m_freerdp->ContextFree = nullptr;

    m_freerdp->Authenticate = authenticate;
    m_freerdp->VerifyCertificateEx = verifyCertificate;
    m_freerdp->VerifyChangedCertificateEx = verifyChangedCertificate;
    m_freerdp->LogonErrorInfo = logonErrorInfo;

    m_freerdp->PreConnect = preConnect;
    m_freerdp->PostConnect = postConnect;
    m_freerdp->PostDisconnect = postDisconnect;

    freerdp_context_new(m_freerdp);

    m_context = reinterpret_cast<RdpContext *>(m_freerdp->context);
    m_context->session = this;

    if (freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry, 0) != CHANNEL_RC_OK) {
        return false;
    }

    auto settings = m_freerdp->settings;
    if (!freerdp_settings_set_string(settings, FreeRDP_ServerHostname, m_host.toUtf8().data())) {
        return false;
    }
    if (!freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, m_port)) {
        return false;
    }

    if (!freerdp_settings_set_string(settings, FreeRDP_Username, m_user.toUtf8().data())) {
        return false;
    }
    if (!freerdp_settings_set_string(settings, FreeRDP_Password, m_password.toUtf8().data())) {
        return false;
    }

    if (m_size.width() > 0 && m_size.height() > 0) {
        if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, m_size.width())) {
            return false;
        }
        if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, m_size.height())) {
            return false;
        }
    }

    switch (m_preferences->colorDepth()) {
    case RdpHostPreferences::ColorDepth::Auto:
    case RdpHostPreferences::ColorDepth::Depth32:
        if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32)) {
            return false;
        }
        break;
    case RdpHostPreferences::ColorDepth::Depth24:
        if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 24)) {
            return false;
        }
        break;
    case RdpHostPreferences::ColorDepth::Depth16:
        if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 16)) {
            return false;
        }
        break;
    case RdpHostPreferences::ColorDepth::Depth8:
        if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 8)) {
            return false;
        }
    }

    switch (m_preferences->acceleration()) {
    case RdpHostPreferences::Acceleration::ForceGraphicsPipeline:
        if (!freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, true)) {
            return false;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, true)) {
            return false;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444v2, true)) {
            return false;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxH264, true)) {
            return false;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, false)) {
            return false;
        }
        if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32)) {
            return false;
        }
        break;
    case RdpHostPreferences::Acceleration::ForceRemoteFx:
        if (!freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, false)) {
            return false;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, false)) {
            return false;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444v2, false)) {
            return false;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxH264, false)) {
            return false;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, true)) {
            return false;
        }
        if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32)) {
            return false;
        }
        break;
    case RdpHostPreferences::Acceleration::Disabled:
        if (!freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, false)) {
            return false;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, false)) {
            return false;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444v2, false)) {
            return false;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxH264, false)) {
            return false;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, false)) {
            return false;
        }
        break;
    case RdpHostPreferences::Acceleration::Auto:
        if (!freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, true)) {
            return false;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, true)) {
            return false;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444v2, true)) {
            return false;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxH264, true)) {
            return false;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, true)) {
            return false;
        }
        if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32)) {
            return false;
        }
        break;
    }

    if (!freerdp_settings_set_bool(settings, FreeRDP_FastPathOutput, true)) {
        return false;
    }
    if (!freerdp_settings_set_bool(settings, FreeRDP_FastPathInput, true)) {
        return false;
    }
    if (!freerdp_settings_set_bool(settings, FreeRDP_FrameMarkerCommandEnabled, true)) {
        return false;
    }
    if (!freerdp_settings_set_bool(settings, FreeRDP_SupportDynamicChannels, true)) {
        return false;
    }

    switch (m_preferences->sound()) {
    case RdpHostPreferences::Sound::Local:
        if (!freerdp_settings_set_bool(settings, FreeRDP_AudioPlayback, true)) {
            return false;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_AudioCapture, true)) {
            return false;
        }
        break;
    case RdpHostPreferences::Sound::Remote:
        if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteConsoleAudio, true)) {
            return false;
        }
        break;
    case RdpHostPreferences::Sound::Disabled:
        if (!freerdp_settings_set_bool(settings, FreeRDP_AudioPlayback, false)) {
            return false;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_AudioCapture, false)) {
            return false;
        }
        break;
    }

    if (!m_preferences->shareMedia().isEmpty()) {
        QByteArray name = "drive";
        QByteArray value = m_preferences->shareMedia().toLocal8Bit();

        char *params[2] = {name.data(), value.data()};
        freerdp_client_add_device_channel(settings, 2, params);
    }

    if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardLayout, m_preferences->rdpKeyboardLayout())) {
        return false;
    }

    switch (m_preferences->tlsSecLevel()) {
    case RdpHostPreferences::TlsSecLevel::Bit80:
        if (!freerdp_settings_set_uint32(settings, FreeRDP_TlsSecLevel, 1)) {
            return false;
        }
        break;
    case RdpHostPreferences::TlsSecLevel::Bit112:
        if (!freerdp_settings_set_uint32(settings, FreeRDP_TlsSecLevel, 2)) {
            return false;
        }
        break;
    case RdpHostPreferences::TlsSecLevel::Bit128:
        if (!freerdp_settings_set_uint32(settings, FreeRDP_TlsSecLevel, 3)) {
            return false;
        }
        break;
    case RdpHostPreferences::TlsSecLevel::Bit192:
        if (!freerdp_settings_set_uint32(settings, FreeRDP_TlsSecLevel, 4)) {
            return false;
        }
        break;
    case RdpHostPreferences::TlsSecLevel::Bit256:
        if (!freerdp_settings_set_uint32(settings, FreeRDP_TlsSecLevel, 5)) {
            return false;
        }
        break;
    case RdpHostPreferences::TlsSecLevel::Any:
    default:
        if (!freerdp_settings_set_uint32(settings, FreeRDP_TlsSecLevel, 0)) {
            return false;
        }
        break;
    }

    const auto proxyHostAddress = QUrl(m_preferences->proxyHost());
    if (!proxyHostAddress.isEmpty()) {
        int defaultPort = 8080;
        switch (m_preferences->proxyProtocol()) {
        case RdpHostPreferences::ProxyProtocol::HTTP:
            if (!freerdp_settings_set_uint32(settings, FreeRDP_ProxyType, PROXY_TYPE_HTTP)) {
                return false;
            }
            break;
        case RdpHostPreferences::ProxyProtocol::SOCKS:
            if (!freerdp_settings_set_uint32(settings, FreeRDP_ProxyType, PROXY_TYPE_SOCKS)) {
                return false;
            }
            defaultPort = 1080;
            break;
        default:
            if (!freerdp_settings_set_uint32(settings, FreeRDP_ProxyType, PROXY_TYPE_NONE)) {
                return false;
            }
            break;
        }

        if (!freerdp_settings_set_string(settings, FreeRDP_ProxyHostname, proxyHostAddress.host().toUtf8().data())) {
            return false;
        }
        if (!freerdp_settings_set_string(settings, FreeRDP_ProxyUsername, m_preferences->proxyUsername().toUtf8().data())) {
            return false;
        }
        if (!freerdp_settings_set_string(settings, FreeRDP_ProxyPassword, m_preferences->proxyPassword().toUtf8().data())) {
            return false;
        }
        if (!freerdp_settings_set_uint32(settings, FreeRDP_ProxyPort, proxyHostAddress.port(defaultPort))) {
            return false;
        }
    }

    const auto gatewayServerAddress = QUrl(m_preferences->proxyHost());
    if (!gatewayServerAddress.isEmpty()) {
        if (!freerdp_settings_set_string(settings, FreeRDP_GatewayHostname, gatewayServerAddress.host().toUtf8().data())) {
            return false;
        }
        if (!freerdp_settings_set_uint32(settings, FreeRDP_GatewayPort, gatewayServerAddress.port(3389))) {
            return false;
        }
        if (!freerdp_settings_set_string(settings, FreeRDP_GatewayUsername, m_preferences->gatewayUsername().toUtf8().data())) {
            return false;
        }
        if (!freerdp_settings_set_string(settings, FreeRDP_GatewayPassword, m_preferences->gatewayPassword().toUtf8().data())) {
            return false;
        }
        if (!freerdp_settings_set_string(settings, FreeRDP_GatewayDomain, m_preferences->gatewayDomain().toUtf8().data())) {
            return false;
        }

        switch (m_preferences->gatewayTransportType()) {
        case RdpHostPreferences::GatewayTransportType::RPC:
            if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, false)) {
                return false;
            }
            if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, true)) {
                return false;
            }
            break;
        case RdpHostPreferences::GatewayTransportType::HTTP:
            if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, true)) {
                return false;
            }
            if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, false)) {
                return false;
            }
            break;
        case RdpHostPreferences::GatewayTransportType::Auto:
        default:
            if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, true)) {
                return false;
            }
            if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, true)) {
                return false;
            }
            break;
        }
    }

    if (!freerdp_connect(m_freerdp)) {
        qWarning(KRDC) << "Unable to connect";
        emitErrorMessage();
        return false;
    }

    m_thread = std::thread(std::bind(&RdpSession::run, this));
    pthread_setname_np(m_thread.native_handle(), "rdp_session");

    return true;
}

void RdpSession::stop()
{
    freerdp_abort_connect(m_freerdp);
    if (m_thread.joinable()) {
        m_thread.join();
    }

    if (m_freerdp) {
        freerdp_context_free(m_freerdp);
        freerdp_free(m_freerdp);

        m_context = nullptr;
        m_freerdp = nullptr;
    }
}

const QImage *RdpSession::videoBuffer() const
{
    return &m_videoBuffer;
}

bool RdpSession::sendEvent(QEvent *event, QWidget *source)
{
    auto input = m_freerdp->context->input;

    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::KeyRelease: {
        auto keyEvent = static_cast<QKeyEvent *>(event);
        auto code = freerdp_keyboard_get_rdp_scancode_from_x11_keycode(keyEvent->nativeScanCode());
        freerdp_input_send_keyboard_event_ex(input, keyEvent->type() == QEvent::KeyPress, code);
        return true;
    }
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseMove: {
        auto mouseEvent = static_cast<QMouseEvent *>(event);
        auto position = mouseEvent->position();
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

bool RdpSession::onPreConnect()
{
    auto settings = m_freerdp->settings;
    if (!freerdp_settings_set_uint32(settings, FreeRDP_OsMajorType, OSMAJORTYPE_UNIX)) {
        return false;
    }
    if (!freerdp_settings_set_uint32(settings, FreeRDP_OsMajorType, OSMINORTYPE_UNSPECIFIED)) {
        return false;
    }

    if (PubSub_SubscribeChannelConnected(m_freerdp->context->pubSub, channelConnected) < 0) {
        return false;
    }
    if (PubSub_SubscribeChannelDisconnected(m_freerdp->context->pubSub, channelDisconnected) < 0) {
        return false;
    }

    if (!freerdp_client_load_addins(m_freerdp->context->channels, settings)) {
        return false;
    }

    return true;
}

bool RdpSession::onPostConnect()
{
    setState(State::Connected);

    auto settings = m_freerdp->settings;
    auto w = freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth);
    auto h = freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight);
    m_videoBuffer = QImage(w, h, QImage::Format_RGBX8888);

    if (!gdi_init_ex(m_freerdp, PIXEL_FORMAT_RGBX32, m_videoBuffer.bytesPerLine(), m_videoBuffer.bits(), nullptr)) {
        qCWarning(KRDC) << "Could not initialize GDI subsystem";
        return false;
    }

    auto gdi = reinterpret_cast<rdpContext *>(m_context)->gdi;
    if (!gdi || gdi->width < 0 || gdi->height < 0) {
        return false;
    }

    m_size = QSize(gdi->width, gdi->height);
    Q_EMIT sizeChanged();

    m_freerdp->update->EndPaint = endPaint;
    m_freerdp->update->DesktopResize = resizeDisplay;
    m_freerdp->update->PlaySound = playSound;

    auto keyboardLayout = freerdp_settings_get_uint32(settings, FreeRDP_KeyboardLayout);
    auto keyboardRemappingList = freerdp_settings_get_string(settings, FreeRDP_KeyboardRemappingList);
    freerdp_keyboard_init_ex(keyboardLayout, keyboardRemappingList);

    return true;
}

void RdpSession::onPostDisconnect()
{
    setState(State::Closed);
    gdi_free(m_freerdp);
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

bool RdpSession::onEndPaint()
{
    if (!m_context) {
        return false;
    }

    auto gdi = reinterpret_cast<rdpContext *>(m_context)->gdi;
    if (!gdi || !gdi->primary) {
        return false;
    }

    auto invalid = gdi->primary->hdc->hwnd->invalid;
    if (invalid->null) {
        return true;
    }

    auto rect = QRect{invalid->x, invalid->y, invalid->w, invalid->h};
    Q_EMIT rectangleUpdated(rect);

    return true;
}

bool RdpSession::onResizeDisplay()
{
    auto gdi = reinterpret_cast<rdpContext *>(m_context)->gdi;
    auto settings = m_freerdp->settings;
    auto w = freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth);
    auto h = freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight);
    m_videoBuffer = QImage(w, h, QImage::Format_RGBX8888);

    if (!gdi_resize_ex(gdi, w, h, m_videoBuffer.bytesPerLine(), PIXEL_FORMAT_RGBX32, m_videoBuffer.bits(), nullptr)) {
        qCWarning(KRDC) << "Failed resizing GDI subsystem";
        return false;
    }

    m_size = QSize(w, h);
    Q_EMIT sizeChanged();

    return true;
}

bool RdpSession::onPlaySound()
{
    QApplication::beep();
    return true;
}

void RdpSession::run()
{
    auto rdpC = reinterpret_cast<rdpContext *>(m_context);

    auto timer = CreateWaitableTimerA(nullptr, FALSE, "rdp-session-timer");
    if (!timer) {
        return;
    }

    LARGE_INTEGER due;
    due.QuadPart = 0;
    if (!SetWaitableTimer(timer, &due, 1, nullptr, nullptr, false)) {
        return;
    }

    setState(State::Running);

    HANDLE handles[MAXIMUM_WAIT_OBJECTS] = {};
    while (!freerdp_shall_disconnect(m_freerdp)) {
        handles[0] = timer;
        auto count = freerdp_get_event_handles(rdpC, &handles[1], ARRAYSIZE(handles) - 1);

        auto status = WaitForMultipleObjects(count, handles, FALSE, INFINITE);
        if (status == WAIT_FAILED) {
            emitErrorMessage();
            break;
        }

        if (freerdp_check_event_handles(rdpC) != TRUE) {
            emitErrorMessage();
            break;
        }
    }

    freerdp_disconnect(m_freerdp);
}

void RdpSession::emitErrorMessage()
{
    const unsigned int error = freerdp_get_last_error(m_freerdp->context);

    if (error == FREERDP_ERROR_CONNECT_CANCELLED) {
        return;
    }

    auto name = freerdp_get_last_error_name(error);
    auto description = freerdp_get_last_error_string(error);
    qCDebug(KRDC) << name << description;

    Q_EMIT errorMessage(error);
}

void RdpSession::initializeClipboard(RdpContext *rdpC, CliprdrClientContext *cliprdr)
{
    if (!rdpC || !cliprdr) {
        return;
    }
    m_clipboard = std::make_unique<RdpClipboard>(rdpC, cliprdr);
}

void RdpSession::destroyClipboard()
{
    if (m_clipboard) {
        m_clipboard.reset(nullptr);
    }
}

bool RdpSession::sendClipboard(const QMimeData *data)
{
    if (!m_clipboard) {
        return false;
    }

    return m_clipboard->sendClipboard(data);
}
