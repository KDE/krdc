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
#include <qpixmap.h>
#include "kvncview.h"
#include "kfullscreenpanel.h"

enum WindowMode {
	WINDOW_MODE_AUTO,
	WINDOW_MODE_NORMAL,
	WINDOW_MODE_FULLSCREEN
};

// Overloaded QScrollView, to let mouse move events through to vnc widget
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
	QVBoxLayout *m_layout;     // the layout for autosizing the scrollview
	QScrollView *m_scrollView; // scrollview that contains the vnc widget
	KProgressDialog *m_progressDialog; // dialog, displayed while connecting
	KProgress *m_progress;             // progress bar for the dialog
	KVncView *m_view;                  // the vnc widget

	KFullscreenPanel *m_fsToolbar;     // toolbar for fullscreen (0 in normal mode)
	QWidget *m_fsToolbarWidget;        // qt designer widget for fs toolbar 
                                           //     (invalid in normal mode)
	QPixmap m_pinup, m_pindown;        // fs toolbar imaged for autohide button
	QWidget *m_toolbar;                // toolbar in normal mode (0 in fs mode)

	static const int TOOLBAR_AUTOHIDE_TIMEOUT = 2000;
	bool m_ftAutoHide; // if true auto hide in fs is activated
	QTimer m_autoHideTimer; // timer for autohide

	QTimer m_bumpScrollTimer; // timer for bump scrolling (in fs, when res too large)
	
	bool m_showProgress; // can disable showing the progress dialog temporary
	QString m_host;      // host string as given from user
	Quality m_quality;   // current quality setting
	QString m_encodings; // string containing the encodings, space separated,
	                     // used for config before connection
	AppData m_appData;   // various config stuff, used before connection

	WindowMode m_isFullscreen;    // fs/normal state
	unsigned int m_oldResolution; // conatins encoded res before fs
	bool m_fullscreenMinimized;   // true if minimized from fs
	QSize m_fullscreenResolution; // xvidmode size (valid only in fs)
	QRect m_oldWindowGeometry;    // geometry before switching to fullscreen
	bool m_wasScaling;            // whether scaling was enabled in norm mode

	static QString m_lastHost; // remembers last value of host input
	static int m_lastQuality;  // remembers last quality selection

	void configureApp(Quality q);
	bool parseHost(QString &s, QString &serverHost, int &serverPort,
		       QString &userName, QString &password);

	void switchToFullscreen();
	void repositionView(bool fullscreen);

	void showProgressDialog();
	void hideProgressDialog();

	static const int TOOLBAR_FPS_1000 = 10000;
	static const int TOOLBAR_SPEED_DOWN = 34;
	static const int TOOLBAR_SPEED_UP = 20;
	void fsToolbarScheduleHidden();

protected:
	void mouseMoveEvent(QMouseEvent *e);
	bool event(QEvent *e);
	bool eventFilter(QObject *watched, QEvent *e);

public:
	KRDC(WindowMode wm = WINDOW_MODE_AUTO, 
	     const QString &host = QString::null, 
	     Quality q = QUALITY_UNKNOWN,
	     const QString &encodings = QString::null);
	~KRDC();
	bool start(bool onlyFailOnCancel);

private slots:
	void changeProgress(RemoteViewStatus s);
	void showingPasswordDialog(bool b);
	void showProgressTimeout();

	void setSize(int w, int h);
	void iconify();

	void bumpScroll();

	void toggleFsToolbarAutoHide();
	void setFsToolbarAutoHide(bool on);
	void showFullscreenToolbar();
	void hideFullscreenToolbarDelayed();
	void hideFullscreenToolbarNow();
	
public slots:
	void quit();
	void enableFullscreen(bool full = false);
	void switchToNormal(bool scaling = false);

signals:
        void disconnected(); 
	void disconnectedError();
};

#endif
