/****************************************************************************
**
** Copyright (C) 2008 David Gross <gdavid.devel@gmail.com>
**
** This file is part of KDE.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; see the file COPYING. If not, write to
** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
** Boston, MA 02110-1301, USA.
**
****************************************************************************/

#include "nxresumesessions.h"

#include <QTreeWidget>
#include <QTreeWidgetItem>

NxResumeSessions::NxResumeSessions() 
{
    nxUi.setupUi(this);

    connect(nxUi.buttonNew, SIGNAL(pressed()), this, SLOT(pressedNew()));
    connect(nxUi.buttonResume, SIGNAL(pressed()), this, SLOT(pressedResume()));
}

NxResumeSessions::~NxResumeSessions()
{
    if(!empty())
        clear();
}

bool NxResumeSessions::empty() const
{
    return false;
}

void NxResumeSessions::clear()
{

}

void NxResumeSessions::addSessions(QList<nxcl::NXResumeData> sessions) 
{
    QTreeWidgetItem *tmp = NULL;

    for(int i = 0; i < sessions.size(); i++)
    {
        tmp = new QTreeWidgetItem();
	tmp->setText(0, QString::number(sessions.at(i).display));
	tmp->setText(1, QString(sessions.at(i).sessionType.c_str()));
	tmp->setText(2, QString(sessions.at(i).sessionID.c_str()));
	tmp->setText(3, QString::number(sessions.at(i).depth));
	tmp->setText(4, QString(sessions.at(i).screen.c_str()));
	tmp->setText(5, QString(sessions.at(i).sessionName.c_str()));

        nxUi.sessionsList->addTopLevelItem(tmp);
    }
}

void NxResumeSessions::pressedNew()
{
    emit newSession();
    hide();
}

void NxResumeSessions::pressedResume()
{
    emit resumeSession(QString(""));
    hide();
}

