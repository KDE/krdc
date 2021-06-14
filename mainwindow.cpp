/*
    SPDX-FileCopyrightText: 2007-2013 Urs Wolfer <uwolfer@kde.org>
    SPDX-FileCopyrightText: 2009-2010 Tony Murray <murraytony@gmail.com>
    SPDX-FileCopyrightText: 2021 Rafał Lalik <rafallalik@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mainwindow.h"
#include "krdc_debug.h"
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
#include "factorwidget.h"

#include <KActionCollection>
#include <KActionMenu>
#include <KComboBox>
#include <KLineEdit>
#include <KLocalizedString>
#include <KMessageBox>
#include <KNotifyConfigWidget>
#include <KPluginMetaData>
#include <KToggleAction>
#include <KToggleFullScreenAction>
#include <KToolBar>

#include <QClipboard>
#include <QDockWidget>
#include <QFontMetrics>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <QStatusBar>
#include <QScrollBar>
#include <QTabWidget>
#include <QTableView>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
        : KXmlGuiWindow(parent),
        m_fullscreenWindow(nullptr),
        m_protocolInput(nullptr),
        m_addressInput(nullptr),
        m_toolBar(nullptr),
        m_currentRemoteView(-1),
        m_systemTrayIcon(nullptr),
        m_dockWidgetTableView(nullptr),
        m_newConnectionTableView(nullptr),
        m_newConnectionWidget(nullptr)
{
    loadAllPlugins();

    setupActions();

    setStandardToolBarMenuEnabled(true);

    m_tabWidget = new TabbedViewWidget(this);
    m_tabWidget->setAutoFillBackground(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setTabPosition((QTabWidget::TabPosition) Settings::tabPosition());

    m_tabWidget->setTabsClosable(Settings::tabCloseButton());

    connect(m_tabWidget, SIGNAL(tabCloseRequested(int)), SLOT(closeTab(int)));

    if (Settings::tabMiddleClick())
        connect(m_tabWidget, SIGNAL(mouseMiddleClick(int)), SLOT(closeTab(int)));

    connect(m_tabWidget, SIGNAL(tabBarDoubleClicked(int)), SLOT(openTabSettings(int)));

    m_tabWidget->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tabWidget->tabBar(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(tabContextMenu(QPoint)));

    m_tabWidget->setMinimumSize(600, 400);
    setCentralWidget(m_tabWidget);

    createDockWidget();

    setupGUI(ToolBar | Keys | Save | Create);

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

    if (Settings::rememberSessions()) // give some time to create and show the window first
        QTimer::singleShot(100, this, SLOT(restoreOpenSessions()));
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupActions()
{
    QAction *connectionAction = actionCollection()->addAction(QStringLiteral("new_connection"));
    connectionAction->setText(i18n("New Connection"));
    connectionAction->setIcon(QIcon::fromTheme(QStringLiteral("network-connect")));
    actionCollection()->setDefaultShortcuts(connectionAction, KStandardShortcut::openNew());
    connect(connectionAction, SIGNAL(triggered()), SLOT(newConnectionPage()));

    QAction *screenshotAction = actionCollection()->addAction(QStringLiteral("take_screenshot"));
    screenshotAction->setText(i18n("Copy Screenshot to Clipboard"));
    screenshotAction->setIconText(i18n("Screenshot"));
    screenshotAction->setIcon(QIcon::fromTheme(QStringLiteral("ksnapshot")));
    connect(screenshotAction, SIGNAL(triggered()), SLOT(takeScreenshot()));

    QAction *fullscreenAction = actionCollection()->addAction(QStringLiteral("switch_fullscreen")); // note: please do not switch to KStandardShortcut unless you know what you are doing (see history of this file)
    fullscreenAction->setText(i18n("Switch to Full Screen Mode"));
    fullscreenAction->setIconText(i18n("Full Screen"));
    fullscreenAction->setIcon(QIcon::fromTheme(QStringLiteral("view-fullscreen")));
    actionCollection()->setDefaultShortcuts(fullscreenAction, KStandardShortcut::fullScreen());
    connect(fullscreenAction, SIGNAL(triggered()), SLOT(switchFullscreen()));

    QAction *viewOnlyAction = actionCollection()->addAction(QStringLiteral("view_only"));
    viewOnlyAction->setCheckable(true);
    viewOnlyAction->setText(i18n("View Only"));
    viewOnlyAction->setIcon(QIcon::fromTheme(QStringLiteral("document-preview")));
    connect(viewOnlyAction, SIGNAL(triggered(bool)), SLOT(viewOnly(bool)));

    QAction *disconnectAction = actionCollection()->addAction(QStringLiteral("disconnect"));
    disconnectAction->setText(i18n("Disconnect"));
    disconnectAction->setIcon(QIcon::fromTheme(QStringLiteral("network-disconnect")));
    actionCollection()->setDefaultShortcuts(disconnectAction, KStandardShortcut::close());
    connect(disconnectAction, SIGNAL(triggered()), SLOT(disconnectHost()));

    QAction *showLocalCursorAction = actionCollection()->addAction(QStringLiteral("show_local_cursor"));
    showLocalCursorAction->setCheckable(true);
    showLocalCursorAction->setIcon(QIcon::fromTheme(QStringLiteral("input-mouse")));
    showLocalCursorAction->setText(i18n("Show Local Cursor"));
    showLocalCursorAction->setIconText(i18n("Local Cursor"));
    connect(showLocalCursorAction, SIGNAL(triggered(bool)), SLOT(showLocalCursor(bool)));

    QAction *grabAllKeysAction = actionCollection()->addAction(QStringLiteral("grab_all_keys"));
    grabAllKeysAction->setCheckable(true);
    grabAllKeysAction->setIcon(QIcon::fromTheme(QStringLiteral("configure-shortcuts")));
    grabAllKeysAction->setText(i18n("Grab All Possible Keys"));
    grabAllKeysAction->setIconText(i18n("Grab Keys"));
    connect(grabAllKeysAction, SIGNAL(triggered(bool)), SLOT(grabAllKeys(bool)));

    QAction *scaleAction = actionCollection()->addAction(QStringLiteral("scale"));
    scaleAction->setCheckable(true);
    scaleAction->setIcon(QIcon::fromTheme(QStringLiteral("zoom-fit-best")));
    scaleAction->setText(i18n("Scale Remote Screen to Fit Window Size"));
    scaleAction->setIconText(i18n("Scale"));
    connect(scaleAction, SIGNAL(triggered(bool)), SLOT(scale(bool)));

    FactorWidget * m_scaleSlider = new FactorWidget(i18n("Scaling Factor"), this, actionCollection());
    QAction * scaleFactorAction = actionCollection()->addAction(QStringLiteral("scale_factor"), m_scaleSlider);
    scaleFactorAction->setIcon(QIcon::fromTheme(QStringLiteral("configure")));

    KStandardAction::quit(this, SLOT(quit()), actionCollection());
    KStandardAction::preferences(this, SLOT(preferences()), actionCollection());
    QAction *configNotifyAction = KStandardAction::configureNotifications(this, SLOT(configureNotifications()), actionCollection());
    configNotifyAction->setVisible(false);
    m_menubarAction = KStandardAction::showMenubar(this, SLOT(showMenubar()), actionCollection());
    m_menubarAction->setChecked(!menuBar()->isHidden());

    KActionMenu *bookmarkMenu = new KActionMenu(i18n("Bookmarks"), actionCollection());
    m_bookmarkManager = new BookmarkManager(actionCollection(), bookmarkMenu->menu(), this);
    actionCollection()->addAction(QStringLiteral("bookmark") , bookmarkMenu);
    connect(m_bookmarkManager, SIGNAL(openUrl(QUrl)), SLOT(newConnection(QUrl)));
}

void MainWindow::loadAllPlugins()
{
    const QVector<KPluginMetaData> offers = KPluginLoader::findPlugins(QStringLiteral("krdc"));
    const KConfigGroup conf = KSharedConfig::openConfig()->group(QStringLiteral("Plugins"));

    for (const KPluginMetaData &plugin : offers) {

        KPluginInfo pluginInfo = KPluginInfo::fromMetaData(plugin);
        pluginInfo.load(conf);
        const bool enabled = pluginInfo.isPluginEnabled();

        if (enabled) {
            RemoteViewFactory *component = nullptr;

            KPluginLoader loader(plugin.fileName());
            KPluginFactory *factory = loader.factory();

            if (factory) {
                component = factory->create<RemoteViewFactory>();
            }

            if (component) {
                const int sorting = plugin.value(QStringLiteral("X-KDE-KRDC-Sorting")).toInt();
                m_remoteViewFactories.insert(sorting, component);
            }
        }
    }
}

void MainWindow::restoreOpenSessions()
{
    const QStringList list = Settings::openSessions();
    QListIterator<QString> it(list);
    while (it.hasNext()) {
        newConnection(QUrl(it.next()));
    }
}

QUrl MainWindow::getInputUrl()
{
    QString userInput = m_addressInput->text();
    qCDebug(KRDC) << "input url " << userInput;
    // percent encode usernames so QUrl can parse it
    int lastAtIndex = userInput.indexOf(QRegExp(QStringLiteral("@[^@]+$")));
    if (lastAtIndex >0) {
        userInput = QString::fromLatin1(QUrl::toPercentEncoding(userInput.left(lastAtIndex))) + userInput.mid(lastAtIndex);
        qCDebug(KRDC) << "input url " << userInput;
    }

    return QUrl(m_protocolInput->currentText() + QStringLiteral("://") + userInput);
}

void MainWindow::newConnection(const QUrl &newUrl, bool switchFullscreenWhenConnected, const QString &tabName)
{
    m_switchFullscreenWhenConnected = switchFullscreenWhenConnected;

    const QUrl url = newUrl.isEmpty() ? getInputUrl() : newUrl;

    if (!url.isValid() || (url.host().isEmpty() && url.port() < 0)
        || (!url.path().isEmpty() && url.path() != QStringLiteral("/"))) {
        KMessageBox::error(this,
                           i18n("The entered address does not have the required form.\n Syntax: [username@]host[:port]"),
                           i18n("Malformed URL"));
        return;
    }

    if (m_protocolInput && m_addressInput) {
        m_protocolInput->setCurrentText(url.scheme());
        m_addressInput->setText(url.authority());
    }

    RemoteView *view = nullptr;
    KConfigGroup configGroup = Settings::self()->config()->group(QStringLiteral("hostpreferences")).group(url.toDisplayString(QUrl::StripTrailingSlash));

    for (RemoteViewFactory *factory : qAsConst(m_remoteViewFactories)) {
        if (factory->supportsUrl(url)) {
            view = factory->createView(this, url, configGroup);
            qCDebug(KRDC) << "Found plugin to handle url (" << url.url() << "): " << view->metaObject()->className();
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
    // if the user press cancel
    if (! prefs->showDialogIfNeeded(this)) return;

    view->showLocalCursor(prefs->showLocalCursor() ? RemoteView::CursorOn : RemoteView::CursorOff);
    view->setViewOnly(prefs->viewOnly());
    bool scale_state = false;
    if (switchFullscreenWhenConnected) scale_state = prefs->fullscreenScale();
    else scale_state = prefs->windowedScale();

    view->enableScaling(scale_state);

    connect(view, SIGNAL(framebufferSizeChanged(int,int)), this, SLOT(resizeTabWidget(int,int)));
    connect(view, SIGNAL(statusChanged(RemoteView::RemoteStatus)), this, SLOT(statusChanged(RemoteView::RemoteStatus)));
    connect(view, SIGNAL(disconnected()), this, SLOT(disconnectHost()));

    view->winId();  // native widget workaround for bug 253365
    QScrollArea *scrollArea = createScrollArea(m_tabWidget, view);

    const int indexOfNewConnectionWidget = m_tabWidget->indexOf(m_newConnectionWidget);
    if (indexOfNewConnectionWidget >= 0)
        m_tabWidget->removeTab(indexOfNewConnectionWidget);

    const int newIndex = m_tabWidget->addTab(scrollArea, QIcon::fromTheme(QStringLiteral("krdc")), tabName.isEmpty() ? url.toDisplayString(QUrl::StripTrailingSlash) : tabName);
    m_tabWidget->setCurrentIndex(newIndex);
    m_remoteViewMap.insert(m_tabWidget->widget(newIndex), view);
    tabChanged(newIndex); // force to update m_currentRemoteView (tabChanged is not emitted when start page has been disabled)

    view->start();
    setFactor(view->hostPreferences()->scaleFactor());

    Q_EMIT factorUpdated(view->hostPreferences()->scaleFactor());
    Q_EMIT scaleUpdated(scale_state);
}

void MainWindow::openFromRemoteDesktopsModel(const QModelIndex &index)
{
    const QString urlString = index.data(10001).toString();
    const QString nameString = index.data(10003).toString();
    if (!urlString.isEmpty()) {
        const QUrl url(urlString);
        // first check if url has already been opened; in case show the tab
        for (auto it = m_remoteViewMap.constBegin(), end = m_remoteViewMap.constEnd(); it != end; ++it) {
            RemoteView *view = it.value();
            if (view->url() == url) {
                QWidget *widget = it.key();
                m_tabWidget->setCurrentWidget(widget);
                return;
            }
        }

        newConnection(url, false, nameString);
    }
}

void MainWindow::selectFromRemoteDesktopsModel(const QModelIndex &index)
{
    const QString urlString = index.data(10001).toString();

    if (!urlString.isEmpty() && m_protocolInput && m_addressInput) {
        const QUrl url(urlString);
        m_addressInput->blockSignals(true); // block signals so we don't filter the address list on click
        m_addressInput->setText(url.authority());
        m_addressInput->blockSignals(false);
        m_protocolInput->setCurrentText(url.scheme());
    }
}

void MainWindow::resizeTabWidget(int w, int h)
{
    qCDebug(KRDC) << "tabwidget resize, view size: w: " << w << ", h: " << h;
    if (m_fullscreenWindow) {
        qCDebug(KRDC) << "in fullscreen mode, refusing to resize";
        return;
    }

    const QSize viewSize = QSize(w,h);
    QDesktopWidget *desktop = QApplication::desktop();

    if (Settings::fullscreenOnConnect()) {
        int currentScreen = desktop->screenNumber(this);
        const QSize screenSize = desktop->screenGeometry(currentScreen).size();

        if (screenSize == viewSize) {
            qCDebug(KRDC) << "screen size equal to target view size -> switch to fullscreen mode";
            switchFullscreen();
            return;
        }
    }

    if (Settings::resizeOnConnect()) {
        QWidget* currentWidget = m_tabWidget->currentWidget();
        const QSize newWindowSize = size() - currentWidget->frameSize() + viewSize;

        const QSize desktopSize = desktop->availableGeometry().size();
        qCDebug(KRDC) << "new window size: " << newWindowSize << " available space:" << desktopSize;

        if ((newWindowSize.width() >= desktopSize.width()) || (newWindowSize.height() >= desktopSize.height())) {
            qCDebug(KRDC) << "remote desktop needs more space than available -> show window maximized";
            setWindowState(windowState() | Qt::WindowMaximized);
            return;
        }
        setWindowState(windowState() & ~ Qt::WindowMaximized);
        resize(newWindowSize);
    }
}

void MainWindow::statusChanged(RemoteView::RemoteStatus status)
{
    qCDebug(KRDC) << status;

    // the remoteview is already deleted, so don't show it; otherwise it would crash
    if (status == RemoteView::Disconnecting || status == RemoteView::Disconnected)
        return;

    RemoteView *view = qobject_cast<RemoteView*>(QObject::sender());
    const QString host = view->host();

    QString iconName = QStringLiteral("krdc");
    QString message;

    switch (status) {
    case RemoteView::Connecting:
        iconName = QStringLiteral("network-connect");
        message = i18n("Connecting to %1", host);
        break;
    case RemoteView::Authenticating:
        iconName = QStringLiteral("dialog-password");
        message = i18n("Authenticating at %1", host);
        break;
    case RemoteView::Preparing:
        iconName = QStringLiteral("view-history");
        message = i18n("Preparing connection to %1", host);
        break;
    case RemoteView::Connected:
        iconName = QStringLiteral("krdc");
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

    m_tabWidget->setTabIcon(m_tabWidget->indexOf(view), QIcon::fromTheme(iconName));
    if (Settings::showStatusBar())
        statusBar()->showMessage(message);
}

void MainWindow::takeScreenshot()
{
    const QPixmap snapshot = currentRemoteView()->takeScreenshot();

    QApplication::clipboard()->setPixmap(snapshot);
}

void MainWindow::switchFullscreen()
{
    qCDebug(KRDC);

    RemoteView* view = currentRemoteView();
    bool scale_state = false;

    if (m_fullscreenWindow) {
        // Leaving full screen mode
        m_fullscreenWindow->setWindowState(Qt::WindowNoState);
        m_fullscreenWindow->hide();

        m_tabWidget->tabBar()->setHidden(m_tabWidget->count() <= 1 && !Settings::showTabBar());
        m_tabWidget->setDocumentMode(false);
        setCentralWidget(m_tabWidget);

        show();
        restoreGeometry(m_mainWindowGeometry);
        if (m_systemTrayIcon) m_systemTrayIcon->setAssociatedWidget(this);

        for (RemoteView * view : qAsConst(m_remoteViewMap)) {
            view->enableScaling(view->hostPreferences()->windowedScale());
        }

        if (m_toolBar) {
            m_toolBar->hideAndDestroy();
            m_toolBar->deleteLater();
            m_toolBar = nullptr;
        }

        actionCollection()->action(QStringLiteral("switch_fullscreen"))->setIcon(QIcon::fromTheme(QStringLiteral("view-fullscreen")));
        actionCollection()->action(QStringLiteral("switch_fullscreen"))->setText(i18n("Switch to Full Screen Mode"));
        actionCollection()->action(QStringLiteral("switch_fullscreen"))->setIconText(i18n("Full Screen"));
        if (view)
            scale_state = view->hostPreferences()->windowedScale();

        m_fullscreenWindow->deleteLater();
        m_fullscreenWindow = nullptr;
    } else {
        // Entering full screen mode
        m_fullscreenWindow = new QWidget(this, Qt::Window);
        m_fullscreenWindow->setWindowTitle(i18nc("window title when in full screen mode (for example displayed in tasklist)",
                                           "KDE Remote Desktop Client (Full Screen)"));

        m_mainWindowGeometry = saveGeometry();

        m_tabWidget->tabBar()->hide();
        m_tabWidget->setDocumentMode(true);

        for (RemoteView *currentView : qAsConst(m_remoteViewMap)) {
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

        actionCollection()->action(QStringLiteral("switch_fullscreen"))->setIcon(QIcon::fromTheme(QStringLiteral("view-restore")));
        actionCollection()->action(QStringLiteral("switch_fullscreen"))->setText(i18n("Switch to Window Mode"));
        actionCollection()->action(QStringLiteral("switch_fullscreen"))->setIconText(i18n("Window Mode"));
        showRemoteViewToolbar();
        if (view)
            scale_state = view->hostPreferences()->fullscreenScale();
    }
    if (m_tabWidget->currentWidget() == m_newConnectionWidget && m_addressInput) {
        m_addressInput->setFocus();
    }

    if (view) {
        Q_EMIT factorUpdated(view->hostPreferences()->scaleFactor());
        Q_EMIT scaleUpdated(scale_state);
    }
    actionCollection()->action(QStringLiteral("scale"))->setChecked(scale_state);
}

QScrollArea *MainWindow::createScrollArea(QWidget *parent, RemoteView *remoteView)
{
    RemoteViewScrollArea *scrollArea = new RemoteViewScrollArea(parent);
    scrollArea->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    connect(scrollArea, SIGNAL(resized(int,int)), remoteView, SLOT(scaleResize(int,int)));

    QPalette palette = scrollArea->palette();
    palette.setColor(QPalette::Window, Settings::backgroundColor());
    scrollArea->setPalette(palette);

    scrollArea->setFrameStyle(QFrame::NoFrame);
    scrollArea->setAutoFillBackground(true);
    scrollArea->setWidget(remoteView);

    return scrollArea;
}

void MainWindow::disconnectHost()
{
    qCDebug(KRDC);

    RemoteView *view = qobject_cast<RemoteView*>(QObject::sender());

    QWidget *widgetToDelete;
    if (view) {
        widgetToDelete = (QWidget*) view->parent()->parent();
        m_remoteViewMap.remove(m_remoteViewMap.key(view));
    } else {
        widgetToDelete = m_tabWidget->currentWidget();
        view = currentRemoteView();
        m_remoteViewMap.remove(m_remoteViewMap.key(view));
    }

    saveHostPrefs(view);
    view->startQuitting();  // some deconstructors can't properly quit, so quit early
    m_tabWidget->removePage(widgetToDelete);
    widgetToDelete->deleteLater();

    // if closing the last connection, create new connection tab
    if (m_tabWidget->count() == 0) {
        newConnectionPage(false);
    }

    // if the newConnectionWidget is the only tab and we are fullscreen, switch to window mode
    if (m_fullscreenWindow && m_tabWidget->count() == 1  && m_tabWidget->currentWidget() == m_newConnectionWidget) {
        switchFullscreen();
    }
}

void MainWindow::closeTab(int index)
{
    if (index == -1) {
        return;
    }
    QWidget *widget = m_tabWidget->widget(index);
    bool isNewConnectionPage = widget == m_newConnectionWidget;

    if (!isNewConnectionPage) {
        RemoteView *view = m_remoteViewMap.value(widget);
        m_remoteViewMap.remove(m_remoteViewMap.key(view));
        view->startQuitting();
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

void MainWindow::openTabSettings(int index)
{
    if (index == -1) {
        newConnectionPage();
        return;
    }
    QWidget *widget = m_tabWidget->widget(index);
    RemoteViewScrollArea *scrollArea = qobject_cast<RemoteViewScrollArea*>(widget);
    if (!scrollArea) return;
    RemoteView *view = qobject_cast<RemoteView*>(scrollArea->widget());
    if (!view) return;

    const QString url = view->url().url();
    qCDebug(KRDC) << url;

    showSettingsDialog(url);
}

void MainWindow::showSettingsDialog(const QString &url)
{
    HostPreferences *prefs = nullptr;

    for (RemoteViewFactory *factory : qAsConst(m_remoteViewFactories)) {
        if (factory->supportsUrl(QUrl(url))) {
            prefs = factory->createHostPreferences(Settings::self()->config()->group(QStringLiteral("hostpreferences")).group(url), this);
            if (prefs) {
                qCDebug(KRDC) << "Found plugin to handle url (" << url << "): " << prefs->metaObject()->className();
            } else {
                qCDebug(KRDC) << "Found plugin to handle url (" << url << "), but plugin does not provide preferences";
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
    const QString title = index.model()->index(index.row(), RemoteDesktopsModel::Title).data(Qt::DisplayRole).toString();
    const QString source = index.model()->index(index.row(), RemoteDesktopsModel::Source).data(Qt::DisplayRole).toString();

    QMenu *menu = new QMenu(url, m_newConnectionTableView);

    QAction *connectAction = menu->addAction(QIcon::fromTheme(QStringLiteral("network-connect")), i18n("Connect"));
    QAction *renameAction = menu->addAction(QIcon::fromTheme(QStringLiteral("edit-rename")), i18n("Rename"));
    QAction *settingsAction = menu->addAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("Settings"));
    QAction *deleteAction = menu->addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete"));

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
        const QString newTitle = QInputDialog::getText(this, i18n("Rename %1", title), i18n("Rename %1 to", title), QLineEdit::EchoMode::Normal, title, &ok);
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

void MainWindow::tabContextMenu(const QPoint &point)
{
    int index = m_tabWidget->tabBar()->tabAt(point);
    QWidget *widget = m_tabWidget->widget(index);
    RemoteViewScrollArea *scrollArea = qobject_cast<RemoteViewScrollArea*>(widget);
    if (!scrollArea) return;
    RemoteView *view = qobject_cast<RemoteView*>(scrollArea->widget());
    if (!view) return;

    const QString url = view->url().toDisplayString(QUrl::StripTrailingSlash);
    qCDebug(KRDC) << url;

    QMenu *menu = new QMenu(url, this);
    QAction *bookmarkAction = menu->addAction(QIcon::fromTheme(QStringLiteral("bookmark-new")), i18n("Add Bookmark"));
    QAction *closeAction = menu->addAction(QIcon::fromTheme(QStringLiteral("tab-close")), i18n("Close Tab"));
    QAction *selectedAction = menu->exec(QCursor::pos());
    if (selectedAction) {
        if (selectedAction == closeAction) {
            closeTab(m_tabWidget->indexOf(widget));
        } else if (selectedAction == bookmarkAction) {
            m_bookmarkManager->addManualBookmark(view->url(), url);
        }
    }
    menu->deleteLater();
}

void MainWindow::showLocalCursor(bool showLocalCursor)
{
    qCDebug(KRDC) << showLocalCursor;

    RemoteView* view = currentRemoteView();
    view->showLocalCursor(showLocalCursor ? RemoteView::CursorOn : RemoteView::CursorOff);
    view->hostPreferences()->setShowLocalCursor(showLocalCursor);
    saveHostPrefs(view);
}

void MainWindow::viewOnly(bool viewOnly)
{
    qCDebug(KRDC) << viewOnly;

    RemoteView* view = currentRemoteView();
    view->setViewOnly(viewOnly);
    view->hostPreferences()->setViewOnly(viewOnly);
    saveHostPrefs(view);
}

void MainWindow::grabAllKeys(bool grabAllKeys)
{
    qCDebug(KRDC);

    RemoteView* view = currentRemoteView();
    view->setGrabAllKeys(grabAllKeys);
    view->hostPreferences()->setGrabAllKeys(grabAllKeys);
    saveHostPrefs(view);
}

void setActionStatus(QAction* action, bool enabled, bool visible, bool checked)
{
    action->setEnabled(enabled);
    action->setVisible(visible);
    action->setChecked(checked);
}

void MainWindow::scale(bool scale)
{
    qCDebug(KRDC);

    RemoteView* view = currentRemoteView();
    view->enableScaling(scale);
    if (m_fullscreenWindow)
        view->hostPreferences()->setFullscreenScale(scale);
    else
        view->hostPreferences()->setWindowedScale(scale);

    saveHostPrefs(view);

    Q_EMIT scaleUpdated(scale);
}

void MainWindow::setFactor(int scale)
{
    float s = float(scale)/100.;

    RemoteView* view = currentRemoteView();
    if (view) {
        view->setScaleFactor(s);

        view->enableScaling(view->scaling());
        view->hostPreferences()->setScaleFactor(scale);

        saveHostPrefs(view);
    }
}

void MainWindow::showRemoteViewToolbar()
{
    qCDebug(KRDC);

    if (!m_toolBar) {
        m_toolBar = new FloatingToolBar(m_fullscreenWindow, m_fullscreenWindow);
        m_toolBar->winId(); // force it to be a native widget (prevents problem with QX11EmbedContainer)
        m_toolBar->setSide(FloatingToolBar::Top);

        KComboBox *sessionComboBox = new KComboBox(m_toolBar);
        sessionComboBox->setStyleSheet(QStringLiteral("QComboBox:!editable{background:transparent;}"));
        sessionComboBox->setModel(m_tabWidget->getModel());
        sessionComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        sessionComboBox->setCurrentIndex(m_tabWidget->currentIndex());
        connect(sessionComboBox, SIGNAL(activated(int)), m_tabWidget, SLOT(setCurrentIndex(int)));
        connect(m_tabWidget, SIGNAL(currentChanged(int)), sessionComboBox, SLOT(setCurrentIndex(int)));
        m_toolBar->addWidget(sessionComboBox);

        QToolBar *buttonBox = new QToolBar(m_toolBar);

        buttonBox->addAction(actionCollection()->action(QStringLiteral("new_connection")));
        buttonBox->addAction(actionCollection()->action(QStringLiteral("switch_fullscreen")));

        QAction *minimizeAction = new QAction(m_toolBar);
        minimizeAction->setIcon(QIcon::fromTheme(QStringLiteral("go-down")));
        minimizeAction->setText(i18n("Minimize Full Screen Window"));
        connect(minimizeAction, SIGNAL(triggered()), m_fullscreenWindow, SLOT(showMinimized()));
        buttonBox->addAction(minimizeAction);

        buttonBox->addAction(actionCollection()->action(QStringLiteral("take_screenshot")));
        buttonBox->addAction(actionCollection()->action(QStringLiteral("view_only")));
        buttonBox->addAction(actionCollection()->action(QStringLiteral("show_local_cursor")));
        buttonBox->addAction(actionCollection()->action(QStringLiteral("grab_all_keys")));
        buttonBox->addAction(actionCollection()->action(QStringLiteral("scale")));
        buttonBox->addAction(actionCollection()->action(QStringLiteral("scale_factor")));
        buttonBox->addAction(actionCollection()->action(QStringLiteral("disconnect")));
        buttonBox->addAction(actionCollection()->action(QStringLiteral("file_quit")));

        QAction *stickToolBarAction = new QAction(m_toolBar);
        stickToolBarAction->setCheckable(true);
        stickToolBarAction->setIcon(QIcon::fromTheme(QStringLiteral("object-locked")));
        stickToolBarAction->setText(i18n("Stick Toolbar"));
        connect(stickToolBarAction, SIGNAL(triggered(bool)), m_toolBar, SLOT(setSticky(bool)));
        buttonBox->addAction(stickToolBarAction);

        m_toolBar->addWidget(buttonBox);
    }
}

void MainWindow::updateActionStatus()
{
    qCDebug(KRDC) << m_tabWidget->currentIndex();

    bool enabled = true;

    if (m_tabWidget->currentWidget() == m_newConnectionWidget)
        enabled = false;

    RemoteView* view = (m_currentRemoteView >= 0 && enabled) ? currentRemoteView() : nullptr;

    actionCollection()->action(QStringLiteral("take_screenshot"))->setEnabled(enabled);
    actionCollection()->action(QStringLiteral("disconnect"))->setEnabled(enabled);

    setActionStatus(actionCollection()->action(QStringLiteral("view_only")),
                    enabled,
                    view ? view->supportsViewOnly() : false,
                    view ? view->viewOnly() : false);

    setActionStatus(actionCollection()->action(QStringLiteral("show_local_cursor")),
                    enabled,
                    view ? view->supportsLocalCursor() : false,
                    view ? view->localCursorState() == RemoteView::CursorOn : false);

    setActionStatus(actionCollection()->action(QStringLiteral("scale")),
                    enabled,
                    view ? view->supportsScaling() : false,
                    view ? view->scaling() : false);

    actionCollection()->action(QStringLiteral("scale_factor"))->setEnabled(enabled);
    actionCollection()->action(QStringLiteral("scale_factor"))->setVisible(view ? view->supportsScaling() : false);

    setActionStatus(actionCollection()->action(QStringLiteral("grab_all_keys")),
                    enabled,
                    enabled,
                    view ? view->grabAllKeys() : false);
}

void MainWindow::preferences()
{
    // An instance of your dialog could be already created and could be
    // cached, in which case you want to display the cached dialog
    // instead of creating another one
    if (PreferencesDialog::showDialog(QStringLiteral("preferences")))
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
        statusBar()->showMessage(QStringLiteral("")); // force creation of statusbar

    m_tabWidget->tabBar()->setHidden((m_tabWidget->count() <= 1 && !Settings::showTabBar()) || m_fullscreenWindow);
    m_tabWidget->setTabPosition((QTabWidget::TabPosition) Settings::tabPosition());
    m_tabWidget->setTabsClosable(Settings::tabCloseButton());

    disconnect(m_tabWidget, SIGNAL(mouseMiddleClick(int)), this, SLOT(closeTab(int))); // just be sure it is not connected twice
    if (Settings::tabMiddleClick())
        connect(m_tabWidget, SIGNAL(mouseMiddleClick(int)), SLOT(closeTab(int)));

    if (Settings::systemTrayIcon() && !m_systemTrayIcon) {
        m_systemTrayIcon = new SystemTrayIcon(this);
        if(m_fullscreenWindow) m_systemTrayIcon->setAssociatedWidget(m_fullscreenWindow);
    } else if (m_systemTrayIcon) {
        delete m_systemTrayIcon;
        m_systemTrayIcon = nullptr;
    }

    // update the scroll areas background color
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        QPalette palette = m_tabWidget->widget(i)->palette();
        palette.setColor(QPalette::Dark, Settings::backgroundColor());
        m_tabWidget->widget(i)->setPalette(palette);
    }

    if (m_protocolInput) {
        m_protocolInput->setCurrentText(Settings::defaultProtocol());
    }

    // Send update configuration message to all views
    for (RemoteView *view : qAsConst(m_remoteViewMap)) {
        view->updateConfiguration();
    }
}

void MainWindow::quit(bool systemEvent)
{
    const bool haveRemoteConnections = !m_remoteViewMap.isEmpty();
    if (systemEvent || !haveRemoteConnections || KMessageBox::warningContinueCancel(this,
            i18n("Are you sure you want to quit the KDE Remote Desktop Client?"),
            i18n("Confirm Quit"),
            KStandardGuiItem::quit(), KStandardGuiItem::cancel(),
            QStringLiteral("DoNotAskBeforeExit")) == KMessageBox::Continue) {

        if (Settings::rememberSessions()) { // remember open remote views for next startup
            QStringList list;
            for (RemoteView *view : qAsConst(m_remoteViewMap)) {
                qCDebug(KRDC) << view->url();
                list.append(view->url().toDisplayString(QUrl::StripTrailingSlash));
            }
            Settings::setOpenSessions(list);
        }

        saveHostPrefs();

        const QMap<QWidget *, RemoteView *> currentViews = m_remoteViewMap;
        for (RemoteView *view : currentViews) {
            view->startQuitting();
        }

        Settings::self()->save();

        qApp->quit();
    }
}

void MainWindow::configureNotifications()
{
    KNotifyConfigWidget::configure(this);
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
    qCDebug(KRDC);
    KMainWindow::saveProperties(group);
    saveHostPrefs();
}


void MainWindow::saveHostPrefs()
{
    for (RemoteView *view : qAsConst(m_remoteViewMap)) {
        saveHostPrefs(view);
    }
}

void MainWindow::saveHostPrefs(RemoteView* view)
{
    // should saving this be a user option?
    if (view && view->scaling()) {
        QSize viewSize = m_tabWidget->currentWidget()->size();
        qCDebug(KRDC) << "saving window size:" << viewSize;
        view->hostPreferences()->setWidth(viewSize.width());
        view->hostPreferences()->setHeight(viewSize.height());
    }

    Settings::self()->config()->sync();
}

void MainWindow::tabChanged(int index)
{
    qCDebug(KRDC) << index;

    m_tabWidget->tabBar()->setHidden((m_tabWidget->count() <= 1 && !Settings::showTabBar()) || m_fullscreenWindow);

    m_currentRemoteView = index;

    if (m_tabWidget->currentWidget() == m_newConnectionWidget) {
        m_currentRemoteView = -1;
        if(m_addressInput) m_addressInput->setFocus();
    }

    const QString tabTitle = m_tabWidget->tabText(index).remove(QLatin1Char('&'));
    setCaption(tabTitle == i18n("New Connection") ? QString() : tabTitle);

    updateActionStatus();
}

QWidget* MainWindow::newConnectionWidget()
{
    if (m_newConnectionWidget)
        return m_newConnectionWidget;

    m_newConnectionWidget = new QWidget(this);

    QVBoxLayout *startLayout = new QVBoxLayout(m_newConnectionWidget);
    startLayout->setContentsMargins(QMargins(8, 4, 8, 4));

    QSortFilterProxyModel *remoteDesktopsModelProxy = new QSortFilterProxyModel(this);
    remoteDesktopsModelProxy->setSourceModel(m_remoteDesktopsModel);
    remoteDesktopsModelProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    remoteDesktopsModelProxy->setFilterRole(10002);

    {
        QHBoxLayout *connectLayout = new QHBoxLayout;

        QLabel *addressLabel = new QLabel(i18n("Connect to:"), m_newConnectionWidget);
        m_protocolInput = new KComboBox(m_newConnectionWidget);
        m_addressInput = new KLineEdit(m_newConnectionWidget);
        m_addressInput->setClearButtonEnabled(true);
        m_addressInput->setPlaceholderText(i18n("Type here to connect to an address and filter the list."));
        connect(m_addressInput, SIGNAL(textChanged(QString)), remoteDesktopsModelProxy, SLOT(setFilterFixedString(QString)));

        for (RemoteViewFactory *factory : qAsConst(m_remoteViewFactories)) {
            m_protocolInput->addItem(factory->scheme());
        }
        m_protocolInput->setCurrentText(Settings::defaultProtocol());

        connect(m_addressInput, SIGNAL(returnPressed()), SLOT(newConnection()));
        m_addressInput->setToolTip(i18n("Type an IP or DNS Name here. Clear the line to get a list of connection methods."));

        QPushButton *connectButton = new QPushButton(m_newConnectionWidget);
        connectButton->setToolTip(i18n("Goto Address"));
        connectButton->setIcon(QIcon::fromTheme(QStringLiteral("go-jump-locationbar")));
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
        m_newConnectionTableView->setModel(remoteDesktopsModelProxy);

        // set up the view so it looks nice
        m_newConnectionTableView->setItemDelegate(new ConnectionDelegate(m_newConnectionTableView));
        m_newConnectionTableView->setShowGrid(false);
        m_newConnectionTableView->setSelectionMode(QAbstractItemView::NoSelection);
        m_newConnectionTableView->verticalHeader()->hide();
        m_newConnectionTableView->verticalHeader()->setDefaultSectionSize(
            m_newConnectionTableView->fontMetrics().height() + 3);
        m_newConnectionTableView->horizontalHeader()->setStretchLastSection(true);
        m_newConnectionTableView->setAlternatingRowColors(true);
        // set up sorting and actions (double click open, right click custom menu)
        m_newConnectionTableView->setSortingEnabled(true);
        m_newConnectionTableView->sortByColumn(Settings::connectionListSortColumn(), Qt::SortOrder(Settings::connectionListSortOrder()));
        m_newConnectionTableView->resizeColumnsToContents();
        connect(m_newConnectionTableView->horizontalHeader(), SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)),
                SLOT(saveConnectionListSort(int,Qt::SortOrder)));
        connect(m_newConnectionTableView, SIGNAL(doubleClicked(QModelIndex)),
                SLOT(openFromRemoteDesktopsModel(QModelIndex)));
        // useful to edit similar address
        connect(m_newConnectionTableView, SIGNAL(clicked(QModelIndex)),
                SLOT(selectFromRemoteDesktopsModel(QModelIndex)));
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
    Settings::self()->save();
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

QMap<QWidget *, RemoteView *> MainWindow::remoteViewList() const
{
    return m_remoteViewMap;
}

QList<RemoteViewFactory *> MainWindow::remoteViewFactoriesList() const
{
    return m_remoteViewFactories.values();
}

RemoteView* MainWindow::currentRemoteView() const
{
    if (m_currentRemoteView >= 0) {
        return m_remoteViewMap.value(m_tabWidget->widget(m_currentRemoteView));
    } else {
        return nullptr;
    }
}

void MainWindow::createDockWidget()
{
    QDockWidget *remoteDesktopsDockWidget = new QDockWidget(this);
    QWidget *remoteDesktopsDockLayoutWidget = new QWidget(remoteDesktopsDockWidget);
    QVBoxLayout *remoteDesktopsDockLayout = new QVBoxLayout(remoteDesktopsDockLayoutWidget);
    remoteDesktopsDockWidget->setObjectName(QStringLiteral("remoteDesktopsDockWidget")); // required for saving position / state
    remoteDesktopsDockWidget->setWindowTitle(i18n("Remote Desktops"));
    QFontMetrics fontMetrics(remoteDesktopsDockWidget->font());
    remoteDesktopsDockWidget->setMinimumWidth(fontMetrics.horizontalAdvance(QStringLiteral("vnc://192.168.100.100:6000")));
    actionCollection()->addAction(QStringLiteral("remote_desktop_dockwidget"),
                                  remoteDesktopsDockWidget->toggleViewAction());

    m_dockWidgetTableView = new QTableView(remoteDesktopsDockLayoutWidget);
    m_remoteDesktopsModel = new RemoteDesktopsModel(this, m_bookmarkManager->getManager());
    QSortFilterProxyModel *remoteDesktopsModelProxy = new QSortFilterProxyModel(this);
    remoteDesktopsModelProxy->setSourceModel(m_remoteDesktopsModel);
    remoteDesktopsModelProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    remoteDesktopsModelProxy->setFilterRole(10002);
    m_dockWidgetTableView->setModel(remoteDesktopsModelProxy);

    m_dockWidgetTableView->setShowGrid(false);
    m_dockWidgetTableView->verticalHeader()->hide();
    m_dockWidgetTableView->verticalHeader()->setDefaultSectionSize(
        m_dockWidgetTableView->fontMetrics().height() + 2);
    m_dockWidgetTableView->horizontalHeader()->hide();
    m_dockWidgetTableView->horizontalHeader()->setStretchLastSection(true);
    // hide all columns, then show the one we want
    for (int i=0; i < remoteDesktopsModelProxy->columnCount(); i++) {
        m_dockWidgetTableView->hideColumn(i);
    }
    m_dockWidgetTableView->showColumn(RemoteDesktopsModel::Title);
    m_dockWidgetTableView->sortByColumn(RemoteDesktopsModel::Title, Qt::AscendingOrder);

    connect(m_dockWidgetTableView, SIGNAL(doubleClicked(QModelIndex)),
            SLOT(openFromRemoteDesktopsModel(QModelIndex)));

    KLineEdit *filterLineEdit = new KLineEdit(remoteDesktopsDockLayoutWidget);
    filterLineEdit->setPlaceholderText(i18n("Filter"));
    filterLineEdit->setClearButtonEnabled(true);
    connect(filterLineEdit, SIGNAL(textChanged(QString)), remoteDesktopsModelProxy, SLOT(setFilterFixedString(QString)));
    remoteDesktopsDockLayout->addWidget(filterLineEdit);
    remoteDesktopsDockLayout->addWidget(m_dockWidgetTableView);
    remoteDesktopsDockWidget->setWidget(remoteDesktopsDockLayoutWidget);
    addDockWidget(Qt::LeftDockWidgetArea, remoteDesktopsDockWidget);
}


