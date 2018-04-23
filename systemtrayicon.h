/****************************************************************************
**
** Copyright (C) 2008 Urs Wolfer <uwolfer @ kde.org>
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

#ifndef SYSTEMTRAYICON_H
#define SYSTEMTRAYICON_H

#include <KNotifications/KStatusNotifierItem>

class MainWindow;

class SystemTrayIcon : public KStatusNotifierItem
{
    Q_OBJECT

public:
    explicit SystemTrayIcon(MainWindow *parent);
    ~SystemTrayIcon() override;

public Q_SLOTS:
    void checkActivatedWindow(bool active);

private:
    MainWindow *m_mainWindow;
};

#endif
