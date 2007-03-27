/***************************************************************************
                            krdc.h  -  main window
                              -------------------
    begin                : Tue May 13 23:10:42 CET 2002
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

#ifndef KRDC_H
#define KRDC_H

#include <kprogressdialog.h>
#include <QSize>
#include <QRect>
#include <QTimer>
#include <QDesktopWidget>
#include <QPixmap>
#include <QEvent>
#include <QVBoxLayout>
#include <QMainWindow>
#include <QMouseEvent>
#include <QScrollArea>

#include "vnc/kvncview.h"
#include "rdp/krdpview.h"
#include "kfullscreenpanel.h"
#include "vidmode.h"
#include "smartptr.h"
#include "keycapturedialog.h"

class KActionCollection;
class KActionMenu;
class KToolBar;


enum WindowMode {
	WINDOW_MODE_AUTO,
	WINDOW_MODE_NORMAL,
	WINDOW_MODE_FULLSCREEN
};

// known protocols
enum Protocol {
	PROTOCOL_AUTO,
	PROTOCOL_VNC,
	PROTOCOL_RDP
};

// Overloaded QScrollArea, to let mouse move events through to remote widget
class RemoteScrollArea : public QScrollArea {
public:
	RemoteScrollArea(QWidget *parent);
protected:
	virtual void mouseMoveEvent(QMouseEvent *e);
};


class KRDC : public QMainWindow
{
	Q_OBJECT
private:
	SmartPtr<KProgressDialog> m_progressDialog; // dialog, displayed while connecting
	QVBoxLayout *m_layout;     // the layout for autosizing the scrollview
	RemoteScrollArea *m_scrollView; // scrollview that contains the remote widget
	KRemoteView *m_view;                  // the remote widget (e.g. KVncView)

	SmartPtr<KeyCaptureDialog> m_keyCaptureDialog; // dialog for key capturing
	KFullscreenPanel *m_fsToolbar;     // toolbar for fullscreen (0 in normal mode)
	QWidget *m_fsToolbarWidget;        // qt designer widget for fs toolbar
                                           //     (invalid in normal mode)
	QPixmap m_pinup, m_pindown;        // fs toolbar imaged for autohide button
	QToolBar *m_toolbar;               // toolbar in normal mode (0 in fs mode)
	KActionMenu *m_popup;               // advanced options popup (0 in fs mode)
	QDesktopWidget m_desktopWidget;

	static const int TOOLBAR_AUTOHIDE_TIMEOUT;
	bool m_ftAutoHide; // if true auto hide in fs is activated
	QTimer m_autoHideTimer; // timer for autohide

	QTimer m_bumpScrollTimer; // timer for bump scrolling (in fs, when res too large)

	bool m_showProgress; // can disable showing the progress dialog temporary
	QString m_host;      // host string as given from user
	Protocol m_protocol; // the used protocol
	Quality m_quality;   // current quality setting
	QString m_encodings; // string containing the encodings, space separated,
	                     // used for config before connection
	QString m_password;  // if not null, contains the password to use
	QString m_resolution;// contains an alternative resolution
	QString m_keymap;    // keymap on the terminal server

	WindowMode m_isFullscreen;    // fs/normal state
	Resolution m_oldResolution;   // conatins encoded res before fs
	bool m_fullscreenMinimized;   // true if window is currently minimized from fs
	QSize m_fullscreenResolution; // xvidmode size (valid only in fs)
	bool m_windowScaling;         // used in startup and fullscreen to determine
	                              // whether scaling should be enabled in norm mode.
	                              // The current state is m_view->scaled().
	bool m_localCursor;           // show local cursor no matter what
	QSize m_initialWindowSize;    // initial window size (windowed mode only),
	                              // invalid after first use
	static QString m_lastHost; // remembers last value of host input
	KActionCollection* m_actionCollection; // used to hold all our KActions for toolbars and menus
	QString m_caption;             // window caption - normally a command line argument 

	bool parseHost(QString &s, Protocol &prot, QString &serverHost, int &serverPort,
	               QString &userName, QString &password);

	void repositionView(bool fullscreen);

	void showProgressDialog();
	void hideProgressDialog();

	static const int TOOLBAR_FPS_1000;
	static const int TOOLBAR_SPEED_DOWN;
	static const int TOOLBAR_SPEED_UP;
	void fsToolbarScheduleHidden();
	KActionMenu *createActionMenu(QWidget *parent) const;

protected:
	virtual void mouseMoveEvent(QMouseEvent *e);
	virtual bool event(QEvent *e);
	virtual bool eventFilter(QObject *watched, QEvent *e);
	virtual QSize sizeHint() const;

public:
	KRDC(WindowMode wm = WINDOW_MODE_AUTO,
	     const QString &host = QString::null,
	     Quality q = QUALITY_UNKNOWN,
	     const QString &encodings = QString::null,
	     const QString &password = QString::null,
	     bool scale = false,
	     bool localCursor = false,
	     QSize initialWindowSize = QSize(),
	     const QString &caption = QString::null );
	~KRDC();

	bool start();

	static void setLastHost(const QString &host);

private slots:
	void changeProgress(RemoteViewStatus s);
	void showingPasswordDialog(bool b);
	void showProgressTimeout();

	void setSize(int w, int h);
	void iconify();
	void toolbarChanged();
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
	void switchToFullscreen(bool scaling = false);
	void viewOnlyToggled();
	void showLocalCursorToggled();

signals:
	void disconnected();
	void disconnectedError();
};

#endif
