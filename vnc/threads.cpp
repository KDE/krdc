/***************************************************************************
                            threads.cpp  -  threads
                             -------------------
    begin                : Thu May 09 17:01:44 CET 2002
    copyright            : (C) 2002 by Tim Jansen
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

#include <kdebug.h>
#include <kapplication.h>

#include "vncviewer.h"
#include "threads.h"

#include <QList>
#include <QVector>

// Maximum idle time for writer thread in ms. When it timeouts, it will request
// another incremental update. Must be smaller than the timeout of the server
// (krfb's is 20s).
static const int MAXIMUM_WAIT_PERIOD = 8000;

// time to postpone incremental updates that have not been requested explicitly
static const int POSTPONED_INCRRQ_WAIT_PERIOD = 110;

static const int MOUSEPRESS_QUEUE_SIZE = 5;
static const int MOUSEMOVE_QUEUE_SIZE = 3;
static const int KEY_QUEUE_SIZE = 8192;


ControllerThread::ControllerThread(KVncView *v, WriterThread &wt, volatile bool &quitFlag) :
	m_view(v),
	m_status(REMOTE_VIEW_CONNECTING),
	m_wthread(wt),
	m_quitFlag(quitFlag),
	m_desktopInitialized(false)
{
}

void ControllerThread::changeStatus(RemoteViewStatus s) {
	m_status = s;
	QApplication::postEvent(m_view, new StatusChangeEvent(s));
}

void ControllerThread::sendFatalError(ErrorCode s) {
	m_quitFlag = true;
	QApplication::postEvent(m_view, new FatalErrorEvent(s));
	m_wthread.kick();
}

/*
 * Calls this from the X11 thread
 */
void ControllerThread::desktopInit() {
	SetVisualAndCmap();
	ToplevelInit();
	DesktopInit(m_view->winId());
	m_desktopInitialized = true;
	m_waiter.wakeAll();
}

void ControllerThread::kick() {
	m_waiter.wakeAll();
}

void ControllerThread::run() {
	int fd;
	fd = ConnectToRFBServer(m_view->host().toLatin1(), m_view->port());
	if (fd < 0) {
		if (fd == -(int)INIT_NO_SERVER)
			sendFatalError(ERROR_NO_SERVER);
		else if (fd == -(int)INIT_NAME_RESOLUTION_FAILURE)
			sendFatalError(ERROR_NAME);
		else
			sendFatalError(ERROR_CONNECTION);
		return;
	}
	if (m_quitFlag) {
		changeStatus(REMOTE_VIEW_DISCONNECTED);
		return;
	}

        changeStatus(REMOTE_VIEW_AUTHENTICATING);

	enum InitStatus s = InitialiseRFBConnection();
	if (s != INIT_OK) {
		if (s == INIT_CONNECTION_FAILED)
			sendFatalError(ERROR_IO);
		else if (s == INIT_SERVER_BLOCKED)
			sendFatalError(ERROR_SERVER_BLOCKED);
		else if (s == INIT_PROTOCOL_FAILURE)
			sendFatalError(ERROR_PROTOCOL);
		else if (s == INIT_AUTHENTICATION_FAILED)
			sendFatalError(ERROR_AUTHENTICATION);
		else if (s == INIT_ABORTED)
			changeStatus(REMOTE_VIEW_DISCONNECTED);
		else
			sendFatalError(ERROR_INTERNAL);
		return;
	}

	kDebug() << "Got VNC connection (INIT_OK)" << endl;

	QApplication::postEvent(m_view,
				new ScreenResizeEvent(si.framebufferWidth,
						      si.framebufferHeight));
	m_wthread.queueUpdateRequest(QRegion(QRect(0,0,si.framebufferWidth,
						   si.framebufferHeight)));

	QApplication::postEvent(m_view, new DesktopInitEvent());
	// FIXME: check if this is necessary. Qt4 removed the wait method
	// not requiring a mutex, but it might be sufficient to e.g. call
	// QThread::currentThread()->wait(1000) here.  I'm not familiar
	// with this code; maybe it is critical that we can suddenly break
	// out of this loop in less than a second, which requires we use
	// m_waiter.wait.
	QMutex mutex;
	while ((!m_quitFlag) && (!m_desktopInitialized)) {
		mutex.lock();
		m_waiter.wait(&mutex, 1000);
		mutex.unlock();
	}

	if (m_quitFlag) {
		changeStatus(REMOTE_VIEW_DISCONNECTED);
		return;
	}

	changeStatus(REMOTE_VIEW_PREPARING);

	if (!SetFormatAndEncodings()) {
		sendFatalError(ERROR_INTERNAL);
		return;
	}

	changeStatus(REMOTE_VIEW_CONNECTED);

	m_wthread.start();

	while (!m_quitFlag) {
		if ((!HandleRFBServerMessage()) && (!m_quitFlag)) {
			sendFatalError(ERROR_IO);
			return;
		}
	}

	m_quitFlag = true;
	changeStatus(REMOTE_VIEW_DISCONNECTED);
	m_wthread.kick();
}

enum RemoteViewStatus ControllerThread::status() {
	return m_status;
}





static WriterThread *writerThread;
void queueIncrementalUpdateRequest() {
	writerThread->queueIncrementalUpdateRequest();
}

void announceIncrementalUpdateRequest() {
	writerThread->announceIncrementalUpdateRequest();
}


WriterThread::WriterThread(KVncView *v, volatile bool &quitFlag) :
	m_quitFlag(quitFlag),
	m_view(v),
	m_lastIncrUpdatePostponed(false),
	m_incrementalUpdateRQ(false),
	m_incrementalUpdateAnnounced(false),
	m_mouseEventNum(0),
	m_keyEventNum(0),
	m_clientCut(QString::null)
{
	writerThread = this;
	m_lastIncrUpdate.start();
}

bool WriterThread::sendIncrementalUpdateRequest() {
	m_lastIncrUpdate.restart();
	return SendIncrementalFramebufferUpdateRequest();
}

bool WriterThread::sendUpdateRequest(const QRegion &region) {
	QVector<QRect> r = region.rects();
	for (int i = 0; i < r.size(); i++)
		if (!SendFramebufferUpdateRequest(r[i].x(),
						  r[i].y(),
						  r[i].width(),
						  r[i].height(), False))
			return false;
	return true;
}

bool WriterThread::sendInputEvents(const QList<InputEvent> &events) {
	QList<InputEvent>::const_iterator it = events.begin();
	while (it != events.end()) {
		if ((*it).type == KeyEventType) {
			if (!SendKeyEvent((*it).e.k.k, (*it).e.k.down ? True : False))
				return false;
		}
		else
			if (!SendPointerEvent((*it).e.m.x, (*it).e.m.y, (*it).e.m.buttons))
				return false;
		it++;
	}
	return true;
}

void WriterThread::queueIncrementalUpdateRequest() {
	m_lock.lock();
	m_incrementalUpdateRQ = true;
	m_waiter.wakeAll();
	m_lock.unlock();
}

void WriterThread::announceIncrementalUpdateRequest() {
	m_lock.lock();
	m_incrementalUpdateAnnounced = true;
	m_lock.unlock();
}


void WriterThread::queueUpdateRequest(const QRegion &r) {
	m_lock.lock();
	m_updateRegionRQ += r;
	m_waiter.wakeAll();
	m_lock.unlock();
}

void WriterThread::queueMouseEvent(int x, int y, int buttonMask) {
	InputEvent e;
	e.type = MouseEventType;
	e.e.m.x = x;
	e.e.m.y = y;
	e.e.m.buttons = buttonMask;

	m_lock.lock();
	if (m_mouseEventNum > 0) {
		if ((e.e.m.x == m_lastMouseEvent.x) &&
		    (e.e.m.y == m_lastMouseEvent.y) &&
		    (e.e.m.buttons == m_lastMouseEvent.buttons)) {
			m_lock.unlock();
			return;
		}
		if (m_mouseEventNum >= MOUSEPRESS_QUEUE_SIZE) {
			m_lock.unlock();
			return;
		}
		if ((m_lastMouseEvent.buttons == buttonMask) &&
		    (m_mouseEventNum >= MOUSEMOVE_QUEUE_SIZE)) {
			m_lock.unlock();
			return;
		}
	}

	m_mouseEventNum++;
	m_lastMouseEvent = e.e.m;

	m_inputEvents.push_back(e);
	m_waiter.wakeAll();
	m_lock.unlock();
}

void WriterThread::queueKeyEvent(unsigned int k, bool down) {
	InputEvent e;
	e.type = KeyEventType;
	e.e.k.k = k;
	e.e.k.down = down;

	m_lock.lock();
	if (m_keyEventNum >= KEY_QUEUE_SIZE) {
		m_lock.unlock();
		return;
	}

	m_keyEventNum++;
	m_inputEvents.push_back(e);
	m_waiter.wakeAll();
	m_lock.unlock();
}

void WriterThread::queueClientCut(const QString &text) {
	m_lock.lock();

	m_clientCut = text;

	m_waiter.wakeAll();
	m_lock.unlock();
}

void WriterThread::kick() {
	m_waiter.wakeAll();
}

void WriterThread::run() {
	bool incrementalUpdateRQ = false;
	bool incrementalUpdateAnnounced = false;
	QRegion updateRegionRQ;
	QList<InputEvent> inputEvents;
	QString clientCut;

	while (!m_quitFlag) {
		m_lock.lock();
		incrementalUpdateRQ = m_incrementalUpdateRQ;
		incrementalUpdateAnnounced = m_incrementalUpdateAnnounced;
		updateRegionRQ = m_updateRegionRQ;
		inputEvents = m_inputEvents;
		clientCut = m_clientCut;

		if ((!incrementalUpdateRQ) &&
		    (updateRegionRQ.isEmpty()) &&
		    (inputEvents.size() == 0) &&
		    (clientCut.isNull())) {
			if (!m_waiter.wait(&m_lock,
					   m_lastIncrUpdatePostponed ?
					   POSTPONED_INCRRQ_WAIT_PERIOD : MAXIMUM_WAIT_PERIOD))
				m_incrementalUpdateRQ = true;
			m_lock.unlock();
		}
		else {
			m_incrementalUpdateRQ = false;
			m_incrementalUpdateAnnounced = false;
			m_updateRegionRQ = QRegion();
			m_inputEvents.clear();
			m_keyEventNum = 0;
			m_mouseEventNum = 0;
			m_clientCut.clear();
			m_lock.unlock();

			// always send incremental update, unless
			// a) a framebuffer update is done ATM and will do the request later, or
			// b) the last unrequested update has been done less than 0.1s ago
			//
			// if the update has not been done because of b, postpone it.
			if (incrementalUpdateRQ || !incrementalUpdateAnnounced) {
				bool sendUpdate;
				if (incrementalUpdateRQ) {
					sendUpdate = true;
					m_lastIncrUpdatePostponed = false;
				}
				else {
					if (m_lastIncrUpdate.elapsed() < 100) {
						sendUpdate = false;
						m_lastIncrUpdatePostponed = true;
					}
					else {
						sendUpdate = true;
						m_lastIncrUpdatePostponed = false;
					}
				}

				if (sendUpdate)
					if (!sendIncrementalUpdateRequest()) {
						sendFatalError(ERROR_IO);
						break;
					}
			}
			else
				m_lastIncrUpdatePostponed = false;

			if (!updateRegionRQ.isEmpty())
				if (!sendUpdateRequest(updateRegionRQ)) {
					sendFatalError(ERROR_IO);
					break;
				}
			if (inputEvents.size() != 0)
				if (!sendInputEvents(inputEvents)) {
					sendFatalError(ERROR_IO);
					break;
				}
			if (!clientCut.isNull())  {
				QByteArray cutTextUtf8(clientCut.toUtf8());
				if (!SendClientCutText(cutTextUtf8.data(), cutTextUtf8.length())) {
					sendFatalError(ERROR_IO);
					break;
				}
			}
		}
	}
	m_quitFlag = true;
}

void WriterThread::sendFatalError(ErrorCode s) {
	m_quitFlag = true;
	QApplication::postEvent(m_view, new FatalErrorEvent(s));
}

