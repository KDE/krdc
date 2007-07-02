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
#include <kconfig.h>
#include <kdebug.h>
#include <kglobal.h>
#include <kwallet.h>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QRegExp>

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


int main(int argc, char *argv[])
{
	KAboutData aboutData( "krdc", 0, ki18n("Remote Desktop Connection"),
			      KDE_VERSION_STRING, ki18n(description), KAboutData::License_GPL,
		ki18n("(c) 2001-2003, Tim Jansen"
		"(c) 2002-2003, Arend van Beelen jr."
		"(c) 2000-2002, Const Kaplinsky\n"
		"(c) 2000, Tridia Corporation\n"
		"(c) 1999, AT&T Laboratories Cambridge\n"
		"(c) 1999-2003, Matthew Chapman\n"), KLocalizedString(), 0,
			      "tim@tjansen.de");
	aboutData.addAuthor(ki18n("Tim Jansen"),KLocalizedString(), "tim@tjansen.de");
	aboutData.addAuthor(ki18n("Arend van Beelen jr."),
			    ki18n("RDP backend"), "arend@auton.nl");
	aboutData.addCredit(ki18n("AT&T Laboratories Cambridge"),
			    ki18n("Original VNC viewer and protocol design"));
	aboutData.addCredit(ki18n("Const Kaplinsky"),
			    ki18n("TightVNC encoding"));
	aboutData.addCredit(ki18n("Tridia Corporation"),
			    ki18n("ZLib encoding"));
	KCmdLineArgs::init( argc, argv, &aboutData );

	KCmdLineOptions options;
	options.add("f");
	options.add("fullscreen", ki18n("Start in fullscreen mode"));
	options.add("w");
	options.add("window", ki18n("Start in regular window"));
	options.add("l");
	options.add("low-quality", ki18n("Low quality mode (Tight Encoding, 8 bit color)"));
	options.add("m");
	options.add("medium-quality", ki18n("Medium quality mode (Tight Encoding, lossy)"));
	options.add("h");
	options.add("high-quality", ki18n("High quality mode, default (Hextile Encoding)"));
	options.add("s");
	options.add("scale", ki18n("Start VNC in scaled mode"));
	options.add("c");
	options.add("local-cursor", ki18n("Show local cursor (VNC only)"));
	options.add("e");
	options.add("encodings ", ki18n("Override VNC encoding list (e.g. 'hextile raw')"));
	options.add("p");
	options.add("password-file ", ki18n("Provide the password in a file"));
	options.add("+[host]", ki18n("The name of the host, e.g. 'localhost:1'"));
	KCmdLineArgs::addCmdLineOptions( options );

	KApplication a;

	QString host;
	KRemoteView::Quality quality = KRemoteView::Unknown;
	QString encodings;
	QString password;
	QString resolution;
	QString keymap;
	WindowMode wm = WINDOW_MODE_AUTO;
	bool scale = false;
	bool localCursor = KGlobal::config()->readEntry("alwaysShowLocalCursor", false);
	QSize initialWindowSize;
	QString caption;

	KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

	if (args->isSet("low-quality"))
		quality = KRemoteView::Low;
	else if (args->isSet("medium-quality"))
		quality = KRemoteView::Medium;
	else if (args->isSet("high-quality"))
		quality = KRemoteView::High;

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
			KMessageBox::error(0, i18n("The password file '%1' does not exist.", passwordFile));
			return 1;
		}
		password = QTextStream(&f).readLine();
		f.close();
	}

	if (args->count() > 0)
		host = args->arg(0);
 	args->clear();

	QString is;
	args = KCmdLineArgs::parsedArgs("kde");
	if( args)
	{
	if (args->isSet("geometry"))
	        is = args->getOption("geometry");
	if (!is.isNull()) {
	        QRegExp re("([0-9]+)[xX]([0-9]+)");
		if (!re.exactMatch(is))
		        args->usage(i18n("Wrong geometry format, must be widthXheight"));
		initialWindowSize = QSize(re.cap(1).toInt(), re.cap(2).toInt());
	}

	if (args->isSet("caption"))
	  caption = args->getOption("caption");
	args->clear();
        }
	MainController mc(&a, wm, host, quality, encodings, password,
			  scale, localCursor, initialWindowSize, caption);
	return mc.main();
}

MainController::MainController(KApplication *app, WindowMode wm,
			       const QString &host,
			       KRemoteView::Quality quality,
			       const QString &encodings,
			       const QString &password,
			       bool scale,
			       bool localCursor,
			       QSize initialWindowSize,
			       QString &caption) :
        m_windowMode(wm),
	m_host(host),
        m_encodings(encodings),
        m_password(password),
	m_scale(scale),
	m_localCursor(localCursor),
	m_initialWindowSize(initialWindowSize),
	m_quality(quality),
	m_caption(caption),
	m_app(app) {
}

MainController::~MainController() {
	delete wallet; wallet = 0;
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
			  m_scale, m_localCursor, m_initialWindowSize,
			  m_caption);

	QObject::connect(m_krdc, SIGNAL(disconnected()),
			 m_app, SLOT(quit()));
	connect(m_krdc, SIGNAL(disconnectedError()),
		SLOT(errorRestartRequested()));

	return m_krdc->start();
}

void MainController::errorRestart() {
	if (!m_host.isEmpty())
		KRDC::setLastHost(m_host);
	m_host.clear(); // only auto-connect once

	m_krdc = 0;

	if (!start())
		m_app->quit();
}

#include "main.moc"
