/***************************************************************************
                          krdc.cpp  -  main window
                             -------------------
    begin                : Tue May 13 23:07:42 CET 2002
    copyright            : (C) 2002-2003 by Tim Jansen
                           (C) 2003 Nadeem Hasan <nhasan@kde.org>
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

#include "krdc.h"
#include "maindialog.h"
#include "hostpreferences.h"

#include <kapplication.h>
#include <kdebug.h>
#include <kcombobox.h>
#include <kurl.h>
#include <kiconloader.h>
#include <klocale.h>
#include <ktoolbar.h>
#include <ktoolbarbutton.h>
#include <kpopupmenu.h>
#include <kmessagebox.h>
#include <kwin.h>
#include <kstartupinfo.h>

#include <qdockarea.h>
#include <qlabel.h>
#include <qwhatsthis.h>
#include <qtooltip.h>

#define BUMP_SCROLL_CONSTANT (200)

const int VIEW_ONLY_ID = 10;

const int FS_AUTOHIDE_ID = 1;
const int FS_FULLSCREEN_ID = 2;
const int FS_SCALE_ID = 3;
const int FS_HOSTLABEL_ID = 4;
const int FS_ADVANCED_ID = 5;
const int FS_ICONIFY_ID = 6;
const int FS_CLOSE_ID = 7;

const int KRDC::TOOLBAR_AUTOHIDE_TIMEOUT = 1000;
const int KRDC::TOOLBAR_FPS_1000 = 10000;
const int KRDC::TOOLBAR_SPEED_DOWN = 34;
const int KRDC::TOOLBAR_SPEED_UP = 20;

QScrollView2::QScrollView2(QWidget *w, const char *name) :
	QScrollView(w, name) {
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
	   QSize initialWindowSize) :
  QWidget(0, 0, Qt::WStyle_ContextHelp),
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
  m_initialWindowSize(initialWindowSize)
{
	connect(&m_autoHideTimer, SIGNAL(timeout()), SLOT(hideFullscreenToolbarNow()));
	connect(&m_bumpScrollTimer, SIGNAL(timeout()), SLOT(bumpScroll()));

	m_pindown = UserIcon("pindown");
	m_pinup   = UserIcon("pinup");

	m_keyCaptureDialog = new KeyCaptureDialog(0, 0);

	setMouseTracking(true);

	KStartupInfo::appStarted();
}

bool KRDC::start()
{
	QString userName, password;
	QString serverHost;
	int serverPort = 5900;

	if (!m_host.isNull() &&
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

	setCaption(i18n("%1 - Remote Desktop Connection").arg(m_host));

	m_scrollView = new QScrollView2(this, "remote scrollview");
	m_scrollView->setFrameStyle(QFrame::NoFrame);
	m_scrollView->setSizePolicy(QSizePolicy(QSizePolicy::Expanding,
					QSizePolicy::Expanding));

	switch(m_protocol)
	{
		case PROTOCOL_AUTO:
			// fall through

		case PROTOCOL_VNC:
			m_view = new KVncView(this, 0, serverHost, serverPort,
			                      m_password.isNull() ? password : m_password,
			                      m_quality, m_encodings);
			break;

		case PROTOCOL_RDP:
			m_view = new KRdpView(this, 0, serverHost, serverPort,
			                      userName, m_password.isNull() ? password : m_password);
			break;
	}

	m_scrollView->addChild(m_view);
	QWhatsThis::add(m_view, i18n("Here you can see the remote desktop. If the other side allows you to control it, you can also move the mouse, click or enter keystrokes. If the content does not fit your screen, click on the toolbar's full screen button or scale button. To end the connection, just close the window."));

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
		m_progressDialog = new KProgressDialog(0, 0, QString::null,
				   "1234567890", false);
		m_progressDialog->showCancelButton(true);
		m_progressDialog->setMinimumDuration(0x7fffffff);//disable effectively
		m_progress = m_progressDialog->progressBar();
		m_progress->setTextEnabled(false);
		m_progress->setTotalSteps(3);
		connect(m_progressDialog, SIGNAL(cancelClicked()),
			SIGNAL(disconnectedError()));
	}

	if (s == REMOTE_VIEW_CONNECTING) {
		m_progress->setValue(0);
		m_progressDialog->setLabel(i18n("Establishing connection..."));
		m_progressDialog->setAllowCancel(false);
		showProgressDialog();
	}
	else if (s == REMOTE_VIEW_AUTHENTICATING) {
		m_progress->setValue(1);
		m_progressDialog->setLabel(i18n("Authenticating..."));
		m_progressDialog->setAllowCancel(true);
	}
	else if (s == REMOTE_VIEW_PREPARING) {
		m_progress->setValue(2);
		m_progressDialog->setLabel(i18n("Preparing desktop..."));
	}
	else if ((s == REMOTE_VIEW_CONNECTED) ||
		 (s == REMOTE_VIEW_DISCONNECTED)) {
		m_progress->setValue(3);
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
	vidmodeNormalSwitch(qt_xdisplay(), m_oldResolution);
	if (m_view)
		m_view->startQuitting();
	emit disconnected();
}

bool KRDC::parseHost(QString &str, Protocol &prot, QString &serverHost, int &serverPort,
		     QString &userName, QString &password) {
	QString s = str;
	userName = QString::null;
	password = QString::null;

	if (prot == PROTOCOL_AUTO || prot == PROTOCOL_VNC) {
		if (s.startsWith(":"))
			s = "localhost" + s;
		if (!s.startsWith("vnc:/"))
			s = "vnc://" + s;
		else if (!s.startsWith("vnc://")) // TODO: fix this in KURL!
			s.insert(4, '/');
	}
	if (prot == PROTOCOL_RDP) {
		if (!s.startsWith("rdp:/"))
			s = "rdp://" + s;
		else if (!s.startsWith("rdp://")) // TODO: fix this in KURL!
			s.insert(4, '/');
	}

	KURL url(s);
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

QPopupMenu *KRDC::createPopupMenu(QWidget *parent) const {
	KPopupMenu *pu = new KPopupMenu(parent);
	pu->insertItem(i18n("View Only"), this, SLOT(viewOnlyToggled()), 0, VIEW_ONLY_ID);
	pu->setCheckable(true);
	pu->setItemChecked(VIEW_ONLY_ID, m_view->viewOnly());
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
		m_oldResolution = vidmodeFullscreenSwitch(qt_xdisplay(),
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

	QIconSet pinIconSet;
	pinIconSet.setPixmap(m_pinup, QIconSet::Automatic, QIconSet::Normal, QIconSet::On);
	pinIconSet.setPixmap(m_pindown, QIconSet::Automatic, QIconSet::Normal, QIconSet::Off);
	t->insertButton("pinup", FS_AUTOHIDE_ID);
	KToolBarButton *pinButton = t->getButton(FS_AUTOHIDE_ID);
	pinButton->setIconSet(pinIconSet);
	QToolTip::add(pinButton, i18n("Autohide on/off"));
	t->setToggle(FS_AUTOHIDE_ID);
	t->setButton(FS_AUTOHIDE_ID, false);
	t->addConnection(FS_AUTOHIDE_ID, SIGNAL(clicked()), this, SLOT(toggleFsToolbarAutoHide()));

	t->insertButton("window_nofullscreen", FS_FULLSCREEN_ID);
	KToolBarButton *fullscreenButton = t->getButton(FS_FULLSCREEN_ID);
	QToolTip::add(fullscreenButton, i18n("Fullscreen"));
	t->setToggle(FS_FULLSCREEN_ID);
	t->setButton(FS_FULLSCREEN_ID, true);
	t->addConnection(FS_FULLSCREEN_ID, SIGNAL(toggled(bool)), this, SLOT(enableFullscreen(bool)));

	m_popup = createPopupMenu(t);
	t->insertButton("configure", FS_ADVANCED_ID, m_popup, true, i18n("Advanced options"));
	KToolBarButton *advancedButton = t->getButton(FS_ADVANCED_ID);
	QToolTip::add(advancedButton, i18n("Advanced options"));
	//advancedButton->setPopupDelay(0);

	QLabel *hostLabel = new QLabel(t);
	hostLabel->setName("kde toolbar widget");
	hostLabel->setAlignment(Qt::AlignCenter);
	hostLabel->setText("   "+m_host+"   ");
	t->insertWidget(FS_HOSTLABEL_ID, 150, hostLabel);
	t->setItemAutoSized(FS_HOSTLABEL_ID, true);

	if (scalingPossible) {
		t->insertButton("viewmagfit", FS_SCALE_ID);
		KToolBarButton *scaleButton = t->getButton(FS_SCALE_ID);
		QToolTip::add(scaleButton, i18n("Scale view"));
		t->setToggle(FS_SCALE_ID);
		t->setButton(FS_SCALE_ID, scaling);
		t->addConnection(FS_SCALE_ID, SIGNAL(toggled(bool)), this, SLOT(switchToFullscreen(bool)));
	}

	t->insertButton("iconify", FS_ICONIFY_ID);
	KToolBarButton *iconifyButton = t->getButton(FS_ICONIFY_ID);
	QToolTip::add(iconifyButton, i18n("Minimize"));
	t->addConnection(FS_ICONIFY_ID, SIGNAL(clicked()), this, SLOT(iconify()));

	t->insertButton("close", FS_CLOSE_ID);
	KToolBarButton *closeButton = t->getButton(FS_CLOSE_ID);
	QToolTip::add(closeButton, i18n("Close"));
	t->addConnection(FS_CLOSE_ID, SIGNAL(clicked()), this, SLOT(quit()));

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
			grabInput(qt_xdisplay(), winId());
		m_view->grabKeyboard();
	}
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
		ungrabInput(qt_xdisplay());
		vidmodeNormalSwitch(qt_xdisplay(), m_oldResolution);
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
		m_dockArea = new QDockArea(Qt::Horizontal, QDockArea::Normal, this);
		m_dockArea->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding,
					QSizePolicy::Fixed));
		KToolBar *t = new KToolBar(m_dockArea);
		m_toolbar = t;
		t->setIconText(KToolBar::IconTextRight);
		connect(t, SIGNAL(placeChanged(QDockWindow::Place)), SLOT(toolbarChanged()));
		t->insertButton("window_fullscreen", 0, true, i18n("Fullscreen"));
		KToolBarButton *fullscreenButton = t->getButton(0);
		QToolTip::add(fullscreenButton, i18n("Fullscreen"));
		QWhatsThis::add(fullscreenButton, i18n("Switches to full screen. If the remote desktop has a different screen resolution, Remote Desktop Connection will automatically switch to the nearest resolution."));
		t->setToggle(0);
		t->setButton(0, false);
		t->addConnection(0, SIGNAL(toggled(bool)), this, SLOT(enableFullscreen(bool)));

		if (m_view->supportsScaling()) {
			t->insertButton("viewmagfit", 1, true, i18n("Scale"));
			KToolBarButton *scaleButton = t->getButton(1);
			QToolTip::add(scaleButton, i18n("Scale view"));
			QWhatsThis::add(scaleButton, i18n("This option scales the remote screen to fit your window size."));
			t->setToggle(1);
			t->setButton(1, scaling);
			t->addConnection(1, SIGNAL(toggled(bool)), this, SLOT(switchToNormal(bool)));
		}

		t->insertButton("key_enter", 2, true, i18n("Special Keys"));
		KToolBarButton *skButton = t->getButton(2);
		QToolTip::add(skButton, i18n("Enter special keys."));
		QWhatsThis::add(skButton, i18n("This option allows you to send special key combinations like Ctrl-Alt-Del to the remote host."));
		t->addConnection(2, SIGNAL(clicked()), m_keyCaptureDialog, SLOT(execute()));

		m_popup = createPopupMenu(t);
		t->insertButton("configure", 3, m_popup, true, i18n("Advanced"));
		KToolBarButton *advancedButton = t->getButton(3);
		QToolTip::add(advancedButton, i18n("Advanced options"));
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
	m_popup->setItemChecked(VIEW_ONLY_ID, s);
	m_view->setViewOnly(s);
}

void KRDC::iconify()
{
	KWin::clearState(winId(), NET::StaysOnTop);

	m_view->releaseKeyboard();
	if (m_oldResolution.valid)
		ungrabInput(qt_xdisplay());

	vidmodeNormalSwitch(qt_xdisplay(), m_oldResolution);
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
	m_oldResolution = vidmodeFullscreenSwitch(qt_xdisplay(),
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
		grabInput(qt_xdisplay(), winId());
	m_view->grabKeyboard();
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
		if ((w >= dw) || (h >= dh))
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

	m_bumpScrollTimer.start(333, true);
}

void KRDC::showFullscreenToolbar() {
	m_fsToolbar->startShow();
	m_autoHideTimer.stop();
}

void KRDC::hideFullscreenToolbarDelayed() {
	if (!m_autoHideTimer.isActive())
		m_autoHideTimer.start(TOOLBAR_AUTOHIDE_TIMEOUT, true);
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
