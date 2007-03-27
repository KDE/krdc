/*
   Copyright (C) 2002-2003 Tim Jansen <tim@tjansen.de>
   Copyright (C) 2004 Nadeem Hasan <nhasan@kde.org>
*/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KEYCAPTUREDIALOG_H
#define KEYCAPTUREDIALOG_H

class KeyCaptureWidget;

#include <kshortcut.h>
// #include <kkeynative.h>
#include <kdialog.h>

class KeyCaptureDialog : public KDialog {
	Q_OBJECT

	bool m_grabbed;
public:
	KeyCaptureDialog(QWidget * parent= 0);
	~KeyCaptureDialog();

public slots:
	void execute();

signals:
	void keyPressed(XEvent *key);

protected:
	bool x11Event(XEvent*);
	void x11EventKeyPress(XEvent*);

	KeyCaptureWidget *m_captureWidget;
};


#endif
