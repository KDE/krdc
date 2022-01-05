/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "vncviewfactory.h"
#include "remoteviewfactory.h"

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(VncViewFactory, "krdc_vnc.json")

VncViewFactory::VncViewFactory(QObject *parent, const QVariantList &args)
        : RemoteViewFactory(parent)
{
    Q_UNUSED(args);

    KLocalizedString::setApplicationDomain("krdc");
}

VncViewFactory::~VncViewFactory()
{
}

bool VncViewFactory::supportsUrl(const QUrl &url) const
{
    return (url.scheme().compare(QLatin1String("vnc"), Qt::CaseInsensitive) == 0);
}

RemoteView *VncViewFactory::createView(QWidget *parent, const QUrl &url, KConfigGroup configGroup)
{
    return new VncView(parent, url, configGroup);
}

HostPreferences *VncViewFactory::createHostPreferences(KConfigGroup configGroup, QWidget *parent)
{
    return new VncHostPreferences(configGroup, parent);
}

QString VncViewFactory::scheme() const
{
    return QLatin1String("vnc");
}

QString VncViewFactory::connectActionText() const
{
    return i18n("New VNC Connection...");
}

QString VncViewFactory::connectButtonText() const
{
    return i18n("Connect to a VNC Remote Desktop");
}

QString VncViewFactory::connectToolTipText() const
{
    return i18n("<html>Enter the address here.<br />"
                "<i>Example: vncserver:1 (host:port / screen)</i></html>");
}

#include "vncviewfactory.moc"
