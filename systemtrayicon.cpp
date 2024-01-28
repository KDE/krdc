/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "systemtrayicon.h"
#include "mainwindow.h"

#include <KActionCollection>
#include <KLocalizedString>

#include <QMenu>

SystemTrayIcon::SystemTrayIcon(MainWindow *parent)
    : KStatusNotifierItem(parent)
    , m_mainWindow(parent)
{
    setIconByName(QLatin1String("krdc"));
    setStatus(KStatusNotifierItem::Active);
    setCategory(KStatusNotifierItem::ApplicationStatus);

    setToolTipIconByName(QLatin1String("krdc"));
    setToolTipTitle(i18n("KDE Remote Desktop Client"));

    contextMenu()->addSeparator();
    contextMenu()->addAction(parent->actionCollection()->action(QLatin1String("bookmark")));
    contextMenu()->addSeparator();

    connect(this, SIGNAL(activateRequested(bool, QPoint)), this, SLOT(checkActivatedWindow(bool)));
}

void SystemTrayIcon::checkActivatedWindow(bool active)
{
    // make sure the fullscreen window stays fullscreen by restoring the FullScreen state upon restore.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (active && associatedWidget() != m_mainWindow) {
        associatedWidget()->setWindowState(Qt::WindowFullScreen);
    }
#else
    if (active && associatedWindow() != m_mainWindow->windowHandle()) {
        associatedWindow()->setWindowState(Qt::WindowFullScreen);
    }
#endif
}

SystemTrayIcon::~SystemTrayIcon()
{
}
