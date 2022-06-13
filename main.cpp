/*
    SPDX-FileCopyrightText: 2001-2003 Tim Jansen <tim@tjansen.de>
    SPDX-FileCopyrightText: 2007-2012 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mainwindow.h"
#include "krdc_debug.h"
#include "krdc_version.h"
#include "settings.h"

#include <KAboutData>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <Kdelibs4ConfigMigrator>
#include <Kdelibs4Migration>
#endif
#include <KLocalizedString>
#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QPluginLoader>
#include <QStandardPaths>

int main(int argc, char **argv)
{
    const QString appName = QStringLiteral("krdc");
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("krdc");
    QElapsedTimer startupTimer;
    startupTimer.start();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
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
            const QStringList fileNames = sourceDir.entryList(QDir::Files |
                                    QDir::NoDotAndDotDot | QDir::NoSymLinks);
            for (const QString &fileName : fileNames) {
                targetFilePath = targetBasePath + fileName;
                if (!QFile::exists(targetFilePath)) {
                    QFile::copy(sourceBasePath + fileName, targetFilePath);
                }
            }
        }
    }
#endif
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
    app.setWindowIcon(QIcon::fromTheme(appName));

    QCommandLineParser parser;
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
            QUrl url = QUrl(args.at(i));
            // no URL scheme, assume argument is only a hostname
            if (url.scheme().isEmpty()) {
                QString defaultProto = Settings::defaultProtocol();
                url.setScheme(defaultProto);
                url.setHost(args.at(i));
                url.setPath(QString());
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
