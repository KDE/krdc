/****************************************************************************
**
** Copyright (C) 2009 Collabora Ltd <info@collabora.co.uk>
** Copyright (C) 2009 Abner Silva <abner.silva@kdemail.net>
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

#include "approvermanager.h"

#include <KCoreAddons/KAboutData>
#include <KI18n/KLocalizedString>

#include <QApplication>

#include <TelepathyQt/Types>
#include <TelepathyQt/Debug>
#include <TelepathyQt/ClientRegistrar>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    KAboutData aboutData(QStringLiteral("krdc_rfb_approver"), i18n("KRDC"), i18n("Approver for KRDC"),
                         i18n("0.1"), KAboutLicense::LicenseKey::GPL, i18n("(C) 2009, Abner Silva"));

    aboutData.addCredit(i18n("Abner Silva"), i18n(""), QStringLiteral("abner.silva@kdemail.net"));
    KAboutData::setApplicationData(aboutData);

    app.setApplicationName(aboutData.componentName());
    app.setApplicationDisplayName(aboutData.displayName());
    app.setOrganizationDomain(aboutData.organizationDomain());
    app.setApplicationVersion(aboutData.version());
    app.setQuitOnLastWindowClosed(false);

    Tp::registerTypes();
    Tp::enableDebug(true);
    Tp::enableWarnings(true);

    Tp::ClientRegistrarPtr registrar = Tp::ClientRegistrar::create();
    Tp::SharedPtr<ApproverManager> approverManager;
    approverManager = Tp::SharedPtr<ApproverManager>(new ApproverManager(0));
    registrar->registerClient(Tp::AbstractClientPtr::dynamicCast(approverManager), QLatin1String("krdc_rfb_approver"));

    return app.exec();
}
