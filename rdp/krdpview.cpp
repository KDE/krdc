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

#include <kconfig.h>
#include <kdebug.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kinstance.h>
#include <kstandarddirs.h>
#include <kapplication.h>
#include <kkeynative.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qdatastream.h>
#include <dcopclient.h>
#include <qbitmap.h>
#include <qlineedit.h>
#include <qdialog.h>
#include <qmutex.h>
#include <qspinbox.h>
#include <qwaitcondition.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "events.h"
#include "krdpview.h"
#include "rdesktop.h"
#include "rdphostpref.h"
#include "rdphostpreferences.h"

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
	setStatus(REMOTE_VIEW_CONNECTING);

	SmartPtr<RdpHostPref> hp;

	if(!rdpAppDataConfigured)
	{
		KConfig *config = KApplication::kApplication()->config();
		config->setGroup("RdpDefaultSettings");
		bool showPrefs = config->readBoolEntry("rdpShowHostPreferences", true);

		HostPreferences hps(config);
		hp = SmartPtr<RdpHostPref>(hps.createHostPref(m_host,
		                                              RdpHostPref::RdpType));
		int wv = hp->width();
		int hv = hp->height();
		QString kl = hp->layout();
		if(showPrefs && hp->askOnConnect())
		{
			// show preferences dialog
			RdpHostPreferences rhp(0, "RdpHostPreferencesDialog", true);
			rhp.setCaption(i18n("RDP Host Preferences for %1").arg(m_host));
			rhp.rdpWidthSpin->setValue(wv);
			rhp.rdpHeightSpin->setValue(hv);
			rhp.rdpKeyboardCombo->setCurrentItem(keymap2int(kl));
			if(wv == 640 && hv == 480)
			{
				rhp.rdpResolutionCombo->setCurrentItem(0);
			}
			else if(wv == 800 && hv == 600)
			{
				rhp.rdpResolutionCombo->setCurrentItem(1);
			}
			else if(wv == 1024 && hv == 768)
			{
				rhp.rdpResolutionCombo->setCurrentItem(2);
			}
			else
			{
				rhp.rdpResolutionCombo->setCurrentItem(3);
				rhp.rdpWidthSpin->setEnabled(true);
				rhp.rdpHeightSpin->setEnabled(true);
			}
			rhp.nextStartupCheckbox->setChecked(true);
			if(rhp.exec() == QDialog::Rejected)
				return false;

			wv = rhp.rdpWidthSpin->value();
			hv = rhp.rdpHeightSpin->value();
			kl = int2keymap(rhp.rdpKeyboardCombo->currentItem());
			hp->setAskOnConnect(rhp.nextStartupCheckbox->isChecked());
			hp->setWidth(wv);
			hp->setHeight(hv);
			hp->setLayout(kl);
			hps.sync();
		}
	}

	// apply settings
	::width = hp->width();
	::height = hp->height();
	strcpy(keymapname, hp->layout().latin1());

	// start the connect thread
	m_cthread.start();
	return true;
}

void KRdpView::switchFullscreen(bool on)
{
	fullscreen = (on ? True : False);
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
