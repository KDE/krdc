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

#ifndef NXCALLBACKS_H
#define NXCALLBACKS_H

#include <nxcl/nxclientlib.h>
#include <nxcl/nxdata.h>

#include <list>
#include <string>
#include <QList>
#include <QObject>

class NxCallbacks : public QObject, public nxcl::NXClientLibExternalCallbacks
{
    Q_OBJECT

public:
    NxCallbacks();
    virtual ~NxCallbacks();

    virtual void write(std::string msg);
    virtual void write(int num, std::string msg);
    virtual void error(std::string msg);
    virtual void debug(std::string msg);
    virtual void stdoutSignal(std::string msg);
    virtual void stderrSignal(std::string msg);
    virtual void stdinSignal(std::string msg);
    virtual void resumeSessionsSignal(std::list<nxcl::NXResumeData> sessions);
    virtual void noSessionsSignal();
    virtual void serverCapacitySignal();
    virtual void connectedSuccessfullySignal();

signals:
    void progress(int, QString);
    void suspendedSessions(QList<nxcl::NXResumeData>);
    void noSessions();
    void atCapacity();

};


#endif
