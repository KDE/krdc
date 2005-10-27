/***************************************************************************
                          kvncview.cpp  -  main widget
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

#include "kvncview.h"
#include "vncprefs.h"
#include "vnchostpref.h"

#include <kapplication.h>
#include <kdebug.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <kpassdlg.h>
#include <kdialogbase.h>
#include <kwallet.h>

#include <qdatastream.h>
//Added by qt3to4:
#include <kvbox.h>
#include <QWheelEvent>
#include <Q3CString>
#include <QFocusEvent>
#include <QPaintEvent>
#include <QEvent>
#include <Q3ValueList>
#include <QMouseEvent>
#include <QCustomEvent>
#include <dcopclient.h>
#include <qclipboard.h>
#include <qbitmap.h>
#include <qmutex.h>
#include <q3vbox.h>
#include <qwaitcondition.h>
#include <QX11Info>
#include "vncviewer.h"

#include <X11/Xlib.h>

/*
 * appData is our application-specific data which can be set by the user with
 * application resource specs.  The AppData structure is defined in the header
 * file.
 */
AppData appData;
bool appDataConfigured = false;

Display* dpy;

static KVncView *kvncview;

//Passwords and KWallet data
extern KWallet::Wallet *wallet;
bool useKWallet = false;
static Q3CString password;
static QMutex passwordLock;
static QWaitCondition passwordWaiter;

const unsigned int MAX_SELECTION_LENGTH = 4096;


KVncView::KVncView(QWidget *parent,
		   const char *name,
		   const QString &_host,
		   int _port,
		   const QString &_password,
		   Quality quality,
		   DotCursorState dotCursorState,
		   const QString &encodings) :
  KRemoteView(parent, name, Qt::WResizeNoErase | Qt::WNoAutoErase | Qt::WStaticContents),
  m_cthread(this, m_wthread, m_quitFlag),
  m_wthread(this, m_quitFlag),
  m_quitFlag(false),
  m_enableFramebufferLocking(false),
  m_scaling(false),
  m_remoteMouseTracking(false),
  m_viewOnly(false),
  m_buttonMask(0),
  m_host(_host),
  m_port(_port),
  m_dontSendCb(false),
  m_cursorState(dotCursorState)
{
	kvncview = this;
	password = _password.latin1();
	dpy = QX11Info::display();
	setFixedSize(16,16);
	setFocusPolicy(Qt::StrongFocus);

	m_cb = QApplication::clipboard();
	connect(m_cb, SIGNAL(selectionChanged()), this, SLOT(selectionChanged()));
	connect(m_cb, SIGNAL(dataChanged()), this, SLOT(clipboardChanged()));

	KStandardDirs *dirs = KGlobal::dirs();
	QBitmap cursorBitmap(dirs->findResource("appdata",
						"pics/pointcursor.png"));
	QBitmap cursorMask(dirs->findResource("appdata",
					      "pics/pointcursormask.png"));
	m_cursor = QCursor(cursorBitmap, cursorMask);

	if ((quality != QUALITY_UNKNOWN) ||
	    !encodings.isNull())
		configureApp(quality, encodings);
}

void KVncView::showDotCursor(DotCursorState state) {
	if (state == m_cursorState)
		return;

	m_cursorState = state;
	showDotCursorInternal();
}

DotCursorState KVncView::dotCursorState() const {
	return m_cursorState;
}

void KVncView::showDotCursorInternal() {
	switch (m_cursorState) {
	case DOT_CURSOR_ON:
		setCursor(m_cursor);
		break;
	case DOT_CURSOR_OFF:
		setCursor(QCursor(Qt::BlankCursor));
		break;
	case DOT_CURSOR_AUTO:
		if (m_enableClientCursor)
			setCursor(QCursor(Qt::BlankCursor));
		else
			setCursor(m_cursor);
		break;
	}
}

QString KVncView::host() {
	return m_host;
}

int KVncView::port() {
	return m_port;
}

void KVncView::startQuitting() {
	m_quitFlag = true;
	m_wthread.kick();
	m_cthread.kick();
}

bool KVncView::isQuitting() {
	return m_quitFlag;
}

void KVncView::configureApp(Quality q, const QString specialEncodings) {
	appDataConfigured = true;
	appData.shareDesktop = 1;
	appData.viewOnly = 0;

	if (q == QUALITY_LOW) {
		appData.useBGR233 = 1;
		appData.encodingsString = "background copyrect softcursor tight zlib hextile raw";
		appData.compressLevel = -1;
		appData.qualityLevel = 1;
		appData.dotCursor = 1;
	}
	else if (q == QUALITY_MEDIUM) {
		appData.useBGR233 = 0;
		appData.encodingsString = "background copyrect softcursor tight zlib hextile raw";
		appData.compressLevel = -1;
		appData.qualityLevel = 7;
		appData.dotCursor = 1;
	}
	else if ((q == QUALITY_HIGH) || (q == QUALITY_UNKNOWN)) {
		appData.useBGR233 = 0;
		appData.encodingsString = "copyrect softcursor hextile raw";
		appData.compressLevel = -1;
		appData.qualityLevel = 9;
		appData.dotCursor = 1;
	}

	if (!specialEncodings.isNull())
		appData.encodingsString = specialEncodings.latin1();

	appData.nColours = 256;
	appData.useSharedColours = 1;
	appData.requestedDepth = 0;

	appData.rawDelay = 0;
	appData.copyRectDelay = 0;

	if (!appData.dotCursor)
		m_cursorState = DOT_CURSOR_OFF;
	showDotCursorInternal();
}

bool KVncView::checkLocalKRfb() {
	if ( m_host != "localhost" && !m_host.isEmpty() )
		return true;
	DCOPClient *d = KApplication::dcopClient();

	int portNum;
	QByteArray sdata, rdata;
	DCOPCString replyType;
	QDataStream arg(&sdata, QIODevice::WriteOnly);
	arg << QString("krfb");
	if (!d->call ("kded", "kinetd", "port(QString)", sdata, replyType, rdata))
		return true;

	if (replyType != "int")
		return true;

	QDataStream answer(&rdata, QIODevice::ReadOnly);
	answer >> portNum;

	if (m_port != portNum)
		return true;

	setStatus(REMOTE_VIEW_DISCONNECTED);
	KMessageBox::error(0,
			   i18n("It is not possible to connect to a local desktop sharing service."),
			   i18n("Connection Failure"));
	emit disconnectedError();
	return false;
}

bool KVncView::start() {

	if (!checkLocalKRfb())
		return false;

	if (!appDataConfigured) {

		HostPreferences *hps = HostPreferences::instance();
		SmartPtr<VncHostPref> hp =
			SmartPtr<VncHostPref>(hps->createHostPref(m_host,
								 VncHostPref::VncType));
		int ci = hp->quality();
		bool kwallet = hp->useKWallet();
		if (hp->askOnConnect()) {
			// show preferences dialog
			KDialogBase *dlg = new KDialogBase( this, "dlg", true,
				i18n( "VNC Host Preferences for %1" ).arg( m_host ),
				KDialogBase::Ok|KDialogBase::Cancel, KDialogBase::Ok, true );

			KVBox *vbox = dlg->makeVBoxMainWidget();
			VncPrefs *prefs = new VncPrefs( vbox );
			QWidget *spacer = new QWidget( vbox );
			vbox->setStretchFactor( spacer, 10 );

			prefs->setQuality( ci );
			prefs->setShowPrefs(true);
			prefs->setUseKWallet(kwallet);

			if ( dlg->exec() == QDialog::Rejected )
				return false;

			ci = prefs->quality();
			hp->setAskOnConnect(prefs->showPrefs());
			hp->setQuality(ci);
			hp->setUseKWallet(prefs->useKWallet());
			hps->sync();

			delete dlg;
		}

		Quality quality;
		if (ci == 0)
			quality = QUALITY_HIGH;
		else if (ci == 1)
			quality = QUALITY_MEDIUM;
		else if (ci == 2)
			quality = QUALITY_LOW;
		else {
			kdDebug() << "Unknown quality";
				return false;
		}

		configureApp(quality);
		useKWallet = hp->useKWallet();
	}

	setStatus(REMOTE_VIEW_CONNECTING);

	m_cthread.start();
	setBackgroundMode(Qt::NoBackground);
	return true;
}

KVncView::~KVncView()
{
	startQuitting();
	m_cthread.wait();
	m_wthread.wait();
	freeResources();
}

bool KVncView::supportsLocalCursor() const {
	return true;
}

bool KVncView::supportsScaling() const {
	return true;
}

bool KVncView::scaling() const {
	return m_scaling;
}

bool KVncView::viewOnly() {
	return m_viewOnly;
}

QSize KVncView::framebufferSize() {
	return m_framebufferSize;
}

void KVncView::setViewOnly(bool s) {
	m_viewOnly = s;

	if (s)
		setCursor(Qt::ArrowCursor);
	else
		showDotCursorInternal();
}

void KVncView::enableScaling(bool s) {
	bool os = m_scaling;
	m_scaling = s;
	if (s != os) {
		if (s) {
			setMaximumSize(m_framebufferSize);
			setMinimumSize(m_framebufferSize.width()/16,
				       m_framebufferSize.height()/16);
		}
		else
			setFixedSize(m_framebufferSize);
	}
}

void KVncView::paintEvent(QPaintEvent *e) {
	drawRegion(e->rect().x(),
		   e->rect().y(),
		   e->rect().width(),
		   e->rect().height());
}

void KVncView::drawRegion(int x, int y, int w, int h) {
	if (m_scaling)
		DrawZoomedScreenRegionX11Thread(winId(), width(), height(),
						x, y, w, h);
	else
		DrawScreenRegionX11Thread(winId(), x, y, w, h);
}

void KVncView::customEvent(QCustomEvent *e)
{
	if (e->type() == ScreenRepaintEventType) {
		ScreenRepaintEvent *sre = (ScreenRepaintEvent*) e;
		drawRegion(sre->x(), sre->y(),sre->width(), sre->height());
	}
	else if (e->type() == ScreenResizeEventType) {
		ScreenResizeEvent *sre = (ScreenResizeEvent*) e;
		m_framebufferSize = QSize(sre->width(), sre->height());
		setFixedSize(m_framebufferSize);
		emit changeSize(sre->width(), sre->height());
	}
	else if (e->type() == DesktopInitEventType) {
		m_cthread.desktopInit();
	}
	else if (e->type() == StatusChangeEventType) {
		StatusChangeEvent *sce = (StatusChangeEvent*) e;
		setStatus(sce->status());
		if (m_status == REMOTE_VIEW_CONNECTED) {
			emit connected();
			setFocus();
			setMouseTracking(true);
		}
		else if (m_status == REMOTE_VIEW_DISCONNECTED) {
			setMouseTracking(false);
			emit disconnected();
		}
		else if (m_status == REMOTE_VIEW_PREPARING) {
			//Login was successfull: Write KWallet password if necessary.
			if ( useKWallet && !password.isNull() && wallet && wallet->isOpen() && !wallet->hasEntry(host())) {
				wallet->writePassword(host(), password);
			}
			delete wallet; wallet=0;
		}
	}
	else if (e->type() == PasswordRequiredEventType) {
		emit showingPasswordDialog(true);

		if (KPasswordDialog::getPassword(this,password, i18n("Access to the system requires a password.")) != KPasswordDialog::Accepted)
			password = Q3CString();

		emit showingPasswordDialog(false);

		passwordLock.lock(); // to guarantee that thread is waiting
		passwordWaiter.wakeAll();
		passwordLock.unlock();
	}
	else if (e->type() == WalletOpenEventType) {
		QString krdc_folder = "KRDC-VNC";
		emit showingPasswordDialog(true); //Bad things happen if you don't do this.

		// Bugfix: Check if wallet has been closed by an outside source
		if ( wallet && !wallet->isOpen() ) {
			delete wallet; wallet=0;
		}

		// Do we need to open the wallet?
		if ( !wallet ) {
			QString walletName = KWallet::Wallet::NetworkWallet();
			wallet = KWallet::Wallet::openWallet(walletName);
		}

		if (wallet && wallet->isOpen()) {
			bool walletOK = wallet->hasFolder(krdc_folder);
			if (walletOK == false) {
				walletOK = wallet->createFolder(krdc_folder);
			}
	
			if (walletOK == true) {
				wallet->setFolder(krdc_folder);
				QString newPass;
				if ( wallet->hasEntry(kvncview->host()) && !wallet->readPassword(kvncview->host(), newPass) ) {
					password=newPass.latin1();
				}
			}
		}

		passwordLock.lock(); // to guarantee that thread is waiting
		passwordWaiter.wakeAll();
		passwordLock.unlock();

		emit showingPasswordDialog(false);
	}
	else if (e->type() == FatalErrorEventType) {
		FatalErrorEvent *fee = (FatalErrorEvent*) e;
		setStatus(REMOTE_VIEW_DISCONNECTED);
		switch (fee->errorCode()) {
		case ERROR_CONNECTION:
			KMessageBox::error(0,
					   i18n("Connection attempt to host failed."),
					   i18n("Connection Failure"));
			break;
		case ERROR_PROTOCOL:
			KMessageBox::error(0,
					   i18n("Remote host is using an incompatible protocol."),
					   i18n("Connection Failure"));
			break;
		case ERROR_IO:
			KMessageBox::error(0,
					   i18n("The connection to the host has been interrupted."),
					   i18n("Connection Failure"));
			break;
		case ERROR_SERVER_BLOCKED:
			KMessageBox::error(0,
					   i18n("Connection failed. The server does not accept new connections."),
					   i18n("Connection Failure"));
			break;
		case ERROR_NAME:
			KMessageBox::error(0,
					   i18n("Connection failed. A server with the given name cannot be found."),
					   i18n("Connection Failure"));
			break;
		case ERROR_NO_SERVER:
			KMessageBox::error(0,
					   i18n("Connection failed. No server running at the given address and port."),
					   i18n("Connection Failure"));
			break;
		case ERROR_AUTHENTICATION:
			//Login failed: Remove wallet entry if there is one.
			if ( useKWallet && wallet && wallet->isOpen() && wallet->hasEntry(host()) ) {
				wallet->removeEntry(host());
			}
			KMessageBox::error(0,
					   i18n("Authentication failed. Connection aborted."),
					   i18n("Authentication Failure"));
			break;
		default:
			KMessageBox::error(0,
					   i18n("Unknown error."),
					   i18n("Unknown Error"));
			break;
		}
		emit disconnectedError();
	}
	else if (e->type() == BeepEventType) {
		QApplication::beep();
	}
	else if (e->type() == ServerCutEventType) {
		ServerCutEvent *sce = (ServerCutEvent*) e;
		QString ctext = QString::fromUtf8(sce->bytes(), sce->length());
		m_dontSendCb = true;
		m_cb->setText(ctext, QClipboard::Clipboard);
		m_cb->setText(ctext, QClipboard::Selection);
		m_dontSendCb = false;
	}
	else if (e->type() == MouseStateEventType) {
		MouseStateEvent *mse = (MouseStateEvent*) e;
		emit mouseStateChanged(mse->x(), mse->y(), mse->buttonMask());
		bool show = m_plom.handlePointerEvent(mse->x(), mse->y());
		if (m_cursorState != DOT_CURSOR_ON)
			showDotCursor(show ? DOT_CURSOR_AUTO : DOT_CURSOR_OFF);
	}
}

void KVncView::mouseEvent(QMouseEvent *e) {
	if (m_status != REMOTE_VIEW_CONNECTED)
		return;
	if (m_viewOnly)
		return;

	if ( e->type() != QEvent::MouseMove ) {
		if ( (e->type() == QEvent::MouseButtonPress) ||
                     (e->type() == QEvent::MouseButtonDblClick)) {
			if ( e->button() & Qt::LeftButton )
				m_buttonMask |= 0x01;
			if ( e->button() & Qt::MidButton )
				m_buttonMask |= 0x02;
			if ( e->button() & Qt::RightButton )
				m_buttonMask |= 0x04;
		}
		else if ( e->type() == QEvent::MouseButtonRelease ) {
			if ( e->button() & Qt::LeftButton )
				m_buttonMask &= 0xfe;
			if ( e->button() & Qt::MidButton )
				m_buttonMask &= 0xfd;
			if ( e->button() & Qt::RightButton )
				m_buttonMask &= 0xfb;
		}
	}

	int x = e->x();
	int y = e->y();
	m_plom.registerPointerState(x, y);
	if (m_scaling) {
		x = (x * m_framebufferSize.width()) / width();
		y = (y * m_framebufferSize.height()) / height();
	}
	m_wthread.queueMouseEvent(x, y, m_buttonMask);

	if (m_enableClientCursor)
		DrawCursorX11Thread(x, y); // in rfbproto.c
}

void KVncView::mousePressEvent(QMouseEvent *e) {
	mouseEvent(e);
	e->accept();
}

void KVncView::mouseDoubleClickEvent(QMouseEvent *e) {
	mouseEvent(e);
	e->accept();
}

void KVncView::mouseReleaseEvent(QMouseEvent *e) {
	mouseEvent(e);
	e->accept();
}

void KVncView::mouseMoveEvent(QMouseEvent *e) {
	mouseEvent(e);
	e->ignore();
}

void KVncView::wheelEvent(QWheelEvent *e) {
	if (m_status != REMOTE_VIEW_CONNECTED)
		return;
	if (m_viewOnly)
		return;

	int eb = 0;
	if ( e->delta() < 0 )
		eb |= 0x10;
	else
		eb |= 0x8;

	int x = e->pos().x();
	int y = e->pos().y();
	if (m_scaling) {
		x = (x * m_framebufferSize.width()) / width();
		y = (y * m_framebufferSize.height()) / height();
	}
	m_wthread.queueMouseEvent(x, y, eb|m_buttonMask);
	m_wthread.queueMouseEvent(x, y, m_buttonMask);
	e->accept();
}

void KVncView::pressKey(XEvent *xe) {
	KKeyNative k(xe);
	uint mod = k.mod();
	if (mod & KKeyNative::modXShift())
		m_wthread.queueKeyEvent(XK_Shift_L, true);
	if (mod & KKeyNative::modXCtrl())
		m_wthread.queueKeyEvent(XK_Control_L, true);
	if (mod & KKeyNative::modXAlt())
		m_wthread.queueKeyEvent(XK_Alt_L, true);
	if (mod & KKeyNative::modXWin())
		m_wthread.queueKeyEvent(XK_Meta_L, true);

	m_wthread.queueKeyEvent(k.sym(), true);
	m_wthread.queueKeyEvent(k.sym(), false);

	if (mod & KKeyNative::modXWin())
		m_wthread.queueKeyEvent(XK_Meta_L, false);
	if (mod & KKeyNative::modXAlt())
		m_wthread.queueKeyEvent(XK_Alt_L, false);
	if (mod & KKeyNative::modXCtrl())
		m_wthread.queueKeyEvent(XK_Control_L, false);
	if (mod & KKeyNative::modXShift())
		m_wthread.queueKeyEvent(XK_Shift_L, false);

	m_mods.clear();
}

bool KVncView::x11Event(XEvent *e) {
	bool pressed;
	if (e->type == KeyPress)
		pressed = true;
	else if (e->type == KeyRelease)
		pressed = false;
	else
		return QWidget::x11Event(e);

	if (!m_viewOnly) {
		unsigned int s = KKeyNative(e).sym();

		switch (s) {
		case XK_Meta_L:
		case XK_Alt_L:
		case XK_Control_L:
		case XK_Shift_L:
		case XK_Meta_R:
		case XK_Alt_R:
		case XK_Control_R:
		case XK_Shift_R:
			if (pressed)
				m_mods[s] = true;
			else if (m_mods.contains(s))
				m_mods.remove(s);
			else
				unpressModifiers();
		}
		m_wthread.queueKeyEvent(s, pressed);
	}
	return true;
}

void KVncView::unpressModifiers() {
	Q3ValueList<unsigned int> keys = m_mods.keys();
	Q3ValueList<unsigned int>::const_iterator it = keys.begin();
	while (it != keys.end()) {
		m_wthread.queueKeyEvent(*it, false);
		it++;
	}
	m_mods.clear();
}

void KVncView::focusOutEvent(QFocusEvent *) {
	unpressModifiers();
}

QSize KVncView::sizeHint() {
	return maximumSize();
}

void KVncView::setRemoteMouseTracking(bool s) {
	m_remoteMouseTracking = s;
}

bool KVncView::remoteMouseTracking() {
	return m_remoteMouseTracking;
}

void KVncView::clipboardChanged() {
	if (m_status != REMOTE_VIEW_CONNECTED)
		return;

	if (m_cb->ownsClipboard() || m_dontSendCb)
		return;

	QString text = m_cb->text(QClipboard::Clipboard);
	if (text.length() > MAX_SELECTION_LENGTH)
		return;

	m_wthread.queueClientCut(text);
}

void KVncView::selectionChanged() {
	if (m_status != REMOTE_VIEW_CONNECTED)
		return;

	if (m_cb->ownsSelection() || m_dontSendCb)
		return;

	QString text = m_cb->text(QClipboard::Selection);
	if (text.length() > MAX_SELECTION_LENGTH)
		return;

	m_wthread.queueClientCut(text);
}


void KVncView::lockFramebuffer() {
	if (m_enableFramebufferLocking)
		m_framebufferLock.lock();
}

void KVncView::unlockFramebuffer() {
	if (m_enableFramebufferLocking)
		m_framebufferLock.unlock();
}

void KVncView::enableClientCursor(bool enable) {
	if (enable) {
		m_enableFramebufferLocking = true; // cant be turned off
	}
	m_enableClientCursor = enable;
	showDotCursorInternal();
}

/*!
	\brief Get a password for this host.
	Tries to get a password from the url or wallet if at all possible. If
	both of these fail, it then asks the user to enter a password.
	\note Lots of dialogs can be popped up during this process. The thread
	locks and signals are there to protect against deadlocks and other
	horribleness. Be careful making changes here. 
*/
int getPassword(char *passwd, int pwlen) {
	int retV = 0;

	//Prepare the system
	passwordLock.lock();

	//Try #1: Did the user give a password in the URL?
	if (!password.isNull()) {
		retV = 1; //got it!
	}

	//Try #2: Is there something in the wallet?
	if ( !retV && useKWallet ) {
		QApplication::postEvent(kvncview, new WalletOpenEvent());
		passwordWaiter.wait(&passwordLock); //block
		if (!password.isNull()) retV = 1; //got it!
	}

	//Last try: Ask the user
	if (!retV) {
		QApplication::postEvent(kvncview, new PasswordRequiredEvent());
		passwordWaiter.wait(&passwordLock); //block
		if (!password.isNull()) retV = 1; //got it!
	}

	//Process the password if we got it, clear it if we didn't
	if (retV) {
		strncpy(passwd, (const char*)password, pwlen);
	} else {
		passwd[0] = 0;
	}

	//Pack up and go home
	passwordLock.unlock();
	if (!retV) kvncview->startQuitting();

	return retV;
}

extern int isQuitFlagSet() {
	return kvncview->isQuitting() ? 1 : 0;
}

extern void DrawScreenRegion(int x, int y, int width, int height) {
/*	KApplication::kApplication()->lock();
	kvncview->drawRegion(x, y, width, height);
	KApplication::kApplication()->unlock();
*/
	QApplication::postEvent(kvncview, new ScreenRepaintEvent(x, y, width, height));
}

// call only from x11 thread!
extern void DrawAnyScreenRegionX11Thread(int x, int y, int width, int height) {
	kvncview->drawRegion(x, y, width, height);
}

extern void EnableClientCursor(int enable) {
	kvncview->enableClientCursor(enable);
}

extern void LockFramebuffer() {
  	kvncview->lockFramebuffer();
}

extern void UnlockFramebuffer() {
  	kvncview->unlockFramebuffer();
}

extern void beep() {
	QApplication::postEvent(kvncview, new BeepEvent());
}

extern void newServerCut(char *bytes, int length) {
	QApplication::postEvent(kvncview, new ServerCutEvent(bytes, length));
}

extern void postMouseEvent(int x, int y, int buttonMask) {
	QApplication::postEvent(kvncview, new MouseStateEvent(x, y, buttonMask));
}

#include "kvncview.moc"
