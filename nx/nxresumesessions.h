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

#ifndef NXRESUMESESSIONS_H
#define NXRESUMESESSIONS_H

#include "ui_nxresumesessions.h"

#include <QDialog>
#include <KDialog>
#include <nxcl/nxdata.h>

class NxResumeSessions : public KDialog
{
    Q_OBJECT

public:
    explicit NxResumeSessions(QList<nxcl::NXResumeData> sessions);
    ~NxResumeSessions();

private:
    Ui::NxSessions nxUi;
    QWidget *nxPage;

private slots:
    void pressedNew();
    void pressedResume();

signals:
    void newSession();
    void resumeSession(QString);
};

#endif
