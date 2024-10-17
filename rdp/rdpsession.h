/*
 * SPDX-FileCopyrightText: 2023 Arjen Hiemstra <ahiemstra@heimr.nl>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <atomic>
#include <memory>
#include <thread>

#include <QImage>
#include <QObject>
#include <QSize>

#include <freerdp/client/cliprdr.h>
#include <freerdp/freerdp.h>
#include <freerdp/version.h>

class RdpClipboard;
class RdpSession;
class RdpView;
class RdpHostPreferences;
class QMimeData;

struct RdpContext {
    rdpClientContext _c;

    RdpSession *session = nullptr;
    RdpClipboard *clipboard = nullptr;
};

struct Certificate {
    QString toString() const;

    QString host;
    qint16 port;
    QString commonName;
    QString subject;
    QString issuer;
    QString fingerprint;
    int flags;
};

class RdpSession : public QObject
{
    Q_OBJECT

public:
    /**
     * Session state.
     */
    enum class State {
        Initial,
        Starting,
        Connected,
        Running,
        Closed,
    };

    enum class CertificateResult {
        DoNotAccept,
        AcceptTemporarily,
        AcceptPermanently,
    };

    RdpSession(RdpView *view);
    ~RdpSession() override;

    /**
     * The current session state.
     */
    State state() const;
    Q_SIGNAL void stateChanged();

    QString host() const;
    void setHost(const QString &newHost);

    QString user() const;
    void setUser(const QString &newUser);

    QString domain() const;
    void setDomain(const QString &newDomain);

    QString password() const;
    void setPassword(const QString &newPassword);

    RdpHostPreferences *preferences() const;
    void setHostPreferences(RdpHostPreferences *preferences);

    QSize size() const;
    void setSize(QSize size);
    Q_SIGNAL void sizeChanged();

    int port() const;
    void setPort(int port);

    bool start();
    void stop();

    bool sendEvent(QEvent *event, QWidget *source);

    void initializeClipboard(RdpContext *krdp, CliprdrClientContext *cliprdr);
    void destroyClipboard();
    bool sendClipboard(const QMimeData *data);

    const QImage *videoBuffer() const;

    Q_SIGNAL void rectangleUpdated(const QRect &rectangle);

    Q_SIGNAL void errorMessage(unsigned int error);

    RdpView *rdpView()
    {
        return m_view;
    };

    static BOOL preConnect(freerdp *);
    static BOOL postConnect(freerdp *);
    static void postDisconnect(freerdp *);
#if FREERDP_VERSION_MAJOR == 3
    static void postFinalDisconnect(freerdp *);
#endif

#if FREERDP_VERSION_MAJOR == 3
    static BOOL authenticateEx(freerdp *instance, char **username, char **password, char **domain, rdp_auth_reason reason);
#else
    static BOOL authenticate(freerdp *instance, char **username, char **password, char **domain);
#endif
    static DWORD verifyCertificateEx(freerdp *, const char *, UINT16 port, const char *, const char *, const char *, const char *, DWORD);
    static DWORD verifyChangedCertificateEx(freerdp *,
                                            const char *,
                                            UINT16,
                                            const char *,
                                            const char *,
                                            const char *,
                                            const char *,
                                            const char *,
                                            const char *,
                                            const char *,
                                            DWORD);
    static BOOL endPaint(rdpContext *);
    static BOOL resizeDisplay(rdpContext *);
    static BOOL playSound(rdpContext *, const PLAY_SOUND_UPDATE *);

#if FREERDP_VERSION_MAJOR == 3
    static void channelConnected(void *context, const ChannelConnectedEventArgs *e);
    static void channelDisconnected(void *context, const ChannelDisconnectedEventArgs *e);
#else
    static void channelConnected(void *context, ChannelConnectedEventArgs *e);
    static void channelDisconnected(void *context, ChannelDisconnectedEventArgs *e);
#endif

    static int logonErrorInfo(freerdp *rdp, UINT32 data, UINT32 type);
    static BOOL presentGatewayMessage(freerdp *instance, UINT32 type, BOOL isDisplayMandatory, BOOL isConsentMandatory, size_t length, const WCHAR *message);
#if FREERDP_VERSION_MAJOR == 3
    static BOOL chooseSmartcard(freerdp *instance, SmartcardCertInfo **cert_list, DWORD count, DWORD *choice, BOOL gateway);
    static SSIZE_T retryDialog(freerdp *instance, const char *what, size_t current, void *userarg);
#endif

    static BOOL clientGlobalInit(void);
    static void clientGlobalUninit(void);

    static BOOL clientContextNew(freerdp *instance, rdpContext *context);
    static void clientContextFree(freerdp *instance, rdpContext *context);

    static int clientContextStart(rdpContext *context);
    static int clientContextStop(rdpContext *context);

    static RDP_CLIENT_ENTRY_POINTS RdpClientEntry();

private:
    void setState(State newState);

    bool onAuthenticate(char **username, char **password, char **domain);
    CertificateResult onVerifyCertificate(const Certificate &certificate);
    CertificateResult onVerifyChangedCertificate(const Certificate &oldCertificate, const Certificate &newCertificate);

    void run();

    void emitErrorMessage();

    RdpView *m_view;

    union {
        // krdc's context
        RdpContext *krdp;
        // freerdp/s context
        rdpContext *rdp;
    } m_context;

    std::unique_ptr<RdpClipboard> m_clipboard;

    State m_state = State::Initial;

    QString m_user;
    QString m_domain;
    QString m_password;
    QString m_host;
    int m_port = -1;
    QSize m_size;
    bool m_firstPasswordTry;

    std::thread m_thread;

    QImage m_videoBuffer;

    RdpHostPreferences *m_preferences;
};
