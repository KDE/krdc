/****************************************************************************
**
** Copyright (C) 2007 - 2009 Urs Wolfer <uwolfer @ kde.org>
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
#include "hostpreferences.h"

#ifdef BUILD_ZEROCONF
#include "zeroconf/zeroconfpage.h"
#endif

#include <KAction>
#include <KActionCollection>
#include <KActionMenu>
#include <KApplication>
#include <KComboBox>
#include <KEditToolBar>
#include <KIcon>
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
#include <KTabWidget>
#include <KToggleAction>
#include <KToggleFullScreenAction>
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
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QTreeView>

MainWindow::MainWindow(QWidget *parent)
        : KXmlGuiWindow(parent),
        m_fullscreenWindow(0),
        m_toolBar(0),
        m_topBottomBorder(0),
        m_leftRightBorder(0),
        m_currentRemoteView(-1),
        m_showStartPage(false),
        m_systemTrayIcon(0),
        m_zeroconfPage(0)
{
    loadAllPlugins();

    setupActions();

    setStandardToolBarMenuEnabled(true);

    m_tabWidget = new KTabWidget(this);
    m_tabWidget->setTabBarHidden(!Settings::showTabBar());
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

    QDockWidget *remoteDesktopsDockWidget = new QDockWidget(this);
    remoteDesktopsDockWidget->setObjectName("remoteDesktopsDockWidget"); // required for saving position / state
    remoteDesktopsDockWidget->setWindowTitle(i18n("Remote desktops"));
    QFontMetrics fontMetrics(remoteDesktopsDockWidget->font());
    remoteDesktopsDockWidget->setMinimumWidth(fontMetrics.width("vnc://192.168.100.100:6000"));
    actionCollection()->addAction("remote_desktop_dockwidget",
                                  remoteDesktopsDockWidget->toggleViewAction());
    m_remoteDesktopsTreeView = new QTreeView(remoteDesktopsDockWidget);
    RemoteDesktopsModel *remoteDesktopsModel = new RemoteDesktopsModel(this);
    connect(remoteDesktopsModel, SIGNAL(modelReset()), this, SLOT(expandTreeViewItems()));
    m_remoteDesktopsTreeView->setModel(remoteDesktopsModel);
    m_remoteDesktopsTreeView->header()->hide();
    m_remoteDesktopsTreeView->expandAll();
    m_remoteDesktopsTreeView->setItemsExpandable(false);
    m_remoteDesktopsTreeView->setRootIsDecorated(false);
    connect(m_remoteDesktopsTreeView, SIGNAL(doubleClicked(const QModelIndex &)),
                                    SLOT(openFromDockWidget(const QModelIndex &)));

    remoteDesktopsDockWidget->setWidget(m_remoteDesktopsTreeView);
    addDockWidget(Qt::LeftDockWidgetArea, remoteDesktopsDockWidget);

    createGUI("krdcui.rc");

    if (Settings::systemTrayIcon()) {
        m_systemTrayIcon = new SystemTrayIcon(this);
        m_systemTrayIcon->setVisible(true);
    }

    connect(m_tabWidget, SIGNAL(currentChanged(int)), SLOT(tabChanged(int)));

    statusBar()->showMessage(i18n("KDE Remote Desktop Client started"));

    updateActionStatus(); // disable remote view actions

    if (Settings::showStartPage())
        createStartPage();

    setAutoSaveSettings(); // e.g toolbar position, mainwindow size, ...

    m_addressNavigator->setFocus();

    if (Settings::rememberSessions()) // give some time to create and show the window first
        QTimer::singleShot(100, this, SLOT(restoreOpenSessions()));
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupActions()
{
    foreach(RemoteViewFactory *factory, m_remoteViewFactories) {
        QAction *connectionAction = actionCollection()->addAction("new_" + factory->scheme() + "_connection");
        connectionAction->setProperty("schemeString", factory->scheme());
        connectionAction->setProperty("toolTipString", factory->connectToolTipText());
        connectionAction->setText(factory->connectActionText());
        connectionAction->setIcon(KIcon("network-connect"));
        connect(connectionAction, SIGNAL(triggered()), SLOT(newConnectionToolTip()));
    }

    QAction *zeroconfAction = actionCollection()->addAction("zeroconf_page");
    zeroconfAction->setText(i18n("Browse Remote Desktop Services on Local Network..."));
    zeroconfAction->setIcon(KIcon("network-connect"));
    connect(zeroconfAction, SIGNAL(triggered()), SLOT(createZeroconfPage()));
#ifndef BUILD_ZEROCONF
    zeroconfAction->deleteLater();
#endif

    QAction *screenshotAction = actionCollection()->addAction("take_screenshot");
    screenshotAction->setText(i18n("Copy Screenshot to Clipboard"));
    screenshotAction->setIcon(KIcon("ksnapshot"));
    connect(screenshotAction, SIGNAL(triggered()), SLOT(takeScreenshot()));

    QAction *fullscreenAction = actionCollection()->addAction("switch_fullscreen");
    fullscreenAction->setText(i18n("Switch to Fullscreen Mode"));
    fullscreenAction->setIcon(KIcon("view-fullscreen"));
    connect(fullscreenAction, SIGNAL(triggered()), SLOT(switchFullscreen()));

    QAction *viewOnlyAction = actionCollection()->addAction("view_only");
    viewOnlyAction->setCheckable(true);
    viewOnlyAction->setText(i18n("View Only"));
    viewOnlyAction->setIcon(KIcon("document-preview"));
    connect(viewOnlyAction, SIGNAL(triggered(bool)), SLOT(viewOnly(bool)));

    KAction *disconnectAction = actionCollection()->addAction("disconnect");
    disconnectAction->setText(i18n("Disconnect"));
    disconnectAction->setIcon(KIcon("system-log-out"));
    disconnectAction->setShortcut(QKeySequence::Close);
    connect(disconnectAction, SIGNAL(triggered()), SLOT(disconnectHost()));

    QAction *showLocalCursorAction = actionCollection()->addAction("show_local_cursor");
    showLocalCursorAction->setCheckable(true);
    showLocalCursorAction->setIcon(KIcon("input-mouse"));
    showLocalCursorAction->setText(i18n("Show Local Cursor"));
    connect(showLocalCursorAction, SIGNAL(triggered(bool)), SLOT(showLocalCursor(bool)));

    QAction *grabAllKeysAction = actionCollection()->addAction("grab_all_keys");
    grabAllKeysAction->setCheckable(true);
    grabAllKeysAction->setIcon(KIcon("configure-shortcuts"));
    grabAllKeysAction->setText(i18n("Grab all possible keys"));
    connect(grabAllKeysAction, SIGNAL(triggered(bool)), SLOT(grabAllKeys(bool)));

    QAction *scaleAction = actionCollection()->addAction("scale");
    scaleAction->setCheckable(true);
    scaleAction->setIcon(KIcon("zoom-fit-best"));
    scaleAction->setText(i18n("Scale remote screen to fit window size"));
    connect(scaleAction, SIGNAL(triggered(bool)), SLOT(scale(bool)));

    KStandardAction::quit(this, SLOT(quit()), actionCollection());
    KStandardAction::preferences(this, SLOT(preferences()), actionCollection());
    KStandardAction::configureToolbars(this, SLOT(configureToolbars()), actionCollection());
    KStandardAction::keyBindings(this, SLOT(configureKeys()), actionCollection());
    QAction *configNotifyAction = KStandardAction::configureNotifications(this, SLOT(configureNotifications()), actionCollection());
    configNotifyAction->setVisible(false);
    m_menubarAction = KStandardAction::showMenubar(this, SLOT(showMenubar()), actionCollection());
    m_menubarAction->setChecked(!menuBar()->isHidden());

    const QString initialProtocol(!m_remoteViewFactories.isEmpty() ? (*m_remoteViewFactories.begin())->scheme() : QString());
    m_addressNavigator = new KUrlNavigator(0, KUrl(initialProtocol + "://"), this);

    QStringList schemes;
    foreach(RemoteViewFactory *factory, m_remoteViewFactories) {
        schemes << factory->scheme();
    }
    m_addressNavigator->setCustomProtocols(schemes);

    m_addressNavigator->setUrlEditable(Settings::normalUrlInputLine());
    connect(m_addressNavigator, SIGNAL(returnPressed()), SLOT(newConnection()));

    QLabel *addressLabel = new QLabel(i18n("Remote desktop:"), this);

    QWidget *addressWidget = new QWidget(this);
    QHBoxLayout *addressLayout = new QHBoxLayout(addressWidget);
    addressLayout->setMargin(0);
    addressLayout->addWidget(addressLabel);
    addressLayout->addWidget(m_addressNavigator, 1);

    KAction *addressLineAction = new KAction(i18nc("Title for remote address input action", "Address"), this);
    actionCollection()->addAction("address_line", addressLineAction);
    addressLineAction->setDefaultWidget(addressWidget);

    QAction *gotoAction = actionCollection()->addAction("goto_address");
    gotoAction->setText(i18n("Goto Address"));
    gotoAction->setIcon(KIcon("go-jump-locationbar"));
    connect(gotoAction, SIGNAL(triggered()), SLOT(newConnection()));

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

    m_remoteViewList.append(view);

//     view->resize(0, 0);
 
    int numNonRemoteView = 0;
    if (m_showStartPage)
        numNonRemoteView++;
    if (m_zeroconfPage)
        numNonRemoteView++;
    
    if (m_remoteViewList.size() == 1) {
        kDebug(5010) << "First connection, restoring window size)";
        restoreWindowSize(view->hostPreferences()->configGroup());
    }

    QScrollArea *scrollArea = createScrollArea(m_tabWidget, view);

    const int newIndex = m_tabWidget->addTab(scrollArea, KIcon("krdc"), url.prettyUrl(KUrl::RemoveTrailingSlash));
    m_tabWidget->setCurrentIndex(newIndex);
    tabChanged(newIndex); // force to update m_currentRemoteView (tabChanged is not emitted when start page has been disabled)

    view->start();
}

void MainWindow::openFromDockWidget(const QModelIndex &index)
{
    const QString data = index.data(Qt::UserRole).toString();
    if (!data.isEmpty()) {
        const KUrl url(data);
        // first check if url has already been opened; in case show the tab
        for (int i = 0; i < m_remoteViewList.count(); ++i) {
            if (m_remoteViewList.at(i)->url() == url) {
                int numNonRemoteView = 0;
                if (m_showStartPage)
                    numNonRemoteView++;
                if (m_zeroconfPage)
                    numNonRemoteView++;
                m_tabWidget->setCurrentIndex(i + numNonRemoteView);
                return;
            }
        }
        newConnection(url);
    }
}

void MainWindow::resizeTabWidget(int w, int h)
{
    kDebug(5010) << "tabwidget resize: w: " << w << ", h: " << h;
    
    RemoteView* view = m_currentRemoteView >= 0 ? m_remoteViewList.at(m_currentRemoteView) : NULL;
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

    RemoteView* view = m_remoteViewList.at(m_currentRemoteView);

    if (m_fullscreenWindow) {
        // Leaving fullscreen mode
        view->enableScaling(view->hostPreferences()->windowedScale());
        show();
        restoreGeometry(m_mainWindowGeometry);

        m_fullscreenWindow->setWindowState(0);
        m_fullscreenWindow->hide();

        QScrollArea *scrollArea = createScrollArea(m_tabWidget, view);

        const int currentTab = m_tabWidget->currentIndex();
        m_tabWidget->insertTab(currentTab, scrollArea, m_tabWidget->tabIcon(currentTab), m_tabWidget->tabText(currentTab));
        m_tabWidget->removeTab(m_tabWidget->currentIndex());
        m_tabWidget->setCurrentIndex(currentTab);

        resizeTabWidget(view->sizeHint().width(), view->sizeHint().height());

        if (m_toolBar) {
            m_toolBar->hideAndDestroy();
            m_toolBar->deleteLater();
            m_toolBar = 0;
        }

        actionCollection()->action("switch_fullscreen")->setIcon(KIcon("view-fullscreen"));
        actionCollection()->action("switch_fullscreen")->setText(i18n("Switch to Fullscreen Mode"));

        m_fullscreenWindow->deleteLater();

        m_fullscreenWindow = 0;
    } else {
        // Entering fullscreen mode
        m_fullscreenWindow = new QWidget(this, Qt::Window);
        m_fullscreenWindow->setWindowTitle(i18nc("window title when in fullscreen mode (for example displayed in tasklist)",
                                                 "KDE Remote Desktop Client (Fullscreen)"));

        QScrollArea *scrollArea = createScrollArea(m_fullscreenWindow, view);
        scrollArea->setFrameShape(QFrame::NoFrame);

        QVBoxLayout *fullscreenLayout = new QVBoxLayout(m_fullscreenWindow);
        fullscreenLayout->setMargin(0);
        fullscreenLayout->addWidget(scrollArea);

        MinimizePixel *minimizePixel = new MinimizePixel(m_fullscreenWindow);
        minimizePixel->winId(); // force it to be a native widget (prevents problem with QX11EmbedContainer)
        connect(minimizePixel, SIGNAL(rightClicked()), m_fullscreenWindow, SLOT(showMinimized()));

        view->enableScaling(view->hostPreferences()->fullscreenScale());
                
        m_fullscreenWindow->show();

        KToggleFullScreenAction::setFullScreen(m_fullscreenWindow, true);

        m_mainWindowGeometry = saveGeometry();
        hide();

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
    scrollArea->setWidget(remoteView);

    return scrollArea;
}

void MainWindow::disconnectHost()
{
    kDebug(5010);

    if (m_fullscreenWindow) { // first close fullscreen view
        switchFullscreen();
    }

    QWidget *tmp = m_tabWidget->currentWidget();
    m_remoteViewList.removeAt(m_currentRemoteView);
    m_tabWidget->removeTab(m_tabWidget->currentIndex());
    tmp->deleteLater();
    
    saveHostPrefs();
}

void MainWindow::closeTab(QWidget *widget)
{
    const int index = m_tabWidget->indexOf(widget);

    kDebug(5010) << index;

    if (m_showStartPage && index == 0) {
        KMessageBox::information(this, i18n("You can enable the start page in the settings again."));

        Settings::setShowStartPage(false);
        m_tabWidget->removeTab(0);
        m_showStartPage = false;
        return;
    }

#ifdef BUILD_ZEROCONF
    if (widget == m_zeroconfPage) {
        closeZeroconfPage();
        return;
    }
#endif

    int numNonRemoteView = 0;
    if (m_showStartPage)
        numNonRemoteView++;
    if (m_zeroconfPage)
        numNonRemoteView++;

    if (index - numNonRemoteView >= 0)
        m_remoteViewList.removeAt(index - numNonRemoteView);

    m_tabWidget->removeTab(index);

    widget->deleteLater();
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

    const QString url = view->url().url();
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
    view->hostPreferences()->setViewOnly(showLocalCursor);
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

        m_toolBar = new FloatingToolBar(m_fullscreenWindow, m_fullscreenWindow);
        m_toolBar->winId(); // force it to be a native widget (prevents problem with QX11EmbedContainer)
        m_toolBar->setSide(FloatingToolBar::Top);

        QLabel *hostLabel = new QLabel(m_remoteViewList.at(m_currentRemoteView)->url().prettyUrl(KUrl::RemoveTrailingSlash), m_toolBar);
        hostLabel->setMargin(4);
        QFont font(hostLabel->font());
        font.setBold(true);
        hostLabel->setFont(font);
        m_toolBar->addWidget(hostLabel);

#if 0 //TODO: implement functionality
        KComboBox *sessionComboBox = new KComboBox(m_toolBar);
        sessionComboBox->setEditable(false);
        sessionComboBox->addItem(i18n("Switch to..."));
        for (int i = 0; i < m_remoteViewList.count(); i++) {
            sessionComboBox->addItem(m_remoteViewList.at(i)->url().prettyUrl(KUrl::RemoveTrailingSlash));
        }
        sessionComboBox->setVisible(m_remoteViewList.count() > 1); // just show it if there are sessions to switch
        m_toolBar->addWidget(sessionComboBox);
#endif

        m_toolBar->addAction(actionCollection()->action("switch_fullscreen"));

        QAction *minimizeAction = new QAction(m_toolBar);
        minimizeAction->setIcon(KIcon("window-suppressed"));
        minimizeAction->setText(i18n("Minimize Fullscreen Window"));
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

    bool enabled;

    if ((m_showStartPage && (m_tabWidget->currentIndex() == 0)) || 
#ifdef BUILD_ZEROCONF
         (m_zeroconfPage && (m_tabWidget->currentIndex() == m_tabWidget->indexOf(m_zeroconfPage))) ||
#endif
         (!m_showStartPage && (m_tabWidget->currentIndex() < 0)))
        enabled = false;
    else
        enabled = true;

    RemoteView* view = m_currentRemoteView >= 0 ? m_remoteViewList.at(m_currentRemoteView) : NULL;
    
    actionCollection()->action("switch_fullscreen")->setEnabled(enabled);
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
    m_addressNavigator->setUrlEditable(Settings::normalUrlInputLine());

    m_tabWidget->setTabBarHidden(!Settings::showTabBar());
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
        m_systemTrayIcon->setVisible(true);
    } else if (m_systemTrayIcon) {
        delete m_systemTrayIcon;
        m_systemTrayIcon = 0;
    }

    if (Settings::showStartPage() && !m_showStartPage)
        createStartPage();
    else if (!Settings::showStartPage() && m_showStartPage) {
        m_tabWidget->removeTab(0);
        m_showStartPage = false;
    }
    updateActionStatus();

    // update the scroll areas background color
    int numNonRemoteView = 0;
    if (m_showStartPage)
        numNonRemoteView++;
    if (m_zeroconfPage)
        numNonRemoteView++;
    for (int i = numNonRemoteView; i < m_tabWidget->count(); ++i) {
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
    // Save default window size for currently active view
    RemoteView* view = m_currentRemoteView >= 0 ? m_remoteViewList.at(m_currentRemoteView) : NULL;
    if (view)
        saveWindowSize(view->hostPreferences()->configGroup());
    
    Settings::self()->config()->sync();
}

void MainWindow::tabChanged(int index)
{
    kDebug(5010) << index;
    
    int numNonRemoteView = 0;
    if (m_showStartPage)
        numNonRemoteView++;
    if (m_zeroconfPage)
        numNonRemoteView++;

    m_currentRemoteView = index - numNonRemoteView;

    const QString tabTitle = m_tabWidget->tabText(index).remove('&');
    setCaption(tabTitle == i18n("Start Page") ? QString() : tabTitle);

    updateActionStatus();
}

void MainWindow::createStartPage()
{
    m_showStartPage = true;

    QWidget *startWidget = new QWidget(this);
    startWidget->setStyleSheet("QWidget { background-color: palette(base) }");

    QVBoxLayout *startLayout = new QVBoxLayout(startWidget);

    QLabel *headerLabel = new QLabel(this);
    headerLabel->setText(i18n("<h1>KDE Remote Desktop Client</h1><br /><br />What would you like to do?<br />"));

    QLabel *headerIconLabel = new QLabel(this);
    headerIconLabel->setPixmap(KIcon("krdc").pixmap(128));

    QHBoxLayout *headerLayout = new QHBoxLayout;
    headerLayout->setMargin(20);
    headerLayout->addWidget(headerLabel, 1, Qt::AlignTop);
    headerLayout->addWidget(headerIconLabel);
    startLayout->addLayout(headerLayout);

    foreach(RemoteViewFactory *factory, m_remoteViewFactories) {
        KPushButton *connectButton = new KPushButton(this);
        connectButton->setProperty("schemeString", factory->scheme());
        connectButton->setProperty("toolTipString", factory->connectToolTipText());
        connectButton->setStyleSheet("KPushButton { padding: 12px; margin: 10px; }");
        connectButton->setIcon(KIcon(actionCollection()->action("new_" + factory->scheme() + "_connection")->icon()));
        connectButton->setText(factory->connectButtonText());
        connect(connectButton, SIGNAL(clicked()), SLOT(newConnectionToolTip()));
        startLayout->addWidget(connectButton);
    }

    KPushButton *zeroconfButton = new KPushButton(this);
    zeroconfButton->setStyleSheet("KPushButton { padding: 12px; margin: 10px; }");
    zeroconfButton->setIcon(KIcon(actionCollection()->action("zeroconf_page")->icon()));
    zeroconfButton->setText(i18n("Browse Remote Desktop Services on Local Network"));
    connect(zeroconfButton, SIGNAL(clicked()), SLOT(createZeroconfPage()));
#ifndef BUILD_ZEROCONF
    zeroconfButton->setVisible(false);
#endif

    startLayout->addWidget(zeroconfButton);
    startLayout->addStretch();

    m_tabWidget->insertTab(0, startWidget, KIcon("krdc"), i18n("Start Page"));
}

void MainWindow::newConnectionToolTip()
{
    QObject *senderObject = qobject_cast<QObject*>(sender());
    const QString toolTip(senderObject->property("toolTipString").toString());
    const QString scheme(senderObject->property("schemeString").toString());

    m_addressNavigator->setUrl(KUrl(scheme + "://"));
    m_addressNavigator->setFocus();

    QToolTip::showText(m_addressNavigator->pos() + pos() + QPoint(m_addressNavigator->width(),
                                                                  m_addressNavigator->height() / 2),
                       toolTip, this);
}

void MainWindow::createZeroconfPage()
{
#ifdef BUILD_ZEROCONF
    if (m_zeroconfPage)
        return;

    m_zeroconfPage = new ZeroconfPage(this);
    connect(m_zeroconfPage, SIGNAL(newConnection(const KUrl, bool)), this, SLOT(newConnection(const KUrl, bool)));
    connect(m_zeroconfPage, SIGNAL(closeZeroconfPage()), this, SLOT(closeZeroconfPage()));
    const int zeroconfTabIndex = m_tabWidget->insertTab(m_showStartPage ? 1 : 0, m_zeroconfPage, KIcon("krdc"), i18n("Browse Local Network"));
    m_tabWidget->setCurrentIndex(zeroconfTabIndex);
#endif
}

void MainWindow::closeZeroconfPage()
{
#ifdef BUILD_ZEROCONF
    const int index = m_tabWidget->indexOf(m_zeroconfPage);
    m_tabWidget->removeTab(index);
    m_zeroconfPage->deleteLater();
    m_zeroconfPage = 0;
#endif
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
}

#include "mainwindow.moc"
