/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rdpviewfactory.h"

#include <QStandardPaths>

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(RdpViewFactory, "krdc_rdp.json")

RdpViewFactory::RdpViewFactory(QObject *parent, const QVariantList &args)
        : RemoteViewFactory(parent)
{
    Q_UNUSED(args);

    KLocalizedString::setApplicationDomain("krdc");

    m_connectToolTipString = i18n("Connect to a Windows Remote Desktop (RDP)");

    QMetaObject::invokeMethod(this, "checkFreerdpAvailability", Qt::DirectConnection);
}

RdpViewFactory::~RdpViewFactory()
{
}

bool RdpViewFactory::supportsUrl(const QUrl &url) const
{
    return (url.scheme().compare(QStringLiteral("rdp"), Qt::CaseInsensitive) == 0);
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
    return i18n("<html>Enter the address here. Port is optional.<br />"
                "<i>Example: rdpserver:3389 (host:port)</i></html>");
}

void RdpViewFactory::checkFreerdpAvailability()
{
    if (QStandardPaths::findExecutable(QStringLiteral("xfreerdp")).isEmpty()) {
        m_connectToolTipString += QLatin1Char('\n') + i18n("The application \"xfreerdp\" cannot be found on your system; make sure it is properly installed "
                                              "if you need RDP support.");
    }
}

#include "rdpviewfactory.moc"
