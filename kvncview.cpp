/***************************************************************************
                          kvncview.cpp  -  main widget
                             -------------------
    begin                : Thu Dec 20 15:11:42 CET 2001
    copyright            : (C) 2001-2002 by Tim Jansen
                           contains portions (event handling) from Keystone:
                           (C) 1999-2000 Richard Moore
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
#include <qdatastream.h>
#include <dcopclient.h>
#include <qcursor.h>
#include <qbitmap.h>
#include <qlineedit.h>
#include <qdialog.h>
#include <qmutex.h>
#include <qwaitcondition.h>
#include "kvncview.h"

#include "vncviewer.h"

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
  QWidget(parent, name),
  m_cthread(this, m_wthread, m_quitFlag),
  m_wthread(this, m_quitFlag),
  m_quitFlag(false),
  m_scaling(false),
  m_buttonMask(0),
  m_host(_host),
  m_port(_port)
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

/*
	KStandardDirs *dirs = KGlobal::dirs();
	QBitmap cursorBitmap(dirs->findResource("appdata", 
						"pics/pointcursor.png"));
	QBitmap cursorMask(dirs->findResource("appdata", 
					      "pics/pointcursormask.png"));
	QCursor cursor(cursorBitmap, cursorMask);
	setCursor(cursor);
*/
	setCursor(QCursor(Qt::BlankCursor));
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
	m_wthread.queueUpdateRequest(QRegion(QRect(0,0,width(),height())));
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
	if (status() == REMOTE_VIEW_CONNECTED) 
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
		emit statusChanged(REMOTE_VIEW_DISCONNECTED);
		switch (fee->errorCode()) {
		case ERROR_CONNECTION:
			KMessageBox::error(0, 
					   i18n("Connection attempt to host failed."),
					   i18n("Connection Failure"));
			break;
		case ERROR_PROTOCOL:
			KMessageBox::error(0, 
					   i18n("Remote host uses a incompatible protocol."),
					   i18n("Connection Failure"));
			break;		
		case ERROR_IO:
			KMessageBox::error(0, 
					   i18n("The connection to the host has been interrupted."),
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
		m_cb->setText(sce->bytes());
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

void KVncView::keyPressEvent(QKeyEvent *e) {
	m_wthread.queueKeyEvent((KeySym)toKeySym(e), true);
}

void KVncView::keyReleaseEvent(QKeyEvent *e) {
	m_wthread.queueKeyEvent((KeySym)toKeySym(e), false);
}

QSize KVncView::sizeHint() {
	return maximumSize();
}

void KVncView::selectionChanged() {
	if (status() != REMOTE_VIEW_CONNECTED)
		return;

	if (m_cb->ownsSelection())
		return;

	QString text = m_cb->text();
	if (text.length() > MAX_SELECTION_LENGTH)
		return;

	m_wthread.queueClientCut(text);
}

int getPassword(char *passwd, int pwlen) {
	int retV = 1;

	passwordLock.lock();
	if (password.isNull()) {
		QThread::postEvent(kvncview, new PasswordRequiredEvent());
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
	KApplication::kApplication()->lock();
	kvncview->drawRegion(x, y, width, height);
	KApplication::kApplication()->unlock();
//	QThread::postEvent(kvncview, new ScreenRepaintEvent(x, y, width, height));	
}

extern void beep() {
	QThread::postEvent(kvncview, new BeepEvent());	
}

extern void newServerCut(char *bytes, int length) {
	QThread::postEvent(kvncview, new ServerCutEvent(bytes, length));
}

unsigned long KVncView::toKeySym(QKeyEvent *k)
{
	int ke = 0;

	ke = k->ascii();
	// Markus: Crappy hack. I dont know why lower case letters are
	// not defined in qkeydefs.h. The key() for e.g. 'l' == 'L'. 
	// This sucks. :-(
	
	if ( (ke >= 'a') && (ke <= 'z') ) {
		ke = k->key();
		ke = ke + 0x20;
		return ke;
	}

	// qkeydefs = xkeydefs! :-)
	if ( ( k->key() >= 0x0a0 ) && k->key() <= 0x0ff )
		return k->key();
	
	if ( ( k->key() >= 0x20 ) && ( k->key() <= 0x7e ) )
		return k->key();
	
	// qkeydefs != xkeydefs! :-(
	// This is gonna suck :-(
	
	switch( k->key() ) {
	case SHIFT:
		return XK_Shift_L;
	case CTRL:
		return XK_Control_L;
	case ALT:
		return XK_Alt_L;
		
	case Key_Escape:
		return  XK_Escape;
	case Key_Tab:
		return XK_Tab;
	case Key_Backspace:
		return XK_BackSpace;
	case Key_Return:
		return XK_Return;
	case Key_Enter:
		return XK_Return;
	case Key_Insert:
		return XK_Insert;
	case Key_Delete:
		return XK_Delete;
	case Key_Pause:
		return XK_Pause;
	case Key_Print:
		return XK_Print;
	case Key_SysReq:
		return XK_Sys_Req;
	case Key_Home:
		return XK_Home;
	case Key_End:
		return XK_End;
	case Key_Left:
		return XK_Left;
	case Key_Up:
		return XK_Up;
	case Key_Right:
		return XK_Right;
	case Key_Down:
		return XK_Down;
	case Key_Prior:
		return XK_Prior;
	case Key_Next:
		return XK_Next;
		
	case Key_Shift:
		return XK_Shift_L;
	case Key_Control:
		return XK_Control_L;
	case Key_Meta:
		return XK_Meta_L;
	case Key_Alt:
		return XK_Alt_L;
	case Key_CapsLock:
		return XK_Caps_Lock;
	case Key_NumLock:
		return XK_Num_Lock;
	case Key_ScrollLock:
		return XK_Scroll_Lock;
		
	case Key_F1:
		return XK_F1;
	case Key_F2:
		return XK_F2;
	case Key_F3:
		return XK_F3;
	case Key_F4:
		return XK_F4;
	case Key_F5:
		return XK_F5;
	case Key_F6:
		return XK_F6;
	case Key_F7:
		return XK_F7;
	case Key_F8:
		return XK_F8;
	case Key_F9:
		return XK_F9;
	case Key_F10:
		return XK_F10;
	case Key_F11:
		return XK_F11;
	case Key_F12:
		return XK_F12;
	case Key_F13:
		return XK_F13;
	case Key_F14:
		return XK_F14;
	case Key_F15:
		return XK_F15;
	case Key_F16:
		return XK_F16;
	case Key_F17:
		return XK_F17;
	case Key_F18:
		return XK_F18;
	case Key_F19:
		return XK_F19;
	case Key_F20:
		return XK_F20;
	case Key_F21:
		return XK_F21;
	case Key_F22:
		return XK_F22;
	case Key_F23:
		return XK_F23;
	case Key_F24:
		return XK_F24;
		
	case Key_unknown:
		return 0;
	default:
		return 0;
	}

	// Puhhhhh done. :-)
	return 0;
}
#include "kvncview.moc"
