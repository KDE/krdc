/***************************************************************************
                          main.h  -  controller object
                             -------------------
    begin                : Sat Jun 15 02:12:00 CET 2002
    copyright            : (C) 2002 by Tim Jansen
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

#ifndef MAIN_H
#define MAIN_H

#include <qobject.h>
#include "krdc.h"
#include "kremoteview.h"
#include "smartptr.h"

class KApplication;

class MainController : public QObject {
	Q_OBJECT
private:
	SmartPtr<KRDC> m_krdc;
	WindowMode m_windowMode;
	QString m_host, m_encodings, m_password, m_resolution;
	bool m_scale;
	bool m_localCursor;
	QSize m_initialWindowSize;
	QString m_keymap;
	Quality m_quality;

	KApplication *m_app;

public:
	MainController(KApplication *app, WindowMode wm,
		       const QString &host,
		       Quality quality,
		       const QString &encodings,
		       const QString &password,
		       bool scale,
		       bool localCursor,
		       QSize initialWindowSize);
	~MainController();
	int main();
	bool start();

private slots:
	void errorRestartRequested();
	void errorRestart();
};

#endif
