/***************************************************************************
                            krdc.h  -  main window
                              -------------------
    begin                : Tue May 13 23:10:42 CET 2002
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

#ifndef KRDC_H
#define KRDC_H

#include <kprogress.h>
#include <qscrollview.h>
#include <qlayout.h> 
#include <qsize.h>
#include <qrect.h>
#include <qtimer.h> 
#include "kvncview.h"

enum WindowMode {
	WINDOW_MODE_AUTO,
	WINDOW_MODE_NORMAL,
	WINDOW_MODE_FULLSCREEN
};

class QScrollView2 : public QScrollView {
public:
	QScrollView2(QWidget *w, const char *name);
protected:
	virtual void mouseMoveEvent(QMouseEvent *e);
};

class KRDC : public QWidget
{
	Q_OBJECT 
private:
	QVBoxLayout *m_layout;
	QScrollView *m_scrollView;
	KProgressDialog *m_progressDialog;
	KProgress *m_progress;
	KVncView *m_view;

	QWidget *m_fsToolbar;
	QWidget *m_toolbar;

	static const int TOOLBAR_AUTOHIDE_TIMEOUT = 2000;
	bool m_ftAutoHide;
	QTimer m_autoHideTimer;

	bool m_showProgress;
	QString m_host;
	Quality m_quality;
	AppData m_appData;

	WindowMode m_isFullscreen;

	void configureApp(Quality q);
	void parseHost(QString &s, QString &serverHost, int &serverPort);

	void switchToFullscreen();
	void switchToNormal();
	void repositionView(bool fullscreen);

	void showProgressDialog();
	void hideProgressDialog();

	static const int TOOLBAR_FPS_1000 = 10000;
	static const int TOOLBAR_SPEED_DOWN = 34;
	static const int TOOLBAR_SPEED_UP = 20;
	QRect getAutoHideToolbarGeometry();
	int getAutoHideToolbarPosition();
	void fsToolbarScheduleHidden();

protected:
	void mouseMoveEvent(QMouseEvent *e);

public:
	KRDC(WindowMode wm = WINDOW_MODE_AUTO, 
	     const QString &host = QString::null, 
	     Quality q = QUALITY_UNKNOWN);
	~KRDC();
	bool start();

private slots:
        void connected(); 
	void changeProgress(RemoteViewStatus s);
	void showingPasswordDialog(bool b);
	void showProgressTimeout();

	void setSize(int w, int h);

	void setFsToolbarAutoHide(bool on);
	void fsToolbarHide();
	
public slots:
	void quit();
	void switchToFullscreen(bool);
 

signals:
        void disconnected(); 
	
};

#endif
