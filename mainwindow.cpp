/****************************************************************************
**
** Copyright (C) 2007 - 2013 Urs Wolfer <uwolfer @ kde.org>
** Copyright (C) 2009 - 2010 Tony Murray <murraytony @ gmail.com>
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

#include "mainwindow.h"

#include "remoteview.h"
#include "settings.h"
#include "config/preferencesdialog.h"
#include "floatingtoolbar.h"
#include "bookmarkmanager.h"
#include "connectiondelegate.h"
#include "remotedesktopsmodel.h"
#include "systemtrayicon.h"
#include "tabbedviewwidget.h"
#include "hostpreferences.h"

#ifdef TELEPATHY_SUPPORT
#include "tubesmanager.h"
#endif

#include <KAction>
#include <KActionCollection>
#include <KActionMenu>
#include <KComboBox>
#include <KEditToolBar>
#include <KIcon>
#include <KInputDialog>
#include <KLineEdit>
#include <KLocale>
#include <KMenu>
#include <KMenuBar>
#include <KMessageBox>
#include <KNotifyConfigWidget>
#include <KPluginInfo>
#include <KPushButton>
#include <KShortcutsDialog>
#include <KStatusBar>
#include <KToggleAction>
#include <KToggleFullScreenAction>
#include <KServiceTypeTrader>

#include <QClipboard>
#include <QDockWidget>
#include <QFontMetrics>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLayout>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
        : KXmlGuiWindow(parent),
        m_fullscreenWindow(0),
        m_protocolInput(0),
        m_addressInput(0),
        m_toolBar(0),
        m_currentRemoteView(-1),
        m_systemTrayIcon(0),
        m_dockWidgetTableView(0),
        m_newConnectionTableView(0),
#ifdef TELEPATHY_SUPPORT
        m_tubesManager(0),
#endif
        m_newConnectionWidget(0)
{
    loadAllPlugins();

    setupActions();

    setStandardToolBarMenuEnabled(true);

    m_tabWidget = new TabbedViewWidget(this);
    m_tabWidget->setMovable(true);
    m_tabWidget->setTabPosition((KTabWidget::TabPosition) Settings::tabPosition());

#if QT_VERSION >= 0x040500
    m_tabWidget->setTabsClosable(Settings::tabCloseButton());
#else
    m_tabWidget->setCloseButtonEnabled(Settings::tabCloseButton());
#endif

    connect(m_tabWidget, SIGNAL(closeRequest(QWidget*)), SLOT(closeTab(QWidget*)));

    if (Settings::tabMiddleClick())
        connect(m_tabWidget, SIGNAL(mouseMiddleClick(QWidget*)), SLOT(closeTab(QWidget*)));

    connect(m_tabWidget, SIGNAL(mouseDoubleClick(QWidget*)), SLOT(openTabSettings(QWidget*)));
    connect(m_tabWidget, SIGNAL(contextMenu(QWidget*,QPoint)), SLOT(tabContextMenu(QWidget*,QPoint)));

    m_tabWidget->setMinimumSize(600, 400);
    setCentralWidget(m_tabWidget);

    createDockWidget();

    createGUI("krdcui.rc");

    if (Settings::systemTrayIcon()) {
        m_systemTrayIcon = new SystemTrayIcon(this);
        if(m_fullscreenWindow) m_systemTrayIcon->setAssociatedWidget(m_fullscreenWindow);
    }

    connect(m_tabWidget, SIGNAL(currentChanged(int)), SLOT(tabChanged(int)));

    if (Settings::showStatusBar())
        statusBar()->showMessage(i18n("KDE Remote Desktop Client started"));

    updateActionStatus(); // disable remote view actions

    if (Settings::openSessions().count() == 0) // just create a new connection tab if there are no open sessions
        m_tabWidget->addTab(newConnectionWidget(), i18n("New Connection"));

    setAutoSaveSettings(); // e.g toolbar position, mainwindow size, ...

    if (Settings::rememberSessions()) // give some time to create and show the window first
        QTimer::singleShot(100, this, SLOT(restoreOpenSessions()));
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupActions()
{
    QAction *connectionAction = actionCollection()->addAction("new_connection");
    connectionAction->setText(i18n("New Connection"));
    connectionAction->setIcon(KIcon("network-connect"));
    connect(connectionAction, SIGNAL(triggered()), SLOT(newConnectionPage()));

    QAction *screenshotAction = actionCollection()->addAction("take_screenshot");
    screenshotAction->setText(i18n("Copy Screenshot to Clipboard"));
    screenshotAction->setIconText(i18n("Screenshot"));
    screenshotAction->setIcon(KIcon("ksnapshot"));
    connect(screenshotAction, SIGNAL(triggered()), SLOT(takeScreenshot()));

    KAction *fullscreenAction = actionCollection()->addAction("switch_fullscreen"); // note: please do not switch to KStandardShortcut unless you know what you are doing (see history of this file)
    fullscreenAction->setText(i18n("Switch to Full Screen Mode"));
    fullscreenAction->setIconText(i18n("Full Screen"));
    fullscreenAction->setIcon(KIcon("view-fullscreen"));
    fullscreenAction->setShortcut(KStandardShortcut::fullScreen());
    connect(fullscreenAction, SIGNAL(triggered()), SLOT(switchFullscreen()));

    QAction *viewOnlyAction = actionCollection()->addAction("view_only");
    viewOnlyAction->setCheckable(true);
    viewOnlyAction->setText(i18n("View Only"));
    viewOnlyAction->setIcon(KIcon("document-preview"));
    connect(viewOnlyAction, SIGNAL(triggered(bool)), SLOT(viewOnly(bool)));

    KAction *disconnectAction = actionCollection()->addAction("disconnect");
    disconnectAction->setText(i18n("Disconnect"));
    disconnectAction->setIcon(KIcon("network-disconnect"));
    disconnectAction->setShortcut(QKeySequence::Close);
    connect(disconnectAction, SIGNAL(triggered()), SLOT(disconnectHost()));

    QAction *showLocalCursorAction = actionCollection()->addAction("show_local_cursor");
    showLocalCursorAction->setCheckable(true);
    showLocalCursorAction->setIcon(KIcon("input-mouse"));
    showLocalCursorAction->setText(i18n("Show Local Cursor"));
    showLocalCursorAction->setIconText(i18n("Local Cursor"));
    connect(showLocalCursorAction, SIGNAL(triggered(bool)), SLOT(showLocalCursor(bool)));

    QAction *grabAllKeysAction = actionCollection()->addAction("grab_all_keys");
    grabAllKeysAction->setCheckable(true);
    grabAllKeysAction->setIcon(KIcon("configure-shortcuts"));
    grabAllKeysAction->setText(i18n("Grab All Possible Keys"));
    grabAllKeysAction->setIconText(i18n("Grab Keys"));
    connect(grabAllKeysAction, SIGNAL(triggered(bool)), SLOT(grabAllKeys(bool)));

    QAction *scaleAction = actionCollection()->addAction("scale");
    scaleAction->setCheckable(true);
    scaleAction->setIcon(KIcon("zoom-fit-best"));
    scaleAction->setText(i18n("Scale Remote Screen to Fit Window Size"));
    scaleAction->setIconText(i18n("Scale"));
    connect(scaleAction, SIGNAL(triggered(bool)), SLOT(scale(bool)));

    KStandardAction::quit(this, SLOT(quit()), actionCollection());
    KStandardAction::preferences(this, SLOT(preferences()), actionCollection());
    KStandardAction::configureToolbars(this, SLOT(configureToolbars()), actionCollection());
    KStandardAction::keyBindings(this, SLOT(configureKeys()), actionCollection());
    QAction *configNotifyAction = KStandardAction::configureNotifications(this, SLOT(configureNotifications()), actionCollection());
    configNotifyAction->setVisible(false);
    m_menubarAction = KStandardAction::showMenubar(this, SLOT(showMenubar()), actionCollection());
    m_menubarAction->setChecked(!menuBar()->isHidden());

    KActionMenu *bookmarkMenu = new KActionMenu(i18n("Bookmarks"), actionCollection());
    m_bookmarkManager = new BookmarkManager(actionCollection(), bookmarkMenu->menu(), this);
    actionCollection()->addAction("bookmark" , bookmarkMenu);
    connect(m_bookmarkManager, SIGNAL(openUrl(KUrl)), SLOT(newConnection(KUrl)));
}

void MainWindow::loadAllPlugins()
{
    const KService::List offers = KServiceTypeTrader::self()->query("KRDC/Plugin");

    KConfigGroup conf(KGlobal::config(), "Plugins");

    for (int i = 0; i < offers.size(); i++) {
        KService::Ptr offer = offers[i];

        RemoteViewFactory *remoteView;

        KPluginInfo description(offer);
        description.load(conf);

        const bool selected = description.isPluginEnabled();

        if (selected) {
            if ((remoteView = createPluginFromService(offer)) != 0) {
                kDebug(5010) << "### Plugin " + description.name() + " found ###";
                kDebug(5010) << "# Version:" << description.version();
                kDebug(5010) << "# Description:" << description.comment();
                kDebug(5010) << "# Author:" << description.author();
                const int sorting = offer->property("X-KDE-KRDC-Sorting").toInt();
                kDebug(5010) << "# Sorting:" << sorting;

                m_remoteViewFactories.insert(sorting, remoteView);
            } else {
                kDebug(5010) << "Error loading KRDC plugin (" << (offers[i])->library() << ')';
            }
        } else {
            kDebug(5010) << "# Plugin " + description.name() + " found, however it's not activated, skipping...";
            continue;
        }
    }

#ifdef TELEPATHY_SUPPORT
    /* Start tube handler */
    m_tubesManager = new TubesManager(this);
    connect(m_tubesManager, SIGNAL(newConnection(KUrl)), SLOT(newConnection(KUrl)));
#endif
}

RemoteViewFactory *MainWindow::createPluginFromService(const KService::Ptr &service)
{
    //try to load the specified library
    KPluginFactory *factory = KPluginLoader(service->library()).factory();

    if (!factory) {
        kError(5010) << "KPluginFactory could not load the plugin:" << service->library();
        return 0;
    }

    RemoteViewFactory *plugin = factory->create<RemoteViewFactory>();

    return plugin;
}

void MainWindow::restoreOpenSessions()
{
    const QStringList list = Settings::openSessions();
    QStringList::ConstIterator it = list.begin();
    QStringList::ConstIterator end = list.end();
    while (it != end) {
        newConnection(*it);
        ++it;
    }
}

KUrl MainWindow::getInputUrl()
{
    return KUrl(m_protocolInput->currentText() + "://" + m_addressInput->text());
}

void MainWindow::newConnection(const KUrl &newUrl, bool switchFullscreenWhenConnected, const QString &tabName)
{
    m_switchFullscreenWhenConnected = switchFullscreenWhenConnected;

    const KUrl url = newUrl.isEmpty() ? getInputUrl() : newUrl;

    if (!url.isValid() || (url.host().isEmpty() && url.port() < 0)
        || (!url.path().isEmpty() && url.path() != QLatin1String("/"))) {
        KMessageBox::error(this,
                           i18n("The entered address does not have the required form.\n Syntax: [username@]host[:port]"),
                           i18n("Malformed URL"));
        return;
    }

    if (m_protocolInput && m_addressInput) {
        int index = m_protocolInput->findText(url.protocol());
        if (index>=0) m_protocolInput->setCurrentIndex(index);
        m_addressInput->setText(url.authority());
    }

    RemoteView *view = 0;
    KConfigGroup configGroup = Settings::self()->config()->group("hostpreferences").group(url.prettyUrl(KUrl::RemoveTrailingSlash));

    foreach(RemoteViewFactory *factory, m_remoteViewFactories) {
        if (factory->supportsUrl(url)) {
            view = factory->createView(this, url, configGroup);
            kDebug(5010) << "Found plugin to handle url (" + url.url() + "): " + view->metaObject()->className();
            break;
        }
    }

    if (!view) {
        KMessageBox::error(this,
                           i18n("The entered address cannot be handled."),
                           i18n("Unusable URL"));
        return;
    }

    // Configure the view
    HostPreferences* prefs = view->hostPreferences();
    if (! prefs->showDialogIfNeeded(this)) {
        delete view;
        return;
    }

    view->showDotCursor(prefs->showLocalCursor() ? RemoteView::CursorOn : RemoteView::CursorOff);
    view->setViewOnly(prefs->viewOnly());
    if (! switchFullscreenWhenConnected) view->enableScaling(prefs->windowedScale());

    connect(view, SIGNAL(framebufferSizeChanged(int,int)), this, SLOT(resizeTabWidget(int,int)));
    connect(view, SIGNAL(statusChanged(RemoteView::RemoteStatus)), this, SLOT(statusChanged(RemoteView::RemoteStatus)));
    connect(view, SIGNAL(disconnected()), this, SLOT(disconnectHost()));

    m_remoteViewList.append(view);

    QScrollArea *scrollArea = createScrollArea(m_tabWidget, view);

    const int indexOfNewConnectionWidget = m_tabWidget->indexOf(m_newConnectionWidget);
    if (indexOfNewConnectionWidget >= 0)
        m_tabWidget->removeTab(indexOfNewConnectionWidget);

    const int newIndex = m_tabWidget->addTab(scrollArea, KIcon("krdc"), tabName.isEmpty() ? url.prettyUrl(KUrl::RemoveTrailingSlash) : tabName);
    m_tabWidget->setCurrentIndex(newIndex);
    tabChanged(newIndex); // force to update m_currentRemoteView (tabChanged is not emitted when start page has been disabled)

    view->start();
}

void MainWindow::openFromRemoteDesktopsModel(const QModelIndex &index)
{
    const QString urlString = index.data(10001).toString();
    const QString nameString = index.data(10003).toString();
    if (!urlString.isEmpty()) {
        const KUrl url(urlString);
        // first check if url has already been opened; in case show the tab
        for (int i = 0; i < m_remoteViewList.count(); ++i) {
            if (m_remoteViewList.at(i)->url() == url) {
                m_tabWidget->setCurrentIndex(i);
                return;
            }
        }
        newConnection(url, false, nameString);
    }
}

void MainWindow::resizeTabWidget(int w, int h)
{
    kDebug(5010) << "tabwidget resize, view size: w: " << w << ", h: " << h;
    if (m_fullscreenWindow) {
        kDebug(5010) << "in fullscreen mode, refusing to resize";
        return;
    }

    const QSize viewSize = QSize(w,h);
    QDesktopWidget *desktop = QApplication::desktop();

    if (Settings::fullscreenOnConnect()) {
        int currentScreen = desktop->screenNumber(this);
        const QSize screenSize = desktop->screenGeometry(currentScreen).size();

        if (screenSize == viewSize) {
            kDebug(5010) << "screen size equal to target view size -> switch to fullscreen mode";
            switchFullscreen();
            return;
        }
    }

    if (Settings::resizeOnConnect()) {
        QWidget* currentWidget = m_tabWidget->currentWidget();
        const QSize newWindowSize = size() - currentWidget->frameSize() + viewSize;

        const QSize desktopSize = desktop->availableGeometry().size();
        kDebug(5010) << "new window size: " << newWindowSize << " available space:" << desktopSize;

        if ((newWindowSize.width() >= desktopSize.width()) || (newWindowSize.height() >= desktopSize.height())) {
            kDebug(5010) << "remote desktop needs more space than available -> show window maximized";
            setWindowState(windowState() | Qt::WindowMaximized);
            return;
        }

        resize(newWindowSize);
    }
}

void MainWindow::statusChanged(RemoteView::RemoteStatus status)
{
    kDebug(5010) << status;

    // the remoteview is already deleted, so don't show it; otherwise it would crash
    if (status == RemoteView::Disconnecting || status == RemoteView::Disconnected)
        return;

    RemoteView *view = qobject_cast<RemoteView*>(QObject::sender());
    const QString host = view->host();

    QString iconName = "krdc";
    QString message;

    switch (status) {
    case RemoteView::Connecting:
        iconName = "network-connect";
        message = i18n("Connecting to %1", host);
        break;
    case RemoteView::Authenticating:
        iconName = "dialog-password";
        message = i18n("Authenticating at %1", host);
        break;
    case RemoteView::Preparing:
        iconName = "view-history";
        message = i18n("Preparing connection to %1", host);
        break;
    case RemoteView::Connected:
        iconName = "krdc";
        message = i18n("Connected to %1", host);

        if (view->grabAllKeys() != view->hostPreferences()->grabAllKeys()) {
            view->setGrabAllKeys(view->hostPreferences()->grabAllKeys());
            updateActionStatus();
        }

        // when started with command line fullscreen argument
        if (m_switchFullscreenWhenConnected) {
            m_switchFullscreenWhenConnected = false;
            switchFullscreen();
        }

        if (Settings::rememberHistory()) {
            m_bookmarkManager->addHistoryBookmark(view);
        }

        break;
    default:
        break;
    }

    m_tabWidget->setTabIcon(m_tabWidget->indexOf(view), KIcon(iconName));
    if (Settings::showStatusBar())
        statusBar()->showMessage(message);
}

void MainWindow::takeScreenshot()
{
    const QPixmap snapshot = m_remoteViewList.at(m_currentRemoteView)->takeScreenshot();

    QApplication::clipboard()->setPixmap(snapshot);
}

void MainWindow::switchFullscreen()
{
    kDebug(5010);

    if (m_fullscreenWindow) {
        // Leaving full screen mode
        m_fullscreenWindow->setWindowState(0);
        m_fullscreenWindow->hide();

        m_tabWidget->setTabBarHidden(m_tabWidget->count() <= 1 && !Settings::showTabBar());
        m_tabWidget->setDocumentMode(false);
        setCentralWidget(m_tabWidget);

        show();
        restoreGeometry(m_mainWindowGeometry);
        if (m_systemTrayIcon) m_systemTrayIcon->setAssociatedWidget(this);

        foreach(RemoteView *currentView, m_remoteViewList) {
            currentView->enableScaling(currentView->hostPreferences()->windowedScale());
        }

        if (m_toolBar) {
            m_toolBar->hideAndDestroy();
            m_toolBar->deleteLater();
            m_toolBar = 0;
        }

        actionCollection()->action("switch_fullscreen")->setIcon(KIcon("view-fullscreen"));
        actionCollection()->action("switch_fullscreen")->setText(i18n("Switch to Full Screen Mode"));
        actionCollection()->action("switch_fullscreen")->setIconText(i18n("Full Screen"));

        m_fullscreenWindow->deleteLater();
        m_fullscreenWindow = 0;
    } else {
        // Entering full screen mode
        m_fullscreenWindow = new QWidget(this, Qt::Window);
        m_fullscreenWindow->setWindowTitle(i18nc("window title when in full screen mode (for example displayed in tasklist)",
                                           "KDE Remote Desktop Client (Full Screen)"));

        m_mainWindowGeometry = saveGeometry();

        m_tabWidget->setTabBarHidden(true);
        m_tabWidget->setDocumentMode(true);

        foreach(RemoteView *currentView, m_remoteViewList) {
            currentView->enableScaling(currentView->hostPreferences()->fullscreenScale());
        }

        QVBoxLayout *fullscreenLayout = new QVBoxLayout(m_fullscreenWindow);
        fullscreenLayout->setContentsMargins(QMargins(0, 0, 0, 0));
        fullscreenLayout->addWidget(m_tabWidget);

        KToggleFullScreenAction::setFullScreen(m_fullscreenWindow, true);

        MinimizePixel *minimizePixel = new MinimizePixel(m_fullscreenWindow);
        minimizePixel->winId(); // force it to be a native widget (prevents problem with QX11EmbedContainer)
        connect(minimizePixel, SIGNAL(rightClicked()), m_fullscreenWindow, SLOT(showMinimized()));
        m_fullscreenWindow->installEventFilter(this);

        m_fullscreenWindow->show();
        hide();  // hide after showing the new window so it stays on the same screen

        if (m_systemTrayIcon) m_systemTrayIcon->setAssociatedWidget(m_fullscreenWindow);
        showRemoteViewToolbar();
    }
    if (m_tabWidget->currentWidget() == m_newConnectionWidget) {
        m_addressInput->setFocus();
    }
}

QScrollArea *MainWindow::createScrollArea(QWidget *parent, RemoteView *remoteView)
{
    RemoteViewScrollArea *scrollArea = new RemoteViewScrollArea(parent);
    scrollArea->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    connect(scrollArea, SIGNAL(resized(int,int)), remoteView, SLOT(scaleResize(int,int)));

    QPalette palette = scrollArea->palette();
    palette.setColor(QPalette::Dark, Settings::backgroundColor());
    scrollArea->setPalette(palette);

    scrollArea->setBackgroundRole(QPalette::Dark);
    scrollArea->setFrameStyle(QFrame::NoFrame);

    scrollArea->setWidget(remoteView);

    return scrollArea;
}

void MainWindow::disconnectHost()
{
    kDebug(5010);

    RemoteView *view = qobject_cast<RemoteView*>(QObject::sender());

    QWidget *widgetToDelete;
    if (view) {
        widgetToDelete = (QWidget*) view->parent()->parent();
        m_remoteViewList.removeOne(view);
    } else {
        widgetToDelete = m_tabWidget->currentWidget();
        view = m_remoteViewList.takeAt(m_currentRemoteView);
    }

    saveHostPrefs(view);
    view->startQuitting();  // some deconstructors can't properly quit, so quit early
    m_tabWidget->removePage(widgetToDelete);
    widgetToDelete->deleteLater();
#ifdef TELEPATHY_SUPPORT
    m_tubesManager->closeTube(view->url());
#endif

    // if closing the last connection, create new connection tab
    if (m_tabWidget->count() == 0) {
        newConnectionPage(false);
    }

    // if the newConnectionWidget is the only tab and we are fullscreen, switch to window mode
    if (m_fullscreenWindow && m_tabWidget->count() == 1  && m_tabWidget->currentWidget() == m_newConnectionWidget) {
        switchFullscreen();
    }
}

void MainWindow::closeTab(QWidget *widget)
{
    bool isNewConnectionPage = widget == m_newConnectionWidget;
    const int index = m_tabWidget->indexOf(widget);

    kDebug(5010) << index;

    if (!isNewConnectionPage) {
        RemoteView *view = m_remoteViewList.takeAt(index);
        view->startQuitting();
#ifdef TELEPATHY_SUPPORT
        m_tubesManager->closeTube(view->url());
#endif
        widget->deleteLater();
    }

    m_tabWidget->removePage(widget);

    // if closing the last connection, create new connection tab
    if (m_tabWidget->count() == 0) {
        newConnectionPage(false);
    }

    // if the newConnectionWidget is the only tab and we are fullscreen, switch to window mode
    if (m_fullscreenWindow && m_tabWidget->count() == 1  && m_tabWidget->currentWidget() == m_newConnectionWidget) {
        switchFullscreen();
    }
}

void MainWindow::openTabSettings(QWidget *widget)
{
    RemoteViewScrollArea *scrollArea = qobject_cast<RemoteViewScrollArea*>(widget);
    if (!scrollArea) return;
    RemoteView *view = qobject_cast<RemoteView*>(scrollArea->widget());
    if (!view) return;

    const QString url = view->url().url();
    kDebug(5010) << url;

    showSettingsDialog(url);
}

void MainWindow::showSettingsDialog(const QString &url)
{
    HostPreferences *prefs = 0;

    foreach(RemoteViewFactory *factory, remoteViewFactoriesList()) {
        if (factory->supportsUrl(url)) {
            prefs = factory->createHostPreferences(Settings::self()->config()->group("hostpreferences").group(url), this);
            if (prefs) {
                kDebug(5010) << "Found plugin to handle url (" + url + "): " + prefs->metaObject()->className();
            } else {
                kDebug(5010) << "Found plugin to handle url (" + url + "), but plugin does not provide preferences";
            }
        }
    }

    if (prefs) {
        prefs->setShownWhileConnected(true);
        prefs->showDialog(this);
    } else {
        KMessageBox::error(this,
                           i18n("The selected host cannot be handled."),
                           i18n("Unusable URL"));
    }
}

void MainWindow::showConnectionContextMenu(const QPoint &pos)
{
    // QTableView does not take headers into account when it does mapToGlobal(), so calculate the offset
    QPoint offset = QPoint(m_newConnectionTableView->verticalHeader()->size().width(),
                           m_newConnectionTableView->horizontalHeader()->size().height());
    QModelIndex index = m_newConnectionTableView->indexAt(pos);

    if (!index.isValid())
        return;

    const QString url = index.data(10001).toString();
    const QString title = m_remoteDesktopsModelProxy->index(index.row(), RemoteDesktopsModel::Title).data(Qt::DisplayRole).toString();
    const QString source = m_remoteDesktopsModelProxy->index(index.row(), RemoteDesktopsModel::Source).data(Qt::DisplayRole).toString();

    KMenu *menu = new KMenu(m_newConnectionTableView);
    menu->addTitle(url);

    QAction *connectAction = menu->addAction(KIcon("network-connect"), i18n("Connect"));
    QAction *renameAction = menu->addAction(KIcon("edit-rename"), i18n("Rename"));
    QAction *settingsAction = menu->addAction(KIcon("configure"), i18n("Settings"));
    QAction *deleteAction = menu->addAction(KIcon("edit-delete"), i18n("Delete"));

    // not very clean, but it works,
    if (!(source == i18nc("Where each displayed link comes from", "Bookmarks") ||
            source == i18nc("Where each displayed link comes from", "History"))) {
        renameAction->setEnabled(false);
        deleteAction->setEnabled(false);
    }

    QAction *selectedAction = menu->exec(m_newConnectionTableView->mapToGlobal(pos + offset));

    if (selectedAction == connectAction) {
        openFromRemoteDesktopsModel(index);
    } else if (selectedAction == renameAction) {
        //TODO: use inline editor if possible
        bool ok = false;
        const QString newTitle = KInputDialog::getText(i18n("Rename %1", title), i18n("Rename %1 to", title), "", &ok, this);
        if (ok && !newTitle.isEmpty()) {
            BookmarkManager::updateTitle(m_bookmarkManager->getManager(), url, newTitle);
        }
    } else if (selectedAction == settingsAction) {
        showSettingsDialog(url);
    } else if (selectedAction == deleteAction) {

        if (KMessageBox::warningContinueCancel(this, i18n("Are you sure you want to delete %1?", url), i18n("Delete %1", title),
                                               KStandardGuiItem::del()) == KMessageBox::Continue) {
            BookmarkManager::removeByUrl(m_bookmarkManager->getManager(), url);
        }
    }

    menu->deleteLater();
}

void MainWindow::tabContextMenu(QWidget *widget, const QPoint &point)
{
    RemoteViewScrollArea *scrollArea = qobject_cast<RemoteViewScrollArea*>(widget);
    if (!scrollArea) return;
    RemoteView *view = qobject_cast<RemoteView*>(scrollArea->widget());
    if (!view) return;

    const QString url = view->url().prettyUrl(KUrl::RemoveTrailingSlash);
    kDebug(5010) << url;

    KMenu *menu = new KMenu(this);
    menu->addTitle(url);
    QAction *bookmarkAction = menu->addAction(KIcon("bookmark-new"), i18n("Add Bookmark"));
    QAction *closeAction = menu->addAction(KIcon("tab-close"), i18n("Close Tab"));
    QAction *selectedAction = menu->exec(point);
    if (selectedAction) {
        if (selectedAction == closeAction) {
            closeTab(widget);
        } else if (selectedAction == bookmarkAction) {
            m_bookmarkManager->addManualBookmark(url, url);
        }
    }
    menu->deleteLater();
}

void MainWindow::showLocalCursor(bool showLocalCursor)
{
    kDebug(5010) << showLocalCursor;

    RemoteView* view = m_remoteViewList.at(m_currentRemoteView);
    view->showDotCursor(showLocalCursor ? RemoteView::CursorOn : RemoteView::CursorOff);
    view->hostPreferences()->setShowLocalCursor(showLocalCursor);
    saveHostPrefs(view);
}

void MainWindow::viewOnly(bool viewOnly)
{
    kDebug(5010) << viewOnly;

    RemoteView* view = m_remoteViewList.at(m_currentRemoteView);
    view->setViewOnly(viewOnly);
    view->hostPreferences()->setViewOnly(viewOnly);
    saveHostPrefs(view);
}

void MainWindow::grabAllKeys(bool grabAllKeys)
{
    kDebug(5010);

    RemoteView* view = m_remoteViewList.at(m_currentRemoteView);
    view->setGrabAllKeys(grabAllKeys);
    view->hostPreferences()->setGrabAllKeys(grabAllKeys);
    saveHostPrefs(view);
}

void MainWindow::scale(bool scale)
{
    kDebug(5010);

    RemoteView* view = m_remoteViewList.at(m_currentRemoteView);
    view->enableScaling(scale);
    if (m_fullscreenWindow)
        view->hostPreferences()->setFullscreenScale(scale);
    else
        view->hostPreferences()->setWindowedScale(scale);

    saveHostPrefs(view);
}

void MainWindow::showRemoteViewToolbar()
{
    kDebug(5010);

    if (!m_toolBar) {
        actionCollection()->action("switch_fullscreen")->setIcon(KIcon("view-restore"));
        actionCollection()->action("switch_fullscreen")->setText(i18n("Switch to Window Mode"));
        actionCollection()->action("switch_fullscreen")->setIconText(i18n("Window Mode"));

        m_toolBar = new FloatingToolBar(m_fullscreenWindow, m_fullscreenWindow);
        m_toolBar->winId(); // force it to be a native widget (prevents problem with QX11EmbedContainer)
        m_toolBar->setSide(FloatingToolBar::Top);

        KComboBox *sessionComboBox = new KComboBox(m_toolBar);
        sessionComboBox->setStyleSheet("QComboBox:!editable{background:transparent;}");
        sessionComboBox->setModel(m_tabWidget->getModel());
        sessionComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        sessionComboBox->setCurrentIndex(m_tabWidget->currentIndex());
        connect(sessionComboBox, SIGNAL(activated(int)), m_tabWidget, SLOT(setCurrentIndex(int)));
        connect(m_tabWidget, SIGNAL(currentChanged(int)), sessionComboBox, SLOT(setCurrentIndex(int)));
        m_toolBar->addWidget(sessionComboBox);

        QToolBar *buttonBox = new QToolBar(m_toolBar);

        buttonBox->addAction(actionCollection()->action("new_connection"));
        buttonBox->addAction(actionCollection()->action("switch_fullscreen"));

        QAction *minimizeAction = new QAction(m_toolBar);
        minimizeAction->setIcon(KIcon("go-down"));
        minimizeAction->setText(i18n("Minimize Full Screen Window"));
        connect(minimizeAction, SIGNAL(triggered()), m_fullscreenWindow, SLOT(showMinimized()));
        buttonBox->addAction(minimizeAction);

        buttonBox->addAction(actionCollection()->action("take_screenshot"));
        buttonBox->addAction(actionCollection()->action("view_only"));
        buttonBox->addAction(actionCollection()->action("show_local_cursor"));
        buttonBox->addAction(actionCollection()->action("grab_all_keys"));
        buttonBox->addAction(actionCollection()->action("scale"));
        buttonBox->addAction(actionCollection()->action("disconnect"));
        buttonBox->addAction(actionCollection()->action("file_quit"));

        QAction *stickToolBarAction = new QAction(m_toolBar);
        stickToolBarAction->setCheckable(true);
        stickToolBarAction->setIcon(KIcon("object-locked"));
        stickToolBarAction->setText(i18n("Stick Toolbar"));
        connect(stickToolBarAction, SIGNAL(triggered(bool)), m_toolBar, SLOT(setSticky(bool)));
        buttonBox->addAction(stickToolBarAction);

        m_toolBar->addWidget(buttonBox);
    }
}

void setActionStatus(QAction* action, bool enabled, bool visible, bool checked)
{
    action->setEnabled(enabled);
    action->setVisible(visible);
    action->setChecked(checked);
}

void MainWindow::updateActionStatus()
{
    kDebug(5010) << m_tabWidget->currentIndex();

    bool enabled = true;

    if (m_tabWidget->currentWidget() == m_newConnectionWidget)
        enabled = false;

    RemoteView* view = (m_currentRemoteView >= 0 && enabled) ? m_remoteViewList.at(m_currentRemoteView) : 0;

    actionCollection()->action("take_screenshot")->setEnabled(enabled);
    actionCollection()->action("disconnect")->setEnabled(enabled);

    setActionStatus(actionCollection()->action("view_only"),
                    enabled,
                    true,
                    view ? view->viewOnly() : false);

    setActionStatus(actionCollection()->action("show_local_cursor"),
                    enabled,
                    view ? view->supportsLocalCursor() : false,
                    view ? view->dotCursorState() == RemoteView::CursorOn : false);

    setActionStatus(actionCollection()->action("scale"),
                    enabled,
                    view ? view->supportsScaling() : false,
                    view ? view->scaling() : false);

    setActionStatus(actionCollection()->action("grab_all_keys"),
                    enabled,
                    enabled,
                    view ? view->grabAllKeys() : false);
}

void MainWindow::preferences()
{
    // An instance of your dialog could be already created and could be
    // cached, in which case you want to display the cached dialog
    // instead of creating another one
    if (PreferencesDialog::showDialog("preferences"))
        return;

    // KConfigDialog didn't find an instance of this dialog, so lets
    // create it:
    PreferencesDialog *dialog = new PreferencesDialog(this, Settings::self());

    // User edited the configuration - update your local copies of the
    // configuration data
    connect(dialog, SIGNAL(settingsChanged(QString)),
            this, SLOT(updateConfiguration()));

    dialog->show();
}

void MainWindow::updateConfiguration()
{
    if (!Settings::showStatusBar())
        statusBar()->deleteLater();
    else
        statusBar()->showMessage(""); // force creation of statusbar

    m_tabWidget->setTabBarHidden((m_tabWidget->count() <= 1 && !Settings::showTabBar()) || m_fullscreenWindow);
    m_tabWidget->setTabPosition((KTabWidget::TabPosition) Settings::tabPosition());
#if QT_VERSION >= 0x040500
    m_tabWidget->setTabsClosable(Settings::tabCloseButton());
#else
    m_tabWidget->setCloseButtonEnabled(Settings::tabCloseButton());
#endif
    disconnect(m_tabWidget, SIGNAL(mouseMiddleClick(QWidget*)), this, SLOT(closeTab(QWidget*))); // just be sure it is not connected twice
    if (Settings::tabMiddleClick())
        connect(m_tabWidget, SIGNAL(mouseMiddleClick(QWidget*)), SLOT(closeTab(QWidget*)));

    if (Settings::systemTrayIcon() && !m_systemTrayIcon) {
        m_systemTrayIcon = new SystemTrayIcon(this);
        if(m_fullscreenWindow) m_systemTrayIcon->setAssociatedWidget(m_fullscreenWindow);
    } else if (m_systemTrayIcon) {
        delete m_systemTrayIcon;
        m_systemTrayIcon = 0;
    }

    // update the scroll areas background color
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        QPalette palette = m_tabWidget->widget(i)->palette();
        palette.setColor(QPalette::Dark, Settings::backgroundColor());
        m_tabWidget->widget(i)->setPalette(palette);
    }

    // Send update configuration message to all views
    for (int i = 0; i < m_remoteViewList.count(); ++i) {
        m_remoteViewList.at(i)->updateConfiguration();
    }

}

void MainWindow::quit(bool systemEvent)
{
    const bool haveRemoteConnections = !m_remoteViewList.isEmpty();
    if (systemEvent || !haveRemoteConnections || KMessageBox::warningContinueCancel(this,
            i18n("Are you sure you want to quit the KDE Remote Desktop Client?"),
            i18n("Confirm Quit"),
            KStandardGuiItem::quit(), KStandardGuiItem::cancel(),
            "DoNotAskBeforeExit") == KMessageBox::Continue) {

        if (Settings::rememberSessions()) { // remember open remote views for next startup
            QStringList list;
            for (int i = 0; i < m_remoteViewList.count(); ++i) {
                kDebug(5010) << m_remoteViewList.at(i)->url();
                list.append(m_remoteViewList.at(i)->url().prettyUrl(KUrl::RemoveTrailingSlash));
            }
            Settings::setOpenSessions(list);
        }

        saveHostPrefs();

        for (int i = 0; i < m_remoteViewList.count(); ++i) {
            m_remoteViewList.at(i)->startQuitting();
        }

        Settings::self()->writeConfig();

        qApp->quit();
    }
}

void MainWindow::configureNotifications()
{
    KNotifyConfigWidget::configure(this);
}

void MainWindow::configureKeys()
{
    KShortcutsDialog::configure(actionCollection());
}

void MainWindow::configureToolbars()
{
    KEditToolBar edit(actionCollection());
    connect(&edit, SIGNAL(newToolBarConfig()), this, SLOT(newToolbarConfig()));
    edit.exec();
}

void MainWindow::showMenubar()
{
    if (m_menubarAction->isChecked())
        menuBar()->show();
    else
        menuBar()->hide();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // check for close events from the fullscreen window.
    if (obj == m_fullscreenWindow && event->type() == QEvent::Close) {
        quit(true);
    }
    // allow other events to pass through.
    return QObject::eventFilter(obj, event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (event->spontaneous()) { // Returns true if the event originated outside the application (a system event); otherwise returns false.
        event->ignore();
        if (Settings::systemTrayIcon()) {
            hide(); // just hide the mainwindow, keep it in systemtray
        } else {
            quit();
        }
    } else {
        quit(true);
    }
}

void MainWindow::saveProperties(KConfigGroup &group)
{
    kDebug(5010);
    KMainWindow::saveProperties(group);
    saveHostPrefs();
}


void MainWindow::saveHostPrefs()
{
    for (int i = 0; i < m_remoteViewList.count(); ++i) {
        saveHostPrefs(m_remoteViewList.at(i));
        m_remoteViewList.at(i)->startQuitting();
    }
}

void MainWindow::saveHostPrefs(RemoteView* view)
{
    // should saving this be a user option?
    if (view && view->scaling()) {
        QSize viewSize = m_tabWidget->currentWidget()->size();
        kDebug(5010) << "saving window size:" << viewSize;
        view->hostPreferences()->setWidth(viewSize.width());
        view->hostPreferences()->setHeight(viewSize.height());
    }

    Settings::self()->config()->sync();
}

void MainWindow::tabChanged(int index)
{
    kDebug(5010) << index;

    m_tabWidget->setTabBarHidden((m_tabWidget->count() <= 1 && !Settings::showTabBar()) || m_fullscreenWindow);

    m_currentRemoteView = index;

    if (m_tabWidget->currentWidget() == m_newConnectionWidget) {
        m_currentRemoteView = -1;
        if(m_addressInput) m_addressInput->setFocus();
    }

    const QString tabTitle = m_tabWidget->tabText(index).remove('&');
    setCaption(tabTitle == i18n("New Connection") ? QString() : tabTitle);

    updateActionStatus();
}

QWidget* MainWindow::newConnectionWidget()
{
    if (m_newConnectionWidget)
        return m_newConnectionWidget;

    m_newConnectionWidget = new QWidget(this);

    QVBoxLayout *startLayout = new QVBoxLayout(m_newConnectionWidget);
    startLayout->setContentsMargins(QMargins(8, 12, 8, 4));

    QLabel *headerLabel = new QLabel(m_newConnectionWidget);
    headerLabel->setText(i18n("<h1>KDE Remote Desktop Client</h1><br />Enter or select the address of the desktop you would like to connect to."));

    QLabel *headerIconLabel = new QLabel(m_newConnectionWidget);
    headerIconLabel->setPixmap(KIcon("krdc").pixmap(80));

    QHBoxLayout *headerLayout = new QHBoxLayout;
    headerLayout->addWidget(headerLabel, 1, Qt::AlignTop);
    headerLayout->addWidget(headerIconLabel);
    startLayout->addLayout(headerLayout);

    {
        QHBoxLayout *connectLayout = new QHBoxLayout;

        QLabel *addressLabel = new QLabel(i18n("Connect to:"), m_newConnectionWidget);
        m_protocolInput = new KComboBox(m_newConnectionWidget);
        m_addressInput = new KLineEdit(m_newConnectionWidget);
        m_addressInput->setClearButtonShown(true);
        m_addressInput->setClickMessage(i18n("Type here to connect to an address and filter the list."));
        connect(m_addressInput, SIGNAL(textChanged(QString)), m_remoteDesktopsModelProxy, SLOT(setFilterFixedString(QString)));

        foreach(RemoteViewFactory *factory, m_remoteViewFactories) {
            m_protocolInput->addItem(factory->scheme());
        }

        connect(m_addressInput, SIGNAL(returnPressed()), SLOT(newConnection()));
        m_addressInput->setToolTip(i18n("Type an IP or DNS Name here. Clear the line to get a list of connection methods."));

        KPushButton *connectButton = new KPushButton(m_newConnectionWidget);
        connectButton->setToolTip(i18n("Goto Address"));
        connectButton->setIcon(KIcon("go-jump-locationbar"));
        connect(connectButton, SIGNAL(clicked()), SLOT(newConnection()));

        connectLayout->addWidget(addressLabel);
        connectLayout->addWidget(m_protocolInput);
        connectLayout->addWidget(m_addressInput, 1);
        connectLayout->addWidget(connectButton);
        connectLayout->setContentsMargins(QMargins(0, 6, 0, 10));
        startLayout->addLayout(connectLayout);
    }

    {
        m_newConnectionTableView = new QTableView(m_newConnectionWidget);
        m_newConnectionTableView->setModel(m_remoteDesktopsModelProxy);

        // set up the view so it looks nice
        m_newConnectionTableView->setItemDelegate(new ConnectionDelegate(m_newConnectionTableView));
        m_newConnectionTableView->setShowGrid(false);
        m_newConnectionTableView->setSelectionMode(QAbstractItemView::NoSelection);
        m_newConnectionTableView->verticalHeader()->hide();
        m_newConnectionTableView->verticalHeader()->setDefaultSectionSize(
            m_newConnectionTableView->fontMetrics().height() + 3);
        m_newConnectionTableView->horizontalHeader()->setStretchLastSection(true);
        m_newConnectionTableView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
        m_newConnectionTableView->setAlternatingRowColors(true);
        // set up sorting and actions (double click open, right click custom menu)
        m_newConnectionTableView->setSortingEnabled(true);
        m_newConnectionTableView->sortByColumn(Settings::connectionListSortColumn(), Qt::SortOrder(Settings::connectionListSortOrder()));
        connect(m_newConnectionTableView->horizontalHeader(), SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)),
                SLOT(saveConnectionListSort(int,Qt::SortOrder)));
        connect(m_newConnectionTableView, SIGNAL(doubleClicked(QModelIndex)),
                SLOT(openFromRemoteDesktopsModel(QModelIndex)));
        m_newConnectionTableView->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_newConnectionTableView, SIGNAL(customContextMenuRequested(QPoint)), SLOT(showConnectionContextMenu(QPoint)));

        startLayout->addWidget(m_newConnectionTableView);
    }

    return m_newConnectionWidget;
}

void MainWindow::saveConnectionListSort(const int logicalindex, const Qt::SortOrder order)
{
    Settings::setConnectionListSortColumn(logicalindex);
    Settings::setConnectionListSortOrder(order);
    Settings::self()->writeConfig();
}

void MainWindow::newConnectionPage(bool clearInput)
{
    const int indexOfNewConnectionWidget = m_tabWidget->indexOf(m_newConnectionWidget);
    if (indexOfNewConnectionWidget >= 0)
        m_tabWidget->setCurrentIndex(indexOfNewConnectionWidget);
    else {
        const int index = m_tabWidget->addTab(newConnectionWidget(), i18n("New Connection"));
        m_tabWidget->setCurrentIndex(index);
    }
    if(clearInput) {
        m_addressInput->clear();
    } else {
        m_addressInput->selectAll();
    }
    m_addressInput->setFocus();
}

QList<RemoteView *> MainWindow::remoteViewList() const
{
    return m_remoteViewList;
}

QList<RemoteViewFactory *> MainWindow::remoteViewFactoriesList() const
{
    return m_remoteViewFactories.values();
}

int MainWindow::currentRemoteView() const
{
    return m_currentRemoteView;
}

void MainWindow::createDockWidget()
{
    QDockWidget *remoteDesktopsDockWidget = new QDockWidget(this);
    QWidget *remoteDesktopsDockLayoutWidget = new QWidget(remoteDesktopsDockWidget);
    QVBoxLayout *remoteDesktopsDockLayout = new QVBoxLayout(remoteDesktopsDockLayoutWidget);
    remoteDesktopsDockWidget->setObjectName("remoteDesktopsDockWidget"); // required for saving position / state
    remoteDesktopsDockWidget->setWindowTitle(i18n("Remote Desktops"));
    QFontMetrics fontMetrics(remoteDesktopsDockWidget->font());
    remoteDesktopsDockWidget->setMinimumWidth(fontMetrics.width("vnc://192.168.100.100:6000"));
    actionCollection()->addAction("remote_desktop_dockwidget",
                                  remoteDesktopsDockWidget->toggleViewAction());

    m_dockWidgetTableView = new QTableView(remoteDesktopsDockLayoutWidget);
    RemoteDesktopsModel *remoteDesktopsModel = new RemoteDesktopsModel(this);
    m_remoteDesktopsModelProxy = new QSortFilterProxyModel(this);
    m_remoteDesktopsModelProxy->setSourceModel(remoteDesktopsModel);
    m_remoteDesktopsModelProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_remoteDesktopsModelProxy->setFilterRole(10002);
    m_dockWidgetTableView->setModel(m_remoteDesktopsModelProxy);

    m_dockWidgetTableView->setShowGrid(false);
    m_dockWidgetTableView->verticalHeader()->hide();
    m_dockWidgetTableView->verticalHeader()->setDefaultSectionSize(
        m_dockWidgetTableView->fontMetrics().height() + 2);
    m_dockWidgetTableView->horizontalHeader()->hide();
    m_dockWidgetTableView->horizontalHeader()->setStretchLastSection(true);
    // hide all columns, then show the one we want
    for (int i=0; i<m_remoteDesktopsModelProxy->columnCount(); i++) {
        m_dockWidgetTableView->hideColumn(i);
    }
    m_dockWidgetTableView->showColumn(RemoteDesktopsModel::Title);
    m_dockWidgetTableView->sortByColumn(RemoteDesktopsModel::Title, Qt::AscendingOrder);

    connect(m_dockWidgetTableView, SIGNAL(doubleClicked(QModelIndex)),
            SLOT(openFromRemoteDesktopsModel(QModelIndex)));

    KLineEdit *filterLineEdit = new KLineEdit(remoteDesktopsDockLayoutWidget);
    filterLineEdit->setClickMessage(i18n("Filter"));
    filterLineEdit->setClearButtonShown(true);
    connect(filterLineEdit, SIGNAL(textChanged(QString)), m_remoteDesktopsModelProxy, SLOT(setFilterFixedString(QString)));
    remoteDesktopsDockLayout->addWidget(filterLineEdit);
    remoteDesktopsDockLayout->addWidget(m_dockWidgetTableView);
    remoteDesktopsDockWidget->setWidget(remoteDesktopsDockLayoutWidget);
    addDockWidget(Qt::LeftDockWidgetArea, remoteDesktopsDockWidget);
}

#include "mainwindow.moc"
