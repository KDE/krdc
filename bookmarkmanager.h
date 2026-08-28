/*
    SPDX-FileCopyrightText: 2007 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef BOOKMARKMANAGER_H
#define BOOKMARKMANAGER_H

#include "core/remoteview.h"

#include <KActionCollection>
#include <KBookmarkContextMenu>
#include <KBookmarkManager>
#include <KBookmarkMenu>
#include <KBookmarkOwner>

#include <QMenu>

class MainWindow;
class BookmarkManager;

class BookmarkContextMenu : public KBookmarkContextMenu
{
    Q_OBJECT

public:
    BookmarkContextMenu(const KBookmark &bookmark, KBookmarkManager *manager, KBookmarkOwner *owner, QWidget *parent = nullptr);

    void addActions() override;

Q_SIGNALS:
    void editBookmark(const QUrl &url);
};

class BookmarkMenu : public KBookmarkMenu
{
public:
    BookmarkMenu(KBookmarkManager *manager, KBookmarkOwner *owner, QMenu *parentMenu, BookmarkManager *bookmarkManager);
    BookmarkMenu(KBookmarkManager *manager, KBookmarkOwner *owner, QMenu *parentMenu, const QString &parentAddress, BookmarkManager *bookmarkManager);

protected:
    QMenu *contextMenu(QAction *action) override;
    void refill() override;
    QAction *actionForBookmark(const KBookmark &bookmark) override;

private:
    void removeAddBookmarkAction();

    BookmarkManager *m_bookmarkManager;
};

class BookmarkManager : public QObject, public KBookmarkOwner
{
    Q_OBJECT

public:
    BookmarkManager(KActionCollection *collection, QMenu *menu, MainWindow *parent);
    ~BookmarkManager() override;

    QUrl currentUrl() const override;
    QString currentTitle() const override;
    bool supportsTabs() const override;
    QList<KBookmarkOwner::FutureBookmark> currentBookmarkList() const override;
    void addHistoryBookmark(RemoteView *view);
    void addManualBookmark(const QUrl &url, const QString &text);
    KBookmarkManager *getManager();
    // removes all bookmarks with url, possibly ignore the history folder and update it's title there if it's set
    static void removeByUrl(KBookmarkManager *manager, const QString &url, bool ignoreHistory = false, const QString &updateTitle = QString());
    static void updateTitle(KBookmarkManager *manager, const QString &url, const QString &title);
    // returns a QStringList for all bookmarks that point to this url using KBookmark::address()
    static const QStringList findBookmarkAddresses(const KBookmarkGroup &group, const QString &url);

Q_SIGNALS:
    void openUrl(const QUrl &url);
    void editBookmark(const QUrl &url);

private Q_SLOTS:
    void openBookmark(const KBookmark &bm, Qt::MouseButtons, Qt::KeyboardModifiers) override;
    void openFolderinTabs(const KBookmarkGroup &bookmarkGroup) override;

private:
    QString urlForView(RemoteView *view) const;
    QString titleForUrl(const QUrl &url) const;

    BookmarkMenu *m_bookmarkMenu;
    KBookmarkManager *m_manager;
    KBookmarkGroup m_historyGroup;

    MainWindow *m_mainWindow;
};

#endif
