/***************************************************************************
                  kvncview.h  -  widget that shows the vnc client
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

#ifndef KVNCVIEW_H
#define KVNCVIEW_H

#include "kremoteview.h"
#include <kapplication.h>
#include <qclipboard.h>
#include <qcursor.h>

#include "pointerlatencyometer.h"
#include "vnctypes.h"
#include "threads.h"

enum DotCursorState {
	DOT_CURSOR_ON,
	DOT_CURSOR_OFF, 
	DOT_CURSOR_AUTO 
};

class KVncView : public KRemoteView
{
	Q_OBJECT 
private:
	ControllerThread m_cthread;
	WriterThread m_wthread;
	volatile bool m_quitFlag; // if set: all threads should die ASAP
	QMutex m_framebufferLock;
	bool m_enableFramebufferLocking;
	bool m_enableClientCursor;

	QSize m_framebufferSize;
	bool m_scaling;
	bool m_remoteMouseTracking;
	
	int m_buttonMask;

	QString m_host;
	int m_port;

	QClipboard *m_cb;
	bool m_dontSendCb;
	QCursor m_cursor;
	bool m_cursorEnabled;
	PointerLatencyOMeter m_plom;

	void setDefaultAppData();
	void mouseEvent(QMouseEvent*);
	unsigned long toKeySym(QKeyEvent *k);
	bool checkLocalKRfb();
	void paintMessage(const QString &msg);
	void showDotCursor(bool show);

protected:
	void paintEvent(QPaintEvent*);
	void customEvent(QCustomEvent*);
	void mousePressEvent(QMouseEvent*);
	void mouseDoubleClickEvent(QMouseEvent*);
	void mouseReleaseEvent(QMouseEvent*);
	void mouseMoveEvent(QMouseEvent*);
	void wheelEvent(QWheelEvent *);
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
	void lockFramebuffer();
	void unlockFramebuffer();
	void enableClientCursor(bool enable);
	virtual bool scaling();
	virtual bool supportsScaling();
	virtual QSize framebufferSize();
	void setRemoteMouseTracking(bool s);
	bool remoteMouseTracking();

	virtual void startQuitting();
	virtual bool isQuitting();
	void disableCursor();
	virtual QString host();
	virtual int port();
	virtual bool start();

public slots:
        virtual void enableScaling(bool s);
        virtual void pressKey(XEvent *k); 

private slots:
	void selectionChanged();
};

#endif
