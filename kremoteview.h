/***************************************************************************
                   kremoteview.h  -  widget that shows the remote framebuffer
                             -------------------
    begin                : Wed Dec 25 23:58:12 CET 2002
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

#ifndef KREMOTEVIEW_H
#define KREMOTEVIEW_H


#include <kkeynative.h>
#include <qwidget.h>
#include "events.h"

typedef enum {
  QUALITY_UNKNOWN,
  QUALITY_HIGH,
  QUALITY_MEDIUM,
  QUALITY_LOW
} Quality;

/**
 * Generic widget that displays a remote framebuffer.
 */
class KRemoteView : public QWidget
{
	Q_OBJECT 
public:
	KRemoteView(QWidget *parent = 0, 
		    const char *name = 0, 
		    WFlags f = 0);

	virtual ~KRemoteView();

	/**
	 * Checks whether the widget is in scale mode.
	 * @return true if scaling is activated
	 */
	virtual bool scaling() = 0;

	/**
	 * Returns the resolution of the remote framebuffer.
	 * @return the remote framebuffer size
	 */
	virtual QSize framebufferSize() = 0;

	/**
	 * Initiate the disconnection. This doesn't need to happen 
	 * immediately. The call must not block.
	 * @see isQuitting()
	 */
	virtual void startQuitting() = 0;

	/**
	 * Checks whether the view is currently quitting.
	 * @return true if it is quitting
	 * @see startQuitting()
	 */
	virtual bool isQuitting() = 0;

	/**
	 * Returns the host the view is connected to.
	 * @return the host the view is connected to
	 */
	virtual QString host() = 0;

	/**
	 * Returns the port the view is connected to.
	 * @return the port the view is connected to
	 */
	virtual int port() = 0;

	/**
	 * Starts connecting. Should not block.
	 * @return true if successful (so far), false
	 *         otherwise
	 * @see connected()
	 * @see disconnected()
	 * @see disconnectedError()
	 * @see statusChanged()
	 */
	virtual bool start() = 0;
        
	/**
	 * Returns the current status of the connection.
	 * @return the status of the connection
	 */
	enum RemoteViewStatus status();

public slots:
        /**
	 * Called to enable or disable scaling.
	 * @param s true to enable, false to disable
	 */
        virtual void enableScaling(bool s) = 0;
        /**
	 * Sends a key to the remote server.
	 * @param k the key to send
	 */ 
        virtual void pressKey(KKeyNative k) = 0; 

signals:
        /**
	 * Emitted when the size of the remote screen changes. Also
	 * called when the size is known for the first time.
	 * @param x the width of the screen
	 * @param y the height of the screen
	 */ 
	void changeSize(int w, int h);

	/**
	 * Emitted when the view connected successfully.
	 */
	void connected();

	/**
	 * Emitted when the view disconnected without error.
	 */
	void disconnected();

	/**
	 * Emitted when the view disconnected with error.
	 */
	void disconnectedError();

	/**
	 * Emitted when the status of the view changed.
	 * @param s the new status
	 */
	void statusChanged(RemoteViewStatus s);

	/**
	 * Emitted when the password dialog is shown or hidden.
	 * @param b true when the dialog is shown, false when it has 
	 *               been hidden
	 */
	void showingPasswordDialog(bool b);

	/**
	 * Emitted when the mouse on the remote side has been moved.
	 * @param x the new x coordinate
	 * @param y the new y coordinate
	 * @param buttonMask the mask of mouse buttons (bit 0 for first mouse
	 *                   button, 1 for second button etc)a
	 */
	void mouseStateChanged(int x, int y, int buttonMask);

protected:
	/**
	 * The status of the remote view.
	 */
	enum RemoteViewStatus m_status;

	/**
	 * Set the status of the connection
	 * @param s the new status
	 */
	virtual void setStatus(RemoteViewStatus s);
};

#endif
