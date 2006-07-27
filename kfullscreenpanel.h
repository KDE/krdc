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

#ifndef KFULLSCREENPANEL_H
#define KFULLSCREENPANEL_H

#include <QWidget>
#include <QTimer>

class QVBoxLayout;
class QEvent;

class Counter : public QObject {
	Q_OBJECT
private:
	QTimer m_timer;
	float m_stopValue, m_currentValue, m_stepSize;
	int m_timeoutMs;

public:
	Counter(float start);

	void count(float stop, float stepSize, float frequency);
private slots:
	void timeout();

signals:	
	void countingDownFinished();
	void countingUpFinished();
	void counted(float value);
};


class KFullscreenPanel : public QWidget {
	Q_OBJECT 
private:
	QWidget *m_child;
	QVBoxLayout *m_layout;
	QSize    m_fsResolution;
	Counter m_counter;

	void doLayout();

public:
	KFullscreenPanel(QWidget* parent, const char *name, 
			 const QSize &resolution);
	~KFullscreenPanel();

	void setChild(QWidget *child);
	void startShow();
	void startHide();

protected:
	void enterEvent(QEvent *e);
	void leaveEvent(QEvent *e);

private slots:
	void movePanel(float posY);
	
signals:
	void mouseEnter();
        void mouseLeave(); 
};

#endif
