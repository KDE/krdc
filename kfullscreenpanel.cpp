/***************************************************************************
                 kfullscreenpanel.cpp  -  auto-hideable toolbar
                             -------------------
    begin                : Tue May 13 23:07:42 CET 2002
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

#include "kfullscreenpanel.h"

KFullscreenPanel::KFullscreenPanel(QWidget* parent, 
				   const char *name,
				   const QSize &resolution) :
	QWidget(parent, name),
	m_child(0),
	m_layout(0),
	m_fsResolution(resolution)
{
}

KFullscreenPanel::~KFullscreenPanel() {
}

void KFullscreenPanel::setChild(QWidget *child) {
	
	if (m_layout)
		delete m_layout;
	m_child = child;

	m_layout = new QVBoxLayout(this);
	m_layout->addWidget(child);
	doLayout();
}

void KFullscreenPanel::doLayout() {
	QSize s = sizeHint();
	setFixedSize(s);
	setGeometry((m_fsResolution.width() - s.width())/2, 0, 
		    s.width(), s.height());	
}

void KFullscreenPanel::startShow() {
	show();
}

void KFullscreenPanel::startHide() {
	hide();
}

void KFullscreenPanel::enterEvent(QEvent*) {
	emit mouseEnter();
}

void KFullscreenPanel::leaveEvent(QEvent*) {
	emit mouseLeave();
}


#include "kfullscreenpanel.moc"

