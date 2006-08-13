/*
   Copyright (C) 2002-2003 Tim Jansen <tim@tjansen.de>
   Copyright (C) 2003 Nadeem Hasan <nhasan@kde.org>
*/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "krdc.h"
#include "maindialog.h"
#include "hostpreferences.h"

#include <kapplication.h>
#include <kconfig.h>
#include <kaction.h>
#include <kactioncollection.h>
#include <kactionmenu.h>
#include <kdebug.h>
#include <kcombobox.h>
#include <kurl.h>
#include <kiconloader.h>
#include <klocale.h>
#include <ktoolbar.h>
#include <kmenu.h>
#include <kmessagebox.h>
#include <kwin.h>
#include <kstartupinfo.h>

#include <Q3DockArea>
#include <QLabel>

//Added by qt3to4:
#include <QEvent>
#include <Q3Frame>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <Q3PopupMenu>
#include <QMouseEvent>
#include <QX11Info>
#include <kicon.h>

#define BUMP_SCROLL_CONSTANT (200)

const int KRDC::TOOLBAR_AUTOHIDE_TIMEOUT = 1000;
const int KRDC::TOOLBAR_FPS_1000 = 10000;
const int KRDC::TOOLBAR_SPEED_DOWN = 34;
const int KRDC::TOOLBAR_SPEED_UP = 20;

QScrollView2::QScrollView2(QWidget *w, const char *name) :
	Q3ScrollView(w, name) {
	setMouseTracking(true);
        viewport()->setMouseTracking(true);
        horizontalScrollBar()->setMouseTracking(true);
        verticalScrollBar()->setMouseTracking(true);
}

void QScrollView2::mouseMoveEvent( QMouseEvent *e )
{
         e->ignore();
}


QString KRDC::m_lastHost = "";

KRDC::KRDC(WindowMode wm, const QString &host,
	   Quality q, const QString &encodings,
	   const QString &password,
	   bool scale,
	   bool localCursor,
	   QSize initialWindowSize,
	   const QString &caption ) :
  QWidget(0, Qt::WStyle_ContextHelp),
  m_layout(0),
  m_scrollView(0),
  m_view(0),
  m_fsToolbar(0),
  m_toolbar(0),
  m_dockArea(0),
  m_popup(0),
  m_ftAutoHide(false),
  m_showProgress(false),
  m_host(host),
  m_protocol(PROTOCOL_AUTO),
  m_quality(q),
  m_encodings(encodings),
  m_password(password),
  m_isFullscreen(wm),
  m_oldResolution(),
  m_fullscreenMinimized(false),
  m_windowScaling(scale),
  m_localCursor(localCursor),
  m_initialWindowSize(initialWindowSize),
  m_actionCollection(new KActionCollection(this)),
  m_caption(caption)
{
	connect(&m_autoHideTimer, SIGNAL(timeout()), SLOT(hideFullscreenToolbarNow()));
	connect(&m_bumpScrollTimer, SIGNAL(timeout()), SLOT(bumpScroll()));

	m_autoHideTimer.setSingleShot(true);
	m_bumpScrollTimer.setSingleShot(true);

	m_pindown = UserIcon("pindown");
	m_pinup   = UserIcon("pinup");

	// m_keyCaptureDialog = new KeyCaptureDialog(0, 0);

	setMouseTracking(true);

	KStartupInfo::appStarted();
}

bool KRDC::start()
{
	QString userName, password;
	QString serverHost;
	int serverPort = 5900;

	if (!m_host.isEmpty() &&
	    (m_host != "vnc:/") &&
	    (m_host != "rdp:/")) {
		if (m_host.startsWith("vnc:/"))
			m_protocol = PROTOCOL_VNC;
		if (m_host.startsWith("rdp:/"))
			m_protocol = PROTOCOL_RDP;
		if (!parseHost(m_host, m_protocol, serverHost, serverPort,
			       userName, password)) {
			KMessageBox::error(0,
			   i18n("The entered host does not have the required form."),
			   i18n("Malformed URL or Host"));
			emit disconnectedError();
			return true;
		}
	} else {

		MainDialog mainDlg(this, "MainDialog");
		mainDlg.setRemoteHost(m_lastHost);

		if (mainDlg.exec() == QDialog::Rejected) {
			return false;
		}

		QString m_host = mainDlg.remoteHost();
		m_lastHost = m_host;
		if (m_host.startsWith("vnc:/"))
			m_protocol = PROTOCOL_VNC;
		if (m_host.startsWith("rdp:/"))
			m_protocol = PROTOCOL_RDP;
		if (!parseHost(m_host, m_protocol, serverHost, serverPort,
			       userName, password)) {
			KMessageBox::error(0,
					i18n("The entered host does not have the required form."),
					i18n("Malformed URL or Host"));
			emit disconnectedError();
			return true;
		}
	}

	setWindowTitle(i18n("%1 - Remote Desktop Connection", serverHost));

	m_scrollView = new QScrollView2(this, "remote scrollview");
	m_scrollView->setFrameStyle(Q3Frame::NoFrame);
	m_scrollView->setSizePolicy(QSizePolicy(QSizePolicy::Expanding,
					QSizePolicy::Expanding));

	switch(m_protocol)
	{
		case PROTOCOL_AUTO:
			// fall through

		case PROTOCOL_VNC:
			m_view = new KVncView(this, 0, serverHost, serverPort,
			                      m_password.isEmpty() ? password : m_password,
			                      m_quality,
			                      m_localCursor ? DOT_CURSOR_ON : DOT_CURSOR_AUTO,
			                      m_encodings,
					      m_caption);
			break;

		case PROTOCOL_RDP:
			m_view = new KRdpView(this, 0, serverHost, serverPort,
			                      userName, m_password.isEmpty() ? password : m_password,
					      RDP_LOGON_NORMAL, // flags
					      QString::null, // domain
					      QString::null, // shell
					      QString::null, // directory
					      m_caption);
			break;
	}

	m_view->setViewOnly(KGlobal::config()->readEntry("viewOnly", false));

	m_scrollView->addChild(m_view);
	m_view->setWhatsThis( i18n("Here you can see the remote desktop. If the other side allows you to control it, you can also move the mouse, click or enter keystrokes. If the content does not fit your screen, click on the toolbar's full screen button or scale button. To end the connection, just close the window."));

	connect(m_view, SIGNAL(changeSize(int,int)), SLOT(setSize(int,int)));
	connect(m_view, SIGNAL(connected()), SLOT(show()));
	connect(m_view, SIGNAL(disconnected()), SIGNAL(disconnected()));
	// note that the disconnectedError() will be disconnected when kremoteview
	// is completely initialized
	connect(m_view, SIGNAL(disconnectedError()), SIGNAL(disconnectedError()));
	connect(m_view, SIGNAL(statusChanged(RemoteViewStatus)),
		SLOT(changeProgress(RemoteViewStatus)));
	connect(m_view, SIGNAL(showingPasswordDialog(bool)),
		SLOT(showingPasswordDialog(bool)));
	connect(m_keyCaptureDialog, SIGNAL(keyPressed(XEvent*)),
		m_view, SLOT(pressKey(XEvent*)));
	return m_view->start();
}

void KRDC::changeProgress(RemoteViewStatus s) {
	if (!m_progressDialog) {
		m_progressDialog = new KProgressDialog();
		m_progressDialog->showCancelButton(true);
		m_progressDialog->progressBar()->setFormat(QString());
		m_progressDialog->progressBar()->setRange(0, 3);
		connect(m_progressDialog, SIGNAL(cancelClicked()),
			SIGNAL(disconnectedError()));
	}

	if (s == REMOTE_VIEW_CONNECTING) {
		m_progressDialog->setLabel(i18n("Establishing connection..."));
		showProgressDialog();
		m_progressDialog->progressBar()->setValue(0);
	}
	else if (s == REMOTE_VIEW_AUTHENTICATING) {
		m_progressDialog->setLabel(i18n("Authenticating..."));
		m_progressDialog->progressBar()->setValue(1);
	}
	else if (s == REMOTE_VIEW_PREPARING) {
		m_progressDialog->setLabel(i18n("Preparing desktop..."));
		m_progressDialog->progressBar()->setValue(2);
	}
	else if ((s == REMOTE_VIEW_CONNECTED) ||
		 (s == REMOTE_VIEW_DISCONNECTED)) {
	        m_progressDialog->progressBar()->setValue(3);
		hideProgressDialog();
		if (s == REMOTE_VIEW_CONNECTED) {
			QObject::disconnect(m_view, SIGNAL(disconnectedError()),
					    this, SIGNAL(disconnectedError()));
			connect(m_view, SIGNAL(disconnectedError()),
				SIGNAL(disconnected()));
		}
		else if (m_isFullscreen == WINDOW_MODE_FULLSCREEN)
			switchToNormal(m_view->scaling());
	}
}

void KRDC::showingPasswordDialog(bool b) {
	if (!m_progressDialog)
		return;
	if (b)
		hideProgressDialog();
	else
		showProgressDialog();
}

void KRDC::showProgressDialog() {
	m_showProgress = true;
	QTimer::singleShot(400, this, SLOT(showProgressTimeout()));
}

void KRDC::hideProgressDialog() {
	m_showProgress = false;
	m_progressDialog->hide();
}

void KRDC::showProgressTimeout() {
	if (!m_showProgress)
		return;

	m_progressDialog->setMinimumSize(300, 50);
	m_progressDialog->show();
}

void KRDC::quit() {
	m_view->releaseKeyboard();
	hide();
	vidmodeNormalSwitch(QX11Info::display(), m_oldResolution);
	if (m_view)
		m_view->startQuitting();
	emit disconnected();
}

bool KRDC::parseHost(QString &str, Protocol &prot, QString &serverHost, int &serverPort,
		     QString &userName, QString &password) {
	QString s = str;
	userName = QString();
	password = QString();

	if (prot == PROTOCOL_AUTO) {
		if(s.startsWith("smb://")>0) {  //we know it's more likely to be windows..
			s = "rdp://" + s.section("smb://", 1);
			prot = PROTOCOL_RDP;
		} else if(s.contains("://") > 0) {
			s = s.section("://",1);
		} else if(s.contains(":/") > 0) {
			s = s.section(":/", 1);
		}
	}
	if (prot == PROTOCOL_AUTO || prot == PROTOCOL_VNC) {
		if (s.startsWith(":"))
			s = "localhost" + s;
		if (!s.startsWith("vnc:/"))
			s = "vnc://" + s;
		else if (!s.startsWith("vnc://")) // TODO: fix this in KUrl!
			s.insert(4, '/');
	}
	if (prot == PROTOCOL_RDP) {
		if (!s.startsWith("rdp:/"))
			s = "rdp://" + s;
		else if (!s.startsWith("rdp://")) // TODO: fix this in KUrl!
			s.insert(4, '/');
	}

	KUrl url(s);
	if (!url.isValid())
		return false;
	serverHost = url.host();
	if (serverHost.isEmpty())
		serverHost = "localhost";
	serverPort = url.port();
	if ((prot == PROTOCOL_AUTO || prot == PROTOCOL_VNC) && serverPort < 100)
		serverPort += 5900;
	if (url.hasUser())
		userName = url.user();
	if (url.hasPass())
		password = url.pass();

	if (url.port()) {
		if (url.hasUser())
			str = QString("%1@%2:%3").arg(userName).arg(serverHost).arg(url.port());
		else
			str = QString("%1:%2").arg(serverHost).arg(url.port());
	}
	else {
		if (url.hasUser())
			str = QString("%1@%2").arg(userName).arg(serverHost);
		else
			str = QString("%1").arg(serverHost);
	}
	return true;
}

KRDC::~KRDC()
{
	// kill explicitly to avoid xlib calls by the threads after closing the window!
	if (m_view)
		delete m_view;
}

void KRDC::enableFullscreen(bool on)
{
	if (on) {
		if (m_isFullscreen != WINDOW_MODE_FULLSCREEN)
			switchToFullscreen(m_view->scaling());
	}
	else {
		if (m_isFullscreen != WINDOW_MODE_NORMAL)
			switchToNormal(m_view->scaling() || m_windowScaling);
	}
	m_view->switchFullscreen(on);
}

QSize KRDC::sizeHint()
{
	if ((m_isFullscreen != WINDOW_MODE_FULLSCREEN) && m_toolbar) {
		int dockHint = m_dockArea->sizeHint().height();
		dockHint = dockHint < 1 ? 1 : dockHint; // fix wrong size hint
		return QSize(m_view->framebufferSize().width(),
			  dockHint + m_view->framebufferSize().height());
	}
	else
		return m_view->framebufferSize();
}

KActionMenu *KRDC::createActionMenu(QWidget *parent) const
{
	KActionMenu *pu = new KActionMenu(KIcon("configure"), i18n("Advanced"),
					  m_actionCollection, "configure");

	if (m_popup)
	        m_popup->setToolTip(i18n("Advanced options"));

	KAction* action = m_actionCollection->action("popupmenu_view_only");

	if (!action)
	{
		new KAction(i18n("View Only"),
			    m_actionCollection,
			    "popupmenu_view_only");
	} else {
	  	action->setCheckable(true);
		action->setChecked(m_view->viewOnly());
		connect(action, SIGNAL(toggled()), this, SLOT(viewOnlyToggled()));
		pu->addAction(action);
	}

	if (m_view->supportsLocalCursor()) {
		KAction* action = m_actionCollection->action("popupmenu_local_cursor");

		if (!action)
		{
			new KAction(i18n("Always Show Local Cursor"),
						m_actionCollection, "popupmenu_local_cursor");
		} else {
		        action->setCheckable(true);
			action->setChecked(m_view->dotCursorState() == DOT_CURSOR_ON);
			connect(action, SIGNAL(toggled()), this, SLOT(showLocalCursorToggled()));
			pu->addAction(action);
		}
	}
	return pu;
}

void KRDC::switchToFullscreen(bool scaling)
{
	int x, y;

	bool fromFullscreen = (m_isFullscreen == WINDOW_MODE_FULLSCREEN);

	QWidget *desktop = QApplication::desktop();
	QSize ds = desktop->size();
	QSize fbs = m_view->framebufferSize();
	bool scalingPossible = m_view->supportsScaling() &&
	  ((fbs.width() >= ds.width()) || (fbs.height() >= ds.height()));

	if (!fromFullscreen) {
		hide();
		m_oldResolution = vidmodeFullscreenSwitch(QX11Info::display(),
							  m_desktopWidget.screenNumber(this),
							  fbs.width(),
							  fbs.height(),
							  x, y);
		if (m_oldResolution.valid)
			m_fullscreenResolution = QSize(x, y);
		else
			m_fullscreenResolution = QApplication::desktop()->size();
		m_isFullscreen = WINDOW_MODE_FULLSCREEN;
		if (!scalingPossible)
			m_windowScaling = m_view->scaling();
	}

	if (m_toolbar) {
		m_toolbar->hide();
		m_toolbar->deleteLater();
		m_toolbar = 0;
		m_dockArea->hide();
		m_dockArea->deleteLater();
		m_dockArea = 0;
	}
	if (m_popup) {
		m_popup->deleteLater();
		m_popup = 0;
	}
	if (m_fsToolbar) {
		m_fsToolbar->hide();
		m_fsToolbar->deleteLater();
		m_fsToolbar = 0;
	}

	if (m_layout)
		delete m_layout;
	m_layout = new QVBoxLayout(this);
	m_layout->addWidget(m_scrollView);

	if (scalingPossible) {
		m_view->enableScaling(scaling);
		if (scaling)
			m_view->resize(ds);
		else
			m_view->resize(fbs);
		repositionView(true);
	}
	else
		m_view->enableScaling(false);

	m_fsToolbar = new KFullscreenPanel(this, "fstoolbar", m_fullscreenResolution);
	connect(m_fsToolbar, SIGNAL(mouseEnter()), SLOT(showFullscreenToolbar()));
	connect(m_fsToolbar, SIGNAL(mouseLeave()), SLOT(hideFullscreenToolbarDelayed()));

	KToolBar *t = new KToolBar(m_fsToolbar);
	m_fsToolbarWidget = t;

	KIcon pinIconSet;
	pinIconSet.addPixmap(m_pinup, QIcon::Normal, QIcon::On);
	pinIconSet.addPixmap(m_pindown, QIcon::Normal, QIcon::Off);

	KAction* action = new KAction(pinIconSet, i18n("Autohide"), m_actionCollection, "pinup");
	action->setToolTip(i18n("Autohide on/off"));
	action->setCheckable(true);
	connect(action, SIGNAL(toggled(bool)), this, SLOT(setFsToolbarAutoHide(bool)));
	t->addAction(action);

	action = new KAction(KIcon("window_nofullscreen"), i18n("Fullscreen"),
								m_actionCollection, "window_nofullscreen");
	action->setToolTip(i18n("Fullscreen"));
	action->setCheckable(true);
	action->setChecked(true);
	connect(action, SIGNAL(toggled(bool)), this, SLOT(enableFullscreen(bool)));
	t->addAction(action);

	m_popup = createActionMenu(t);
	t->addAction(m_popup);
	//advancedButton->setPopupDelay(0);

	QLabel *hostLabel = new QLabel(t);
	hostLabel->setObjectName("kde toolbar widget");
	hostLabel->setAlignment(Qt::AlignCenter);
	hostLabel->setText("   "+m_host+"   ");
	t->addWidget(hostLabel);

	if (scalingPossible) {
		action = new KAction(KIcon("viewmagfit"), i18n("Scale"), m_actionCollection, "viewmagfit");
		action->setToolTip(i18n("Scale view"));
		action->setCheckable(true);
		action->setChecked(scaling);
		connect(action, SIGNAL(toggled(bool)), this, SLOT(switchToFullscreen(bool)));
	}

	action = new KAction(KIcon("iconify"), i18n("Iconify"), m_actionCollection, "iconify");
	action->setToolTip(i18n("Minimize"));
	action->setCheckable(true);
	connect(action, SIGNAL(toggled(bool)), this, SLOT(iconify()));
	t->addAction(action);

	action = new KAction(KIcon("close"), i18n("Close"), m_actionCollection, "close");
	action->setToolTip(i18n("Close"));
	action->setCheckable(true);
	connect(action, SIGNAL(toggled(bool)), this, SLOT(quit()));
	t->addAction(action);

	m_fsToolbar->setChild(t);

	repositionView(true);
	showFullScreen();

	setMaximumSize(m_fullscreenResolution.width(),
		       m_fullscreenResolution.height());
	setGeometry(0, 0, m_fullscreenResolution.width(),
		    m_fullscreenResolution.height());

	KWin::setState(winId(), NET::StaysOnTop);

	m_ftAutoHide = !m_ftAutoHide;
	setFsToolbarAutoHide(!m_ftAutoHide);

	if (!fromFullscreen) {
		if (m_oldResolution.valid)
			grabInput(QX11Info::display(), winId());
		m_view->grabKeyboard();
	}

  m_view->switchFullscreen( true );
}

void KRDC::switchToNormal(bool scaling)
{
	bool fromFullscreen = (m_isFullscreen == WINDOW_MODE_FULLSCREEN);
	bool scalingChanged = (scaling != m_view->scaling());
	m_windowScaling = false; // delete remembered scaling value
	if (fromFullscreen) {
		KWin::clearState(winId(), NET::StaysOnTop);
		hide();
	}
	m_isFullscreen = WINDOW_MODE_NORMAL;
	m_view->enableScaling(scaling);

	m_view->releaseKeyboard();
	if (m_oldResolution.valid) {
		ungrabInput(QX11Info::display());
		vidmodeNormalSwitch(QX11Info::display(), m_oldResolution);
		m_oldResolution = Resolution();
	}

	if (m_popup) {
		m_popup->deleteLater();
		m_popup = 0;
	}
	if (m_fsToolbar) {
		m_fsToolbar->hide();
		m_fsToolbar->deleteLater();
		m_fsToolbar = 0;
	}

	if (!m_toolbar) {
		m_dockArea = new Q3DockArea(Qt::Horizontal, Q3DockArea::Normal, this);
		m_dockArea->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding,
					QSizePolicy::Fixed));
		KToolBar *t = new KToolBar(m_dockArea);
		m_toolbar = t;
		t->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		connect(t, SIGNAL(placeChanged(Q3DockWindow::Place)), SLOT(toolbarChanged()));

		KAction* action = new KAction(KIcon("window_fullscreen"), i18n("Fullscreen"),
									m_actionCollection, "window_fullscreen");
		action->setToolTip(i18n("Fullscreen"));
		action->setWhatsThis( i18n("Switches to full screen. If the remote desktop has a different screen resolution, Remote Desktop Connection will automatically switch to the nearest resolution."));
		action->setCheckable(true);
		connect(action, SIGNAL(toggled(bool)), this, SLOT(enableFullscreen(bool)));
		t->addAction(action);

		if (m_view->supportsScaling()) {
			KAction* action = new KAction(KIcon("viewmagfit"), i18n("Scale"), m_actionCollection, "viewmagfit");
			action->setToolTip(i18n("Scale View"));
			action->setWhatsThis( i18n("This option scales the remote screen to fit your window size."));
			action->setCheckable(true);
			action->setChecked(scaling);
			connect(action, SIGNAL(toggled(bool)), this, SLOT(switchToNormal(bool)));
			t->addAction(action);
		}

		action = new KAction(KIcon("key_enter"), i18n("Special Keys"), m_actionCollection, "key_enter");
		action->setToolTip(i18n("Enter special keys"));
		action->setWhatsThis( i18n("This option allows you to send special key combinations like Ctrl-Alt-Del to the remote host."));
		action->setCheckable(true);
		connect(action, SIGNAL(toggled(bool)), m_keyCaptureDialog, SLOT(execute(bool)));
		t->addAction(action);

		m_popup = createActionMenu(t);
		t->addAction(m_popup);
		//advancedButton->setPopupDelay(0);

		if (m_layout)
			delete m_layout;
		m_layout = new QVBoxLayout(this);
		m_layout->addWidget(m_dockArea);
		m_layout->addWidget(m_scrollView);
		m_layout->setGeometry(QRect(0, 0, m_scrollView->width(),
		                            m_dockArea->height() + m_scrollView->height()));
	}

	if (scaling) {
		m_scrollView->installEventFilter(this);
		m_view->resize(m_scrollView->size());
	}
	else {
		m_scrollView->removeEventFilter(this);
		m_view->resize(m_view->framebufferSize());
	}

	setMaximumSize(sizeHint());

	repositionView(false);


	if (!fromFullscreen) {
		if (m_initialWindowSize.isValid()) {
			resize(m_initialWindowSize);
			m_initialWindowSize = QSize();
		}
		else if (!scalingChanged)
			resize(sizeHint());
		show();
		if (scalingChanged)
			m_view->update();
	}
	else
		showNormal();
}

void KRDC::viewOnlyToggled() {
	bool s = !m_view->viewOnly();
	m_view->setViewOnly(s);

	KAction* action = m_actionCollection->action("popupmenu_view_only");
	if (!action)
	{
		action->setChecked(s);
	}
}

void KRDC::showLocalCursorToggled() {
	bool s = (m_view->dotCursorState() != DOT_CURSOR_ON);
	m_view->showDotCursor(s ? DOT_CURSOR_ON : DOT_CURSOR_AUTO);

	KAction* action = m_actionCollection->action("popupmenu_local_cursor");
	if (action)
	{
		action->setChecked(s);
	}
}

void KRDC::iconify()
{
	KWin::clearState(winId(), NET::StaysOnTop);

	m_view->releaseKeyboard();
	if (m_oldResolution.valid)
		ungrabInput(QX11Info::display());

	vidmodeNormalSwitch(QX11Info::display(), m_oldResolution);
	m_oldResolution = Resolution();
	showNormal();
	showMinimized();
	m_fullscreenMinimized = true;
}

void KRDC::toolbarChanged() {
	setMaximumSize(sizeHint());

	// resize window when toolbar is docked and it was maximized
	QSize fs = m_view->framebufferSize();
	QSize cs = size();
	QSize cs1(cs.width(), cs.height()-1); // adjusted for QDockArea.height()==1
	if ((fs == cs) || (fs == cs1))
		resize(sizeHint());
}


bool KRDC::event(QEvent *e) {
/* used to change resolution when fullscreen was minimized */
        if ((!m_fullscreenMinimized) || (e->type() != QEvent::WindowActivate))
		return QWidget::event(e);

	m_fullscreenMinimized = false;
	int x, y;
	m_oldResolution = vidmodeFullscreenSwitch(QX11Info::display(),
						  m_desktopWidget.screenNumber(this),
						  m_view->width(),
						  m_view->height(),
						  x, y);
	if (m_oldResolution.valid)
		m_fullscreenResolution = QSize(x, y);
	else
		m_fullscreenResolution = QApplication::desktop()->size();

	showFullScreen();
	setGeometry(0, 0, m_fullscreenResolution.width(),
		    m_fullscreenResolution.height());
	if (m_oldResolution.valid)
		grabInput(QX11Info::display(), winId());
	m_view->switchFullscreen( true );
	KWin::setState(winId(), NET::StaysOnTop);

	return QWidget::event(e);
}

bool KRDC::eventFilter(QObject *watched, QEvent *e) {
/* used to get events from QScrollView  on resize  for scale mode*/
	if (watched != m_scrollView)
		return false;
	if (e->type() != QEvent::Resize)
		return false;

	QResizeEvent *re = (QResizeEvent*) e;
	m_view->resize(re->size());
	return false;
}

void KRDC::setSize(int w, int h)
{
	int dw, dh;

	QWidget *desktop = QApplication::desktop();
	dw = desktop->width();
	dh = desktop->height();

	switch (m_isFullscreen) {
	case WINDOW_MODE_AUTO:
		if ((w > dw) || (h > dh))
			switchToFullscreen(m_windowScaling);
		else
			switchToNormal(m_windowScaling);
		break;
	case WINDOW_MODE_NORMAL:
		switchToNormal(m_windowScaling);
		break;
	case WINDOW_MODE_FULLSCREEN:
		switchToFullscreen(m_windowScaling);
 	        break;
	}
}

void KRDC::repositionView(bool fullscreen) {
	int ox = 0;
	int oy = 0;

	if (!m_scrollView)
		return;

	QSize s = m_view->size();

	if (fullscreen) {
		QSize d = m_fullscreenResolution;
		bool margin = false;
		if (d.width() > s.width())
			ox = (d.width() - s.width()) / 2;
		else if (d.width() < s.width())
			margin = true;

		if (d.height() > s.height())
			oy = (d.height() - s.height()) / 2;
		else if (d.height() < s.height())
			margin = true;

		if (margin)
			m_layout->setMargin(1);
	}

	m_scrollView->moveChild(m_view, ox, oy);
}

void KRDC::toggleFsToolbarAutoHide() {
	setFsToolbarAutoHide(!m_ftAutoHide);
}

void KRDC::setFsToolbarAutoHide(bool on) {

	if (on == m_ftAutoHide)
		return;
	if (m_isFullscreen != WINDOW_MODE_FULLSCREEN)
		return;

	m_ftAutoHide = on;
	if (!on)
		showFullscreenToolbar();
}

void KRDC::hideFullscreenToolbarNow() {
	if (m_fsToolbar && m_ftAutoHide)
		m_fsToolbar->startHide();
}

void KRDC::bumpScroll() {
	int x = QCursor::pos().x();
	int y = QCursor::pos().y();
	QSize s = m_view->size();
	QSize d = m_fullscreenResolution;

	if (d.width() < s.width()) {
		if (x == 0)
			m_scrollView->scrollBy(-BUMP_SCROLL_CONSTANT, 0);
		else if (x == d.width()-1)
			m_scrollView->scrollBy(BUMP_SCROLL_CONSTANT, 0);
	}
	if (d.height() < s.height()) {
		if (y == 0)
			m_scrollView->scrollBy(0, -BUMP_SCROLL_CONSTANT);
		else if (y == d.height()-1)
			m_scrollView->scrollBy(0, BUMP_SCROLL_CONSTANT);
	}

	m_bumpScrollTimer.start(333);
}

void KRDC::showFullscreenToolbar() {
	m_fsToolbar->startShow();
	m_autoHideTimer.stop();
}

void KRDC::hideFullscreenToolbarDelayed() {
	if (!m_autoHideTimer.isActive())
		m_autoHideTimer.start(TOOLBAR_AUTOHIDE_TIMEOUT);
}

void KRDC::mouseMoveEvent(QMouseEvent *e) {
	if (m_isFullscreen != WINDOW_MODE_FULLSCREEN)
		return;

	int x = e->x();
	int y = e->y();

	/* Bump Scrolling */

	QSize s = m_view->size();
	QSize d = m_fullscreenResolution;
	if ((d.width() < s.width()) || d.height() < s.height()) {
 		if ((x == 0) || (x >= d.width()-1) ||
		    (y == 0) || (y >= d.height()-1))
			bumpScroll();
		else
			m_bumpScrollTimer.stop();
	}

	/* Toolbar autohide */

	if (!m_ftAutoHide)
		return;

	int dw = d.width();
	int w = m_fsToolbar->width();
	int x1 = (dw - w)/4;
	int x2 = dw - x1;

	if (((y <= 0) && (x >= x1) && (x <= x2)))
		showFullscreenToolbar();
	else
		hideFullscreenToolbarDelayed();
	e->accept();
}

void KRDC::setLastHost(const QString &lastHost) {
	m_lastHost = lastHost;
}

#include "krdc.moc"
