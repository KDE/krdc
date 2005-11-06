/***************************************************************************
                           main.cpp  -  main control
                             -------------------
    begin                : Thu Dec 20 15:11:42 CET 2001
    copyright            : (C) 2001-2003 by Tim Jansen
    email                : tim@tjansen.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <kapplication.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kdebug.h>
#include <kwallet.h>
#include <qwindowdefs.h>
#include <qtimer.h>
#include <qfile.h>
#include <qtextstream.h>
#include <qregexp.h>

#include "../config.h"
#include "main.h"

// NOTE: I'm not comfortable with the wallet being global data and this high up
// in the heirarchy, but there are 3 reasons for its current placement here:
// 1) There are some important threading issues where it comes to the password
// handling code, and a lot of it is done outside of the objects. 
// 2) Different backends need access to the same wallet. so that it is not
// opened multiple times.
// 3) MainController is about the only thing that isn't deleted in between connection
// attempts.
KWallet::Wallet *wallet = 0;

static const char description[] = I18N_NOOP("Remote desktop connection");

static KCmdLineOptions options[] =
{
	{ "f", 0, 0 },
	{ "fullscreen", I18N_NOOP("Start in fullscreen mode"), 0 },
	{ "w", 0, 0 },
	{ "window", I18N_NOOP("Start in regular window"), 0 },
	{ "l", 0, 0 },
	{ "low-quality", I18N_NOOP("Low quality mode (Tight Encoding, 8 bit color)"), 0 },
	{ "m", 0, 0 },
	{ "medium-quality", I18N_NOOP("Medium quality mode (Tight Encoding, lossy)"), 0 },
	{ "h", 0, 0 },
	{ "high-quality", I18N_NOOP("High quality mode, default (Hextile Encoding)"), 0 },
	{ "s", 0, 0 },
	{ "scale", I18N_NOOP("Start VNC in scaled mode"), 0 },
	{ "c", 0, 0 },
	{ "local-cursor", I18N_NOOP("Show local cursor (VNC only)"), 0 },
	{ "e", 0, 0 },
	{ "encodings ", I18N_NOOP("Override VNC encoding list (e.g. 'hextile raw')"), 0 },
	{ "p", 0, 0 },
	{ "password-file ", I18N_NOOP("Provide the password in a file"), 0 },
	{ "+[host]", I18N_NOOP("The name of the host, e.g. 'localhost:1'"), 0 },
	KCmdLineLastOption
};


int main(int argc, char *argv[])
{
	KAboutData aboutData( "krdc", I18N_NOOP("Remote Desktop Connection"),
			      VERSION, description, KAboutData::License_GPL,
		"(c) 2001-2003, Tim Jansen"
		"(c) 2002-2003, Arend van Beelen jr."
		"(c) 2000-2002, Const Kaplinsky\n"
		"(c) 2000, Tridia Corporation\n"
		"(c) 1999, AT&T Laboratories Cambridge\n"
		"(c) 1999-2003, Matthew Chapman\n", 0, 0,
			      "tim@tjansen.de");
	aboutData.addAuthor("Tim Jansen",0, "tim@tjansen.de");
	aboutData.addAuthor("Arend van Beelen jr.",
			    I18N_NOOP("RDP backend"), "arend@auton.nl");
	aboutData.addCredit("AT&T Laboratories Cambridge",
			    I18N_NOOP("Original VNC viewer and protocol design"));
	aboutData.addCredit("Const Kaplinsky",
			    I18N_NOOP("TightVNC encoding"));
	aboutData.addCredit("Tridia Corporation",
			    I18N_NOOP("ZLib encoding"));
	KCmdLineArgs::init( argc, argv, &aboutData );
	KCmdLineArgs::addCmdLineOptions( options );

	KApplication a;

	QString host = QString::null;
	Quality quality = QUALITY_UNKNOWN;
	QString encodings = QString::null;
	QString password = QString::null;
	QString resolution = QString::null;
	QString keymap = QString::null;
	WindowMode wm = WINDOW_MODE_AUTO;
	bool scale = false;
	bool localCursor = false;
	QSize initialWindowSize;

	KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

	if (args->isSet("low-quality"))
		quality = QUALITY_LOW;
	else if (args->isSet("medium-quality"))
		quality = QUALITY_MEDIUM;
	else if (args->isSet("high-quality"))
		quality = QUALITY_HIGH;

	if (args->isSet("fullscreen"))
		wm = WINDOW_MODE_FULLSCREEN;
	else if (args->isSet("window"))
		wm = WINDOW_MODE_NORMAL;

	if (args->isSet("scale"))
		scale = true;

	if (args->isSet("local-cursor"))
		localCursor = true;

	if (args->isSet("encodings"))
		encodings = args->getOption("encodings");

	if (args->isSet("password-file")) {
		QString passwordFile = args->getOption("password-file");
		QFile f(passwordFile);
		if (!f.open(QIODevice::ReadOnly)) {
			KMessageBox::error(0, i18n("The password file '%1' does not exist.").arg(passwordFile));
			return 1;
		}
		password = QTextStream(&f).readLine();
		f.close();
	}

	if (args->count() > 0)
		host = args->arg(0);

    QString is;
    KCmdLineArgs *args = KCmdLineArgs::parsedArgs("kde");
    if (args && args->isSet("geometry"))
            is = args->getOption("geometry");
	if (!is.isNull()) {
		QRegExp re("([0-9]+)[xX]([0-9]+)");
		if (!re.exactMatch(is))
			args->usage(i18n("Wrong geometry format, must be widthXheight"));
		initialWindowSize = QSize(re.cap(1).toInt(), re.cap(2).toInt());
	}

	MainController mc(&a, wm, host, quality, encodings, password,
			  scale, localCursor, initialWindowSize);
	return mc.main();
}

MainController::MainController(KApplication *app, WindowMode wm,
			       const QString &host,
			       Quality quality,
			       const QString &encodings,
			       const QString &password,
			       bool scale,
			       bool localCursor,
			       QSize initialWindowSize) :
        m_windowMode(wm),
	m_host(host),
        m_encodings(encodings),
        m_password(password),
	m_scale(scale),
	m_localCursor(localCursor),
	m_initialWindowSize(initialWindowSize),
	m_quality(quality),
	m_app(app) {
}

MainController::~MainController() {
	if ( wallet ) {
		delete wallet; wallet = 0;
	}
}

int MainController::main() {

	if (start())
		return m_app->exec();
	else
		return 0;
}

void MainController::errorRestartRequested() {
	QTimer::singleShot(0, this, SLOT(errorRestart()));
}

bool MainController::start() {
	m_krdc = new KRDC(m_windowMode, m_host,
			  m_quality, m_encodings, m_password,
			  m_scale, m_localCursor, m_initialWindowSize);
	m_app->setMainWidget(m_krdc);

	QObject::connect(m_krdc, SIGNAL(disconnected()),
			 m_app, SLOT(quit()));
	connect(m_krdc, SIGNAL(disconnectedError()),
		SLOT(errorRestartRequested()));

	return m_krdc->start();
}

void MainController::errorRestart() {
	if (!m_host.isEmpty())
		KRDC::setLastHost(m_host);
	m_host = QString::null; // only auto-connect once

	// unset KRDC as main widget, to avoid quit on delete
	m_app->setMainWidget(0);

	m_krdc = 0;

	if (!start())
		m_app->quit();
}

#include "main.moc"
