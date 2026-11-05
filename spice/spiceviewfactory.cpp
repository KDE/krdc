/*
    SPDX-FileCopyrightText: 2024 KRDC Contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "spiceviewfactory.h"

#include <KMessageBox>
#include <KPluginFactory>

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrlQuery>

#include <glib.h>

#include <memory>
#include <utility>

K_PLUGIN_CLASS_WITH_JSON(SpiceViewFactory, "krdc_spice.json")

static const char VvGroup[] = "virt-viewer";
static constexpr int MaxPort = 65535;
// Far larger than any real .vv file, which is a few kilobytes including its certificate.
static constexpr qint64 MaxVvFileSize = 1024 * 1024;

static QString readVvString(GKeyFile *file, const char *key)
{
    gchar *value = g_key_file_get_string(file, VvGroup, key, nullptr);
    const QString result = QString::fromUtf8(value);
    g_free(value);
    return result;
}

// Returns false if the port is invalid. An absent or negative port is left as "no port".
static bool readVvPort(const QHash<QString, QString> &entries, const QString &key, int *port)
{
    const QString text = entries.value(key);
    if (text.isEmpty()) {
        return true;
    }
    bool ok;
    const int value = text.toInt(&ok);
    if (!ok || value > MaxPort) {
        return false;
    }
    *port = value;
    return true;
}

static QHash<QString, QString> readVvFile(const QString &filePath)
{
    const QFileInfo fileInfo(filePath);
    if (!filePath.endsWith(QStringLiteral(".vv"), Qt::CaseInsensitive) || !fileInfo.isFile() || fileInfo.size() > MaxVvFileSize) {
        return {};
    }

    const std::unique_ptr<GKeyFile, decltype(&g_key_file_free)> file(g_key_file_new(), g_key_file_free);
    if (!g_key_file_load_from_file(file.get(), QFile::encodeName(filePath).constData(), G_KEY_FILE_NONE, nullptr)
        || readVvString(file.get(), "type") != QStringLiteral("spice")) {
        return {};
    }

    QHash<QString, QString> entries;
    for (const char *key : {"title", "host", "port", "tls-port", "password", "ca", "host-subject", "proxy"}) {
        entries.insert(QString::fromLatin1(key), readVvString(file.get(), key));
    }
    return entries;
}

static std::optional<SpiceFileConnection> parseConnection(const QHash<QString, QString> &entries)
{
    SpiceFileConnection connection;
    connection.title = entries.value(QStringLiteral("title"));
    connection.host = entries.value(QStringLiteral("host"));
    connection.password = entries.value(QStringLiteral("password"));
    connection.ca = entries.value(QStringLiteral("ca")).toUtf8();
    connection.certSubject = entries.value(QStringLiteral("host-subject"));
    QString proxy = entries.value(QStringLiteral("proxy"));
    if (!proxy.isEmpty()) {
        if (!proxy.contains(QStringLiteral("://"))) {
            proxy.prepend(QStringLiteral("http://"));
        }
        // SPICE ignores a proxy with another scheme and connects directly, and it reads any "@" as the
        // end of the user name, even in a query or path. Keep only the parts that both parsers agree on.
        const QUrl proxyUrl(proxy, QUrl::StrictMode);
        const bool supportedScheme = proxyUrl.scheme() == QStringLiteral("http") || proxyUrl.scheme() == QStringLiteral("https");
        if (!proxyUrl.isValid() || !supportedScheme || proxyUrl.host().isEmpty()) {
            return std::nullopt;
        }
        connection.proxy = proxyUrl.adjusted(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment).toString(QUrl::FullyEncoded);
    }
    if (!readVvPort(entries, QStringLiteral("port"), &connection.port) || !readVvPort(entries, QStringLiteral("tls-port"), &connection.tlsPort)
        || (connection.port <= 0 && connection.tlsPort <= 0)) {
        return std::nullopt;
    }

    // The host goes into the request to the proxy as it is, so it must not be able to add to it.
    static const QRegularExpression unsafeCharacters(QStringLiteral("[\\s\\p{Cc}]"));
    if (connection.host.isEmpty() || connection.host.contains(unsafeCharacters)) {
        return std::nullopt;
    }
    return connection;
}

SpiceViewFactory::SpiceViewFactory(QObject *parent, const QVariantList &args)
    : RemoteViewFactory(parent)
{
    Q_UNUSED(args);

    KLocalizedString::setApplicationDomain("krdc");
}

SpiceViewFactory::~SpiceViewFactory()
{
}

bool SpiceViewFactory::supportsUrl(const QUrl &url) const
{
    return (url.scheme().compare(QLatin1String("spice"), Qt::CaseInsensitive) == 0);
}

RemoteViewFactory::ConnectionFile SpiceViewFactory::loadConnectionFile(const QUrl &url) const
{
    const QHash<QString, QString> entries = readVvFile(url.toLocalFile());
    const std::optional<SpiceFileConnection> connection = parseConnection(entries);
    if (!connection) {
        return {};
    }

    QUrl loadedUrl;
    loadedUrl.setScheme(QStringLiteral("spice"));
    loadedUrl.setHost(connection->host);
    // Only a proxy can reach a host that is not a valid address, so name the connection after it.
    if (loadedUrl.host().isEmpty()) {
        loadedUrl = QUrl();
        loadedUrl.setScheme(QStringLiteral("spice"));
        loadedUrl.setHost(QUrl(connection->proxy).host());
        if (loadedUrl.host().isEmpty()) {
            return {};
        }
    }

    // The marker keeps a file's details apart from other connections to the host. The title follows
    // the virtual machine and the host stays the same, unlike the port, which can change, so its
    // settings stay with it.
    const QString identity = connection->title.isEmpty() ? connection->host : connection->title;
    QUrlQuery query;
    query.addQueryItem(SpiceFileConnection::UrlQueryKey, QString::fromLatin1(QUrl::toPercentEncoding(identity)));
    loadedUrl.setQuery(query);

    return {loadedUrl, entries};
}

bool SpiceViewFactory::importFileSettings(const QHash<QString, QString> &entries, KConfigGroup configGroup, QWidget *parent) const
{
    Q_UNUSED(configGroup);

    const std::optional<SpiceFileConnection> parsed = parseConnection(entries);
    if (!parsed) {
        return false;
    }
    const SpiceFileConnection &connection = *parsed;

    // A proxy carries the connection, and the file also names the certificate to trust, so the
    // user has to accept each proxy.
    if (!connection.proxy.isEmpty()) {
        const QString proxyAddress = QUrl(connection.proxy).adjusted(QUrl::RemoveUserInfo).toString(QUrl::FullyEncoded);
        const auto answer = KMessageBox::warningContinueCancel(
            parent,
            i18nc("@info %1 is a server address",
                  "This file connects through the proxy “%1”, which could intercept the session. Continue only if you trust this file.",
                  proxyAddress),
            i18nc("@title:window", "Use Proxy from File?"));
        if (answer != KMessageBox::Continue) {
            return false;
        }
    }

    m_acceptedConnection = connection;
    return true;
}

RemoteView *SpiceViewFactory::createView(QWidget *parent, const QUrl &url, KConfigGroup configGroup)
{
    return new SpiceView(parent, url, configGroup, std::exchange(m_acceptedConnection, std::nullopt));
}

HostPreferences *SpiceViewFactory::createHostPreferences(KConfigGroup configGroup, QWidget *parent)
{
    return new SpiceHostPreferences(configGroup, parent);
}

QString SpiceViewFactory::scheme() const
{
    return QLatin1String("spice");
}

QString SpiceViewFactory::connectActionText() const
{
    return i18n("New SPICE Connection…");
}

QString SpiceViewFactory::connectButtonText() const
{
    return i18n("Connect to a SPICE Remote Desktop");
}

QString SpiceViewFactory::connectToolTipText() const
{
    return i18n(
        "<html>Enter the address here.<br />"
        "<i>Example: spice://myhost:5900</i></html>");
}

#include "spiceviewfactory.moc"
