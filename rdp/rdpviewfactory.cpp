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

#include "rdpviewfactory.h"

#include <QStandardPaths>

#include <KCoreAddons/KExportPlugin>

K_PLUGIN_FACTORY_WITH_JSON(KrdcFactory, "krdc_rdp.json", registerPlugin<RdpViewFactory>();)

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
