/***************************************************************************
                   kremoteview.cpp  -  widget that shows the remote framebuffer
                             -------------------
    begin                : Wed Dec 26 00:21:14 CET 2002
    copyright            : (C) 2002-2003 by Tim Jansen
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

#include "kremoteview.h"

KRemoteView::KRemoteView(QWidget *parent,
			 const char *name,
			 Qt::WFlags f) :
	QWidget(parent),
        m_status(REMOTE_VIEW_DISCONNECTED)
{
        setObjectName( name );
	setWindowFlags( f );
}

enum RemoteViewStatus KRemoteView::status() {
	return m_status;
}

void KRemoteView::setStatus(RemoteViewStatus s) {
	if (m_status == s)
		return;

	if (((1+(int)m_status) != (int)s) &&
	    (s != REMOTE_VIEW_DISCONNECTED)) {
		// follow state transition rules

		if (s == REMOTE_VIEW_DISCONNECTING) {
              	    if (m_status == REMOTE_VIEW_DISCONNECTED)
			return;
		}
		else {
			Q_ASSERT(((int) s) >= 0);
			if (((int)m_status) > ((int)s) ) {
				m_status = REMOTE_VIEW_DISCONNECTED;
				emit statusChanged(REMOTE_VIEW_DISCONNECTED);
			}
			// smooth state transition
			int origState = (int)m_status;
			for (int i = origState; i < (int)s; i++) {
				m_status = (RemoteViewStatus) i;
				emit statusChanged((RemoteViewStatus) i);
			}
		}
	}
	m_status = s;
	emit statusChanged(m_status);
}

KRemoteView::~KRemoteView() {
}

bool KRemoteView::supportsScaling() const {
	return false;
}

bool KRemoteView::supportsLocalCursor() const {
	return false;
}

void KRemoteView::showDotCursor(DotCursorState) {
}

DotCursorState KRemoteView::dotCursorState() const {
	return DOT_CURSOR_OFF;
}

bool KRemoteView::scaling() const {
	return false;
}

void KRemoteView::enableScaling(bool) {
}

void KRemoteView::switchFullscreen(bool) {
}

#include "kremoteview.moc"
