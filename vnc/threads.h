/***************************************************************************
                          threads.h  -  threads for kvncview
                             -------------------
    begin                : Thu May 09 16:01:42 CET 2002
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

#ifndef THREADS_H
#define THREADS_H

#include <qthread.h>
#include <qregion.h>
#include <qrect.h>
#include <qmutex.h>
#include <qwaitcondition.h>
#include <qevent.h>
#include <q3valuelist.h>
#include <qdatetime.h>

#include <stdlib.h> 

#include "events.h"
#include "vnctypes.h"

class KVncView;

enum EventType {
	MouseEventType,
	KeyEventType
};


struct MouseEvent {
	int x, y, buttons;
};

struct KeyEvent {
	unsigned int k;
	bool down;
};

struct InputEvent {
	EventType type;
	union {
		MouseEvent m;
		KeyEvent k;
	} e;
};


class WriterThread : public QThread {
private:
	QMutex m_lock;
	QWaitCondition m_waiter;
	volatile bool &m_quitFlag;
	KVncView *m_view;

	QTime m_lastIncrUpdate; // start()ed when a incr update is sent
	bool m_lastIncrUpdatePostponed;

	// all things that can be send follow:
	bool m_incrementalUpdateRQ; // for sending an incremental request
	bool m_incrementalUpdateAnnounced; // set when a RQ will come soon
	QRegion m_updateRegionRQ;  // for sending updates, null if it is done
	Q3ValueList<InputEvent> m_inputEvents; // list of unsent input events
	MouseEvent m_lastMouseEvent;
	int m_mouseEventNum, m_keyEventNum;
	QString m_clientCut;

	void sendFatalError(ErrorCode s);

public:
	WriterThread(KVncView *v, volatile bool &quitFlag);
	
	void queueIncrementalUpdateRequest();
	void announceIncrementalUpdateRequest();
	void queueUpdateRequest(const QRegion &r);
	void queueMouseEvent(int x, int y, int buttonMask);
	void queueKeyEvent(unsigned int k, bool down);
	void queueClientCut(const QString &text);
	void kick();
	
protected:
	void run();
	bool sendIncrementalUpdateRequest();
	bool sendUpdateRequest(const QRegion &r);
	bool sendInputEvents(const Q3ValueList<InputEvent> &events);
};



class ControllerThread : public QThread { 
private:
	KVncView *m_view;
	enum RemoteViewStatus m_status;
	WriterThread &m_wthread;
	volatile bool &m_quitFlag;
	volatile bool m_desktopInitialized;
	QWaitCondition m_waiter;

	void changeStatus(RemoteViewStatus s);
	void sendFatalError(ErrorCode s);

public:
	ControllerThread(KVncView *v, WriterThread &wt, volatile bool &quitFlag);
	enum RemoteViewStatus status();	
	void desktopInit();
	void kick();

protected:
	void run();
};



#endif
