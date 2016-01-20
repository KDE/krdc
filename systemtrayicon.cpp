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

#include "systemtrayicon.h"
#include "mainwindow.h"

#include <KXmlGui/KActionCollection>
#include <KLocalizedString>

#include <QMenu>

SystemTrayIcon::SystemTrayIcon(MainWindow *parent)
        : KStatusNotifierItem(parent),
        m_mainWindow(parent)
{
    setIconByName(QLatin1String("krdc"));
    setStatus(KStatusNotifierItem::Active);
    setCategory(KStatusNotifierItem::ApplicationStatus);

    setToolTipIconByName(QLatin1String("krdc"));
    setToolTipTitle(i18n("KDE Remote Desktop Client"));

    contextMenu()->addSeparator();
    contextMenu()->addAction(parent->actionCollection()->action(QLatin1String("bookmark")));
    contextMenu()->addSeparator();

    connect(this, SIGNAL(activateRequested(bool,QPoint)), this, SLOT(checkActivatedWindow(bool)));
}

void SystemTrayIcon::checkActivatedWindow(bool active)
{
    // make sure the fullscreen window stays fullscreen by restoring the FullScreen state upon restore.
    if(active && associatedWidget() != m_mainWindow) {
        associatedWidget()->setWindowState(Qt::WindowFullScreen);
    }
}

SystemTrayIcon::~SystemTrayIcon()
{
}

