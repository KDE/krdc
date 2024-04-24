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

#include <winpr/clipboard.h>

#include <freerdp/client/cliprdr.h>
#include <freerdp/freerdp.h>

class RdpClipboard;
class RdpSession;
class RdpView;
class RdpHostPreferences;
class QMimeData;

struct RdpContext {
    rdpContext _c;

    RdpSession *session = nullptr;
    wClipboard *clipboard;
    UINT32 numServerFormats;
    UINT32 requestedFormatId;
    CLIPRDR_FORMAT *serverFormats;
    CliprdrClientContext *cliprdr;
    UINT32 clipboardCapabilities;
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

    void initializeClipboard(RdpContext *rdpC, CliprdrClientContext *cliprdr);
    void destroyClipboard();
    bool sendClipboard(const QMimeData *data);

    const QImage *videoBuffer() const;

    Q_SIGNAL void rectangleUpdated(const QRect &rectangle);

    Q_SIGNAL void errorMessage(unsigned int error);

    RdpView *rdpView()
    {
        return m_view;
    };

private:
    friend BOOL preConnect(freerdp *);
    friend BOOL postConnect(freerdp *);
    friend void postDisconnect(freerdp *);
    friend BOOL authenticate(freerdp *, char **, char **, char **);
    friend DWORD verifyCertificate(freerdp *, const char *, UINT16 port, const char *, const char *, const char *, const char *, DWORD);
    friend DWORD verifyChangedCertificate(freerdp *,
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
    friend BOOL endPaint(rdpContext *);
    friend BOOL resizeDisplay(rdpContext *);
    friend BOOL playSound(rdpContext *, const PLAY_SOUND_UPDATE *);

    void setState(State newState);

    bool onPreConnect();
    bool onPostConnect();
    void onPostDisconnect();

    bool onAuthenticate(char **username, char **password, char **domain);
    CertificateResult onVerifyCertificate(const Certificate &certificate);
    CertificateResult onVerifyChangedCertificate(const Certificate &oldCertificate, const Certificate &newCertificate);

    bool onEndPaint();
    bool onResizeDisplay();
    bool onPlaySound();

    void run();

    void emitErrorMessage();

    RdpView *m_view;

    freerdp *m_freerdp = nullptr;
    RdpContext *m_context = nullptr;
    RdpClipboard *m_clipboard = nullptr;

    State m_state = State::Initial;

    QString m_user;
    QString m_domain;
    QString m_password;
    QString m_host;
    int m_port = -1;
    QSize m_size;

    std::thread m_thread;

    QImage m_videoBuffer;

    RdpHostPreferences *m_preferences;
};
