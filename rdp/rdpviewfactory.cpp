/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rdpviewfactory.h"

#include <QFile>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>
#include <QUrlQuery>

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(RdpViewFactory, "krdc_rdp.json")

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

QUrl RdpViewFactory::loadUrlFromFile(const QUrl &url) const
{
    QString filePath = url.toLocalFile();
    if (!filePath.toLower().endsWith(QStringLiteral(".rdp"))) {
        return {};
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QUrl loadedUrl;
    QUrlQuery query;
    loadedUrl.setScheme(QStringLiteral("rdp"));
    loadedUrl.setPath(QString());

    QTextStream in(&file);
    while (!in.atEnd()) {
        QStringList line = in.readLine().split(QLatin1Char(':'));
        if (line.size() < 3) {
            continue;
        }
        QString key = line.at(0).toLower();
        // full address:s:hostname[:port]
        if (key == QStringLiteral("full address")) {
            loadedUrl.setHost(line.at(2));
            if (line.size() > 3) {
                bool ok;
                uint port = line.at(3).toUInt(&ok);
                if (ok) {
                    loadedUrl.setPort(port);
                }
            }
        }

        if (key == QStringLiteral("username")) {
            loadedUrl.setUserName(line.at(2));
        }

        if (key == QStringLiteral("domain")) {
            query.addQueryItem(QStringLiteral("domain"), line.at(2));
        }

        if (key == QStringLiteral("password")) {
            loadedUrl.setPassword(line.at(2));
        }
    }

    loadedUrl.setQuery(query);
    file.close();
    return loadedUrl;
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
    return i18n("New RDP Connection...");
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
