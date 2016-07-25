/****************************************************************************
**
** Copyright (C) 2001-2003 Tim Jansen <tim@tjansen.de>
** Copyright (C) 2007 - 2012 Urs Wolfer <uwolfer @ kde.org>
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
#include "krdc_debug.h"
#include "krdc_version.h"

#include <KCoreAddons/KAboutData>
#include <Kdelibs4ConfigMigrator>
#include <Kdelibs4Migration>
#include <KLocalizedString>
#include <QDir>
#include <QFile>
#include <QTime>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QPluginLoader>
#include <QStandardPaths>

int main(int argc, char **argv)
{
    KLocalizedString::setApplicationDomain("krdc");
    const QString appName = QStringLiteral("krdc");
    QApplication app(argc, argv);
    QTime startupTimer;
    startupTimer.start();

    app.setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    Kdelibs4ConfigMigrator migrate(appName);
    migrate.setConfigFiles(QStringList() << QStringLiteral("krdcrc"));
    if (migrate.migrate()) {
        Kdelibs4Migration dataMigrator;
        const QString sourceBasePath = dataMigrator.saveLocation("data", QStringLiteral("krdc"));
        const QString targetBasePath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/krdc/");
        QString targetFilePath;
        QDir sourceDir(sourceBasePath);
        QDir targetDir(targetBasePath);
        if (sourceDir.exists()) {
            if (!targetDir.exists()) {
                QDir().mkpath(targetBasePath);
            }
            QStringList fileNames = sourceDir.entryList(QDir::Files |
                                    QDir::NoDotAndDotDot | QDir::NoSymLinks);
            foreach (const QString &fileName, fileNames) {
                targetFilePath = targetBasePath + fileName;
                if (!QFile::exists(targetFilePath)) {
                    QFile::copy(sourceBasePath + fileName, targetFilePath);
                }
            }
        }
    }

    KAboutData aboutData(appName, i18n("KRDC"), QStringLiteral(KRDC_VERSION_STRING),
                         i18n("KDE Remote Desktop Client"), KAboutLicense::LicenseKey::GPL);

    aboutData.setCopyrightStatement(i18n("(c) 2007-2016, Urs Wolfer\n"
                               "(c) 2001-2003, Tim Jansen\n"
                               "(c) 2002-2003, Arend van Beelen jr.\n"
                               "(c) 2000-2002, Const Kaplinsky\n"
                               "(c) 2000, Tridia Corporation\n"
                               "(c) 1999, AT&T Laboratories Boston\n"
                               "(c) 1999-2003, Matthew Chapman\n"
                               "(c) 2009, Collabora Ltd"));

    aboutData.addAuthor(i18n("Urs Wolfer"), i18n("Developer, Maintainer"), QStringLiteral("uwolfer@kde.org"));
    aboutData.addAuthor(i18n("Tony Murray"), i18n("Developer"), QStringLiteral("murraytony@gmail.com"));
    aboutData.addAuthor(i18n("Tim Jansen"), i18n("Former Developer"), QStringLiteral("tim@tjansen.de"));
    aboutData.addAuthor(i18n("Arend van Beelen jr."), i18n("Initial RDP backend"), QStringLiteral("arend@auton.nl"));
    aboutData.addCredit(i18n("Brad Hards"), i18n("Google Summer of Code 2007 KRDC project mentor"),
                        QStringLiteral("bradh@frogmouth.net"));
    aboutData.addCredit(i18n("LibVNCServer / LibVNCClient developers"), i18n("VNC client library"),
                        QStringLiteral("libvncserver-common@lists.sf.net"), QStringLiteral("http://libvncserver.sourceforge.net/"));
    aboutData.addAuthor(i18n("Abner Silva"), i18n("Telepathy Tubes Integration"), QStringLiteral("abner.silva@kdemail.net"));
    aboutData.setOrganizationDomain("kde.org");
    KAboutData::setApplicationData(aboutData);

    app.setApplicationName(aboutData.componentName());
    app.setApplicationDisplayName(aboutData.displayName());
    app.setOrganizationDomain(aboutData.organizationDomain());
    app.setApplicationVersion(aboutData.version());
    app.setWindowIcon(QIcon::fromTheme(appName));

    QCommandLineParser parser;
    parser.addVersionOption();
    parser.addHelpOption();
    aboutData.setupCommandLine(&parser);

    // command line options
    parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("fullscreen"),
                                        i18n("Start KRDC with the provided URL in fullscreen mode (works only with one URL)")));
    parser.addPositionalArgument(QStringLiteral("url"), i18n("URLs to connect after startup"));

    parser.process(app);
    aboutData.processCommandLine(&parser);
    MainWindow *mainwindow = new MainWindow;
    mainwindow->show();
    const QStringList args = parser.positionalArguments();
    if (args.length() > 0) {
        for (int i = 0; i < args.length(); ++i) {
            QUrl url = QUrl::fromLocalFile(args.at(i));
            if (url.scheme().isEmpty() || url.host().isEmpty()) { // unusable url; try to recover it...
                QString arg = args.at(i);

                qCDebug(KRDC) << "unusable url; try to recover it:" << arg;

                if (arg.lastIndexOf(QLatin1Char('/')) != 0)
                    arg = arg.right(arg.length() - arg.lastIndexOf(QLatin1Char('/')) - 1);

                if (!arg.contains(QStringLiteral("://")))
                    arg.prepend(QStringLiteral("vnc://")); // vnc was default in kde3 times...

                qCDebug(KRDC) << "recovered url:" << arg;

                url = QUrl(arg);
            }
            if (!url.isValid()) {
                continue;
            }

            mainwindow->newConnection(url, parser.isSet(QStringLiteral("fullscreen")));
        }
    }
    qCDebug(KRDC) << "########## KRDC ready:" << startupTimer.elapsed() << "ms ##########";

    return app.exec();
}
