/***************************************************************************
                    keycapturedialog2.h - KeyCaptureDialog
                             -------------------
    begin                : Wed Dec 25 01:16:23 CET 2002
    copyright            : (C) 2002-2003 by Tim Jansen
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

#ifndef KEYCAPTUREDIALOG2_H
#define KEYCAPTUREDIALOG2_H

#include "keycapturedialog.h"

#include <kshortcut.h>
#include <kkeynative.h>


class KeyCaptureDialog2 : public KeyCaptureDialog {
	Q_OBJECT
public:
	KeyCaptureDialog2(QWidget *w = 0, 
			  const char *name = 0, 
			  bool modal = true);
	~KeyCaptureDialog2();

protected:
	bool x11Event(XEvent*);
	void x11EventKeyPress(XEvent*);

public slots:
	void execute();

signals:
	void keyPressed(XEvent *key);
};


#endif
