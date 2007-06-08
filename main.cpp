/****************************************************************************
**
** Copyright (C) 2001-2003 Tim Jansen <tim@tjansen.de>
** Copyright (C) 2007 Urs Wolfer <uwolfer @ kde.org>
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

static const char description[] = I18N_NOOP("KDE remote desktop connection");

int main(int argc, char **argv)
{
    KAboutData aboutData("krdc", I18N_NOOP("KRDC"),
                         KDE_VERSION_STRING, description, KAboutData::License_GPL,
                         I18N_NOOP("(c) 2007, Urs Wolfer\n"
                                   "(c) 2001-2003, Tim Jansen\n"
                                   "(c) 2002-2003, Arend van Beelen jr.\n"
                                   "(c) 2000-2002, Const Kaplinsky\n"
                                   "(c) 2000, Tridia Corporation\n"
                                   "(c) 1999, AT&T Laboratories Cambridge\n"
                                   "(c) 1999-2003, Matthew Chapman"));

    aboutData.addAuthor("Urs Wolfer", I18N_NOOP("Developer, Maintainer"), "uwolfer@kde.org");
    aboutData.addAuthor("Tim Jansen", I18N_NOOP("Former Developer"), "tim@tjansen.de");
    aboutData.addAuthor("Arend van Beelen jr.", I18N_NOOP("RDP backend"), "arend@auton.nl");
    aboutData.addCredit("AT&T Laboratories Cambridge", I18N_NOOP("Original VNC viewer and protocol design"));
    aboutData.addCredit("Const Kaplinsky", I18N_NOOP("TightVNC encoding"));
    aboutData.addCredit("Tridia Corporation", I18N_NOOP("ZLib encoding"));

    KCmdLineArgs::init(argc, argv, &aboutData);

    KApplication app;

    MainWindow *mainwindow = new MainWindow;
    mainwindow->show();
    return app.exec();
}
