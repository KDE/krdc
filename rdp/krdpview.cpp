/*
     krdpview.h, implementation of the KRdpView class
     Copyright (C) 2002 Arend van Beelen jr.

     This program is free software; you can redistribute it and/or modify it under the terms of the
     GNU General Public License as published by the Free Software Foundation; either version 2 of
     the License, or (at your option) any later version.

     This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
     without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
     the GNU General Public License for more details.

     You should have received a copy of the GNU General Public License along with this program; if
     not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
     MA 02111-1307 USA

     For any questions, comments or whatever, you may mail me at: arend@auton.nl
*/

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

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "events.h"
#include "krdpview.h"
#include "rdesktop.h"

// global variables from rdesktop
extern int width;           // width of the remote desktop
extern int height;          // height of the remote desktop
extern Atom protocol_atom;  // used to handle the WM_DELETE_WINDOW protocol
extern BOOL fullscreen;     // are we in fullscreen mode?

static KRdpView *krdpview;

// constructor
KRdpView::KRdpView(QWidget *parent, const char *name, 
                   const QString &host, int port,
                   const QString &resolution,
                   const QString &user, const QString &password,
                   int flags, const QString &domain,
                   const QString &shell, const QString &directory) : 
  KRemoteView(parent, name, Qt::WResizeNoErase | Qt::WRepaintNoErase | Qt::WStaticContents),
  m_name(name),
  m_host(host),
  m_port(port),
  m_user(user),
  m_password(password),
  m_flags(flags),
  m_domain(domain),
  m_shell(shell),
  m_directory(directory),
  m_buttonMask(0),
  m_quitFlag(false),
  m_cthread(this, m_wthread, m_quitFlag),
  m_wthread(this, m_quitFlag)
{
	::width = resolution.section('x', 0, 0).toInt();
	::height = resolution.section('x', 1, 1).toInt();
	
	krdpview = this;
	setFixedSize(16,16);
}

// destructor
KRdpView::~KRdpView()
{
	startQuitting();
	m_cthread.wait();
	m_wthread.wait();
}

// returns the size of the framebuffer
QSize KRdpView::framebufferSize()
{
	return QSize(::width, ::height);
}

// returns the suggested size
QSize KRdpView::sizeHint()
{
	return maximumSize();
}

// start closing the connection
void KRdpView::startQuitting()
{
	m_quitFlag = true;
	m_wthread.kick();
	m_cthread.kick();
}

// are we currently closing the connection?
bool KRdpView::isQuitting()
{
	return m_quitFlag;
}

// return the host we're connected to
QString KRdpView::host()
{
	return m_host;
}

// return the port number we're connected on
int KRdpView::port()
{
	return m_port;
}

// open a connection
bool KRdpView::start()
{ 
	// start the connect thread
	m_cthread.start();
	return true;
}

// captures pressed keys
void KRdpView::pressKey(XEvent *e)
{
	m_wthread.queueX11Event(e);
}

// receive a custom event
void KRdpView::customEvent(QCustomEvent *e)
{
	if(e->type() == ScreenResizeEventType)
	{ 
		setFixedSize(::width, ::height);
		emit changeSize(::width, ::height);
	}
	else if(e->type() == DesktopInitEventType)
	{  
		m_cthread.desktopInit();
	}
	else if(e->type() == StatusChangeEventType)
	{  
		StatusChangeEvent *sce = (StatusChangeEvent *) e;
		setStatus(sce->status());
		if(m_status == REMOTE_VIEW_CONNECTED)
		{
			emit connected();
			setFocus();
			setMouseTracking(true);
		}
		else if(m_status == REMOTE_VIEW_DISCONNECTED)
		{
			setMouseTracking(false);
			emit disconnected();
		}
	}
	else if(e->type() == FatalErrorEventType)
	{
		FatalErrorEvent *fee = (FatalErrorEvent*) e;
		setStatus(REMOTE_VIEW_DISCONNECTED);
		switch(fee->errorCode())
		{ 
			case ERROR_CONNECTION:
				KMessageBox::error(0, i18n("Connection attempt to host failed."),
				                      i18n("Connection Failure"));
				break;
			default:
				KMessageBox::error(0, i18n("Unknown error."),
				                      i18n("Unknown Error"));
				break;
		}
		emit disconnectedError();
	}
}

// captures X11 events
bool KRdpView::x11Event(XEvent *e)
{
	if((e->type != KeyPress && e->type != KeyRelease && e->type != ButtonPress && e->type != ButtonRelease &&
	    e->type != MotionNotify && e->type != FocusIn && e->type != FocusOut && e->type != EnterNotify && 
	    e->type != LeaveNotify && e->type != ClientMessage && e->type != Expose && e->type != MappingNotify) ||
	   (e->type == ClientMessage && e->xclient.message_type != protocol_atom))
		return QWidget::x11Event(e);

	m_wthread.queueX11Event(e);
	return true;
}

#include "krdpview.moc"
