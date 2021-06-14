/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
