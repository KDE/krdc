/*
    SPDX-FileCopyrightText: 2007 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef BOOKMARKMANAGER_H
#define BOOKMARKMANAGER_H

#include "core/remoteview.h"

#include <KBookmarkManager>
#include <KActionCollection>
#include <KBookmarkMenu>

#include <QMenu>

class MainWindow;

class BookmarkManager : public QObject, public KBookmarkOwner
{
    Q_OBJECT

public:
    BookmarkManager(KActionCollection *collection, QMenu *menu, MainWindow *parent);
    ~BookmarkManager() override;

    QUrl currentUrl() const override;
    QString currentTitle() const override;
    virtual bool addBookmarkEntry() const;
    virtual bool editBookmarkEntry() const;
    bool supportsTabs() const override;
    QList<KBookmarkOwner::FutureBookmark> currentBookmarkList() const override;
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
