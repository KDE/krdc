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

#ifndef BOOKMARKMANAGER_H
#define BOOKMARKMANAGER_H

#include <KBookmarkManager>

class KActionCollection;
class KBookmarkMenu;
class KMenu;
class KUrl;

class MainWindow;

class BookmarkManager : public QObject, public KBookmarkOwner
{
    Q_OBJECT

public:
    BookmarkManager(KActionCollection *collection, KMenu *menu, MainWindow *parent);
    ~BookmarkManager();

    virtual QString currentUrl() const;
    virtual QString currentTitle() const;
    virtual bool addBookmarkEntry() const;
    virtual bool editBookmarkEntry() const;
    virtual bool supportsTabs() const;
    virtual QList<QPair<QString, QString> > currentBookmarkList() const;
    void addHistoryBookmark();

signals:
    void openUrl(const KUrl &url);

private slots:
    void openBookmark(const KBookmark &bm, Qt::MouseButtons, Qt::KeyboardModifiers);
    void openFolderinTabs(const KBookmarkGroup &bookmarkGroup);

private:
    KMenu *m_menu;
    KBookmarkMenu *m_bookmarkMenu;
    KBookmarkManager *m_manager;
    KBookmarkGroup m_historyGroup;

    MainWindow *m_mainWindow;
};

#endif
