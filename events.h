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

#include "kremoteview.h"

#include <kdebug.h>
#include <QCustomEvent>

const int ScreenResizeEventType = 41001;

class ScreenResizeEvent : public QEvent
{
private:
	int m_width, m_height;
public:
	ScreenResizeEvent(int w, int h) :
	        QEvent( (Type)ScreenResizeEventType ),
		m_width(w),
		m_height(h)
	{
	  // kDebug() << "New ScreenResize: " << w << ", " << h << endl;
	};
	int width() const { return m_width; };
	int height() const { return m_height; };
	Type type() const { return (Type) ScreenResizeEventType; };
};

const int StatusChangeEventType = 41002;

class StatusChangeEvent : public QEvent
{
private:
	KRemoteView::RemoteStatus m_status;
public:
	StatusChangeEvent(KRemoteView::RemoteStatus s) :
	        QEvent( (Type)StatusChangeEventType ),
		m_status(s)
	{};
	KRemoteView::RemoteStatus status() const { return m_status; };
	Type type() const { return (Type)StatusChangeEventType; };
};

const int PasswordRequiredEventType = 41003;

class PasswordRequiredEvent : public QEvent
{
public:
	PasswordRequiredEvent() :
	        QEvent( (Type)PasswordRequiredEventType )
	{};
	Type type() const { return (Type)PasswordRequiredEventType; };
};

const int FatalErrorEventType = 41004;

class FatalErrorEvent : public QEvent
{
	KRemoteView::ErrorCode m_error;
public:
	FatalErrorEvent(KRemoteView::ErrorCode e) :
	        QEvent( (Type)FatalErrorEventType ),
		m_error(e)
	{};

	KRemoteView::ErrorCode errorCode() const { return m_error; };
	Type type() const { return (Type)FatalErrorEventType; };
};

const int DesktopInitEventType = 41005;

class DesktopInitEvent : public QEvent
{
public:
        DesktopInitEvent() : QEvent( (Type)DesktopInitEventType )
	  {};
	  Type type() const { return (Type)DesktopInitEventType; };
};

const int ScreenRepaintEventType = 41006;

class ScreenRepaintEvent : public QEvent
{
private:
	int m_x, m_y, m_width, m_height;
public:
	ScreenRepaintEvent(int x, int y, int w, int h) :
	        QEvent( (Type)ScreenRepaintEventType ),
		m_x(x),
		m_y(y),
		m_width(w),
		m_height(h)
	{};
	int x() const { return m_x; };
	int y() const { return m_y; };
	int width() const { return m_width; };
	int height() const { return m_height; };
	Type type() const { return (Type)ScreenRepaintEventType; };
};

const int BeepEventType = 41007;

class BeepEvent : public QEvent
{
public:
        BeepEvent() : QEvent( (Type)BeepEventType)
	  {};
	Type type() const { return (Type)BeepEventType; };
};

const int ServerCutEventType = 41008;

class ServerCutEvent : public QEvent
{
private:
	char *m_bytes;
	int m_length;
public:
	ServerCutEvent(char *bytes, int length) :
	        QEvent( (Type)ServerCutEventType ),
		m_bytes(bytes),
		m_length(length)
	{};
	~ServerCutEvent() {
		free(m_bytes);
	}
	int length() const { return m_length; };
	char *bytes() const { return m_bytes; };
	Type type() const { return (Type)ServerCutEventType; };
};

const int MouseStateEventType = 41009;

class MouseStateEvent : public QEvent
{
private:
	int m_x, m_y, m_buttonMask;
public:
	MouseStateEvent(int x, int y, int buttonMask) :
	        QEvent( (Type)MouseStateEventType ),
		m_x(x),
		m_y(y),
		m_buttonMask(buttonMask)
	{};
	~MouseStateEvent() {
	}
	int x() const { return m_x; };
	int y() const { return m_y; };
	Type buttonMask() const { return (Type)m_buttonMask; };
};

const int WalletOpenEventType = 41010;

class WalletOpenEvent : public QEvent
{
public:
	WalletOpenEvent() :
	        QEvent( (Type)WalletOpenEventType )
	{};
};

#endif
