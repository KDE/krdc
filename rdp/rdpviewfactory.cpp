/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rdpviewfactory.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>
#include <QUrlQuery>

#include <KMessageBox>
#include <KPluginFactory>

#include <utility>

K_PLUGIN_CLASS_WITH_JSON(RdpViewFactory, "krdc_rdp.json")

static constexpr int DefaultGatewayPort = 443;
// The largest size the host preferences dialog accepts.
static constexpr int MaxDesktopSize = 9999;
static constexpr int MaxPort = 65535;
// Far larger than any real .rdp file, which is a few kilobytes even when signed.
static constexpr qint64 MaxRdpFileSize = 1024 * 1024;

static constexpr int ScreenModeWindowed = 1;
static constexpr int ScreenModeFullScreen = 2;
// Value of "gatewayusagemethod". KRDC has no equivalent of the other methods, such as using the
// gateway only when a direct connection fails.
static constexpr int GatewayAlways = 1;

// Reads the "key:type:value" lines of an .rdp file. Keys are in lower case, as their case varies.
static QHash<QString, QString> readRdpFile(const QString &filePath)
{
    QHash<QString, QString> entries;
    const QFileInfo fileInfo(filePath);
    if (!filePath.endsWith(QStringLiteral(".rdp"), Qt::CaseInsensitive) || !fileInfo.isFile() || fileInfo.size() > MaxRdpFileSize) {
        return entries;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return entries;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        const int typeStart = line.indexOf(QLatin1Char(':'));
        const int valueStart = line.indexOf(QLatin1Char(':'), typeStart + 1);
        if (typeStart < 0 || valueStart < 0) {
            continue;
        }
        entries.insert(line.left(typeStart).trimmed().toLower(), line.mid(valueStart + 1));
    }
    return entries;
}

static int readInt(const QHash<QString, QString> &entries, const QString &key, int fallback)
{
    bool ok;
    const int value = entries.value(key).toInt(&ok);
    return ok ? value : fallback;
}

// Splits host, host:port, [ipv6], [ipv6]:port or a bare IPv6 address. The port is -1 if there is
// none. Returns false if the address is malformed.
static bool parseAddress(const QString &text, QString *host, int *port)
{
    const QString address = text.trimmed();
    QString portText;
    *port = -1;
    if (address.startsWith(QLatin1Char('['))) {
        const int hostEnd = address.indexOf(QLatin1Char(']'));
        if (hostEnd < 0) {
            return false;
        }
        *host = address.mid(1, hostEnd - 1);
        const QString rest = address.mid(hostEnd + 1);
        if (rest.isEmpty()) {
            return true;
        }
        if (!rest.startsWith(QLatin1Char(':'))) {
            return false;
        }
        portText = rest.mid(1);
    } else if (address.count(QLatin1Char(':')) == 1) {
        *host = address.section(QLatin1Char(':'), 0, 0);
        portText = address.section(QLatin1Char(':'), 1);
    } else {
        *host = address;
        return true;
    }

    bool ok;
    *port = portText.toInt(&ok);
    return ok && *port > 0 && *port <= MaxPort;
}

RdpViewFactory::RdpViewFactory(QObject *parent, const QVariantList &args)
    : RemoteViewFactory(parent)
{
    Q_UNUSED(args);

    KLocalizedString::setApplicationDomain("krdc");

    m_connectToolTipString = i18n("Connect to a Windows Remote Desktop (RDP)");
}

RdpViewFactory::~RdpViewFactory()
{
}

bool RdpViewFactory::supportsUrl(const QUrl &url) const
{
    return (url.scheme().compare(QStringLiteral("rdp"), Qt::CaseInsensitive) == 0);
}

RemoteViewFactory::ConnectionFile RdpViewFactory::loadConnectionFile(const QUrl &url) const
{
    QHash<QString, QString> entries = readRdpFile(url.toLocalFile());

    QString host;
    int port;
    if (!parseAddress(entries.value(QStringLiteral("full address")), &host, &port) || host.isEmpty()) {
        return {};
    }

    QUrl loadedUrl;
    loadedUrl.setScheme(QStringLiteral("rdp"));
    loadedUrl.setHost(host);
    if (loadedUrl.host().isEmpty()) {
        return {};
    }
    if (port < 0 && entries.contains(QStringLiteral("server port"))) {
        port = readInt(entries, QStringLiteral("server port"), -1);
        if (port <= 0 || port > MaxPort) {
            return {};
        }
    }
    // Host settings are stored by URL, so the default port must not make this a different host.
    if (port > 0 && port != TCP_PORT_RDP) {
        loadedUrl.setPort(port);
    }
    const QString userName = entries.value(QStringLiteral("username"));
    if (!userName.isEmpty()) {
        loadedUrl.setUserName(userName, QUrl::DecodedMode);
    }
    // The password stays in the URL, out of the settings.
    const QString password = entries.take(QStringLiteral("password"));
    if (!password.isEmpty()) {
        loadedUrl.setPassword(password, QUrl::DecodedMode);
    }

    const QString domain = entries.value(QStringLiteral("domain")).trimmed();
    if (!domain.isEmpty()) {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("domain"), domain);
        loadedUrl.setQuery(query);
    }

    return {loadedUrl, std::move(entries)};
}

bool RdpViewFactory::importFileSettings(const QHash<QString, QString> &entries, KConfigGroup configGroup, QWidget *parent) const
{
    // Any file can name any host, so a file must not change a host the user already has settings for.
    if (configGroup.exists()) {
        return true;
    }

    QString gatewayAddress;
    if (readInt(entries, QStringLiteral("gatewayusagemethod"), 0) == GatewayAlways) {
        const QString gatewayHostname = entries.value(QStringLiteral("gatewayhostname"));
        QString gatewayHost;
        int gatewayPort;
        if (parseAddress(gatewayHostname, &gatewayHost, &gatewayPort) && !gatewayHost.isEmpty()) {
            QUrl gatewayUrl;
            gatewayUrl.setHost(gatewayHost);
            gatewayUrl.setPort(gatewayPort < 0 ? DefaultGatewayPort : gatewayPort);
            if (!gatewayUrl.host().isEmpty()) {
                gatewayAddress = gatewayUrl.authority();
            }
        }
        // Connecting without the gateway would send the login details somewhere the file did not ask for.
        if (gatewayAddress.isEmpty()) {
            KMessageBox::error(parent,
                               i18nc("@info %1 is a server address", "This file requires a gateway, but its address “%1” is not valid.", gatewayHostname),
                               i18nc("@title:window", "Invalid Gateway"));
            return false;
        }
    }

    // A gateway carries the connection and may receive the login details, so the user has to accept one.
    if (!gatewayAddress.isEmpty()
        && KMessageBox::warningContinueCancel(parent,
                                              i18nc("@info %1 is a server address",
                                                    "This file connects through the gateway “%1”, which could capture your login details. Continue only if you "
                                                    "trust this file.",
                                                    gatewayAddress),
                                              i18nc("@title:window", "Use Gateway from File?"))
            != KMessageBox::Continue) {
        return false;
    }

    RdpHostPreferences preferences(configGroup);

    // Sharing local folders, smart cards or other devices, and the security settings, are left out on
    // purpose: a file sent by an attacker could use them to expose this computer to the server it names.
    if (!gatewayAddress.isEmpty()) {
        preferences.setUseGateway(true);
        preferences.setGatewayServer(gatewayAddress);
        // Empty for this host, so that this gateway does not get the credentials of the default one.
        preferences.setGatewayUsername(QString());
        preferences.setGatewayPassword(QString());
        preferences.setGatewayDomain(QString());
    }

    const int screenMode = readInt(entries, QStringLiteral("screen mode id"), 0);
    const int width = readInt(entries, QStringLiteral("desktopwidth"), 0);
    const int height = readInt(entries, QStringLiteral("desktopheight"), 0);
    if (screenMode != ScreenModeWindowed && readInt(entries, QStringLiteral("use multimon"), 0) != 0) {
        preferences.setResolution(RdpHostPreferences::Resolution::AllScreens);
    } else if (screenMode == ScreenModeFullScreen) {
        preferences.setResolution(RdpHostPreferences::Resolution::MatchScreen);
    } else if (width > 0 && width <= MaxDesktopSize && height > 0 && height <= MaxDesktopSize) {
        preferences.setResolution(RdpHostPreferences::Resolution::Custom);
        preferences.setWidth(width);
        preferences.setHeight(height);
    }

    const int smartSizing = readInt(entries, QStringLiteral("smart sizing"), -1);
    if (smartSizing >= 0) {
        preferences.setWindowedScale(smartSizing != 0);
    }

    switch (readInt(entries, QStringLiteral("audiomode"), -1)) {
    case 0:
        preferences.setSound(RdpHostPreferences::Sound::Local);
        break;
    case 1:
        preferences.setSound(RdpHostPreferences::Sound::Remote);
        break;
    case 2:
        preferences.setSound(RdpHostPreferences::Sound::Disabled);
        break;
    default:
        break;
    }

    // Sharing the clipboard is on by default, so a file can only turn it off.
    if (readInt(entries, QStringLiteral("redirectclipboard"), 1) == 0) {
        preferences.setClipboardSharing(false);
    }
    return true;
}

RemoteView *RdpViewFactory::createView(QWidget *parent, const QUrl &url, KConfigGroup configGroup)
{
    return new RdpView(parent, url, configGroup);
}

HostPreferences *RdpViewFactory::createHostPreferences(KConfigGroup configGroup, QWidget *parent)
{
    return new RdpHostPreferences(configGroup, parent);
}

QString RdpViewFactory::scheme() const
{
    return QStringLiteral("rdp");
}

QString RdpViewFactory::connectActionText() const
{
    return i18n("New RDP Connection…");
}

QString RdpViewFactory::connectButtonText() const
{
    return m_connectToolTipString;
}

QString RdpViewFactory::connectToolTipText() const
{
    return i18n(
        "<html>Enter the address here. Port is optional.<br />"
        "<i>Example: rdpserver:3389 (host:port)</i></html>");
}

#include "rdpviewfactory.moc"
