/***************************************************************************
                  kvncview.h  -  widget that shows the vnc client
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

#ifndef KVNCVIEW_H
#define KVNCVIEW_H


#include <kapplication.h>
#include <qwidget.h>

#include "threads.h"
#include "vnctypes.h"



class KVncView : public QWidget
{
	Q_OBJECT 
private:
	ControllerThread m_cthread;
	WriterThread m_wthread;
	volatile bool m_quitFlag;
	enum RemoteViewStatus m_status;

	int m_buttonMask;

	QString m_host;
	int m_port;

	void setDefaultAppData();
	void mouseEvent(QMouseEvent*);
	unsigned long toKeySym(QKeyEvent *k);

protected:
	void paintEvent(QPaintEvent*);
	void customEvent(QCustomEvent*);
	void mousePressEvent(QMouseEvent*);
	void mouseReleaseEvent(QMouseEvent*);
	void mouseMoveEvent(QMouseEvent*);
	void keyPressEvent(QKeyEvent*);
	void keyReleaseEvent(QKeyEvent*);

public:
	KVncView(QWidget* parent=0, const char *name=0, 
		    const QString &host = QString(""), int port = 5900,
		    AppData *data = 0);
	~KVncView();
	
	void startQuitting();
	bool isQuitting();
	void disableCursor();

	QString host();
	int port();
	void start();
	enum RemoteViewStatus status();

signals:
	void changeSize(int x, int y);
	void connected();
	void disconnected();
	void statusChanged(RemoteViewStatus s);
	void showingPasswordDialog(bool b);
};

#endif
