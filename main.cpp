/***************************************************************************
                           main.cpp  -  main control
                             -------------------
    begin                : Thu Dec 20 15:11:42 CET 2001
    copyright            : (C) 2001-2002 by Tim Jansen
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
#include <kdebug.h>
#include <qwindowdefs.h>
#include <qtimer.h>

#include "main.h" 


#define VERSION "0.1"

static const char *description = I18N_NOOP("Remote Desktop Connection");
	

static KCmdLineOptions options[] =
{
	{ "f", 0, 0 },
	{ "fullscreen", I18N_NOOP("Start in fullscreen mode."), 0 }, 
	{ "w", 0, 0 },
	{ "window", I18N_NOOP("Start in regular window."), 0 }, 
	{ "l", 0, 0 },
	{ "low-quality", I18N_NOOP("Low quality mode (Tight Encoding, 8 bit color)."), 0 }, 
	{ "m", 0, 0 },
	{ "medium-quality", I18N_NOOP("Medium quality mode (Tight Encoding, lossy)."), 0 }, 
	{ "h", 0, 0 },
	{ "high-quality", I18N_NOOP("High quality mode, default (Hextile Encoding)."), 0 }, 
	{ "e", 0, 0 },
	{ "encodings ", I18N_NOOP("Override encoding list (e.g. 'hextile raw')."), 0 }, 
	{ "+[host]", I18N_NOOP("The name of the host, e.g. 'localhost:1'."), 0 },
	{ 0, 0, 0 }
};


int main(int argc, char *argv[])
{
	KAboutData aboutData( "krdc", I18N_NOOP("Remote Desktop Connection"),
			      VERSION, description, KAboutData::License_GPL,
		"(c) 2002, Tim Jansen"
		"(c) 2000-2001, Const Kaplinsky\n"
		"(c) 2000, Tridia Corporation\n"
		"(c) 1999, AT&T Laboratories Cambridge\n", 0, 0, 
			      "tim@tjansen.de");
	aboutData.addAuthor("Tim Jansen",0, "tim@tjansen.de");
	aboutData.addCredit("AT&T Laboratories Cambridge",
			    I18N_NOOP("original VNC viewer and protocol design"));
	aboutData.addCredit("Const Kaplinsky",
			    I18N_NOOP("TightVNC encoding"));
	aboutData.addCredit("Tridia Corporation",
			    I18N_NOOP("ZLib encoding"));
	KCmdLineArgs::init( argc, argv, &aboutData );
	KCmdLineArgs::addCmdLineOptions( options ); 

	KApplication a;

	Quality quality = QUALITY_MEDIUM;
	QString encodings = QString::null;
	WindowMode wm = WINDOW_MODE_AUTO;

	KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

	KRDC *krdc;

	if (args->isSet("low-quality"))
		quality = QUALITY_LOW;
	else if (args->isSet("medium-quality"))
		quality = QUALITY_MEDIUM;

	if (args->isSet("fullscreen"))
		wm = WINDOW_MODE_FULLSCREEN;
	else if (args->isSet("window"))
		wm = WINDOW_MODE_NORMAL;

	if (args->isSet("encodings"))
		encodings = args->getOption("encodings");

	if (args->count() > 0) {
		QString host = args->arg(0);
		krdc = new KRDC(wm, host, quality, encodings);
		a.setMainWidget(krdc);
		QObject::connect(krdc, SIGNAL(disconnected()), 
				 &a, SLOT(quit()));
		QObject::connect(krdc, SIGNAL(disconnectedError()), 
				 &a, SLOT(quit()));

		if (!krdc->start(false))
			return 0;
		
		return a.exec();
	}

	MainController mc(&a, wm, encodings);
	return mc.main();
}

MainController::MainController(KApplication *app, WindowMode wm,
			       const QString &encodings) :
	m_krdc(0),
        m_windowMode(wm),
        m_encodings(encodings),
	m_app(app) {
}

MainController::~MainController() {
	if (m_krdc)
		delete m_krdc;
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
	m_krdc = new KRDC(m_windowMode, QString::null, 
			  QUALITY_UNKNOWN, m_encodings);
	m_app->setMainWidget(m_krdc);

	QObject::connect(m_krdc, SIGNAL(disconnected()), 
			 m_app, SLOT(quit()));
	connect(m_krdc, SIGNAL(disconnectedError()),
		SLOT(errorRestartRequested()));

	return m_krdc->start(true);
}

void MainController::errorRestart() {
	m_app->setMainWidget(0);

	if (m_krdc)
		delete m_krdc;

	if (!start()) 
		m_app->quit();
}

#include "main.moc"
