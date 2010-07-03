/****************************************************************************
**
** Copyright (C) 2007 - 2009 Urs Wolfer <uwolfer @ kde.org>
** Copyright (C) 2009 Tony Murray <murraytony @ gmail.com>
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
#include <KApplication>
#include <KComboBox>
#include <KEditToolBar>
#include <KIcon>
#include <KLineEdit>
#include <KLocale>
#include <KMenu>
#include <KMenuBar>
#include <KMessageBox>
#include <KNotifyConfigWidget>
#include <KPluginInfo>
#include <KPushButton>
#include <KShortcut>
#include <KShortcutsDialog>
#include <KStatusBar>
#include <KToggleAction>
#include <KToggleFullScreenAction>
#include <KUrlComboBox>
#include <KUrlNavigator>
#include <KServiceTypeTrader>
#include <KStandardDirs>

#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopWidget>
#include <QDockWidget>
#include <QFontMetrics>
#include <QHeaderView>
#include <QLabel>
#include <QLayout>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QTreeView>

MainWindow::MainWindow(QWidget *parent)
        : KXmlGuiWindow(parent),
        m_fullscreenWindow(0),
        m_addressNavigator(0),
        m_toolBar(0),
        m_topBottomBorder(0),
        m_leftRightBorder(0),
        m_currentRemoteView(-1),
        m_systemTrayIcon(0),
        m_remoteDesktopsTreeView(0),
        m_remoteDesktopsNewConnectionTabTreeView(0),
#ifdef TELEPATHY_SUPPORT
        m_tubesManager(0),
#endif
        m_newConnectionWidget(0)
{
    loadAllPlugins();

    setupActions();

    setStandardToolBarMenuEnabled(true);

    m_tabWidget = new TabbedViewWidget(this);
    m_tabWidget->setTabPosition((KTabWidget::TabPosition) Settings::tabPosition());

#if QT_VERSION <= 0x040500
    m_tabWidget->setTabsClosable(Settings::tabCloseButton());
#else
    m_tabWidget->setCloseButtonEnabled(Settings::tabCloseButton());
#endif

    connect(m_tabWidget, SIGNAL(closeRequest(QWidget *)), SLOT(closeTab(QWidget *)));

    if (Settings::tabMiddleClick())
        connect(m_tabWidget, SIGNAL(mouseMiddleClick(QWidget *)), SLOT(closeTab(QWidget *)));

    connect(m_tabWidget, SIGNAL(mouseDoubleClick(QWidget *)), SLOT(openTabSettings(QWidget *)));
    connect(m_tabWidget, SIGNAL(contextMenu(QWidget *, const QPoint &)), SLOT(tabContextMenu(QWidget *, const QPoint &)));

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
        m_tabWidget->addTab(newConnectionWidget(), "New Connection");

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
    m_tubesManager = Tp::SharedPtr<TubesManager>(new TubesManager(this));
    connect(m_tubesManager.data(),
            SIGNAL(newConnection(KUrl)),
            SLOT(newConnection(KUrl)));

    m_registrar = Tp::ClientRegistrar::create();
    m_registrar->registerClient(Tp::AbstractClientPtr::dynamicCast(m_tubesManager), "krdc_rfb_handler");
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

void MainWindow::newConnection(const KUrl &newUrl, bool switchFullscreenWhenConnected)
{
    m_switchFullscreenWhenConnected = switchFullscreenWhenConnected;

    const KUrl url = newUrl.isEmpty() ? m_addressNavigator->uncommittedUrl() : newUrl;

    if (!url.isValid() || (url.host().isEmpty() && url.port() < 0)
            || !url.path().isEmpty()) {
        KMessageBox::error(this,
                           i18n("The entered address does not have the required form."),
                           i18n("Malformed URL"));
        return;
    }

    if (m_addressNavigator)
        m_addressNavigator->setUrl(KUrl(url.scheme().toLower() + "://"));

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
    if (! prefs->showDialogIfNeeded()) {
        delete view;
        return;
    }

    saveHostPrefs();

    view->setGrabAllKeys(prefs->grabAllKeys());
    view->showDotCursor(prefs->showLocalCursor() ? RemoteView::CursorOn : RemoteView::CursorOff);
    view->setViewOnly(prefs->viewOnly());
    if (! switchFullscreenWhenConnected) view->enableScaling(prefs->windowedScale()); 

    connect(view, SIGNAL(framebufferSizeChanged(int, int)), this, SLOT(resizeTabWidget(int, int)));
    connect(view, SIGNAL(statusChanged(RemoteView::RemoteStatus)), this, SLOT(statusChanged(RemoteView::RemoteStatus)));
    connect(view, SIGNAL(disconnected()), this, SLOT(disconnectHost()));

    m_remoteViewList.append(view);

//     view->resize(0, 0);
 
    if (m_remoteViewList.size() == 1) {
        kDebug(5010) << "First connection, restoring window size.";
        restoreWindowSize(view->hostPreferences()->configGroup());
    }

    QScrollArea *scrollArea = createScrollArea(m_tabWidget, view);

    const int indexOfNewConnectionWidget = m_tabWidget->indexOf(m_newConnectionWidget);
    if (indexOfNewConnectionWidget >= 0)
        m_tabWidget->removeTab(indexOfNewConnectionWidget);

    const int newIndex = m_tabWidget->addTab(scrollArea, KIcon("krdc"), url.prettyUrl(KUrl::RemoveTrailingSlash));
    m_tabWidget->setCurrentIndex(newIndex);
    tabChanged(newIndex); // force to update m_currentRemoteView (tabChanged is not emitted when start page has been disabled)

    view->start();
}

void MainWindow::openFromDockWidget(const QModelIndex &index)
{
    const QString data = index.data(10001).toString();
    if (!data.isEmpty()) {
        const KUrl url(data);
        // first check if url has already been opened; in case show the tab
        for (int i = 0; i < m_remoteViewList.count(); ++i) {
            if (m_remoteViewList.at(i)->url() == url) {
                m_tabWidget->setCurrentIndex(i);
                return;
            }
        }
        newConnection(url);
    }
}

void MainWindow::resizeTabWidget(int w, int h)
{
    kDebug(5010) << "tabwidget resize: w: " << w << ", h: " << h;
    if (m_fullscreenWindow) {
        kDebug(5010) << "in fullscreen mode, refusing to resize";
        return;
    }

    RemoteView* view = m_currentRemoteView >= 0 ? m_remoteViewList.at(m_currentRemoteView) : 0;
    if (view && view->scaling()) return;

    if (m_topBottomBorder == 0) { // the values are not cached yet
        QScrollArea *tmp = qobject_cast<QScrollArea *>(m_tabWidget->currentWidget());

        m_leftRightBorder = m_tabWidget->width() - m_tabWidget->currentWidget()->width() + (2 * tmp->frameWidth());
        m_topBottomBorder = m_tabWidget->height() - m_tabWidget->currentWidget()->height() + (2 * tmp->frameWidth());

        kDebug(5010) << "tabwidget border: w: " << m_leftRightBorder << ", h: " << m_topBottomBorder;
    }

    const int newTabWidth = w + m_leftRightBorder;
    const int newTabHeight = h + m_topBottomBorder;

    const QSize newWindowSize = size() - m_tabWidget->size() + QSize(newTabWidth, newTabHeight);

    const QSize desktopSize = QSize(QApplication::desktop()->availableGeometry().width(),
                              QApplication::desktop()->availableGeometry().height());

    if ((newWindowSize.height() >= desktopSize.height()) || (newWindowSize.width() >= desktopSize.width())) {
        kDebug(5010) << "remote desktop needs more place than available -> show window maximized";
        setWindowState(windowState() | Qt::WindowMaximized);
        return;
    }

    //WORKAROUND: QTabWidget resize problem. Let's see if there is a clean solution for this issue.
    m_tabWidget->setMinimumSize(newTabWidth, newTabHeight);
    m_tabWidget->adjustSize();
    QCoreApplication::processEvents();
    m_tabWidget->setMinimumSize(500, 400);
}

void MainWindow::statusChanged(RemoteView::RemoteStatus status)
{
    kDebug(5010) << status;

    // the remoteview is already deleted, so don't show it; otherwise it would crash
    if (status == RemoteView::Disconnecting || status == RemoteView::Disconnected)
        return;

    const QString host = m_remoteViewList.at(m_currentRemoteView)->host();

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

        // when started with command line fullscreen argument
        if (m_switchFullscreenWhenConnected) {
            m_switchFullscreenWhenConnected = false;
            switchFullscreen();
        }

        m_bookmarkManager->addHistoryBookmark();

        break;
    default:
        break;
    }

    m_tabWidget->setTabIcon(m_tabWidget->currentIndex(), KIcon(iconName));
    if (Settings::showStatusBar())
        statusBar()->showMessage(message);
}

void MainWindow::takeScreenshot()
{
    const QPixmap snapshot = QPixmap::grabWidget(m_remoteViewList.at(m_currentRemoteView));

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

        foreach (RemoteView *currentView, m_remoteViewList) {
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

        foreach (RemoteView *currentView, m_remoteViewList) {
            currentView->enableScaling(currentView->hostPreferences()->fullscreenScale());
        }

        QVBoxLayout *fullscreenLayout = new QVBoxLayout(m_fullscreenWindow);
        fullscreenLayout->setMargin(0);
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
}

QScrollArea *MainWindow::createScrollArea(QWidget *parent, RemoteView *remoteView)
{
    RemoteViewScrollArea *scrollArea = new RemoteViewScrollArea(parent);
    scrollArea->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    connect(scrollArea, SIGNAL(resized(int, int)), remoteView, SLOT(scaleResize(int, int)));

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
        newConnectionPage();
    }

    // if the newConnectionWidget is the only tab and we are fullscreen, switch to window mode
    if (m_fullscreenWindow && m_tabWidget->count() == 1  && m_tabWidget->currentWidget() == m_newConnectionWidget) {
        switchFullscreen();
    }
}

void MainWindow::closeTab(QWidget *widget)
{
    bool isNewConnectionPage = m_tabWidget->currentWidget() == m_newConnectionWidget;
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
        newConnectionPage();
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
        prefs->showDialog();
        delete prefs;
    } else {
        KMessageBox::error(this,
                           i18n("The selected host cannot be handled."),
                           i18n("Unusable URL"));
    }
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
    saveHostPrefs();
}

void MainWindow::viewOnly(bool viewOnly)
{
    kDebug(5010) << viewOnly;

    RemoteView* view = m_remoteViewList.at(m_currentRemoteView);
    view->setViewOnly(viewOnly);
    view->hostPreferences()->setViewOnly(viewOnly);
    saveHostPrefs();
}

void MainWindow::grabAllKeys(bool grabAllKeys)
{
    kDebug(5010);

    RemoteView* view = m_remoteViewList.at(m_currentRemoteView);
    view->setGrabAllKeys(grabAllKeys);
    view->hostPreferences()->setGrabAllKeys(grabAllKeys);
    saveHostPrefs();
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
    
    saveHostPrefs();
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

        m_toolBar->addAction(actionCollection()->action("new_connection"));
        m_toolBar->addAction(actionCollection()->action("switch_fullscreen"));

        QAction *minimizeAction = new QAction(m_toolBar);
        minimizeAction->setIcon(KIcon("go-down"));
        minimizeAction->setText(i18n("Minimize Full Screen Window"));
        connect(minimizeAction, SIGNAL(triggered()), m_fullscreenWindow, SLOT(showMinimized()));
        m_toolBar->addAction(minimizeAction);

        m_toolBar->addAction(actionCollection()->action("take_screenshot"));
        m_toolBar->addAction(actionCollection()->action("view_only"));
        m_toolBar->addAction(actionCollection()->action("show_local_cursor"));
        m_toolBar->addAction(actionCollection()->action("grab_all_keys"));
        m_toolBar->addAction(actionCollection()->action("scale"));
        m_toolBar->addAction(actionCollection()->action("disconnect"));

        QAction *stickToolBarAction = new QAction(m_toolBar);
        stickToolBarAction->setCheckable(true);
        stickToolBarAction->setIcon(KIcon("object-locked"));
        stickToolBarAction->setText(i18n("Stick Toolbar"));
        connect(stickToolBarAction, SIGNAL(triggered(bool)), m_toolBar, SLOT(setSticky(bool)));
        m_toolBar->addAction(stickToolBarAction);
    }

    m_toolBar->showAndAnimate();
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
    connect(dialog, SIGNAL(settingsChanged(const QString&)),
            this, SLOT(updateConfiguration()));

    dialog->show();
}

void MainWindow::updateConfiguration()
{
    if (m_addressNavigator)
        m_addressNavigator->setUrlEditable(Settings::normalUrlInputLine());

    if (!Settings::showStatusBar())
        statusBar()->deleteLater();
    else
        statusBar()->showMessage(""); // force creation of statusbar

    m_tabWidget->setTabBarHidden((m_tabWidget->count() <= 1 && !Settings::showTabBar()) || m_fullscreenWindow);
    m_tabWidget->setTabPosition((KTabWidget::TabPosition) Settings::tabPosition());
#if QT_VERSION <= 0x040500
    m_tabWidget->setTabsClosable(Settings::tabCloseButton());
#else
    m_tabWidget->setCloseButtonEnabled(Settings::tabCloseButton());
#endif
    disconnect(m_tabWidget, SIGNAL(mouseMiddleClick(QWidget *)), this, SLOT(closeTab(QWidget *))); // just be sure it is not connected twice
    if (Settings::tabMiddleClick())
        connect(m_tabWidget, SIGNAL(mouseMiddleClick(QWidget *)), SLOT(closeTab(QWidget *)));

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
    connect(&edit, SIGNAL(newToolbarConfig()), this, SLOT(newToolbarConfig()));
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

void MainWindow::saveHostPrefs(RemoteView* view)
{
    // Save default window size for currently active view
    if (!view)
        view = m_currentRemoteView >= 0 ? m_remoteViewList.at(m_currentRemoteView) : 0;

    if (view)
        saveWindowSize(view->hostPreferences()->configGroup());

    Settings::self()->config()->sync();
}

void MainWindow::tabChanged(int index)
{
    kDebug(5010) << index;

    m_tabWidget->setTabBarHidden((m_tabWidget->count() <= 1 && !Settings::showTabBar()) || m_fullscreenWindow);

    m_currentRemoteView = index;

    if (m_tabWidget->currentWidget() == m_newConnectionWidget)
        m_currentRemoteView = -1;

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
    startLayout->setMargin(20);

    QLabel *headerLabel = new QLabel(this);
    headerLabel->setText(i18n("<h1>KDE Remote Desktop Client</h1><br /><br />What would you like to do?<br />"));

    QLabel *headerIconLabel = new QLabel(this);
    headerIconLabel->setPixmap(KIcon("krdc").pixmap(128));

    QHBoxLayout *headerLayout = new QHBoxLayout;
    headerLayout->addWidget(headerLabel, 1, Qt::AlignTop);
    headerLayout->addWidget(headerIconLabel);
    startLayout->addLayout(headerLayout);

    {
        QHBoxLayout *connectLayout = new QHBoxLayout;
        const QString initialProtocol(!m_remoteViewFactories.isEmpty() ? (*m_remoteViewFactories.begin())->scheme() : QString());
        m_addressNavigator = new KUrlNavigator(0, KUrl(initialProtocol + "://"), this);

        QStringList schemes;
        foreach(RemoteViewFactory *factory, m_remoteViewFactories) {
            schemes << factory->scheme();
        }
        m_addressNavigator->setCustomProtocols(schemes);

        m_addressNavigator->setUrlEditable(Settings::normalUrlInputLine());
        connect(m_addressNavigator, SIGNAL(returnPressed()), SLOT(newConnection()));
        m_addressNavigator->setFocus();

        QLabel *addressLabel = new QLabel(i18n("Connect to:"), this);

        KPushButton *connectButton = new KPushButton(this);
        connectButton->setToolTip(i18n("Goto Address"));
        connectButton->setIcon(KIcon("go-jump-locationbar"));
        connect(connectButton, SIGNAL(clicked()), SLOT(newConnection()));

        connectLayout->addWidget(addressLabel);
        connectLayout->addWidget(m_addressNavigator, 1);
        connectLayout->addWidget(connectButton);
        startLayout->addLayout(connectLayout);
    }

    {
        QWidget *remoteDesktopsDockLayoutWidget = new QWidget(this);
        QVBoxLayout *remoteDesktopsDockLayout = new QVBoxLayout(remoteDesktopsDockLayoutWidget);
        remoteDesktopsDockLayout->setMargin(0);
        m_remoteDesktopsNewConnectionTabTreeView = new QTreeView(remoteDesktopsDockLayoutWidget);

        m_remoteDesktopsNewConnectionTabTreeView->setModel(m_remoteDesktopsModelProxy);
        m_remoteDesktopsNewConnectionTabTreeView->header()->hide();
        m_remoteDesktopsNewConnectionTabTreeView->expandAll();
        m_remoteDesktopsNewConnectionTabTreeView->setItemsExpandable(true);
        m_remoteDesktopsNewConnectionTabTreeView->setRootIsDecorated(false);
        connect(m_remoteDesktopsNewConnectionTabTreeView, SIGNAL(doubleClicked(const QModelIndex &)),
                                        SLOT(openFromDockWidget(const QModelIndex &)));

        KLineEdit *filterLineEdit = new KLineEdit(remoteDesktopsDockLayoutWidget);
        filterLineEdit->setClickMessage(i18n("Filter"));
        filterLineEdit->setClearButtonShown(true);
        connect(filterLineEdit, SIGNAL(textChanged(const QString &)), SLOT(updateFilter(const QString &)));
        remoteDesktopsDockLayout->addWidget(filterLineEdit);
        remoteDesktopsDockLayout->addWidget(m_remoteDesktopsNewConnectionTabTreeView);
        startLayout->addWidget(remoteDesktopsDockLayoutWidget);
    }

    return m_newConnectionWidget;
}


void MainWindow::newConnectionPage()
{
    const int indexOfNewConnectionWidget = m_tabWidget->indexOf(m_newConnectionWidget);
    if (indexOfNewConnectionWidget >= 0)
        m_tabWidget->setCurrentIndex(indexOfNewConnectionWidget);
    else {
        const int index = m_tabWidget->addTab(newConnectionWidget(), "New Connection");
        m_tabWidget->setCurrentIndex(index);
    }
#if 0 // not usable anymore, probably use it in another way? -uwolfer
    QObject *senderObject = qobject_cast<QObject*>(sender());
    if (senderObject) {
        const QString scheme(senderObject->property("schemeString").toString());
        if (!scheme.isEmpty()) {
            m_addressNavigator->setUrl(KUrl(scheme + "://"));
        }
        const QString toolTip(senderObject->property("toolTipString").toString());
        if (!toolTip.isEmpty()) {
            QToolTip::showText(m_addressNavigator->pos() + pos() + QPoint(m_addressNavigator->width() / 2,
                                                                          m_addressNavigator->height() / 2),
                            toolTip, this);
        }
    }
#endif
    m_addressNavigator->setFocus();
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

void MainWindow::expandTreeViewItems()
{
    metaObject()->invokeMethod(m_remoteDesktopsTreeView, "expandAll", Qt::QueuedConnection);
    if (m_remoteDesktopsNewConnectionTabTreeView)
        metaObject()->invokeMethod(m_remoteDesktopsNewConnectionTabTreeView, "expandAll", Qt::QueuedConnection);
}

void MainWindow::updateFilter(const QString &text)
{
    m_remoteDesktopsModelProxy->setFilterRegExp(QRegExp("IGNORE|" + text, Qt::CaseInsensitive, QRegExp::RegExp));
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
    m_remoteDesktopsTreeView = new QTreeView(remoteDesktopsDockLayoutWidget);
    RemoteDesktopsModel *remoteDesktopsModel = new RemoteDesktopsModel(this);

    m_remoteDesktopsModelProxy = new QSortFilterProxyModel(this);
    m_remoteDesktopsModelProxy->setSourceModel(remoteDesktopsModel);
    m_remoteDesktopsModelProxy->setFilterKeyColumn(0);
    m_remoteDesktopsModelProxy->setFilterRole(10002);

    connect(remoteDesktopsModel, SIGNAL(modelReset()), this, SLOT(expandTreeViewItems()));
    m_remoteDesktopsTreeView->setModel(m_remoteDesktopsModelProxy);
    m_remoteDesktopsTreeView->header()->hide();
    m_remoteDesktopsTreeView->expandAll();
    m_remoteDesktopsTreeView->setItemsExpandable(true);
    m_remoteDesktopsTreeView->setRootIsDecorated(false);
    connect(m_remoteDesktopsTreeView, SIGNAL(doubleClicked(const QModelIndex &)),
                                    SLOT(openFromDockWidget(const QModelIndex &)));

    KLineEdit *filterLineEdit = new KLineEdit(remoteDesktopsDockLayoutWidget);
    filterLineEdit->setClickMessage(i18n("Filter"));
    filterLineEdit->setClearButtonShown(true);
    connect(filterLineEdit, SIGNAL(textChanged(const QString &)), SLOT(updateFilter(const QString &)));
    remoteDesktopsDockLayout->addWidget(filterLineEdit);
    remoteDesktopsDockLayout->addWidget(m_remoteDesktopsTreeView);
    remoteDesktopsDockWidget->setWidget(remoteDesktopsDockLayoutWidget);
    addDockWidget(Qt::LeftDockWidgetArea, remoteDesktopsDockWidget);
}

#include "mainwindow.moc"
