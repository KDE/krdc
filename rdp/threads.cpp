/*
     threads.cpp, implementation of threading classes
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

#include <time.h>

#include <kapplication.h>

#include "events.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "rdesktop.h"
#include "krdpview.h"
#include "threads.h"

static const int WAIT_PERIOD = 8000;
static const unsigned int X11_QUEUE_SIZE = 8192;

// constructor
RdpControllerThread::RdpControllerThread(KRdpView *v, RdpWriterThread &wt, volatile bool &quitFlag) :
  m_view(v),
  m_status(REMOTE_VIEW_CONNECTING),
  m_wthread(wt),
  m_quitFlag(quitFlag),
  m_desktopInitialized(false)
{
}

void RdpControllerThread::changeStatus(RemoteViewStatus s)
{
	m_status = s;
	QApplication::postEvent(m_view, new StatusChangeEvent(s));
}

// creates the window for rdesktop, called from Qt thread
void RdpControllerThread::desktopInit()
{
	ui_create_window(m_view->winId());
	m_desktopInitialized = true;
	m_waiter.wakeAll();
}

void RdpControllerThread::kick()
{
	m_waiter.wakeAll();
}

void RdpControllerThread::sendFatalError(ErrorCode s)
{
	m_quitFlag = true;
	QApplication::postEvent(m_view, new FatalErrorEvent(s));
	m_wthread.kick();
}

// starts the thread -- starts and maintains the connection
void RdpControllerThread::run()
{
	uint8 type;
	STREAM s;

	// start connecting
	changeStatus(REMOTE_VIEW_CONNECTING);
  
	KApplication::kApplication()->lock();
	ui_init(m_view->x11Display(), m_view->x11Visual());
	KApplication::kApplication()->unlock();

	QCString host(m_view->m_host.utf8());
	if(!sec_connect(host.data()))
	{
		QApplication::postEvent(m_view, new FatalErrorEvent(ERROR_CONNECTION));
		return;
	}

	changeStatus(REMOTE_VIEW_AUTHENTICATING);
	QCString domain(m_view->m_domain.utf8());
	QCString user(m_view->m_user.utf8());
	QCString password(m_view->m_password.utf8());
	QCString shell(m_view->m_shell.utf8());
	QCString directory(m_view->m_directory.utf8());
	rdp_send_logon_info(m_view->m_flags, domain.data(), user.data(),
	                    password.data(), shell.data(), directory.data());

	changeStatus(REMOTE_VIEW_PREPARING);
	QApplication::postEvent(m_view, new ScreenResizeEvent(0, 0)); // KRdpView actually already knows the new size through
	                                                              // the global width and height, I don't need to tell him
	QApplication::postEvent(m_view, new DesktopInitEvent());
	while((!m_quitFlag) && (!m_desktopInitialized)) 
		m_waiter.wait(1000);

	if(m_quitFlag)
	{
		changeStatus(REMOTE_VIEW_DISCONNECTED);
		return;
	}

	changeStatus(REMOTE_VIEW_CONNECTED);

	KApplication::kApplication()->lock();
	m_view->grabKeyboard();
	KApplication::kApplication()->unlock();

	// start the writer thread
	m_wthread.start();

	// the main loop
	while(!m_quitFlag && (s = rdp_recv(&type)) != NULL)
	{
		switch(type)
		{
			case RDP_PDU_DEMAND_ACTIVE:
				process_demand_active(s);
				break;

			case RDP_PDU_DEACTIVATE:
				break;

			case RDP_PDU_DATA:
				KApplication::kApplication()->lock();
				process_data_pdu(s);
				KApplication::kApplication()->unlock();
				break;

			case 0:
				break;

			default:
				QCString s = "PDU %d\n";
				unimpl(s.data(), type);
		}
	}
	m_quitFlag = true;

	// start disconnecting
	changeStatus(REMOTE_VIEW_DISCONNECTING);
  
	sec_disconnect();
	changeStatus(REMOTE_VIEW_DISCONNECTED);
	m_wthread.kick();
}



// constructor
RdpWriterThread::RdpWriterThread(KRdpView *v, volatile bool &quitFlag) :
  m_view(v),
  m_quitFlag(quitFlag)
{
}

// puts an X11 event in queue, called from the Qt thread
void RdpWriterThread::queueX11Event(XEvent *e)
{
	m_lock.lock();
	if(m_x11Events.size() >= X11_QUEUE_SIZE)
	{
		m_lock.unlock();
		return;
	}

	m_x11Events.push_back(*e);
	m_waiter.wakeAll();
	m_lock.unlock();
}

void RdpWriterThread::sendFatalError(ErrorCode s)
{
	m_quitFlag = true;
	QApplication::postEvent(m_view, new FatalErrorEvent(s));
}

void RdpWriterThread::kick()
{
	m_waiter.wakeAll();
}

// starts the thread and gets input from the user
void RdpWriterThread::run()
{
	QValueList<XEvent> x11Events;

	while(!m_quitFlag)
	{
		m_lock.lock();
		x11Events = m_x11Events;

		if(x11Events.size() == 0)
		{
			m_waiter.wait(&m_lock, WAIT_PERIOD);
			m_lock.unlock();
		}
		else
		{
			m_x11Events.clear();
			m_lock.unlock();

			if(x11Events.size() != 0)
			{
				if(!sendX11Events(x11Events))
				{
					m_quitFlag = true;
					break;
				}
			}
		}
	}
	m_quitFlag = true;
}

bool RdpWriterThread::sendX11Events(const QValueList<XEvent> &events)
{
	bool locked = false;
	
	QValueList<XEvent>::const_iterator it = events.begin();
	
	while(it != events.end())
	{
		if(locked == false &&
		   (*it).type != MotionNotify && (*it).type != ButtonPress && (*it).type != ButtonRelease)
		{
			KApplication::kApplication()->lock();
			locked = true;
		}
			
		if(!xwin_process_event(*it))
		{
			if(locked == true)
				KApplication::kApplication()->unlock();
				
			return false;
		}
		it++;
	}
	
	if(locked == true)
		KApplication::kApplication()->unlock();
	
	return true;
}
