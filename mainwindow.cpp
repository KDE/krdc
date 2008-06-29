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

#include "mainwindow.h"

#include "remoteview.h"
#include "settings.h"
#include "config/preferencesdialog.h"
#include "floatingtoolbar.h"
#include "bookmarkmanager.h"
#include "remotedesktopsmodel.h"
#include "systemtrayicon.h"
#ifdef BUILD_RDP
#include "rdpview.h"
#endif
#ifdef BUILD_NX
#include "nxview.h"
#endif
#ifdef BUILD_VNC
#include "vncview.h"
#endif
#ifdef BUILD_ZEROCONF
#include "zeroconfpage.h"
#endif

#include <KAction>
#include <KActionCollection>
#include <KActionMenu>
#include <KApplication>
#include <KComboBox>
#include <KEditToolBar>
#include <KIcon>
#include <KLocale>
#include <KMenuBar>
#include <KMessageBox>
#include <KNotifyConfigWidget>
#include <KPushButton>
#include <KShortcut>
#include <KShortcutsDialog>
#include <KStatusBar>
#include <KTabWidget>
#include <KToggleAction>
#include <KToggleFullScreenAction>
#include <KUrlNavigator>

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
    setupActions();

    setStandardToolBarMenuEnabled(true);

    m_tabWidget = new KTabWidget(this);
    m_tabWidget->setCloseButtonEnabled(true);
    connect(m_tabWidget, SIGNAL(closeRequest(QWidget *)), SLOT(closeTab(QWidget *)));

    m_tabWidget->setMinimumSize(600, 400);
    setCentralWidget(m_tabWidget);

    QDockWidget *remoteDesktopsDockWidget = new QDockWidget(this);
    remoteDesktopsDockWidget->setObjectName("remoteDesktopsDockWidget"); // required for saving position / state
    remoteDesktopsDockWidget->setWindowTitle(i18n("Remote desktops"));
    QFontMetrics fontMetrics(remoteDesktopsDockWidget->font());
    remoteDesktopsDockWidget->setMinimumWidth(fontMetrics.width("vnc://192.168.100.100:6000"));
    actionCollection()->addAction("remote_desktop_dockwidget",
                                  remoteDesktopsDockWidget->toggleViewAction());
    QTreeView *remoteDesktopsTreeView = new QTreeView(remoteDesktopsDockWidget);
    RemoteDesktopsModel *remoteDesktopsModel = new RemoteDesktopsModel(this);
    connect(remoteDesktopsModel, SIGNAL(modelReset()), remoteDesktopsTreeView, SLOT(expandAll()));
    remoteDesktopsTreeView->setModel(remoteDesktopsModel);
    remoteDesktopsTreeView->header()->hide();
    remoteDesktopsTreeView->expandAll();
    connect(remoteDesktopsTreeView, SIGNAL(doubleClicked(const QModelIndex &)),
                                    SLOT(openFromDockWidget(const QModelIndex &)));

    remoteDesktopsDockWidget->setWidget(remoteDesktopsTreeView);
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

    if (Settings::rememberSessions()) // give some time to create and show the window first
        QTimer::singleShot(100, this, SLOT(restoreOpenSessions()));
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupActions()
{
    QAction *vncConnectionAction = actionCollection()->addAction("new_vnc_connection");
    vncConnectionAction->setText(i18n("New VNC Connection..."));
    vncConnectionAction->setIcon(KIcon("network-connect"));
    connect(vncConnectionAction, SIGNAL(triggered()), SLOT(newVncConnection()));
#ifndef BUILD_VNC
    vncConnectionAction->deleteLater();
#endif

    QAction *nxConnectionAction = actionCollection()->addAction("new_nx_connection");
    nxConnectionAction->setText(i18n("New NX Connection..."));
    nxConnectionAction->setIcon(KIcon("network-connect"));
    connect(nxConnectionAction, SIGNAL(triggered()), SLOT(newNxConnection()));
#ifndef BUILD_NX
    nxConnectionAction->deleteLater();
#endif

    QAction *rdpConnectionAction = actionCollection()->addAction("new_rdp_connection");
    rdpConnectionAction->setText(i18n("New RDP Connection..."));
    rdpConnectionAction->setIcon(KIcon("network-connect"));
    connect(rdpConnectionAction, SIGNAL(triggered()), SLOT(newRdpConnection()));
#ifndef BUILD_RDP
    rdpConnectionAction->deleteLater();
#endif

    QAction *zeroconfAction = actionCollection()->addAction("zeroconf_page");
    zeroconfAction->setText(i18n("Browse Remote Desktop Services on Local Network..."));
    zeroconfAction->setIcon(KIcon("network-connect"));
    connect(zeroconfAction, SIGNAL(triggered()), SLOT(createZeroconfPage()));
#ifndef BUILD_ZEROCONF
    zeroconfAction->setVisible(false);
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

    QAction *logoutAction = actionCollection()->addAction("logout");
    logoutAction->setText(i18n("Log Out"));
    logoutAction->setIcon(KIcon("system-log-out"));
    connect(logoutAction, SIGNAL(triggered()), SLOT(logout()));

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

    QAction *quitAction = KStandardAction::quit(this, SLOT(quit()), actionCollection());
    actionCollection()->addAction("quit", quitAction);
    QAction *preferencesAction = KStandardAction::preferences(this, SLOT(preferences()), actionCollection());
    actionCollection()->addAction("preferences", preferencesAction);
    QAction *configToolbarAction = KStandardAction::configureToolbars(this, SLOT(configureToolbars()), actionCollection());
    actionCollection()->addAction("configure_toolbars", configToolbarAction);
    QAction *keyBindingsAction = KStandardAction::keyBindings(this, SLOT(configureKeys()), actionCollection());
    actionCollection()->addAction("configure_keys", keyBindingsAction);
    QAction *cinfigNotifyAction = KStandardAction::configureNotifications(this, SLOT(configureNotifications()), actionCollection());
    cinfigNotifyAction->setVisible(false);
    actionCollection()->addAction("configure_notifications", cinfigNotifyAction);
    m_menubarAction = KStandardAction::showMenubar(this, SLOT(showMenubar()), actionCollection());
    m_menubarAction->setChecked(!menuBar()->isHidden());
    actionCollection()->addAction("settings_showmenubar", m_menubarAction);

    QString initialProtocol;
#ifdef BUILD_RDP
    initialProtocol = "rdp";
#endif
#ifdef BUILD_NX
    initialProtocol = "nx";
#endif
#ifdef BUILD_VNC
    initialProtocol = "vnc";
#endif

    m_addressNavigator = new KUrlNavigator(0, KUrl(initialProtocol + "://"), this);
    m_addressNavigator->setCustomProtocols(QStringList()
#ifdef BUILD_VNC
                                           << "vnc"
#endif
#ifdef BUILD_NX
                                           << "nx"
#endif
#ifdef BUILD_RDP
                                           << "rdp"
#endif
                                          );
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

void MainWindow::restoreOpenSessions()
{
    QStringList list = Settings::openSessions();
    QStringList::Iterator it = list.begin();
    QStringList::Iterator end = list.end();
    while (it != end) {
        newConnection(*it);
        ++it;
    }
}

void MainWindow::newConnection(const KUrl &newUrl, bool switchFullscreenWhenConnected)
{
    m_switchFullscreenWhenConnected = switchFullscreenWhenConnected;

    KUrl url = newUrl.isEmpty() ? m_addressNavigator->uncommittedUrl() : newUrl;

    if (!url.isValid() || (url.host().isEmpty() && url.port() < 0)
            || !url.path().isEmpty()) {
        KMessageBox::error(this,
                           i18n("The entered address does not have the required form."),
                           i18n("Malformed URL"));
        return;
    }

    m_addressNavigator->setUrl(KUrl(url.scheme().toLower() + "://"));

    RemoteView *view = 0;

    if (url.scheme().toLower() == "vnc") {
#ifdef BUILD_VNC
        view = new VncView(this, url);
#endif
    } else if (url.scheme().toLower() == "nx") {
#ifdef BUILD_NX
        view = new NxView(this, url);
#endif
    } else if (url.scheme().toLower() == "rdp") {
#ifdef BUILD_RDP
        view = new RdpView(this, url);
#endif
    } else
    {
        KMessageBox::error(this,
                           i18n("The entered address cannot be handled."),
                           i18n("Unusable URL"));
        return;
    }

    if (!view) {
        KMessageBox::error(this, i18n("Support for %1:// has not been enabled during build.",
                    url.scheme().toLower()),
                i18n("Unusable URL"));
        return;
    }

    connect(view, SIGNAL(changeSize(int, int)), this, SLOT(resizeTabWidget(int, int)));
    connect(view, SIGNAL(statusChanged(RemoteView::RemoteStatus)), this, SLOT(statusChanged(RemoteView::RemoteStatus)));

    m_remoteViewList.append(view);

    view->resize(0, 0);
 
    int numNonRemoteView = 0;
    if (m_showStartPage)
        numNonRemoteView++;
    if (m_zeroconfPage)
        numNonRemoteView++;

    QScrollArea *scrollArea = createScrollArea(m_tabWidget, m_remoteViewList.at(m_remoteViewList.count() - 1));

    int newIndex = m_tabWidget->addTab(scrollArea, KIcon("krdc"), url.prettyUrl(KUrl::RemoveTrailingSlash));
    m_tabWidget->setCurrentIndex(newIndex);
    tabChanged(newIndex); // force to update m_currentRemoteView (tabChanged is not emitted when start page has been disabled)

    view->start();
}

void MainWindow::openFromDockWidget(const QModelIndex &index)
{
    if (index.data(Qt::UserRole).toBool()) {
        KUrl url(index.data().toString());
        // first check if url has already been opened; in case show the tab
        for (int i = 0; i < m_remoteViewList.count(); i++) {
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

    if (m_topBottomBorder == 0) { // the values are not cached yet
        QScrollArea *tmp = qobject_cast<QScrollArea *>(m_tabWidget->currentWidget());

        m_leftRightBorder = m_tabWidget->width() - m_tabWidget->currentWidget()->width() + (2 * tmp->frameWidth());
        m_topBottomBorder = m_tabWidget->height() - m_tabWidget->currentWidget()->height() + (2 * tmp->frameWidth());

        kDebug(5010) << "tabwidget border: w: " << m_leftRightBorder << ", h: " << m_topBottomBorder;
    }

    int newTabWidth = w + m_leftRightBorder;
    int newTabHeight = h + m_topBottomBorder;

    QSize newWindowSize = size() - m_tabWidget->size() + QSize(newTabWidth, newTabHeight);

    QSize desktopSize = QSize(QApplication::desktop()->availableGeometry().width(),
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

    QString host = m_remoteViewList.at(m_currentRemoteView)->host();

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
        iconName = "krdc";
        message = QString();
    }

    m_tabWidget->setTabIcon(m_tabWidget->currentIndex(), KIcon(iconName));
    statusBar()->showMessage(message);
}

void MainWindow::takeScreenshot()
{
    QPixmap snapshot = QPixmap::grabWidget(m_remoteViewList.at(m_currentRemoteView));

    QApplication::clipboard()->setPixmap(snapshot);
}

void MainWindow::switchFullscreen()
{
    kDebug(5010);

    if (m_fullscreenWindow) {
        show();
        restoreGeometry(m_mainWindowGeometry);

        m_fullscreenWindow->setWindowState(0);
        m_fullscreenWindow->hide();

        QScrollArea *scrollArea = createScrollArea(m_tabWidget, m_remoteViewList.at(m_currentRemoteView));

        int currentTab = m_tabWidget->currentIndex();
        m_tabWidget->insertTab(currentTab, scrollArea, m_tabWidget->tabIcon(currentTab), m_tabWidget->tabText(currentTab));
        m_tabWidget->removeTab(m_tabWidget->currentIndex());
        m_tabWidget->setCurrentIndex(currentTab);

        resizeTabWidget(m_remoteViewList.at(m_currentRemoteView)->sizeHint().width(),
                        m_remoteViewList.at(m_currentRemoteView)->sizeHint().height());

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
        m_fullscreenWindow = new QWidget(this, Qt::Window);
        m_fullscreenWindow->setWindowTitle(i18nc("window title when in fullscreen mode (for example displayed in tasklist)",
                                                 "KDE Remote Desktop Client (Fullscreen)"));

        QScrollArea *scrollArea = createScrollArea(m_fullscreenWindow, m_remoteViewList.at(m_currentRemoteView));
        scrollArea->setFrameShape(QFrame::NoFrame);

        QVBoxLayout *fullscreenLayout = new QVBoxLayout(m_fullscreenWindow);
        fullscreenLayout->setMargin(0);
        fullscreenLayout->addWidget(scrollArea);

        MinimizePixel *minimizePixel = new MinimizePixel(m_fullscreenWindow);
        minimizePixel->winId(); // force it to be a native widget (prevents problem with QX11EmbedContainer)
        connect(minimizePixel, SIGNAL(rightClicked()), m_fullscreenWindow, SLOT(showMinimized()));

        m_fullscreenWindow->show();

        KToggleFullScreenAction::setFullScreen(m_fullscreenWindow, true);

        // show the toolbar after we have switched to fullscreen mode
        QTimer::singleShot(100, this, SLOT(showRemoteViewToolbar()));

        m_mainWindowGeometry = saveGeometry();
        hide();
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

void MainWindow::logout()
{
    kDebug(5010);

    if (m_fullscreenWindow) { // first close fullscreen view
        switchFullscreen();
    }

    QWidget *tmp = m_tabWidget->currentWidget();

    m_remoteViewList.removeAt(m_currentRemoteView);

    m_tabWidget->removeTab(m_tabWidget->currentIndex());

    tmp->deleteLater();
}

void MainWindow::closeTab(QWidget *widget)
{
    int index = m_tabWidget->indexOf(widget);

    kDebug(5010) << index;

    if (m_showStartPage && index == 0) {
        KMessageBox::information(this, i18n("The start page cannot be closed. "
                                            "If you want to disable it, you can do so in the settings."));
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

void MainWindow::showLocalCursor(bool showLocalCursor)
{
    kDebug(5010) << showLocalCursor;

    m_remoteViewList.at(m_currentRemoteView)->showDotCursor(showLocalCursor ? RemoteView::CursorOn : RemoteView::CursorOff);
}

void MainWindow::viewOnly(bool viewOnly)
{
    kDebug(5010) << viewOnly;

    m_remoteViewList.at(m_currentRemoteView)->setViewOnly(viewOnly);
}

void MainWindow::grabAllKeys(bool grabAllKeys)
{
    kDebug(5010);

    m_remoteViewList.at(m_currentRemoteView)->setGrabAllKeys(grabAllKeys);
}

void MainWindow::scale(bool scale)
{
    kDebug(5010);

    m_remoteViewList.at(m_currentRemoteView)->enableScaling(scale);
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
        m_toolBar->addAction(actionCollection()->action("logout"));

        QAction *stickToolBarAction = new QAction(m_toolBar);
        stickToolBarAction->setCheckable(true);
        stickToolBarAction->setIcon(KIcon("object-locked"));
        stickToolBarAction->setText(i18n("Stick Toolbar"));
        connect(stickToolBarAction, SIGNAL(triggered(bool)), m_toolBar, SLOT(setSticky(bool)));
        m_toolBar->addAction(stickToolBarAction);
    }

    m_toolBar->showAndAnimate();
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

    actionCollection()->action("switch_fullscreen")->setEnabled(enabled);
    actionCollection()->action("take_screenshot")->setEnabled(enabled);
    actionCollection()->action("view_only")->setEnabled(enabled);
    actionCollection()->action("grab_all_keys")->setEnabled(enabled);
    actionCollection()->action("scale")->setEnabled(enabled);
    actionCollection()->action("logout")->setEnabled(enabled);

    bool viewOnlyChecked = false;
    if (m_currentRemoteView >= 0)
        viewOnlyChecked = enabled && m_remoteViewList.at(m_currentRemoteView)->viewOnly();
    actionCollection()->action("view_only")->setChecked(viewOnlyChecked);

    bool showLocalCursorChecked = false;
    if (m_currentRemoteView >= 0)
        showLocalCursorChecked = enabled && m_remoteViewList.at(m_currentRemoteView)->dotCursorState() == RemoteView::CursorOn;
    actionCollection()->action("show_local_cursor")->setChecked(showLocalCursorChecked);

    bool showLocalCursorVisible = false;
    if (m_currentRemoteView >= 0)
        showLocalCursorVisible = enabled && m_remoteViewList.at(m_currentRemoteView)->supportsLocalCursor();
    actionCollection()->action("show_local_cursor")->setVisible(showLocalCursorVisible);

    bool scaleVisible = false;
    if (m_currentRemoteView >= 0)
        scaleVisible = enabled && m_remoteViewList.at(m_currentRemoteView)->supportsScaling();
    actionCollection()->action("scale")->setVisible(scaleVisible);
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
    for (int i = numNonRemoteView; i < m_tabWidget->count(); i++) {
        QPalette palette = m_tabWidget->widget(i)->palette();
        palette.setColor(QPalette::Dark, Settings::backgroundColor());
        m_tabWidget->widget(i)->setPalette(palette);
    }
}

void MainWindow::quit()
{
    bool haveRemoteConnections = m_remoteViewList.count();
    if (!haveRemoteConnections || KMessageBox::warningContinueCancel(this,
                                           i18n("Are you sure you want to quit the KDE Remote Desktop Client?"),
                                           i18n("Confirm Quit"),
                                           KStandardGuiItem::quit(), KStandardGuiItem::cancel(),
                                           "DoNotAskBeforeExit") == KMessageBox::Continue) {

        if (Settings::rememberSessions()) { // remember open remote views for next startup
            QStringList list;
            for (int i = 0; i < m_remoteViewList.count(); i++) {
                kDebug(5010) << m_remoteViewList.at(i)->url();
                list.append(m_remoteViewList.at(i)->url().prettyUrl(KUrl::RemoveTrailingSlash));
            }
            Settings::setOpenSessions(list);
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
    event->ignore();

    if (Settings::systemTrayIcon()) {
        hide(); // just hide the mainwindow, keep it in systemtray
    } else {
        quit();
    }
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

    QString tabTitle = m_tabWidget->tabText(index).remove('&');

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

    KPushButton *vncConnectButton = new KPushButton(this);
    vncConnectButton->setStyleSheet("KPushButton { padding: 12px; margin: 10px; }");
    vncConnectButton->setIcon(KIcon(actionCollection()->action("new_vnc_connection")->icon()));
    vncConnectButton->setText(i18n("Connect to a VNC Remote Desktop"));
    connect(vncConnectButton, SIGNAL(clicked()), SLOT(newVncConnection()));
#ifndef BUILD_VNC
    vncConnectButton->setVisible(false);
#endif

    KPushButton *nxConnectButton = new KPushButton(this);
    nxConnectButton->setStyleSheet("KPushButton { padding: 12px; margin: 10px; }");
    nxConnectButton->setIcon(KIcon(actionCollection()->action("new_nx_connection")->icon()));
    nxConnectButton->setText(i18n("Connect to a NX Remote Desktop"));
    connect(nxConnectButton, SIGNAL(clicked()), SLOT(newNxConnection()));
#ifndef BUILD_NX
    nxConnectButton->setVisible(false);
#endif

    KPushButton *rdpConnectButton = new KPushButton(this);
    rdpConnectButton->setStyleSheet("KPushButton { padding: 12px; margin: 10px; }");
    rdpConnectButton->setIcon(KIcon(actionCollection()->action("new_rdp_connection")->icon()));
    rdpConnectButton->setText(i18n("Connect to a Windows Remote Desktop (RDP)"));
    connect(rdpConnectButton, SIGNAL(clicked()), SLOT(newRdpConnection()));
#ifndef BUILD_RDP
    rdpConnectButton->setVisible(false);
#endif

    KPushButton *zeroconfButton = new KPushButton(this);
    zeroconfButton->setStyleSheet("KPushButton { padding: 12px; margin: 10px; }");
    zeroconfButton->setIcon(KIcon(actionCollection()->action("zeroconf_page")->icon()));
    zeroconfButton->setText(i18n("Browse Remote Desktop Services on Local Network"));
    connect(zeroconfButton, SIGNAL(clicked()), SLOT(createZeroconfPage()));
#ifndef BUILD_ZEROCONF
    zeroconfButton->setVisible(false);
#endif

    startLayout->addLayout(headerLayout);
    startLayout->addWidget(vncConnectButton);
    startLayout->addWidget(nxConnectButton);
    startLayout->addWidget(rdpConnectButton);
    startLayout->addWidget(zeroconfButton);
    startLayout->addStretch();

    m_tabWidget->insertTab(0, startWidget, KIcon("krdc"), i18n("Start Page"));
}

void MainWindow::newVncConnection()
{
    m_addressNavigator->setUrl(KUrl("vnc://"));
    m_addressNavigator->setFocus();

    QToolTip::showText(m_addressNavigator->pos() + pos() + QPoint(m_addressNavigator->width(),
                                                                  m_addressNavigator->height() + 20),
                       i18n("<html>Enter the address here.<br />"
                            "<i>Example: vncserver:1 (host:port / screen)</i></html>"), this);
}

void MainWindow::newNxConnection()
{
    m_addressNavigator->setUrl(KUrl("nx://"));
    m_addressNavigator->setFocus();

    QToolTip::showText(m_addressNavigator->pos() + pos() + QPoint(m_addressNavigator->width(),
                                                                  m_addressNavigator->height() + 20),
                       i18n("<html>Enter the address here.<br />"
                            "<i>Example: nxserver (host)</i></html>"), this);
}

void MainWindow::newRdpConnection()
{
    m_addressNavigator->setUrl(KUrl("rdp://"));
    m_addressNavigator->setFocus();

    QToolTip::showText(m_addressNavigator->pos() + pos() + QPoint(m_addressNavigator->width(),
                                                                  m_addressNavigator->height() + 20),
                       i18n("<html>Enter the address here. Port is optional.<br />"
                            "<i>Example: rdpserver:3389 (host:port)</i></html>"), this);
}

void MainWindow::createZeroconfPage()
{
#ifdef BUILD_ZEROCONF
    if (m_zeroconfPage)
        return;

    m_zeroconfPage = new ZeroconfPage(this);
    connect(m_zeroconfPage, SIGNAL(newConnection(const KUrl, bool)), this, SLOT(newConnection(const KUrl, bool)));
    connect(m_zeroconfPage, SIGNAL(closeZeroconfPage()), this, SLOT(closeZeroconfPage()));
    int zeroconfTabIndex = m_tabWidget->insertTab(m_showStartPage ? 1 : 0, m_zeroconfPage, KIcon("krdc"), i18n("Browse Local Network"));
    m_tabWidget->setCurrentIndex(zeroconfTabIndex);
#endif
}

void MainWindow::closeZeroconfPage()
{
#ifdef BUILD_ZEROCONF
    int index = m_tabWidget->indexOf(m_zeroconfPage);
    m_tabWidget->removeTab(index);
    m_zeroconfPage->deleteLater();
    m_zeroconfPage = 0;
    tabChanged(index); // force update again because m_zeroconfPage was not null before
#endif
}

QList<RemoteView *> MainWindow::remoteViewList() const
{
    return m_remoteViewList;
}

int MainWindow::currentRemoteView() const
{
    return m_currentRemoteView;
}

#include "mainwindow.moc"
