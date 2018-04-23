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

#include "testviewfactory.h"

#include <KLocalizedString>

K_PLUGIN_FACTORY_WITH_JSON(KrdcFactory, "krdc_test.json", registerPlugin<TestViewFactory>();)

TestViewFactory::TestViewFactory(QObject *parent, const QVariantList &args)
        : RemoteViewFactory(parent)
{
    Q_UNUSED(args);

    KLocalizedString::setApplicationDomain("krdc");
}

TestViewFactory::~TestViewFactory()
{
}

bool TestViewFactory::supportsUrl(const QUrl &url) const
{
    return (url.scheme().compare(QLatin1String("test"), Qt::CaseInsensitive) == 0);
}

RemoteView *TestViewFactory::createView(QWidget *parent, const QUrl &url, KConfigGroup configGroup)
{
    return new TestView(parent, url, configGroup);
}

HostPreferences *TestViewFactory::createHostPreferences(KConfigGroup configGroup, QWidget *parent)
{
    Q_UNUSED(configGroup);
    Q_UNUSED(parent);

    return nullptr;
}

QString TestViewFactory::scheme() const
{
    return QLatin1String("test");
}

QString TestViewFactory::connectActionText() const
{
    return QLatin1String("New Test Connection..."); // no i18n required, just internal test plugin!
}

QString TestViewFactory::connectButtonText() const
{
    return QLatin1String("KRDC Test Connection"); // no i18n required, just internal test plugin!
}

QString TestViewFactory::connectToolTipText() const
{
    return QLatin1String("<html>Enter the address here. Port is optional.<br />"
            "<i>Example: testserver (host)</i></html>"); // no i18n required, just internal test plugin!
}

#include "testviewfactory.moc"
