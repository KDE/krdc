/***************************************************************************
                          kvncview.cpp  -  main widget
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

#include "passworddialog.h"
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
#include <qmutex.h>
#include <qwaitcondition.h>

#include "kvncview.h"
#include "vncviewer.h"

#include <X11/Xlib.h>

/*
 * appData is our application-specific data which can be set by the user with
 * application resource specs.  The AppData structure is defined in the header
 * file.
 */

AppData appData;

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
		   AppData *data) : 
  QWidget(parent, name, Qt::WResizeNoErase | Qt::WRepaintNoErase | Qt::WStaticContents),
  ThreadSafeEventReceiver(this),
  m_cthread(this, m_wthread, m_quitFlag),
  m_wthread(this, m_quitFlag),
  m_quitFlag(false),
  m_scaling(false),
  m_buttonMask(0),
  m_host(_host),
  m_port(_port),
  m_dontSendCb(false)
{
	kvncview = this;
	password = _password;
	dpy = qt_xdisplay();
	if (data) 
		appData = *data;
	else
		setDefaultAppData();
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

	/* Reverse m_cursorOn because showDotCursor only works when state changed */
	m_cursorOn = appData.dotCursor ? false : true;
	showDotCursor(!m_cursorOn);
}

void KVncView::showDotCursor(bool show) {
	if (show == m_cursorOn)
		return;

	m_cursorOn = show;
	if (!show)
		setCursor(QCursor(Qt::BlankCursor));
	else
		setCursor(m_cursor);
}

void KVncView::setDefaultAppData() {
	appData.shareDesktop = True;
	appData.viewOnly = False;

	appData.useBGR233 = False;
	appData.encodingsString = "copyrect softcursor hextile corre rre raw";
	appData.compressLevel = -1;
	appData.qualityLevel = 9;

	appData.nColours = 256;
	appData.useSharedColours = True;
	appData.requestedDepth = 0;

	appData.dotCursor = False;

	appData.rawDelay = 0;
	appData.copyRectDelay = 0;
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

enum RemoteViewStatus KVncView::status() {
	return m_status;
}

bool KVncView::checkLocalKRfb() {
	if ((m_host != "localhost") && (m_host != ""))
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

	emit statusChanged(REMOTE_VIEW_DISCONNECTED);
	KMessageBox::error(0, 
			   i18n("It is not possible to connect to a local Desktop Sharing service."),
			   i18n("Connection Failure"));
	emit disconnectedError();
	return false;
}

bool KVncView::start() {
	if (!checkLocalKRfb())
		return false;
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
	waitForLastEvent();
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
	if (ignoreEvents())
		return;

	if (e->type() == ScreenRepaintEventType) {  
		gotEvent(e);
		ScreenRepaintEvent *sre = (ScreenRepaintEvent*) e;
		drawRegion(sre->x(), sre->y(),sre->width(), sre->height());
	}
	else if (e->type() == ScreenResizeEventType) {  
		gotEvent(e);
		ScreenResizeEvent *sre = (ScreenResizeEvent*) e;
		m_framebufferSize = QSize(sre->width(), sre->height());
		setFixedSize(m_framebufferSize);
		emit changeSize(sre->width(), sre->height());
	}
	else if (e->type() == DesktopInitEventType) {  
		gotEvent(e);
		m_cthread.desktopInit();
	}
	else if (e->type() == StatusChangeEventType) {  
		gotEvent(e);
		StatusChangeEvent *sce = (StatusChangeEvent*) e;
		m_status = sce->status();
		emit statusChanged(m_status);
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
		gotEvent(e);
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
		gotEvent(e);
		FatalErrorEvent *fee = (FatalErrorEvent*) e;
		emit statusChanged(REMOTE_VIEW_DISCONNECTED);
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
					   i18n("Connection failed. A server with the given name can not be found."),
					   i18n("Connection Failure"));
			break;
		case ERROR_NO_SERVER:
			KMessageBox::error(0, 
					   i18n("Connection failed. No server running at the given address."),
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
		gotEvent(e);
		QApplication::beep();
	}
	else if (e->type() == ServerCutEventType) { 
		gotEvent(e);
		ServerCutEvent *sce = (ServerCutEvent*) e;
		m_dontSendCb = true;
		m_cb->setText(sce->bytes());
		m_dontSendCb = false;
	}
	else if (e->type() == MouseStateEventType) {
		gotEvent(e);
		MouseStateEvent *mse = (MouseStateEvent*) e;
		emit mouseStateChanged(mse->x(), mse->y(), mse->buttonMask());
		showDotCursor(m_plom.handlePointerEvent(mse->x(), mse->y()));
	}
}

void KVncView::mouseEvent(QMouseEvent *e) {
	if (status() != REMOTE_VIEW_CONNECTED)
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
	if (status() != REMOTE_VIEW_CONNECTED)
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
	if (status() != REMOTE_VIEW_CONNECTED)
		return;

	if (m_cb->ownsSelection() || m_dontSendCb)
		return;

	QString text;
	text = m_cb->text();
	if (text.length() > MAX_SELECTION_LENGTH)
		return;

	m_wthread.queueClientCut(text);
}

int getPassword(char *passwd, int pwlen) {
	int retV = 1;

	passwordLock.lock();
	if (password.isNull()) {
		kvncview->sendEvent(new PasswordRequiredEvent());
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
	kvncview->sendEvent(new ScreenRepaintEvent(x, y, width, height));	
}

extern void beep() {
	kvncview->sendEvent(new BeepEvent());	
}

extern void newServerCut(char *bytes, int length) {
	kvncview->sendEvent(new ServerCutEvent(bytes, length));
}

extern void postMouseEvent(int x, int y, int buttonMask) {
	kvncview->sendEvent(new MouseStateEvent(x, y, buttonMask));
}

#include "kvncview.moc"
