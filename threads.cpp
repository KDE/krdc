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
#include "kdebug.h"

#include "vncviewer.h"
#include "threads.h"

static const int WAIT_PERIOD = 5000;
static const unsigned int MOUSEPRESS_QUEUE_SIZE = 10;
static const unsigned int MOUSEMOVE_QUEUE_SIZE = 5;
static const unsigned int KEY_QUEUE_SIZE = 8192;


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
	QThread::postEvent(m_view, new StatusChangeEvent(s));
}

void ControllerThread::sendFatalError(ErrorCode s) {
	m_quitFlag = true;
	QThread::postEvent(m_view, new FatalErrorEvent(s));
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
	if (!ConnectToRFBServer(m_view->host().latin1(), m_view->port())) {
		sendFatalError(ERROR_CONNECTION);
		return;
	}
	if (m_quitFlag) {
		changeStatus(REMOTE_VIEW_DISCONNECTED);
		return;
	}

        changeStatus(REMOTE_VIEW_AUTHENTICATING);

	enum InitStatus s = InitialiseRFBConnection();
	if (s == INIT_CONNECTION_FAILED) {
		sendFatalError(ERROR_IO);
		return;
	}
	else if (s == INIT_PROTOCOL_FAILURE) {
		sendFatalError(ERROR_PROTOCOL);
		return;
	}
	else if (s == INIT_AUTHENTICATION_FAILED) {
		sendFatalError(ERROR_AUTHENTICATION);
		return;
	}
	else if (s == INIT_ABORTED) {
		changeStatus(REMOTE_VIEW_DISCONNECTED);
		return;
	}

	QThread::postEvent(m_view, new ScreenResizeEvent(si.framebufferWidth, 
							 si.framebufferHeight));

	QThread::postEvent(m_view, new DesktopInitEvent());
	while ((!m_quitFlag) && (!m_desktopInitialized)) 
		m_waiter.wait(1000);

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
		if (!HandleRFBServerMessage()) {
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


WriterThread::WriterThread(KVncView *v, volatile bool &quitFlag) :
	m_quitFlag(quitFlag),
	m_view(v),
	m_clientCut(QString::null)
{
	writerThread = this;
}

bool WriterThread::sendIncrementalUpdateRequest() {
	return SendIncrementalFramebufferUpdateRequest();
}

bool WriterThread::sendUpdateRequest(const QRegion &region) {
	QMemArray<QRect> r = region.rects();
	for (unsigned int i = 0; i < r.size(); i++) 
		if (!SendFramebufferUpdateRequest(r[i].x(), 
						  r[i].y(), 
						  r[i].width(), 
						  r[i].height(), False))
			return false;
	return true;
}

bool WriterThread::sendMouseEvents(const QValueList<MouseEvent> &events) {
	QValueList<MouseEvent>::const_iterator it = events.begin();
	while (it != events.end()) {
		if (!SendPointerEvent((*it).x, (*it).y, (*it).buttons))
			return false;
		it++;
	}
	return true;
}

bool WriterThread::sendKeyEvents(const QValueList<KeyEvent> &events) {
	QValueList<KeyEvent>::const_iterator it = events.begin();
	while (it != events.end()) {
		if (!SendKeyEvent((*it).k, (*it).down ? True : False))
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


void WriterThread::queueUpdateRequest(const QRegion &r) {
	m_lock.lock();
	m_updateRegionRQ += r;
	m_waiter.wakeAll();
	m_lock.unlock();
}

void WriterThread::queueMouseEvent(int x, int y, int buttonMask) {
	MouseEvent e;
	e.x = x;
	e.y = y;
	e.buttons = buttonMask;

	m_lock.lock();
	if (m_mouseEvents.size() > 0) {
		if ((e.x == m_mouseEvents.last().x) &&
		    (e.y == m_mouseEvents.last().y) &&
		    (e.buttons == m_mouseEvents.last().buttons)) {
			m_lock.unlock();
			return;
		}
		if (m_mouseEvents.size() >= MOUSEPRESS_QUEUE_SIZE) {
			m_lock.unlock();
			return;
		}
		if ((m_mouseEvents.last().buttons == buttonMask) &&
		    (m_mouseEvents.size() >= MOUSEMOVE_QUEUE_SIZE)) {
			m_lock.unlock();
			return;
		}
	}

	m_mouseEvents.push_back(e);
	m_waiter.wakeAll();
	m_lock.unlock();
}

void WriterThread::queueKeyEvent(unsigned int k, bool down) {
	KeyEvent e;
	e.k = k;
	e.down = down;

	m_lock.lock();
	if (m_keyEvents.size() >= KEY_QUEUE_SIZE) {
		m_lock.unlock();
		return;
	}

	m_keyEvents.push_back(e);
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
	QRegion updateRegionRQ;
	QValueList<MouseEvent> mouseEvents;
	QValueList<KeyEvent> keyEvents;
	QString clientCut;

	while (!m_quitFlag) {
		m_lock.lock();
		incrementalUpdateRQ = m_incrementalUpdateRQ;
		updateRegionRQ = m_updateRegionRQ;
		mouseEvents = m_mouseEvents;
		keyEvents = m_keyEvents;
		clientCut = m_clientCut;

		if ((!incrementalUpdateRQ) &&
		    (updateRegionRQ.isNull()) &&
		    (mouseEvents.size() == 0) &&
		    (keyEvents.size() == 0) && 
		    (clientCut.isNull())) {
			m_waiter.wait(&m_lock, WAIT_PERIOD);
			m_lock.unlock();
		}
		else {
			m_incrementalUpdateRQ = false;
			m_updateRegionRQ = QRegion();
			m_mouseEvents.clear(); 
			m_keyEvents.clear();
			m_clientCut = QString::null;
			m_lock.unlock();

			if (incrementalUpdateRQ) 
				if (!sendIncrementalUpdateRequest()) {
					sendFatalError(ERROR_IO);
					break;
				}
			if (!updateRegionRQ.isNull())
				if (!sendUpdateRequest(updateRegionRQ)) {
					sendFatalError(ERROR_IO);
					break;
				}
			if (mouseEvents.size() != 0)
				if (!sendMouseEvents(mouseEvents)) {
					sendFatalError(ERROR_IO);
					break;
				}
			if (keyEvents.size() != 0)
				if (!sendKeyEvents(keyEvents)) {
					sendFatalError(ERROR_IO);
					break;
				}
			if (!clientCut.isNull()) 
				if (!SendClientCutText(clientCut.latin1(), 
						       (int)clientCut.length())) {
					sendFatalError(ERROR_IO);
					break;
				}
		}
	}
	m_quitFlag = true;
}

void WriterThread::sendFatalError(ErrorCode s) {
	m_quitFlag = true;
	QThread::postEvent(m_view, new FatalErrorEvent(s));
}

