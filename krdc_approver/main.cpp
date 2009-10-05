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

#include <KAboutData>
#include <KCmdLineArgs>
#include <KLocalizedString>
#include <KUniqueApplication>

#include <TelepathyQt4/Types>
#include <TelepathyQt4/Debug>
#include <TelepathyQt4/ClientRegistrar>

int main(int argc, char **argv)
{
    KAboutData aboutData("krdc_rfb_approver", "KRDC", ki18n("KRDC"), "0.1",
            ki18n("Approver for KRDC"), KAboutData::License_GPL,
            ki18n("(C) 2009, Abner Silva"));
    aboutData.setProgramIconName("krdc");
    aboutData.addAuthor(ki18nc("@info:credit", "Abner Silva"), KLocalizedString(),
            "abner.silva@kdemail.net");

    KCmdLineArgs::init(argc, argv, &aboutData);

    if (!KUniqueApplication::start())
        return 0;

    KUniqueApplication app;
    app.disableSessionManagement();

    Tp::registerTypes();
    Tp::enableDebug(true);
    Tp::enableWarnings(true);

    Tp::ClientRegistrarPtr registrar = Tp::ClientRegistrar::create();
    Tp::SharedPtr<ApproverManager> approverManager;
    approverManager = Tp::SharedPtr<ApproverManager>(new ApproverManager(0));
    registrar->registerClient(Tp::AbstractClientPtr::dynamicCast(approverManager), "krdc_rfb_approver");

    return app.exec();
}
