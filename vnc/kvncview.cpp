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
#include "passworddialog.h"
#include "vnchostpreferences.h"
#include <kdebug.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kinstance.h>
#include <kstandarddirs.h>
#include <kapplication.h>
#include <kkeynative.h>
#include <qdatastream.h>
#include <dcopclient.h>
#include <qbitmap.h>
#include <qlineedit.h>
#include <qdialog.h>
#include <qcombobox.h>
#include <qmutex.h>
#include <qwaitcondition.h>

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

static QString password;
static QMutex passwordLock;
static QWaitCondition passwordWaiter;

const unsigned int MAX_SELECTION_LENGTH = 4096;


KVncView::KVncView(QWidget *parent, 
		   const char *name, 
		   const QString &_host,
		   int _port,
		   const QString &_password,
		   Quality quality,
		   const QString &encodings) : 
  KRemoteView(parent, name, Qt::WResizeNoErase | Qt::WRepaintNoErase | Qt::WStaticContents),
  m_cthread(this, m_wthread, m_quitFlag),
  m_wthread(this, m_quitFlag),
  m_quitFlag(false),
  m_enableFramebufferLocking(false),
  m_scaling(false),
  m_buttonMask(0),
  m_host(_host),
  m_port(_port),
  m_dontSendCb(false)
{
	kvncview = this;
	password = _password;
	dpy = qt_xdisplay();
	setFixedSize(16,16);
	setFocusPolicy(QWidget::StrongFocus);

	m_cb = QApplication::clipboard();
	m_cb->setSelectionMode(true);
	connect(m_cb, SIGNAL(selectionChanged()), this, SLOT(selectionChanged()));

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

void KVncView::showDotCursor(bool show) {
	bool s = show;
	if (m_enableClientCursor)
		s = false;
	if (show == m_cursorEnabled)
		return;

	m_cursorEnabled = s;
	if (!s)
		setCursor(QCursor(Qt::BlankCursor));
	else
		setCursor(m_cursor);
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

	/* Reverse m_cursorEnabled because showDotCursor only works when state changed */
	m_cursorEnabled = appData.dotCursor ? false : true;
	showDotCursor(!m_cursorEnabled);
}

bool KVncView::checkLocalKRfb() {
	if ( m_host != "localhost" && !m_host.isEmpty() )
		return true;
	DCOPClient *d = KApplication::dcopClient();

	int portNum;
	QByteArray sdata, rdata;
	QCString replyType;
	QDataStream arg(sdata, IO_WriteOnly);
	arg << QString("krfb");
	if (!d->call ("kded", "kinetd", "port(QString)", sdata, replyType, rdata))
		return true;

	if (replyType != "int")
		return true;

	QDataStream answer(rdata, IO_ReadOnly);
	answer >> portNum;

	if (m_port != portNum)
		return true;

	setStatus(REMOTE_VIEW_DISCONNECTED);
	KMessageBox::error(0, 
			   i18n("It is not possible to connect to a local Desktop Sharing service."),
			   i18n("Connection Failure"));
	emit disconnectedError();
	return false;
}

bool KVncView::start() {
	if (!checkLocalKRfb())
		return false;

	if (!appDataConfigured) {
		// show preferences dialog
		VncHostPreferences vhp(0, "VncHostPreferencesDialog", true);
		if (vhp.exec() == QDialog::Rejected)
			return false;

		Quality quality;
		int ci = vhp.qualityCombo->currentItem();
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

bool KVncView::supportsScaling() {
	return true;
}

bool KVncView::scaling() {
	return m_scaling;
}

QSize KVncView::framebufferSize() {
	return m_framebufferSize;
}

void KVncView::enableScaling(bool s) {
	bool os = m_scaling;
	m_scaling = s;
	if (s != os) {
		if (s) {
			setMaximumSize(m_framebufferSize);
			setMinimumSize(m_framebufferSize.width()/16,
				       m_framebufferSize.height()/16);
/*			if (height() != heightForWidth(width()))
				resize(m_framebufferSize.width(),
					heightForWidth(m_framebufferSize.width()));
*/		}
		else
			setFixedSize(m_framebufferSize);
	}
}

int KVncView::heightForWidth(int w) const {
	if (m_scaling)
		return w * m_framebufferSize.height() / m_framebufferSize.width();
	else
		return 0;
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
	}
	else if (e->type() == PasswordRequiredEventType) {  
		PasswordDialog pd(0, 0, true);

		emit showingPasswordDialog(true);

		if (pd.exec() == QDialog::Accepted) 
			password = pd.passwordInput->text();
		else {
			password = QString::null;
		}

		emit showingPasswordDialog(false);

		passwordLock.lock(); // to guarantee that thread is waiting
		passwordWaiter.wakeAll();
		passwordLock.unlock();
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
		m_dontSendCb = true;
		m_cb->setText(sce->bytes());
		m_dontSendCb = false;
	}
	else if (e->type() == MouseStateEventType) {
		MouseStateEvent *mse = (MouseStateEvent*) e;
		emit mouseStateChanged(mse->x(), mse->y(), mse->buttonMask());
		showDotCursor(m_plom.handlePointerEvent(mse->x(), mse->y()));
	}
}

void KVncView::mouseEvent(QMouseEvent *e) {
	if (m_status != REMOTE_VIEW_CONNECTED)
		return;

	if ( e->type() != QEvent::MouseMove ) {
		if ( (e->type() == QEvent::MouseButtonPress) ||
                     (e->type() == QEvent::MouseButtonDblClick)) {
			if ( e->button() & LeftButton )
				m_buttonMask |= 0x01;
			if ( e->button() & MidButton )
				m_buttonMask |= 0x02;
			if ( e->button() & RightButton )
				m_buttonMask |= 0x04;
		}
		else if ( e->type() == QEvent::MouseButtonRelease ) {
			if ( e->button() & LeftButton )
				m_buttonMask &= 0xfe;
			if ( e->button() & MidButton )
				m_buttonMask &= 0xfd;
			if ( e->button() & RightButton )
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
	if (mod & KKeyNative::modX(KKey::SHIFT))
		m_wthread.queueKeyEvent(XK_Shift_L, true);
	if (mod & KKeyNative::modX(KKey::CTRL))
		m_wthread.queueKeyEvent(XK_Control_L, true);
	if (mod & KKeyNative::modX(KKey::ALT))
		m_wthread.queueKeyEvent(XK_Alt_L, true);
	if (mod & KKeyNative::modX(KKey::WIN))
		m_wthread.queueKeyEvent(XK_Meta_L, true);

	m_wthread.queueKeyEvent(k.sym(), true);
	m_wthread.queueKeyEvent(k.sym(), false);

	if (mod & KKeyNative::modX(KKey::WIN))
		m_wthread.queueKeyEvent(XK_Meta_L, false);
	if (mod & KKeyNative::modX(KKey::ALT))
		m_wthread.queueKeyEvent(XK_Alt_L, false);
	if (mod & KKeyNative::modX(KKey::CTRL))
		m_wthread.queueKeyEvent(XK_Control_L, false);
	if (mod & KKeyNative::modX(KKey::SHIFT))
		m_wthread.queueKeyEvent(XK_Shift_L, false);
}

bool KVncView::x11Event(XEvent *e) {
	bool pressed;
	if (e->type == KeyPress)
		pressed = true;
	else if (e->type == KeyRelease)
		pressed = false;
	else
		return QWidget::x11Event(e);

	m_wthread.queueKeyEvent(KKeyNative(e).sym(), pressed);
	return true;
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

void KVncView::selectionChanged() {
	if (m_status != REMOTE_VIEW_CONNECTED)
		return;

	if (m_cb->ownsSelection() || m_dontSendCb)
		return;

	QString text;
	text = m_cb->text();
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
		showDotCursor(false);
	}
	else
		showDotCursor(appData.dotCursor);
	m_enableClientCursor = enable;

}

int getPassword(char *passwd, int pwlen) {
	int retV = 1;

	passwordLock.lock();
	if (password.isNull()) {
		QApplication::postEvent(kvncview, new PasswordRequiredEvent());
		passwordWaiter.wait(&passwordLock);
	}
	if (!password.isNull())
		strncpy(passwd, password.latin1(), pwlen);
	else {
		passwd[0] = 0;
		retV = 0;
	}
	passwordLock.unlock();

	if (!retV)
		kvncview->startQuitting();
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
