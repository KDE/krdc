/*
    SPDX-FileCopyrightText: 2024 KRDC Contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "spiceviewfactory.h"

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(SpiceViewFactory, "krdc_spice.json")

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

RemoteView *SpiceViewFactory::createView(QWidget *parent, const QUrl &url, KConfigGroup configGroup)
{
    return new SpiceView(parent, url, configGroup);
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
