/****************************************************************************
**
** Copyright (C) 2009 Urs Wolfer <uwolfer @ kde.org>
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

#include "konsoleviewfactory.h"

#include <KLocalizedString>

K_PLUGIN_FACTORY_WITH_JSON(KrdcFactory, "krdc_konsole.json", registerPlugin<KonsoleViewFactory>();)

KonsoleViewFactory::KonsoleViewFactory(QObject *parent, const QVariantList &args)
        : RemoteViewFactory(parent)
{
    Q_UNUSED(args);
    KLocalizedString::setApplicationDomain("krdc");
}

KonsoleViewFactory::~KonsoleViewFactory()
{
}

bool KonsoleViewFactory::supportsUrl(const QUrl &url) const
{
    return (url.scheme().compare("konsole", Qt::CaseInsensitive) == 0);
}

RemoteView *KonsoleViewFactory::createView(QWidget *parent, const QUrl &url, KConfigGroup configGroup)
{
    return new KonsoleView(parent, url, configGroup);
}

HostPreferences *KonsoleViewFactory::createHostPreferences(KConfigGroup configGroup, QWidget *parent)
{
    Q_UNUSED(configGroup);
    Q_UNUSED(parent);

    return 0;
}

QString KonsoleViewFactory::scheme() const
{
    return "konsole";
}

QString KonsoleViewFactory::connectActionText() const
{
    return i18n("New Konsole Connection..."); //FIXME
}

QString KonsoleViewFactory::connectButtonText() const
{
    return i18n("KRDC Konsole Connection");
}

QString KonsoleViewFactory::connectToolTipText() const
{
    return i18n("<html>Enter the address here. Port is optional.<br />"
                "<i>Example: konsoleserver (host)</i></html>");
}

#include "konsoleviewfactory.moc"
