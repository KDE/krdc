/*
     threads.h, declaration of threading classes
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

#ifndef RDP_THREADS_H
#define RDP_THREADS_H

#include <qthread.h>
#include <qregion.h>
#include <qrect.h>
#include <qmutex.h>
#include <qwaitcondition.h>
#include <qevent.h>
#include <qvaluelist.h>

class KRdpView;


// the RdpWriterThread is the thread that communicates with the user
class RdpWriterThread : public QThread
{
	public:
		RdpWriterThread(KRdpView *krdpview, volatile bool &quitFlag);

		void queueX11Event(XEvent *e);
		void queueClientCut(const QString &text);
		void kick();

	private:
		QMutex m_lock;
		QWaitCondition m_waiter;
		KRdpView *m_view;
		volatile bool &m_quitFlag;

		// all things that can be send follow:
		QValueList<XEvent> m_x11Events;            // list of unsent x11 events

		void sendFatalError(ErrorCode s);

	protected:
		void run();
		bool sendX11Events(const QValueList<XEvent> &events);
};



// the RdpControllerThread is the thread that communicates with the RDP server
class RdpControllerThread : public QThread
{
	public:
		RdpControllerThread(KRdpView *v, RdpWriterThread &wt, volatile bool &quitFlag);
		void desktopInit();
		void kick();
  
	private:
		KRdpView *m_view;
		enum RemoteViewStatus m_status;
		RdpWriterThread &m_wthread;
		volatile bool &m_quitFlag;
		volatile bool m_desktopInitialized;
		QWaitCondition m_waiter;

		void changeStatus(RemoteViewStatus s);
		void sendFatalError(ErrorCode s);

	protected:
		void run();
};

#endif
