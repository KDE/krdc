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
    void addManualBookmark(const QString &url, const QString &text);
    KBookmarkManager* getManager();
    // removes all bookmarks with url, possibly ignore the history folder and update it's title there if it's set
    static void removeByUrl(KBookmarkManager *manager, const QString &url, bool ignoreHistory = false, const QString updateTitle = QString());
    static void updateTitle(KBookmarkManager *manager, const QString &url, const QString &title);
    // returns a QStringList for all bookmarks that point to this url using KBookmark::address()
    static QStringList findBookmarkAddresses(KBookmarkManager *manager, const QString &url);

signals:
    void openUrl(const KUrl &url);

private slots:
    void openBookmark(const KBookmark &bm, Qt::MouseButtons, Qt::KeyboardModifiers);
    void openFolderinTabs(const KBookmarkGroup &bookmarkGroup);

private:
    KBookmarkMenu *m_bookmarkMenu;
    KBookmarkManager *m_manager;
    KBookmarkGroup m_historyGroup;

    MainWindow *m_mainWindow;
    static void findBookmarkAddressesRecursive(QStringList *bookmarkAddresses, const KBookmarkGroup &group, const QString &url);
};

#endif
