/****************************************************************************
 * **
 * ** Copyright (C) 2008 David Gross <gdavid.devel@gmail.com>
 * **
 * ** This file is part of KDE.
 * **
 * ** This program is free software; you can redistribute it and/or modify
 * ** it under the terms of the GNU General Public License as published by
 * ** the Free Software Foundation; either version 2 of the License, or
 * ** (at your option) any later version.
 * **
 * ** This program is distributed in the hope that it will be useful,
 * ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 * ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * ** GNU General Public License for more details.
 * **
 * ** You should have received a copy of the GNU General Public License
 * ** along with this program; see the file COPYING. If not, write to
 * ** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * ** Boston, MA 02110-1301, USA.
 * **
 * ****************************************************************************/

#include "nxcallbacks.h"

NxCallbacks::NxCallbacks()
{
}

NxCallbacks::~NxCallbacks()
{
}

void NxCallbacks::write(std::string msg)
{
}

void NxCallbacks::write(int num, std::string msg)
{
    emit progress(num, QString(msg.c_str()));
}

void NxCallbacks::error(std::string msg)
{
}

void NxCallbacks::debug(std::string msg)
{
}

void NxCallbacks::stdoutSignal(std::string msg) 
{
}

void NxCallbacks::stderrSignal(std::string msg)
{
}

void NxCallbacks::stdinSignal(std::string msg)
{
}

void NxCallbacks::resumeSessionsSignal(std::list<nxcl::NXResumeData> sessions)
{
    QList<nxcl::NXResumeData> qsessions;

    for(std::list<nxcl::NXResumeData>::const_iterator it = sessions.begin(); it != sessions.end(); it++) 
        qsessions << (*it);
    
    emit suspendedSessions(qsessions);
}

void NxCallbacks::noSessionsSignal() 
{
    emit noSessions();
}

void NxCallbacks::serverCapacitySignal()
{
    emit atCapacity();
}

void NxCallbacks::connectedSuccessfullySignal()
{
}

