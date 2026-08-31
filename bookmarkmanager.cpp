/*
    SPDX-FileCopyrightText: 2007 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "bookmarkmanager.h"
#include "krdc_debug.h"
#include "mainwindow.h"

#include <KBookmarkAction>
#include <KBookmarkActionMenu>
#include <KLocalizedString>

#include <QStandardPaths>

BookmarkContextMenu::BookmarkContextMenu(const KBookmark &bookmark, KBookmarkManager *manager, KBookmarkOwner *owner, QWidget *parent)
    : KBookmarkContextMenu(bookmark, manager, owner, parent)
{
}

void BookmarkContextMenu::addActions()
{
    if (bookmark().isGroup()) {
        KBookmarkContextMenu::addActions();
        return;
    }

    addAction(KBookmarkContextMenu::tr("Copy Link Address", "@action:inmenu"), this, &KBookmarkContextMenu::slotCopyLocation);
    addAction(KBookmarkContextMenu::tr("Properties", "@action:inmenu"), this, [this]() {
        Q_EMIT editBookmark(bookmark().address(), bookmark().fullText(), bookmark().url());
    });

    addSeparator();
    addAction(QIcon::fromTheme(QStringLiteral("edit-delete")),
              KBookmarkContextMenu::tr("Delete Bookmark", "@action:inmenu"),
              this,
              &KBookmarkContextMenu::slotRemove);
}

BookmarkMenu::BookmarkMenu(KBookmarkManager *manager, KBookmarkOwner *owner, QMenu *parentMenu, BookmarkManager *bookmarkManager)
    : KBookmarkMenu(manager, owner, parentMenu)
    , m_bookmarkManager(bookmarkManager)
{
    removeAddBookmarkAction();
}

BookmarkMenu::BookmarkMenu(KBookmarkManager *manager, KBookmarkOwner *owner, QMenu *parentMenu, const QString &parentAddress, BookmarkManager *bookmarkManager)
    : KBookmarkMenu(manager, owner, parentMenu, parentAddress)
    , m_bookmarkManager(bookmarkManager)
{
}

QMenu *BookmarkMenu::contextMenu(QAction *action)
{
    auto *bookmarkAction = dynamic_cast<KBookmarkActionInterface *>(action);
    if (!bookmarkAction) {
        return nullptr;
    }

    auto *menu = new BookmarkContextMenu(bookmarkAction->bookmark(), manager(), owner());
    connect(menu, &BookmarkContextMenu::editBookmark, m_bookmarkManager, &BookmarkManager::editBookmark);

    return menu;
}

void BookmarkMenu::refill()
{
    KBookmarkMenu::refill();
    removeAddBookmarkAction();
}

QAction *BookmarkMenu::actionForBookmark(const KBookmark &bookmark)
{
    if (bookmark.isGroup()) {
        auto *actionMenu = new KBookmarkActionMenu(bookmark, this);
        m_actions.append(actionMenu);

        auto *subMenu = new BookmarkMenu(manager(), owner(), actionMenu->menu(), bookmark.address(), m_bookmarkManager);
        m_lstSubMenus.append(subMenu);
        return actionMenu;
    }

    if (bookmark.isSeparator()) {
        auto *separator = new QAction(this);
        separator->setSeparator(true);
        m_actions.append(separator);
        return separator;
    }

    auto *action = new KBookmarkAction(bookmark, owner(), this);
    m_actions.append(action);
    return action;
}

void BookmarkMenu::removeAddBookmarkAction()
{
    if (QAction *action = addBookmarkAction()) {
        parentMenu()->removeAction(action);
    }
}

BookmarkManager::BookmarkManager(KActionCollection *collection, QMenu *menu, MainWindow *parent)
    : QObject(parent)
    , KBookmarkOwner()
    , m_mainWindow(parent)
{
    const QString dir = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QString(), QStandardPaths::LocateOption::LocateDirectory);
    const QString file = dir + QLatin1String("krdc/bookmarks.xml");

    m_manager = new KBookmarkManager(file, this);
    m_bookmarkMenu = new BookmarkMenu(m_manager, this, menu, this);
    collection->addActions(menu->actions());

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
    const QUrl url = QUrl::fromUserInput(urlString);
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
    Q_EMIT openUrl(bm.url());
}

void BookmarkManager::openFolderinTabs(const KBookmarkGroup &bookmarkGroup)
{
    KBookmark bm = bookmarkGroup.first();
    while (!bm.isNull()) {
        Q_EMIT openUrl(bm.url());
        bm = bookmarkGroup.next(bm);
    }
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
    QList<KBookmarkOwner::FutureBookmark> list;

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
    m_manager->emitChanged();
}

void BookmarkManager::updateBookmark(const QString &address, const QString &name, const QUrl &url)
{
    KBookmark bookmark = m_manager->findByAddress(address);
    if (bookmark.isNull() || bookmark.isGroup()) {
        return;
    }

    bookmark.setFullText(name);
    bookmark.setUrl(url);
    m_manager->emitChanged(bookmark.parentGroup());
}

KBookmarkManager *BookmarkManager::getManager()
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

        if (bm.url() == QUrl::fromUserInput(url)) {
            bookmarkAddresses.append(bm.address());
        }
        bm = group.next(bm);
    }
    return bookmarkAddresses;
}

void BookmarkManager::removeByUrl(KBookmarkManager *manager, const QString &url, bool ignoreHistory, const QString &updateTitle)
{
    const QStringList addresses = findBookmarkAddresses(manager->root(), url);
    for (const QString &address : addresses) {
        KBookmark bm = manager->findByAddress(address);
        if (ignoreHistory && bm.parentGroup().metaDataItem(QStringLiteral("krdc-history")) == QLatin1String("historyfolder")) {
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
    const QStringList addresses = findBookmarkAddresses(manager->root(), url);
    for (const QString &address : addresses) {
        KBookmark bm = manager->findByAddress(address);
        bm.setFullText(title);
        qCDebug(KRDC) << "Update" << bm.fullText();
    }
    manager->emitChanged();
}
