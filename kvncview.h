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
#include <qclipboard.h>
#include <qwidget.h>
#include <qcursor.h>
#include <qmap.h>

#include "threadsafeeventreceiver.h"
#include "pointerlatencyometer.h"
#include "threads.h"
#include "vnctypes.h"

enum DotCursorState {
	DOT_CURSOR_ON,
	DOT_CURSOR_OFF, 
	DOT_CURSOR_AUTO 
};

class KVncView : public QWidget, public ThreadSafeEventReceiver
{
	Q_OBJECT 
private:
	ControllerThread m_cthread;
	WriterThread m_wthread;
	volatile bool m_quitFlag; // if set: all threads should die ASAP
	enum RemoteViewStatus m_status;

	QSize m_framebufferSize;
	bool m_scaling;
	bool m_remoteMouseTracking;
	
	int m_buttonMask;
        QMap<unsigned int,bool> m_mods;

	QString m_host;
	int m_port;

	QClipboard *m_cb;
	bool m_dontSendCb;
	QCursor m_cursor;
	bool m_cursorOn;
	PointerLatencyOMeter m_plom;

	void setDefaultAppData();
	void mouseEvent(QMouseEvent*);
	unsigned long toKeySym(QKeyEvent *k);
	bool checkLocalKRfb();
	void paintMessage(const QString &msg);
	void showDotCursor(bool show);
        void unpressModifiers();
        
protected:
	void paintEvent(QPaintEvent*);
	void customEvent(QCustomEvent*);
	void mousePressEvent(QMouseEvent*);
	void mouseDoubleClickEvent(QMouseEvent*);
	void mouseReleaseEvent(QMouseEvent*);
	void mouseMoveEvent(QMouseEvent*);
	void wheelEvent(QWheelEvent *);
	void focusOutEvent(QFocusEvent *);
	bool x11Event(XEvent*);

public:
	KVncView(QWidget* parent=0, const char *name=0,
		 const QString &host = QString(""), int port = 5900,
		 const QString &password = QString::null, 
		 AppData *data = 0);
	~KVncView();
	QSize sizeHint();
	int heightForWidth (int w) const;
	void drawRegion(int x, int y, int w, int h);
	bool scaling();
	QSize framebufferSize();
	void setRemoteMouseTracking(bool s);
	bool remoteMouseTracking();

	void startQuitting();
	bool isQuitting();
	void disableCursor();
	QString host();
	int port();
	bool start();
	enum RemoteViewStatus status();

public slots:
        void enableScaling(bool s);

private slots:
	void selectionChanged();

signals:
	void changeSize(int x, int y);
	void connected();
	void disconnected();
	void disconnectedError();
	void statusChanged(RemoteViewStatus s);
	void showingPasswordDialog(bool b);
	void mouseStateChanged(int x, int y, int buttonMask);
};

#endif
