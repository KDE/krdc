/****************************************************************************
**
** Copyright (C) 2008 Urs Wolfer <uwolfer @ kde.org>
**
** This file is part of KDE.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; see the file COPYING. If not, write to
** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
** Boston, MA 02110-1301, USA.
**
****************************************************************************/

#include "vncviewfactory.h"
#include "remoteviewfactory.h"

#include <KCoreAddons/KExportPlugin>

K_PLUGIN_FACTORY_WITH_JSON(KrdcFactory, "krdc_vnc.json", registerPlugin<VncViewFactory>();)

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
