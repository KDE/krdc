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
#include <qvaluelist.h>

#include "vnctypes.h"


class KVncView;

enum RemoteViewStatus {
	REMOTE_VIEW_CONNECTING,
	REMOTE_VIEW_AUTHENTICATING,
	REMOTE_VIEW_PREPARING,
	REMOTE_VIEW_CONNECTED,
	REMOTE_VIEW_DISCONNECTED
};

enum ErrorCode {
	ERROR_NONE = 0,
	ERROR_INTERNAL,
	ERROR_CONNECTION,
	ERROR_PROTOCOL,
	ERROR_IO,
	ERROR_AUTHENTICATION
};

const int ScreenResizeEventType = 781001;

class ScreenResizeEvent : public QCustomEvent
{
private:
	int m_width, m_height;
public:
	ScreenResizeEvent(int w, int h) : 
		QCustomEvent(ScreenResizeEventType), 
		m_width(w),
		m_height(h) 
	{};
	int width() const { return m_width; };
	int height() const { return m_height; };
};

const int StatusChangeEventType = 781002;

class StatusChangeEvent : public QCustomEvent
{
private:
	RemoteViewStatus m_status;
public:
	StatusChangeEvent(RemoteViewStatus s) :
		QCustomEvent(StatusChangeEventType),
		m_status(s)
	{};
	RemoteViewStatus status() const { return m_status; };
};

const int PasswordRequiredEventType = 781003;

class PasswordRequiredEvent : public QCustomEvent
{
public:
	PasswordRequiredEvent() : 
		QCustomEvent(PasswordRequiredEventType)
	{};
};

const int FatalErrorEventType = 781004;

class FatalErrorEvent : public QCustomEvent
{
	ErrorCode m_error;
public:
	FatalErrorEvent(ErrorCode e) : 
		QCustomEvent(FatalErrorEventType),
		m_error(e)
	{};

	ErrorCode errorCode() { return m_error; }
};

const int DesktopInitEventType = 781005;

class DesktopInitEvent : public QCustomEvent
{
public:
	DesktopInitEvent() : 
		QCustomEvent(DesktopInitEventType)
	{};
};

const int ScreenRepaintEventType = 781006;

class ScreenRepaintEvent : public QCustomEvent
{
private:
	int m_x, m_y, m_width, m_height;
public:
	ScreenRepaintEvent(int x, int y, int w, int h) : 
		QCustomEvent(ScreenRepaintEventType), 
		m_x(x),
		m_y(y),
		m_width(w),
		m_height(h) 
	{};
	int x() const { return m_x; };
	int y() const { return m_y; };
	int width() const { return m_width; };
	int height() const { return m_height; };
};

const int BeepEventType = 781007;

class BeepEvent : public QCustomEvent
{
public:
	BeepEvent() : 
		QCustomEvent(BeepEventType)
	{};
};

struct MouseEvent {
	int x, y, buttons;
};

struct KeyEvent {
	unsigned int k;
	bool down;
};


class WriterThread : public QThread {
private:
	QMutex m_lock;
	QWaitCondition m_waiter;
	volatile bool &m_quitFlag;
	KVncView *m_view;

	// all things that can be send follow:
	int m_incrementalUpdateRQ; // for sending an incremental request
	QRegion m_updateRegionRQ;  // for sending updates, null if it is done
	QValueList<MouseEvent> m_mouseEvents; // list of unsent mouse events
	QValueList<KeyEvent> m_keyEvents;     // list of unsent key events

	void sendFatalError(ErrorCode s);

public:
	WriterThread(KVncView *v, volatile bool &quitFlag);
	
	void queueIncrementalUpdateRequest();
	void queueUpdateRequest(const QRegion &r);
	void queueMouseEvent(int x, int y, int buttonMask);
	void queueKeyEvent(unsigned int k, bool down);
	void kick();
	
protected:
	void run();
	bool sendIncrementalUpdateRequest();
	bool sendUpdateRequest(const QRegion &r);
	bool sendMouseEvents(const QValueList<MouseEvent> &events);
	bool sendKeyEvents(const QValueList<KeyEvent> &events);
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
