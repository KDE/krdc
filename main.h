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

#include <kapplication.h>
#include <qobject.h>
#include "krdc.h"
#include "kvncview.h"

class MainController : public QObject {
	Q_OBJECT
private:
	KRDC *m_krdc;
	WindowMode m_windowMode;
	QString m_encodings, m_password;

	KApplication *m_app;

public:
	MainController(KApplication *app, WindowMode wm,
		       const QString &encodings,
		       const QString &password);
	~MainController();
	int main();
	bool start();

private slots:
	void errorRestartRequested();
	void errorRestart();
};

#endif
