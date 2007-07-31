/****************************************************************************
**
** Copyright (C) 2007 Urs Wolfer <uwolfer @ kde.org>
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

#include "bookmarkmanager.h"

#include "mainwindow.h"

#include <KBookmarkMenu>
#include <KStandardDirs>

BookmarkManager::BookmarkManager(KActionCollection *collection, KMenu *menu, MainWindow *parent)
  : QObject(parent),
    KBookmarkOwner(),
    m_mainWindow(parent)
{
    m_menu = menu;

    QString file = KStandardDirs::locateLocal("data", "krdc/bookmarks.xml");

    KBookmarkManager *manager = KBookmarkManager::managerForFile(file, "krdc");

    manager->setUpdate(true);

    m_bookmarkMenu = new KBookmarkMenu(manager, this, m_menu, collection );
}

BookmarkManager::~BookmarkManager()
{
}

void BookmarkManager::openBookmark(const KBookmark &bm, Qt::MouseButtons, Qt::KeyboardModifiers)
{
    emit openUrl(bm.url());
}

bool BookmarkManager::addBookmarkEntry() const
{
    return true;
}

bool BookmarkManager::editBookmarkEntry() const
{
    return true;
}

QString BookmarkManager::currentUrl() const
{
    if (m_mainWindow->currentRemoteView() >= 0)
        return m_mainWindow->remoteViewList().at(m_mainWindow->currentRemoteView())
            ->url().prettyUrl(KUrl::RemoveTrailingSlash);
    else
        return QString();
}

QString BookmarkManager::currentTitle() const
{
    return currentUrl();
}

bool BookmarkManager::supportsTabs() const
{
    return true;
}

QList<QPair<QString, QString> > BookmarkManager::currentBookmarkList() const
{
    QList<QPair<QString, QString> > list;

    QListIterator<RemoteView *> iter(m_mainWindow->remoteViewList());

    while (iter.hasNext()) {
        RemoteView *next = iter.next();
        QString url = next->url().prettyUrl(KUrl::RemoveTrailingSlash);
        list << QPair<QString,QString>(url, url);
    }

    return list;
}

#include "bookmarkmanager.moc"
