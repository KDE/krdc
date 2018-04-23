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

#include "core/remoteview.h"

#include <KBookmarkManager>
#include <KXmlGui/KActionCollection>
#include <KBookmarks/KBookmarkMenu>

#include <QMenu>

class MainWindow;

class BookmarkManager : public QObject, public KBookmarkOwner
{
    Q_OBJECT

public:
    BookmarkManager(KActionCollection *collection, QMenu *menu, MainWindow *parent);
    ~BookmarkManager() override;

    QUrl currentUrl() const Q_DECL_OVERRIDE;
    QString currentTitle() const Q_DECL_OVERRIDE;
    virtual bool addBookmarkEntry() const;
    virtual bool editBookmarkEntry() const;
    bool supportsTabs() const Q_DECL_OVERRIDE;
    QList<KBookmarkOwner::FutureBookmark> currentBookmarkList() const Q_DECL_OVERRIDE;
    void addHistoryBookmark(RemoteView *view);
    void addManualBookmark(const QUrl &url, const QString &text);
    KBookmarkManager* getManager();
    // removes all bookmarks with url, possibly ignore the history folder and update it's title there if it's set
    static void removeByUrl(KBookmarkManager *manager, const QString &url, bool ignoreHistory = false, const QString updateTitle = QString());
    static void updateTitle(KBookmarkManager *manager, const QString &url, const QString &title);
    // returns a QStringList for all bookmarks that point to this url using KBookmark::address()
    static const QStringList findBookmarkAddresses(const KBookmarkGroup &group, const QString &url);

Q_SIGNALS:
    void openUrl(const QUrl &url);

private Q_SLOTS:
    void openBookmark(const KBookmark &bm, Qt::MouseButtons, Qt::KeyboardModifiers) override;
    void openFolderinTabs(const KBookmarkGroup &bookmarkGroup) override;

private:
    QString urlForView(RemoteView *view) const;
    QString titleForUrl(const QUrl &url) const;

    KBookmarkMenu *m_bookmarkMenu;
    KBookmarkManager *m_manager;
    KBookmarkGroup m_historyGroup;

    MainWindow *m_mainWindow;
};

#endif
