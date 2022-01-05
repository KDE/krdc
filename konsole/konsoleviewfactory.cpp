/*
    SPDX-FileCopyrightText: 2009 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "konsoleviewfactory.h"

#include <KLocalizedString>

K_PLUGIN_CLASS_WITH_JSON(KonsoleViewFactory, "krdc_konsole.json")

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
