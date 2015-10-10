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
#include "logging.h"

#include <KBookmarks/KBookmarkOwner>
#include <KI18n/KLocalizedString>

#include <QStandardPaths>

BookmarkManager::BookmarkManager(KActionCollection *collection, QMenu *menu, MainWindow *parent)
        : QObject(parent),
        KBookmarkOwner(),
        m_mainWindow(parent)
{
    const QString dir = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QString(),
                                               QStandardPaths::LocateOption::LocateDirectory);
    const QString file = dir + QLatin1String("/krdc/bookmarks.xml");
    m_manager = KBookmarkManager::managerForFile(file, QLatin1String("krdc"));
    m_manager->setUpdate(true);
    m_bookmarkMenu = new KBookmarkMenu(m_manager, this, menu, collection);

    KBookmarkGroup root = m_manager->root();
    KBookmark bm = root.first();
    while (!bm.isNull()) {
        if (bm.metaDataItem(QLatin1String("krdc-history")) == QLatin1String("historyfolder")) // get it also when user renamed it
            break;
        bm = root.next(bm);
    }

    if (bm.isNull()) {
        qCDebug(KRDC) << "History folder not found. Create it.";
        bm = m_manager->root().createNewFolder(i18n("History"));
        bm.setMetaDataItem(QStringLiteral("krdc-history"), QStringLiteral("historyfolder"));
        m_manager->emitChanged();
    }

    m_historyGroup = bm.toGroup();
}

BookmarkManager::~BookmarkManager()
{
    delete m_bookmarkMenu;
}

void BookmarkManager::addHistoryBookmark(RemoteView *view)
{
    KBookmark bm = m_historyGroup.first();
    const QString urlString = urlForView(view);
    const QUrl url = QUrl(urlString);
    while (!bm.isNull()) {
        if (bm.url() == url) {
            qCDebug(KRDC) << "Found URL. Move it at the history start.";
            m_historyGroup.moveBookmark(bm, KBookmark());
            bm.updateAccessMetadata();
            m_manager->emitChanged(m_historyGroup);
            return;
        }
        bm = m_historyGroup.next(bm);
    }

    if (bm.isNull()) {
        qCDebug(KRDC) << "Add new history bookmark.";
        bm = m_historyGroup.addBookmark(titleForUrl(url), url, QString());
        bm.updateAccessMetadata();
        m_historyGroup.moveBookmark(bm, KBookmark());
        m_manager->emitChanged(m_historyGroup);
    }
}

void BookmarkManager::openBookmark(const KBookmark &bm, Qt::MouseButtons, Qt::KeyboardModifiers)
{
    emit openUrl(bm.url());
}

void BookmarkManager::openFolderinTabs(const KBookmarkGroup &bookmarkGroup)
{
    KBookmark bm = bookmarkGroup.first();
    while (!bm.isNull()) {
        emit openUrl(bm.url());
        bm = bookmarkGroup.next(bm);
    }
}

bool BookmarkManager::addBookmarkEntry() const
{
    return true;
}

bool BookmarkManager::editBookmarkEntry() const
{
    return true;
}

QUrl BookmarkManager::currentUrl() const
{
    RemoteView *view = m_mainWindow->currentRemoteView();
    if (view)
        return QUrl(urlForView(view));
    else
        return QUrl();
}

QString BookmarkManager::urlForView(RemoteView *view) const
{
    return view->url().toDisplayString(QUrl::UrlFormattingOption::StripTrailingSlash);
}

QString BookmarkManager::currentTitle() const
{
    return titleForUrl(currentUrl());
}

QString BookmarkManager::titleForUrl(const QUrl &url) const
{
    return url.toDisplayString(QUrl::UrlFormattingOption::StripTrailingSlash);
}

bool BookmarkManager::supportsTabs() const
{
    return true;
}

QList<KBookmarkOwner::FutureBookmark> BookmarkManager::currentBookmarkList() const
{
    QList<KBookmarkOwner::FutureBookmark>  list;

    QMapIterator<QWidget *, RemoteView *> iter(m_mainWindow->remoteViewList());

    while (iter.hasNext()) {
        RemoteView *next = iter.next().value();
        const QUrl url = next->url();
        const QString title = titleForUrl(url);
        KBookmarkOwner::FutureBookmark bookmark = KBookmarkOwner::FutureBookmark(title, url, QString());
        list.append(bookmark);
    }

    return list;
}

void BookmarkManager::addManualBookmark(const QUrl &url, const QString &text)
{
    m_manager->root().addBookmark(text, url, QString());
    emit m_manager->emitChanged();
}

KBookmarkManager* BookmarkManager::getManager()
{
    return m_manager;
}

const QStringList BookmarkManager::findBookmarkAddresses(const KBookmarkGroup &group, const QString &url)
{
    QStringList bookmarkAddresses = QStringList();
    KBookmark bm = group.first();
    while (!bm.isNull()) {
        if (bm.isGroup()) {
            bookmarkAddresses.append(findBookmarkAddresses(bm.toGroup(), url));
        }

        if (bm.url() == QUrl::fromLocalFile(url)) {
            bookmarkAddresses.append(bm.address());
        }
        bm = group.next(bm);
    }
    return bookmarkAddresses;
}

void BookmarkManager::removeByUrl(KBookmarkManager *manager, const QString &url, bool ignoreHistory, const QString updateTitle)
{
    foreach(const QString &address, findBookmarkAddresses(manager->root(), url)) {
        KBookmark bm = manager->findByAddress(address);
        if (ignoreHistory && bm.parentGroup().metaDataItem(QLatin1String("krdc-history")) == QLatin1String("historyfolder")) {
            if (!updateTitle.isEmpty()) {
                qCDebug(KRDC) << "Update" << bm.fullText();
                bm.setFullText(updateTitle);
            }
        } else {
            if (!bm.isGroup()) { // please don't delete groups... happened in testing
                qCDebug(KRDC) << "Delete" << bm.fullText();
                bm.parentGroup().deleteBookmark(bm);
            }
        }
    }

    manager->emitChanged();
}

void BookmarkManager::updateTitle(KBookmarkManager *manager, const QString &url, const QString &title)
{
    foreach(const QString &address, findBookmarkAddresses(manager->root(), url)) {
        KBookmark bm = manager->findByAddress(address);
        bm.setFullText(title);
        qCDebug(KRDC) << "Update" << bm.fullText();
    }
    manager->emitChanged();
}


