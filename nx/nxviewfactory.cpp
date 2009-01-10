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

#include "nxviewfactory.h"

#include <KDebug>

KRDC_PLUGIN_EXPORT(NxViewFactory)

NxViewFactory::NxViewFactory(QObject *parent, const QVariantList &args)
        : RemoteViewFactory(parent)
{
    Q_UNUSED(args);

    KGlobal::locale()->insertCatalog("krdc");
}

NxViewFactory::~NxViewFactory()
{
}

bool NxViewFactory::supportsUrl(const KUrl &url) const
{
    return (url.scheme().compare("nx", Qt::CaseInsensitive) == 0);
}

RemoteView *NxViewFactory::createView(QWidget *parent, const KUrl &url, KConfigGroup configGroup)
{
    return new NxView(parent, url, configGroup);
}

HostPreferences *NxViewFactory::createHostPreferences(KConfigGroup configGroup, QWidget *parent)
{
    return new NxHostPreferences(configGroup, parent);
}

QString NxViewFactory::scheme() const
{
    return "nx";
}

QString NxViewFactory::connectActionText() const
{
    return i18n("New NX Connection...");
}

QString NxViewFactory::connectButtonText() const
{
    return i18n("Connect to a NX Remote Desktop");
}

QString NxViewFactory::connectToolTipText() const
{
    return i18n("<html>Enter the address here.<br />"
                "<i>Example: nxserver (host)</i></html>");
}

#include "moc_nxviewfactory.cpp"
