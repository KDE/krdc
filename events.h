/***************************************************************************
                           events.h  - QCustomEvents
                             -------------------
    begin                : Wed Jun 05 01:13:42 CET 2002
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

#include <stdlib.h> 

#ifndef EVENTS_H
#define EVENTS_H

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
	ERROR_NAME,
	ERROR_NO_SERVER,
	ERROR_SERVER_BLOCKED,
	ERROR_AUTHENTICATION
};

const int ScreenResizeEventType = 41001;

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

const int StatusChangeEventType = 41002;

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

const int PasswordRequiredEventType = 41003;

class PasswordRequiredEvent : public QCustomEvent
{
public:
	PasswordRequiredEvent() : 
		QCustomEvent(PasswordRequiredEventType)
	{};
};

const int FatalErrorEventType = 41004;

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

const int DesktopInitEventType = 41005;

class DesktopInitEvent : public QCustomEvent
{
public:
	DesktopInitEvent() : 
		QCustomEvent(DesktopInitEventType)
	{};
};

const int ScreenRepaintEventType = 41006;

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

const int BeepEventType = 41007;

class BeepEvent : public QCustomEvent
{
public:
	BeepEvent() : 
		QCustomEvent(BeepEventType)
	{};
};

const int ServerCutEventType = 41008;

class ServerCutEvent : public QCustomEvent
{
private:
	char *m_bytes;
	int m_length;
public:
	ServerCutEvent(char *bytes, int length) :
		QCustomEvent(ServerCutEventType),
		m_bytes(bytes),
		m_length(length)
	{};
	~ServerCutEvent() {
		free(m_bytes);
	}
	int length() const { return m_length; };
	char *bytes() const { return m_bytes; };
};

const int MouseStateEventType = 41009;

class MouseStateEvent : public QCustomEvent
{
private:
	int m_x, m_y, m_buttonMask;
public:
	MouseStateEvent(int x, int y, int buttonMask) :
		QCustomEvent(MouseStateEventType),
		m_x(x),
		m_y(y),
		m_buttonMask(buttonMask)
	{};
	~MouseStateEvent() {
	}
	int x() const { return m_x; };
	int y() const { return m_y; };
	int buttonMask() const { return m_buttonMask; };
};

#endif
