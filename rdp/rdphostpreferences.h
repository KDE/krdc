/*
SPDX-FileCopyrightText: 2007-2012 Urs Wolfer <uwolfer@kde.org>
SPDX-FileCopyrightText: 2012 AceLan Kao <acelan@acelan.idv.tw>

SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef RDPHOSTPREFERENCES_H
#define RDPHOSTPREFERENCES_H

#include "hostpreferences.h"
#include "ui_rdppreferences.h"

#include <QFileDialog>

class RdpHostPreferences : public HostPreferences
{
    Q_OBJECT

public:
    enum class Resolution {
        Small,
        Medium,
        Large,
        MatchWindow,
        MatchScreen,
        Custom,
    };

    enum class Sound {
        Local,
        Remote,
        Disabled,
    };

    enum class Acceleration {
        Auto,
        ForceGraphicsPipeline,
        ForceRemoteFx,
        Disabled,
    };

    enum class ColorDepth {
        Auto,
        Depth32,
        Depth24,
        Depth16,
        Depth8,
    };

    enum class TlsSecLevel {
        Any,
        Bit80,
        Bit112,
        Bit128,
        Bit192,
        Bit256,
    };

    enum class ProxyProtocol {
        HTTP,
        SOCKS
    };

    enum class GatewayTransportType {
        Auto,
        HTTP,
        RPC
    };

    explicit RdpHostPreferences(KConfigGroup configGroup, QObject *parent = nullptr);
    ~RdpHostPreferences() override;

    bool scaleToSize() const;
    void setScaleToSize(bool scale);

    Resolution resolution() const;
    void setResolution(Resolution resolution);

    ColorDepth colorDepth() const;
    void setColorDepth(ColorDepth colorDepth);

    QString keyboardLayout() const;
    int rdpKeyboardLayout() const;
    void setKeyboardLayout(const QString &keyboardLayout);

    Sound sound() const;
    void setSound(Sound sound);

    Acceleration acceleration() const;
    void setAcceleration(Acceleration acceleration);

    void setShareMedia(const QString &shareMedia);
    QString shareMedia() const;

    TlsSecLevel tlsSecLevel() const;
    void setTlsSecLevel(TlsSecLevel tlsSecLevel);

    // Proxy settings
    ProxyProtocol proxyProtocol() const;
    void setProxyProtocol(ProxyProtocol proxyProtocol);

    QString proxyUsername() const;
    void setProxyUsername(const QString &proxyUsername);

    QString proxyPassword() const;
    void setProxyPassword(const QString &proxyPassword);

    QString proxyHost() const;
    void setProxyHost(const QString &proxyHost);

    // Gateway settings
    GatewayTransportType gatewayTransportType() const;
    void setGatewayTransportType(GatewayTransportType gatewayTransportType);

    QString gatewayUsername() const;
    void setGatewayUsername(const QString &gatewayUsername);

    QString gatewayPassword() const;
    void setGatewayPassword(const QString &gatewayPassword);

    QString gatewayServer() const;
    void setGatewayServer(const QString &gatewayServer);

    QString gatewayDomain() const;
    void setGatewayDomain(const QString &gatewayDomain);

    QString smartcardName() const;
    void setSmartcardName(const QString &smartcardName);

    bool securityNLA() const;
    void setSecurityNLA(bool enabled);

    bool securityTLS() const;
    void setSecurityTLS(bool enabled);

    bool securityRDP() const;
    void setSecurityRDP(bool enabled);

    bool securityEXT() const;
    void setSecurityEXT(bool enabled);

protected:
    QWidget *createProtocolSpecificConfigPage() override;
    void acceptConfig() override;

private:
    void updateWidthHeight(Resolution resolution);
    void updateColorDepth(Acceleration acceleration);

    Ui::RdpPreferences rdpUi;
};

#endif
