/***************************************************************************
                   kremoteview.cpp  -  widget that shows the remote framebuffer
                             -------------------
    begin                : Wed Dec 26 00:21:14 CET 2002
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

#include "kremoteview.h"

KRemoteView::KRemoteView(QWidget *parent, 
			 const char *name, 
			 WFlags f) : 
	QWidget(parent, name, f) {
}

#include "kremoteview.moc"
