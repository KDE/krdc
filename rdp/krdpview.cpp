/*
     krdpview.h, implementation of the KRdpView class
     Copyright (C) 2002 Arend van Beelen jr.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published by
     the Free Software Foundation; either version 2 of the License, or (at
     your option) any later version.

     This program is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
     or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
     for more details.

     You should have received a copy of the GNU General Public License along
     with this program; if not, write to the Free Software Foundation, Inc.,
     59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

     For any questions, comments or whatever, you may mail me at: arend@auton.nl
*/

#include <klocale.h>
#include <kmessagebox.h>
#include <kdialogbase.h>

#include <qvbox.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#undef Bool

#include "events.h"
#include "krdpview.h"
#include "rdesktop.h"
#include "rdphostpref.h"
#include "rdpprefs.h"

// global variables from rdesktop
extern int width;           // width of the remote desktop
extern int height;          // height of the remote desktop
extern Atom protocol_atom;  // used to handle the WM_DELETE_WINDOW protocol
extern BOOL fullscreen;     // are we in fullscreen mode?
extern char keymapname[16];

bool rdpAppDataConfigured = false;


static KRdpView *krdpview;

// constructor
KRdpView::KRdpView(QWidget *parent, const char *name, 
                   const QString &host, int port,
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
  m_viewOnly(false),
  m_cthread(this, m_wthread, m_quitFlag),
  m_wthread(this, m_quitFlag)
{
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
	SmartPtr<RdpHostPref> hp, rdpDefaults;

	if(!rdpAppDataConfigured)
	{
		HostPreferences *hps = HostPreferences::instance();

		hp = SmartPtr<RdpHostPref>(hps->createHostPref(m_host,
		                                              RdpHostPref::RdpType));
		int wv = hp->width();
		int hv = hp->height();
		QString kl = hp->layout();
		if(hp->askOnConnect())
		{
			// show preferences dialog
			KDialogBase *dlg = new KDialogBase( this, "dlg", true,
				i18n( "RDP Host Preferences for %1" ).arg( m_host ),
				KDialogBase::Ok|KDialogBase::Cancel, KDialogBase::Ok, true );

			QVBox *vbox = dlg->makeVBoxMainWidget();
			RdpPrefs *prefs = new RdpPrefs( vbox );
			QWidget *spacer = new QWidget( vbox );
			vbox->setStretchFactor( spacer, 10 );

			prefs->setRdpWidth( wv );
			prefs->setRdpHeight( hv );
			prefs->setResolution();
			prefs->setKbLayout( keymap2int( kl ) );
			prefs->setShowPrefs( true );

			if ( dlg->exec() == QDialog::Rejected )
				return false;

			wv = prefs->rdpWidth();
			hv = prefs->rdpHeight();
			kl = int2keymap( prefs->kbLayout() );
			hp->setAskOnConnect( prefs->showPrefs() );
			hp->setWidth(wv);
			hp->setHeight(hv);
			hp->setLayout(kl);
			hps->sync();
		}
	}

	// apply settings
	::width = hp->width();
	::height = hp->height();
	strcpy(keymapname, hp->layout().latin1());

	// start the connect thread
	setStatus(REMOTE_VIEW_CONNECTING);
	m_cthread.start();
	return true;
}

void KRdpView::switchFullscreen(bool on)
{
	fullscreen = (on ? True : False);

	if(on == false)
	{
		grabKeyboard();
	}
}

// captures pressed keys
void KRdpView::pressKey(XEvent *e)
{
	m_wthread.queueX11Event(e);
}

bool KRdpView::viewOnly() {
	return m_viewOnly;
}

void KRdpView::setViewOnly(bool s) {
	m_viewOnly = s;
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
	if(e->type == KeyPress || e->type == KeyRelease || e->type == ButtonPress || e->type == ButtonRelease ||
	   e->type == MotionNotify || e->type == FocusIn || e->type == FocusOut || e->type == EnterNotify ||
	   e->type == LeaveNotify || e->type == Expose || e->type == MappingNotify || 
	   e->type == ClientMessage && e->xclient.message_type == protocol_atom)
	{
		if ((!m_viewOnly) || e->type == Expose || e->type == MappingNotify || 
		    e->type == ClientMessage && e->xclient.message_type == protocol_atom) 
			m_wthread.queueX11Event(e);
		if(e->type != MotionNotify)
			return true;
	}
	
	return QWidget::x11Event(e);
}

#include "krdpview.moc"
