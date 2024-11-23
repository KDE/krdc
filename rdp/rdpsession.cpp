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
#include <freerdp/client/disp.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/event.h>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/gdi/gfx.h>
#include <freerdp/input.h>
#include <freerdp/locale/keyboard.h>
#include <freerdp/utils/signal.h>
#include <winpr/synch.h>

#include "rdpview.h"

#include "krdc_debug.h"

BOOL RdpSession::preConnect(freerdp *rdp)
{
    WINPR_ASSERT(rdp);
    auto ctx = rdp->context;
    WINPR_ASSERT(ctx);

    auto settings = ctx->settings;
    WINPR_ASSERT(settings);

    if (!freerdp_settings_set_uint32(settings, FreeRDP_OsMajorType, OSMAJORTYPE_UNIX)) {
        return false;
    }
    if (!freerdp_settings_set_uint32(settings, FreeRDP_OsMajorType, OSMINORTYPE_UNSPECIFIED)) {
        return false;
    }

    if (PubSub_SubscribeChannelConnected(ctx->pubSub, channelConnected) < 0) {
        return false;
    }
    if (PubSub_SubscribeChannelDisconnected(ctx->pubSub, channelDisconnected) < 0) {
        return false;
    }

#if FREERDP_VERSION_MAJOR == 2
    if (!freerdp_client_load_addins(ctx->channels, settings)) {
        return false;
    }
#endif
    return true;
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

    auto settings = ctx->settings;
    WINPR_ASSERT(settings);

    session->setState(State::Connected);

    auto &buffer = session->m_videoBuffer;
    buffer = QImage(freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth),
                    freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight),
                    QImage::Format_RGBX8888);

    if (!gdi_init_ex(rdp, PIXEL_FORMAT_RGBX32, buffer.bytesPerLine(), buffer.bits(), nullptr)) {
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
    ctx->update->PlaySound = playSound;

    auto keyboardLayout = freerdp_settings_get_uint32(settings, FreeRDP_KeyboardLayout);
    auto keyboardRemappingList = freerdp_settings_get_string(settings, FreeRDP_KeyboardRemappingList);
    freerdp_keyboard_init_ex(keyboardLayout, keyboardRemappingList);
    return true;
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

#if FREERDP_VERSION_MAJOR == 3
void RdpSession::postFinalDisconnect(freerdp *)
{
}
#endif

#if FREERDP_VERSION_MAJOR == 3
BOOL RdpSession::authenticateEx(freerdp *instance, char **username, char **password, char **domain, rdp_auth_reason reason)
{
    Q_UNUSED(reason);
#else
BOOL RdpSession::authenticate(freerdp *instance, char **username, char **password, char **domain)
{
#endif
    auto session = reinterpret_cast<RdpContext *>(instance->context)->session;
    // TODO: this needs to handle:
    // gateway
    // user
    // smartcard
    // AAD
    // needs new settings
    if (session->onAuthenticate(username, password, domain)) {
        return true;
    }

    return false;
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

    KMessageBox::error(nullptr, typeString + QStringLiteral(" ") + dataString, i18nc("@title:dialog", "Logon Error"));
    return 1;
}

BOOL RdpSession::presentGatewayMessage(freerdp *instance, UINT32 type, BOOL isDisplayMandatory, BOOL isConsentMandatory, size_t length, const WCHAR *message)
{
    Q_UNUSED(instance);
    Q_UNUSED(type);
    Q_UNUSED(isDisplayMandatory);
    Q_UNUSED(isConsentMandatory);
    Q_UNUSED(length);
    Q_UNUSED(message);
    // TODO: Implement
    // TODO: run on UI thread
    // TODO: Block, wait for result
    return false;
}

#if FREERDP_VERSION_MAJOR == 3
BOOL RdpSession::chooseSmartcard(freerdp *instance, SmartcardCertInfo **cert_list, DWORD count, DWORD *choice, BOOL gateway)
{
    Q_UNUSED(instance);
    Q_UNUSED(cert_list);
    Q_UNUSED(count);
    Q_UNUSED(choice);
    Q_UNUSED(gateway);
    // TODO: Implement
    // TODO: Move this to UI thread
    // TODO: Block for result as this might be just a informative message
    return false;
}

SSIZE_T RdpSession::retryDialog(freerdp *instance, const char *what, size_t current, void *userarg)
{
    Q_UNUSED(instance);
    Q_UNUSED(what);
    Q_UNUSED(current);
    Q_UNUSED(userarg);
    // TODO: Implement
    // TODO: Move this to UI thread
    // TODO: Block for result as this might be just a informative message
    return -1;
}
#endif

BOOL RdpSession::clientGlobalInit()
{
#if defined(_WIN32)
    WSADATA wsaData = {0};
    const DWORD wVersionRequested = MAKEWORD(1, 1);
    const int rc = WSAStartup(wVersionRequested, &wsaData);
    if (rc != 0) {
        WLog_ERR(SDL_TAG, "WSAStartup failed with %s [%d]", gai_strerrorA(rc), rc);
        return false;
    }
#endif
    if (freerdp_handle_signals() != 0)
        return false;

    return true;
}

void RdpSession::clientGlobalUninit()
{
#if defined(_WIN32)
    WSACleanup();
#endif
}

BOOL RdpSession::clientContextNew(freerdp *instance, rdpContext *context)
{
    if (!instance || !context)
        return false;

    instance->PreConnect = preConnect;
    instance->PostConnect = postConnect;
    instance->PostDisconnect = postDisconnect;
#if FREERDP_VERSION_MAJOR == 3
    instance->PostFinalDisconnect = postFinalDisconnect;
#endif
#if FREERDP_VERSION_MAJOR == 3
    instance->AuthenticateEx = authenticateEx;
#else
    instance->Authenticate = authenticate;
#endif
    instance->VerifyCertificateEx = verifyCertificateEx;
    instance->VerifyChangedCertificateEx = verifyChangedCertificateEx;
    instance->LogonErrorInfo = logonErrorInfo;
    instance->PresentGatewayMessage = presentGatewayMessage;
#if FREERDP_VERSION_MAJOR == 3
    instance->ChooseSmartcard = chooseSmartcard;
    instance->RetryDialog = retryDialog;
#endif
    // TODO
    // instance->GetAccessToken = RdpAADView::getAccessToken;

    return true;
}

void RdpSession::clientContextFree(freerdp *instance, rdpContext *context)
{
    Q_UNUSED(instance);
    auto ctx = reinterpret_cast<RdpContext *>(context);
    if (!ctx)
        return;
    ctx->session = nullptr;
}

int RdpSession::clientContextStart(rdpContext *context)
{
    auto kcontext = reinterpret_cast<RdpContext *>(context);
    WINPR_ASSERT(kcontext);

    auto session = kcontext->session;
    WINPR_ASSERT(session);

    auto settings = context->settings;
    WINPR_ASSERT(settings);

    session->setState(State::Starting);

    qCInfo(KRDC) << "Starting RDP session";

    auto preferences = session->m_preferences;

    if (!freerdp_settings_set_string(settings, FreeRDP_ServerHostname, session->m_host.toUtf8().data())) {
        return -1;
    }
    if (!freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, session->m_port)) {
        return -1;
    }

    if (session->m_size.width() > 0 && session->m_size.height() > 0) {
        if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, session->m_size.width())) {
            return -1;
        }
        if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, session->m_size.height())) {
            return -1;
        }
    }

    switch (preferences->colorDepth()) {
    case RdpHostPreferences::ColorDepth::Auto:
    case RdpHostPreferences::ColorDepth::Depth32:
        if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32)) {
            return -1;
        }
        break;
    case RdpHostPreferences::ColorDepth::Depth24:
        if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 24)) {
            return -1;
        }
        break;
    case RdpHostPreferences::ColorDepth::Depth16:
        if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 16)) {
            return -1;
        }
        break;
    case RdpHostPreferences::ColorDepth::Depth8:
        if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 8)) {
            return -1;
        }
    }

    switch (preferences->acceleration()) {
    case RdpHostPreferences::Acceleration::ForceGraphicsPipeline:
        if (!freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, true)) {
            return -1;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, true)) {
            return -1;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444v2, true)) {
            return -1;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxH264, true)) {
            return -1;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, false)) {
            return -1;
        }
        if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32)) {
            return -1;
        }
        break;
    case RdpHostPreferences::Acceleration::ForceRemoteFx:
        if (!freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, false)) {
            return -1;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, false)) {
            return -1;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444v2, false)) {
            return -1;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxH264, false)) {
            return -1;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, true)) {
            return -1;
        }
        if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32)) {
            return -1;
        }
        break;
    case RdpHostPreferences::Acceleration::Disabled:
        if (!freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, false)) {
            return -1;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, false)) {
            return -1;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444v2, false)) {
            return -1;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxH264, false)) {
            return -1;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, false)) {
            return -1;
        }
        break;
    case RdpHostPreferences::Acceleration::Auto:
        if (!freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, true)) {
            return -1;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, true)) {
            return -1;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444v2, true)) {
            return -1;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_GfxH264, true)) {
            return -1;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, true)) {
            return -1;
        }
        if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32)) {
            return -1;
        }
        break;
    }

    if (!freerdp_settings_set_bool(settings, FreeRDP_FastPathOutput, true)) {
        return -1;
    }
    if (!freerdp_settings_set_bool(settings, FreeRDP_FastPathInput, true)) {
        return -1;
    }
    if (!freerdp_settings_set_bool(settings, FreeRDP_FrameMarkerCommandEnabled, true)) {
        return -1;
    }
    if (!freerdp_settings_set_bool(settings, FreeRDP_SupportDynamicChannels, true)) {
        return -1;
    }

    switch (preferences->sound()) {
    case RdpHostPreferences::Sound::Local:
        if (!freerdp_settings_set_bool(settings, FreeRDP_AudioPlayback, true)) {
            return -1;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_AudioCapture, true)) {
            return -1;
        }
        break;
    case RdpHostPreferences::Sound::Remote:
        if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteConsoleAudio, true)) {
            return -1;
        }
        break;
    case RdpHostPreferences::Sound::Disabled:
        if (!freerdp_settings_set_bool(settings, FreeRDP_AudioPlayback, false)) {
            return -1;
        }
        if (!freerdp_settings_set_bool(settings, FreeRDP_AudioCapture, false)) {
            return -1;
        }
        break;
    }

    if (!preferences->shareMedia().isEmpty()) {
        QByteArray name = "drive";
        QByteArray value = preferences->shareMedia().toUtf8();
#if FREERDP_VERSION_MAJOR == 3
        const char *params[2] = {name.data(), value.data()};
#else
        char *params[2] = {name.data(), value.data()};
#endif
        freerdp_client_add_device_channel(settings, 2, params);
    }

    if (!preferences->smartcardName().isEmpty()) {
        QByteArray name = "smartcard";
        QByteArray value = preferences->smartcardName().toLocal8Bit();

#if FREERDP_VERSION_MAJOR == 3
        const char *params[2] = {name.data(), value.data()};
#else
        char *params[2] = {name.data(), value.data()};
#endif
        freerdp_client_add_device_channel(settings, 2, params);
    }

    if (!freerdp_settings_set_uint32(settings, FreeRDP_KeyboardLayout, preferences->rdpKeyboardLayout())) {
        return -1;
    }

    switch (preferences->tlsSecLevel()) {
    case RdpHostPreferences::TlsSecLevel::Bit80:
        if (!freerdp_settings_set_uint32(settings, FreeRDP_TlsSecLevel, 1)) {
            return -1;
        }
        break;
    case RdpHostPreferences::TlsSecLevel::Bit112:
        if (!freerdp_settings_set_uint32(settings, FreeRDP_TlsSecLevel, 2)) {
            return -1;
        }
        break;
    case RdpHostPreferences::TlsSecLevel::Bit128:
        if (!freerdp_settings_set_uint32(settings, FreeRDP_TlsSecLevel, 3)) {
            return -1;
        }
        break;
    case RdpHostPreferences::TlsSecLevel::Bit192:
        if (!freerdp_settings_set_uint32(settings, FreeRDP_TlsSecLevel, 4)) {
            return -1;
        }
        break;
    case RdpHostPreferences::TlsSecLevel::Bit256:
        if (!freerdp_settings_set_uint32(settings, FreeRDP_TlsSecLevel, 5)) {
            return -1;
        }
        break;
    case RdpHostPreferences::TlsSecLevel::Any:
    default:
        if (!freerdp_settings_set_uint32(settings, FreeRDP_TlsSecLevel, 0)) {
            return -1;
        }
        break;
    }

    if (!freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, preferences->securityNLA())) {
        return -1;
    }
    if (!freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, preferences->securityTLS())) {
        return -1;
    }
    if (!freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, preferences->securityRDP())) {
        return -1;
    }
    if (!freerdp_settings_set_bool(settings, FreeRDP_ExtSecurity, preferences->securityEXT())) {
        return -1;
    }

    const auto proxyHostAddress = QUrl(preferences->proxyHost());
    if (!proxyHostAddress.isEmpty()) {
        int defaultPort = 8080;
        switch (preferences->proxyProtocol()) {
        case RdpHostPreferences::ProxyProtocol::HTTP:
            if (!freerdp_settings_set_uint32(settings, FreeRDP_ProxyType, PROXY_TYPE_HTTP)) {
                return -1;
            }
            break;
        case RdpHostPreferences::ProxyProtocol::SOCKS:
            if (!freerdp_settings_set_uint32(settings, FreeRDP_ProxyType, PROXY_TYPE_SOCKS)) {
                return -1;
            }
            defaultPort = 1080;
            break;
        default:
            if (!freerdp_settings_set_uint32(settings, FreeRDP_ProxyType, PROXY_TYPE_NONE)) {
                return -1;
            }
            break;
        }

        if (!freerdp_settings_set_string(settings, FreeRDP_ProxyHostname, proxyHostAddress.host().toUtf8().data())) {
            return -1;
        }
        if (!freerdp_settings_set_string(settings, FreeRDP_ProxyUsername, preferences->proxyUsername().toUtf8().data())) {
            return -1;
        }
        if (!freerdp_settings_set_string(settings, FreeRDP_ProxyPassword, preferences->proxyPassword().toUtf8().data())) {
            return -1;
        }
        if (!freerdp_settings_set_uint16(settings, FreeRDP_ProxyPort, proxyHostAddress.port(defaultPort))) {
            return -1;
        }
    }

    const auto gatewayServerAddress = QUrl(preferences->proxyHost());
    if (!gatewayServerAddress.isEmpty()) {
        if (!freerdp_settings_set_string(settings, FreeRDP_GatewayHostname, gatewayServerAddress.host().toUtf8().data())) {
            return -1;
        }
        if (!freerdp_settings_set_uint32(settings, FreeRDP_GatewayPort, gatewayServerAddress.port(3389))) {
            return -1;
        }
        if (!freerdp_settings_set_string(settings, FreeRDP_GatewayUsername, preferences->gatewayUsername().toUtf8().data())) {
            return -1;
        }
        if (!freerdp_settings_set_string(settings, FreeRDP_GatewayPassword, preferences->gatewayPassword().toUtf8().data())) {
            return -1;
        }
        if (!freerdp_settings_set_string(settings, FreeRDP_GatewayDomain, preferences->gatewayDomain().toUtf8().data())) {
            return -1;
        }

        switch (preferences->gatewayTransportType()) {
        case RdpHostPreferences::GatewayTransportType::RPC:
            if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, false)) {
                return -1;
            }
            if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, true)) {
                return -1;
            }
            break;
        case RdpHostPreferences::GatewayTransportType::HTTP:
            if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, true)) {
                return -1;
            }
            if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, false)) {
                return -1;
            }
            break;
        case RdpHostPreferences::GatewayTransportType::Auto:
        default:
            if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, true)) {
                return -1;
            }
            if (!freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, true)) {
                return -1;
            }
            break;
        }
    }

    if (!freerdp_connect(context->instance)) {
        qWarning(KRDC) << "Unable to connect";
        session->emitErrorMessage();
        return -1;
    }

    session->m_thread = std::thread(std::bind(&RdpSession::run, session));
    pthread_setname_np(session->m_thread.native_handle(), "rdp_session");

    return 0;
}

int RdpSession::clientContextStop(rdpContext *context)
{
    auto kcontext = reinterpret_cast<RdpContext *>(context);
    WINPR_ASSERT(kcontext);

#if FREERDP_VERSION_MAJOR == 3
    /* We do not want to use freerdp_abort_connect_context here.
     * It would change the exit code and we do not want that. */
    HANDLE event = freerdp_abort_event(context);
    if (!SetEvent(event))
        return -1;
#else
    freerdp_abort_connect(context->instance);
#endif

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
    entry.GlobalInit = clientGlobalInit;
    entry.GlobalUninit = clientGlobalUninit;
    entry.ContextSize = sizeof(RdpContext);
    entry.ClientNew = clientContextNew;
    entry.ClientFree = clientContextFree;
    entry.ClientStart = clientContextStart;
    entry.ClientStop = clientContextStop;

    return entry;
}

BOOL RdpSession::endPaint(rdpContext *context)
{
    WINPR_ASSERT(context);
    auto session = reinterpret_cast<RdpContext *>(context)->session;
    WINPR_ASSERT(session);

    auto gdi = context->gdi;
    if (!gdi || !gdi->primary) {
        return false;
    }

    auto invalid = gdi->primary->hdc->hwnd->invalid;
    if (invalid->null) {
        return true;
    }

    auto rect = QRect{invalid->x, invalid->y, invalid->w, invalid->h};
    Q_EMIT session->rectangleUpdated(rect);
    return true;
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
    buffer = QImage(freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth),
                    freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight),
                    QImage::Format_RGBX8888);

    if (!gdi_resize_ex(gdi, buffer.width(), buffer.height(), buffer.bytesPerLine(), PIXEL_FORMAT_RGBX32, buffer.bits(), nullptr)) {
        qCWarning(KRDC) << "Failed resizing GDI subsystem";
        return false;
    }

    session->m_size = buffer.size();
    Q_EMIT session->sizeChanged();

    return true;
}

BOOL RdpSession::playSound(rdpContext *context, const PLAY_SOUND_UPDATE *play_sound)
{
    Q_UNUSED(context);
    Q_UNUSED(play_sound);
    QApplication::beep();
    return true;
}

#if FREERDP_VERSION_MAJOR == 3
void RdpSession::channelConnected(void *context, const ChannelConnectedEventArgs *e)
#else
void RdpSession::channelConnected(void *context, ChannelConnectedEventArgs *e)
#endif
{
    if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0) {
        CliprdrClientContext *cliprdr = (CliprdrClientContext *)e->pInterface;

        auto session = reinterpret_cast<RdpContext *>(context)->session;
        WINPR_ASSERT(session);

        auto krdp = reinterpret_cast<RdpContext *>(context);
        WINPR_ASSERT(krdp);

        session->initializeClipboard(krdp, cliprdr);
    } else if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0) {
        auto disp = reinterpret_cast<DispClientContext *>(e->pInterface);
        WINPR_ASSERT(disp);
        Q_UNUSED(disp);
        // TODO: Implement display channel
        // TODO: Should be fairly easy as you only need to provide monitor(=fullscreen or multimonitor) or window (=single monitor on RDP side) layout
        // (orientation, resolution, dpi, ...) on size / dpi / ... change. word of advice: do not send more than one update per second (some windows servers
        // crash the session otherwise)
#if FREERDP_VERSION_MAJOR == 3
    } else {
        freerdp_client_OnChannelConnectedEventHandler(context, e);
#endif
    }
}

#if FREERDP_VERSION_MAJOR == 3
void RdpSession::channelDisconnected(void *context, const ChannelDisconnectedEventArgs *e)
#else
void RdpSession::channelDisconnected(void *context, ChannelDisconnectedEventArgs *e)
#endif
{
    if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0) {
        auto session = reinterpret_cast<RdpContext *>(context)->session;
        WINPR_ASSERT(session);

        session->destroyClipboard();
    } else if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0) {
        auto disp = reinterpret_cast<DispClientContext *>(e->pInterface);
        WINPR_ASSERT(disp);
        Q_UNUSED(disp);
        // TODO: Implement display channel
#if FREERDP_VERSION_MAJOR == 3
    } else {
        freerdp_client_OnChannelDisconnectedEventHandler(context, e);
#endif
    }
}

QString Certificate::toString() const
{
    return i18nc("@label", "Host: %1:%2\nCommon Name: %3\nSubject: %4\nIssuer: %5\nFingerprint: %6\n", host, port, commonName, subject, issuer, fingerprint);
}

RdpSession::RdpSession(RdpView *view)
    : QObject(nullptr)
    , m_view(view)
    , m_firstPasswordTry(true)
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
    if (freerdp_client_start(m_context.rdp) == CHANNEL_RC_OK) {
        return true;
    }

    qWarning(KRDC) << "freerdp_client_start() failed";
    return false;
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
        auto code = freerdp_keyboard_get_rdp_scancode_from_x11_keycode(keyEvent->nativeScanCode());
#if FREERDP_VERSION_MAJOR == 3
        freerdp_input_send_keyboard_event_ex(input, keyEvent->type() == QEvent::KeyPress, keyEvent->isAutoRepeat(), code);
#else
        freerdp_input_send_keyboard_event_ex(input, keyEvent->type() == QEvent::KeyPress, code);
#endif
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
        } else if (delta.x() != 0) {
            value = std::clamp(std::abs(delta.x()), 0, 0xFF);
            flags |= PTR_FLAGS_HWHEEL;
            if (wheelEvent->angleDelta().x() < 0) {
                flags |= PTR_FLAGS_WHEEL_NEGATIVE;
                flags = (flags & 0xFF00) | (0x100 - value);
            } else {
                flags |= value;
            }
        } else {
            break;
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

    if (m_firstPasswordTry && m_user.size()) {
        *username = qstrdup(m_user.toUtf8().data());
        if (m_password.size()) {
            *password = qstrdup(m_password.toUtf8().data());
            m_firstPasswordTry = false;
            return true;
        }
    }

    std::unique_ptr<KPasswordDialog> dialog;
    dialog = std::make_unique<KPasswordDialog>(nullptr, KPasswordDialog::ShowUsernameLine | KPasswordDialog::ShowKeepPassword);
    dialog->setPrompt(i18nc("@label", "Access to this system requires a username and password."));
    dialog->setUsername(m_user);
    dialog->setPassword(m_password);

    if (!dialog->exec()) {
        return false;
    }

    m_user = dialog->username();
    m_password = dialog->password();

    *username = qstrdup(m_user.toUtf8().data());
    *password = qstrdup(m_password.toUtf8().data());

    if (dialog->keepPassword()) {
        m_view->savePassword(m_password);
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

void RdpSession::run()
{
    auto instance = m_context.rdp->instance;

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
#if FREERDP_VERSION_MAJOR == 3
    while (!freerdp_shall_disconnect_context(m_context.rdp)) {
#else
    while (!freerdp_shall_disconnect(instance)) {
#endif
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

void RdpSession::initializeClipboard(RdpContext *krdp, CliprdrClientContext *cliprdr)
{
    if (!krdp || !cliprdr) {
        return;
    }
    m_clipboard = std::make_unique<RdpClipboard>(krdp, cliprdr);
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
