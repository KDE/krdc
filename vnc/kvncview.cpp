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

#include <qdatastream.h>
#include <dcopclient.h>
#include <qclipboard.h>
#include <qbitmap.h>
#include <qmutex.h>
#include <qvbox.h>
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

static QCString password;
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
  KRemoteView(parent, name, Qt::WResizeNoErase | Qt::WRepaintNoErase | Qt::WStaticContents),
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
	dpy = qt_xdisplay();
	setFixedSize(16,16);
	setFocusPolicy(QWidget::StrongFocus);

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
		if (hp->askOnConnect()) {
			// show preferences dialog
			KDialogBase *dlg = new KDialogBase( this, "dlg", true,
				i18n( "VNC Host Preferences for %1" ).arg( m_host ),
				KDialogBase::Ok|KDialogBase::Cancel, KDialogBase::Ok, true );

			QVBox *vbox = dlg->makeVBoxMainWidget();
			VncPrefs *prefs = new VncPrefs( vbox );
			QWidget *spacer = new QWidget( vbox );
			vbox->setStretchFactor( spacer, 10 );

			prefs->setQuality( ci );
			prefs->setShowPrefs(true);

			if ( dlg->exec() == QDialog::Rejected )
				return false;

			ci = prefs->quality();
			hp->setAskOnConnect(prefs->showPrefs());
			hp->setQuality(ci);
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
	}
	else if (e->type() == PasswordRequiredEventType) {
		emit showingPasswordDialog(true);

		if (KPasswordDialog::getPassword(password, i18n("Access to the system requires a password.")) != KPasswordDialog::Accepted)
			password = QCString();

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
	QValueList<unsigned int> keys = m_mods.keys();
	QValueList<unsigned int>::const_iterator it = keys.begin();
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

int getPassword(char *passwd, int pwlen) {
	int retV = 1;

	passwordLock.lock();
	if (password.isNull()) {
		QApplication::postEvent(kvncview, new PasswordRequiredEvent());
		passwordWaiter.wait(&passwordLock);
	}
	if (!password.isNull())
		strncpy(passwd, (const char*)password, pwlen);
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
