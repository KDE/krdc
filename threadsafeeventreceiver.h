/*
 *  Makes receiving events using QThread::postEvent thread safe
 *  Copyright (C) 2002 Tim Jansen <tim@tjansen.de>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */

#ifndef __THREADSAFEEVENTRECEIVER_H
#define __THREADSAFEEVENTRECEIVER_H

#include <kapplication.h>
#include <kdebug.h>
#include <qthread.h>
#include <qmutex.h>
#include <qevent.h>

/**
 * Makes event posting from a QThread to a QObject thread safe.
 * To use it, 
 * - let the receiving class inherit from this class
 * - send all events using sendEvent() instead of QThread::postEvent()
 * - when receiving a event from a thread, call gotEvent()
 * - in the receiver's destrutor, call waitForLastEvent()
 * - if you dont want to receive the events that will be send during
 *   waitForLastEvent, check whether ignoreEvents() is true
 */
class ThreadSafeEventReceiver {
 private:
	QObject *m_receiver;
	QMutex *m_openEventLock;
	volatile int m_openEventNum;
	volatile bool m_ignoreEvents;

 public:
	ThreadSafeEventReceiver(QObject *o) :
		m_receiver(o),
		m_openEventNum(0),
		m_ignoreEvents(false) {
		m_openEventLock = new QMutex();
	}

	~ThreadSafeEventReceiver() {
		delete m_openEventLock;
	}

	void sendEvent(QEvent *e) {
		m_openEventLock->lock();
		m_openEventNum++;
		m_openEventLock->unlock();
#if QT_VERSION < 0x030200
		QThread::postEvent(m_receiver, e);
#else
		QApplication::postEvent(m_receiver, e);
#endif
	}

	void gotEvent(QEvent *) { 
		m_openEventLock->lock();
		m_openEventNum--;
		m_openEventLock->unlock();
		if (m_openEventNum < 0) 
			kdError() << "Got unexpected event!" << endl;
	}

	void waitForLastEvent() {
		int c = 0;
		int snap = 0;
		m_ignoreEvents = true;
		while (c < 3) {
			m_openEventLock->lock();
			snap = m_openEventNum;
			m_openEventLock->unlock();
			if (snap == 0)
				return;
			KApplication::kApplication()->processEvents();
			c++;
		}
	}

	bool ignoreEvents() {
		return m_ignoreEvents;
	}
};

#endif
