/****************************************************************************
**
** Copyright (C) 2001-2003 Tim Jansen <tim@tjansen.de>
** Copyright (C) 2007 - 2008 Urs Wolfer <uwolfer @ kde.org>
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

#include "mainwindow.h"

#include <KApplication>
#include <KLocale>
#include <KCmdLineArgs>
#include <KAboutData>
#include <KDebug>

#include <QTime>

int main(int argc, char **argv)
{
    QTime startupTimer;;
    startupTimer.start();
    KAboutData aboutData("krdc", 0, ki18n("KRDC"), KDE_VERSION_STRING,
                         ki18n("KDE remote desktop connection"), KAboutData::License_GPL,
                         ki18n("(c) 2007-2009, Urs Wolfer\n"
                               "(c) 2001-2003, Tim Jansen\n"
                               "(c) 2002-2003, Arend van Beelen jr.\n"
                               "(c) 2000-2002, Const Kaplinsky\n"
                               "(c) 2000, Tridia Corporation\n"
                               "(c) 1999, AT&T Laboratories Boston\n"
                               "(c) 1999-2003, Matthew Chapman\n"
                               "(c) 2009, Collabora Ltd"));

    aboutData.addAuthor(ki18n("Urs Wolfer"), ki18n("Developer, Maintainer"), "uwolfer@kde.org");
    aboutData.addAuthor(ki18n("Tim Jansen"), ki18n("Former Developer"), "tim@tjansen.de");
    aboutData.addAuthor(ki18n("Arend van Beelen jr."), ki18n("Initial RDP backend"), "arend@auton.nl");
    aboutData.addCredit(ki18n("Brad Hards"), ki18n("Google Summer of Code 2007 KRDC project mentor"),
                        "bradh@frogmouth.net");
    aboutData.addCredit(ki18n("LibVNCServer / LibVNCClient developers"), ki18n("VNC client library"),
                        "libvncserver-common@lists.sf.net");
    aboutData.addAuthor(ki18n("Abner Silva"), ki18n("Telepathy Tubes Integration"), "abner.silva@kdemail.net");

    KCmdLineArgs::init(argc, argv, &aboutData);

    KCmdLineOptions options;
    options.add("fullscreen", ki18n("Start KRDC with the provided URL in fullscreen mode (works only with one URL)"));
    options.add("!+[URL]", ki18n("URLs to connect after startup"));

    KCmdLineArgs::addCmdLineOptions(options);

    KApplication app;

    MainWindow *mainwindow = new MainWindow;
    mainwindow->show();

    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

    if (args->count() > 0) {
        for (int i = 0; i < args->count(); ++i) {
            KUrl u(args->url(i));

            if (u.scheme().isEmpty() || u.host().isEmpty()) { // unusable url; try to recover it...
                QString arg(args->url(i).url());

                kDebug(5010) << "unusable url; try to recover it:" << arg;

                if (arg.lastIndexOf('/') != 0)
                    arg = arg.right(arg.length() - arg.lastIndexOf('/') - 1);

                if (!arg.contains("://"))
                    arg.prepend("vnc://"); // vnc was default in kde3 times...

                kDebug(5010) << "recovered url:" << arg;

                u = arg;
            }

            if (!u.isValid())
                continue;

            mainwindow->newConnection(u, ((args->isSet("fullscreen")) && (args->count() == 1)));
        }
    }

    kDebug(5010) << "########## KRDC ready:" << startupTimer.elapsed() << "ms ##########";

    return app.exec();
}
