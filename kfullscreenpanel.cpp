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
#include <kdebug.h>

Counter::Counter(float start) :
	m_currentValue(start) {
	connect(&m_timer, SIGNAL(timeout()), SLOT(timeout()));
}

void Counter::count(float stop, float stepSize, float frequency) {
	m_timer.stop();
	m_stopValue = stop;
	m_stepSize = stepSize;
	m_timeoutMs = (int)(1000.0 / frequency);
	timeout();
}

void Counter::timeout() {
	if (m_stepSize < 0) {
		if (m_currentValue <= m_stopValue) {
			m_currentValue = m_stopValue;
			emit counted(m_currentValue);
			emit countingDownFinished();
			return;
		}
	} else {
		if (m_currentValue >= m_stopValue) {
			m_currentValue = m_stopValue;
			emit counted(m_currentValue);
			emit countingUpFinished();
			return;
		}
	}
	emit counted(m_currentValue);
	
	m_currentValue += m_stepSize;
	m_timer.start(m_timeoutMs, true);
}


KFullscreenPanel::KFullscreenPanel(QWidget* parent, 
				   const char *name,
				   const QSize &resolution) :
	QWidget(parent, name),
	m_child(0),
	m_layout(0),
	m_fsResolution(resolution),
	m_counter(0)
{
	connect(&m_counter, SIGNAL(countingDownFinished()), SLOT(hide()));
	connect(&m_counter, SIGNAL(counted(float)), SLOT(movePanel(float)));
}

KFullscreenPanel::~KFullscreenPanel() {
}

void KFullscreenPanel::movePanel(float posY) {
	move(x(), (int)posY);
	if (!isVisible())
		show();
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
	m_counter.count(0, height()/3.0, 24);
}

void KFullscreenPanel::startHide() {
	m_counter.count(-height(), -height()/12.0, 24);
}

void KFullscreenPanel::enterEvent(QEvent*) {
	emit mouseEnter();
}

void KFullscreenPanel::leaveEvent(QEvent*) {
	emit mouseLeave();
}


#include "kfullscreenpanel.moc"

