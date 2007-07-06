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
#ifdef BUILD_RDP
#include "rdpview.h"
#endif
#ifdef BUILD_VNC
#include "vncview.h"
#endif

#include <KAction>
#include <KActionCollection>
#include <KApplication>
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
#include <KUrlNavigator>

#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopWidget>
#include <QLabel>
#include <QLayout>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>

MainWindow::MainWindow(QWidget *parent)
  : KXmlGuiWindow(parent),
    m_fullscreenWindow(0),
    m_toolBar(0),
    m_topBottomBorder(0),
    m_leftRightBorder(0),
    m_currentRemoteView(0),
    m_showStartPage(false)
{
    setupActions();

    createGUI("krdcui.rc");

    setStandardToolBarMenuEnabled(true);

    m_tabWidget = new KTabWidget(this);
    m_tabWidget->setMinimumSize(600, 400);
    setCentralWidget(m_tabWidget);

    connect(m_tabWidget, SIGNAL(currentChanged(int)), SLOT(tabChanged(int)));

    statusBar()->showMessage(i18n("KRDC started"));

    updateActionStatus(); // disable remote view actions

    if (Settings::showStartPage())
        createStartPage();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupActions()
{
    QAction *vncConnectionAction = actionCollection()->addAction("new_vnc_connection");
    vncConnectionAction->setText(i18n("&New VNC Connection..."));
    vncConnectionAction->setIcon(KIcon("krdc"));
    connect(vncConnectionAction, SIGNAL(triggered()), SLOT(newVncConnection()));

    QAction *rdpConnectionAction = actionCollection()->addAction("new_rdp_connection");
    rdpConnectionAction->setText(i18n("&New RDP Connection..."));
    rdpConnectionAction->setIcon(KIcon("krdc"));
    connect(rdpConnectionAction, SIGNAL(triggered()), SLOT(newRdpConnection()));

    QAction *screenshotAction = actionCollection()->addAction("take_screenshot");
    screenshotAction->setText(i18n("&Copy Screenshot to Clipboard"));
    screenshotAction->setIcon(KIcon("ksnapshot"));
    connect(screenshotAction, SIGNAL(triggered()), SLOT(slotTakeScreenshot()));

    QAction *fullscreenAction = actionCollection()->addAction("switch_fullscreen");
    fullscreenAction->setText(i18n("&Switch to Fullscreen Mode"));
    fullscreenAction->setIcon(KIcon("view-fullscreen"));
    connect(fullscreenAction, SIGNAL(triggered()), SLOT(slotSwitchFullscreen()));

    QAction *viewOnlyAction = actionCollection()->addAction("view_only");
    viewOnlyAction->setCheckable(true);
    viewOnlyAction->setText(i18n("&View Only"));
    viewOnlyAction->setIcon(KIcon("kgpg-sign-kgpg"));
    connect(viewOnlyAction, SIGNAL(triggered(bool)), SLOT(slotViewOnly(bool)));

    QAction *logoutAction = actionCollection()->addAction("logout");
    logoutAction->setText(i18n("&Log Out"));
    logoutAction->setIcon(KIcon("system-log-out"));
    connect(logoutAction, SIGNAL(triggered()), SLOT(slotLogout()));

    QAction *quitAction = KStandardAction::quit(this, SLOT(slotQuit()), actionCollection());
    actionCollection()->addAction("quit", quitAction);
    QAction *preferencesAction = KStandardAction::preferences(this, SLOT(slotPreferences()), actionCollection());
    actionCollection()->addAction("preferences", preferencesAction);
    QAction *configToolbarAction = KStandardAction::configureToolbars(this, SLOT(slotConfigureToolbars()), actionCollection());
    actionCollection()->addAction("configure_toolbars", configToolbarAction);
    QAction *keyBindingsAction = KStandardAction::keyBindings(this, SLOT(slotConfigureKeys()), actionCollection());
    actionCollection()->addAction("configure_keys", keyBindingsAction);
    QAction *cinfigNotifyAction = KStandardAction::configureNotifications(this, SLOT(slotConfigureNotifications()), actionCollection());
    actionCollection()->addAction("configure_notifications", cinfigNotifyAction);
    m_menubarAction = KStandardAction::showMenubar(this, SLOT(slotShowMenubar()), actionCollection());
    m_menubarAction->setChecked(!menuBar()->isHidden());
    actionCollection()->addAction("settings_showmenubar", m_menubarAction);

    m_addressNavigator = new KUrlNavigator(0, KUrl("vnc://"), this);
    m_addressNavigator->setCustomProtocols(QStringList() << "vnc" << "rdp");
    m_addressNavigator->setUrlEditable(Settings::normalUrlInputLine());
    connect(m_addressNavigator, SIGNAL(returnPressed()), SLOT(slotNewConnection()));

    QLabel *addressLabel = new QLabel(i18n("Remote desktop:"), this);

    QWidget *addressWidget = new QWidget(this);
    QHBoxLayout *addressLayout = new QHBoxLayout(addressWidget);
    addressLayout->setMargin(0);
    addressLayout->addWidget(addressLabel);
    addressLayout->addWidget(m_addressNavigator, 1);

    KAction *addressLineAction = new KAction(i18n("Address"), this);
    actionCollection()->addAction("address_line", addressLineAction);
    addressLineAction->setDefaultWidget(addressWidget);

    QAction *gotoAction = actionCollection()->addAction("goto_address");
    gotoAction->setText(i18n("&Goto address"));
    gotoAction->setIcon(KIcon("browser-go"));
    connect(gotoAction, SIGNAL(triggered()), SLOT(slotNewConnection()));
}

void MainWindow::slotNewConnection()
{
    KUrl url = m_addressNavigator->uncommittedUrl();

    if (!url.isValid()) {
        KMessageBox::error(this,
                           i18n("The entered address does not have the required form."),
                           i18n("Malformed URL"));
        return;
    }

    m_addressNavigator->setUrl(KUrl(url.scheme().toLower() + "://"));

    QScrollArea *scrollArea = createScrollArea(m_tabWidget, 0);

    RemoteView *view;

#ifdef BUILD_VNC
    if (url.scheme().toLower() == "vnc") {
        view = new VncView(scrollArea, url);
    }
    else
#endif

#ifdef BUILD_RDP
    if (url.scheme().toLower() == "rdp") {
        view = new RdpView(scrollArea, url);
    }
    else
#endif
    {
        KMessageBox::error(this,
                           i18n("The entered address cannot be handled."),
                           i18n("Unusable URL"));
        return;
    }

    connect(view, SIGNAL(changeSize(int, int)), this, SLOT(resizeTabWidget(int, int)));

    m_remoteViewList.append(view);

    view->resize(0, 0);
    view->start();

    scrollArea->setWidget(m_remoteViewList.at(m_tabWidget->count() - m_showStartPage ? 1 : 0));

    int newIndex = m_tabWidget->addTab(scrollArea, KIcon("krdc"), url.prettyUrl(KUrl::RemoveTrailingSlash));
    m_tabWidget->setCurrentIndex(newIndex);

    statusBar()->showMessage(i18n("Connected to %1 via %2", url.host(), url.scheme().toUpper()));

    updateActionStatus();
}

void MainWindow::resizeTabWidget(int w, int h)
{
    kDebug(5010) << "tabwidget resize: w: " << w << ", h: " << h << endl;

    if (m_topBottomBorder == 0) { // the values are not cached yet
        QScrollArea *tmp = qobject_cast<QScrollArea *>(m_tabWidget->currentWidget());

        m_leftRightBorder = m_tabWidget->width() - m_tabWidget->currentWidget()->width() + (2 * tmp->frameWidth());
        m_topBottomBorder = m_tabWidget->height() - m_tabWidget->currentWidget()->height() + (2 * tmp->frameWidth());

        kDebug(5010) << "tabwidget border: w: " << m_leftRightBorder << ", h: " << m_topBottomBorder << endl;
    }

    int newTabWidth = w + m_leftRightBorder;
    int newTabHeight = h + m_topBottomBorder;

    QSize newWindowSize = size() - m_tabWidget->size() + QSize(newTabWidth, newTabHeight);

    QSize desktopSize = QSize(QApplication::desktop()->availableGeometry().width(),
                              QApplication::desktop()->availableGeometry().height());

    if ((newWindowSize.height() >= desktopSize.height()) || (newWindowSize.width() >= desktopSize.width())) {
        kDebug(5010) << "remote desktop needs more place than available -> show window maximized" << endl;
        showMaximized();
        return;
    }

    //WORKAROUND: QTabWidget resize problem. Let's see if there is a clean solution for this issue.
    m_tabWidget->setMinimumSize(newTabWidth, newTabHeight);
    m_tabWidget->adjustSize();
    QCoreApplication::processEvents();
    m_tabWidget->setMinimumSize(500, 400);
}

void MainWindow::slotTakeScreenshot()
{
    QPixmap snapshot = QPixmap::grabWidget(m_remoteViewList.at(m_currentRemoteView));

    QApplication::clipboard()->setPixmap(snapshot);
}

void MainWindow::slotSwitchFullscreen()
{
    kDebug(5010) << "slotSwitchFullscreen" << endl;

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
            m_toolBar = 0;
        }

        actionCollection()->action("switch_fullscreen")->setIcon(KIcon("view-fullscreen"));
        actionCollection()->action("switch_fullscreen")->setText(i18n("Switch to Fullscreen Mode"));

        m_fullscreenWindow->deleteLater();

        m_fullscreenWindow = 0;
    } else {
        m_fullscreenWindow = new QWidget(this, Qt::Window);
        m_fullscreenWindow->setWindowTitle(i18n("KRDC Fullscreen"));

        QScrollArea *scrollArea = createScrollArea(m_fullscreenWindow, m_remoteViewList.at(m_currentRemoteView));
        scrollArea->setFrameShape(QFrame::NoFrame);

        QVBoxLayout *fullscreenLayout = new QVBoxLayout();
        fullscreenLayout->setMargin(0);
        fullscreenLayout->addWidget(scrollArea);
        m_fullscreenWindow->setLayout(fullscreenLayout);

        m_fullscreenWindow->show();

        m_fullscreenWindow->setWindowState(Qt::WindowFullScreen);

        // show the toolbar after we have switched to fullscreen mode
        QTimer::singleShot(100, this, SLOT(showRemoteViewToolbar()));

        m_mainWindowGeometry = saveGeometry();
        hide();
    }
}

QScrollArea *MainWindow::createScrollArea(QWidget *parent, RemoteView *remoteView)
{
    QScrollArea *scrollArea = new QScrollArea(parent);
    scrollArea->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    QPalette palette = scrollArea->palette();
    palette.setColor(QPalette::Dark, Settings::backgroundColor());
    scrollArea->setPalette(palette);

    scrollArea->setBackgroundRole(QPalette::Dark);
    scrollArea->setWidget(remoteView);

    return scrollArea;
}

void MainWindow::slotLogout()
{
    kDebug(5010) << "slotLogout" << endl;

    if (m_fullscreenWindow) { // first close fullscreen view
        slotSwitchFullscreen();
        slotLogout();
    }

    QWidget *tmp = m_tabWidget->currentWidget();

    m_remoteViewList.removeAt(m_currentRemoteView);

    m_tabWidget->removeTab(m_tabWidget->currentIndex());

    tmp->deleteLater();

    updateActionStatus();
}

void MainWindow::slotViewOnly(bool viewOnly)
{
    kDebug(5010) << "slotViewOnly" << endl;

    m_remoteViewList.at(m_currentRemoteView)->setViewOnly(viewOnly);
}

void MainWindow::showRemoteViewToolbar()
{
    kDebug(5010) << "showRemoteViewToolbar" << endl;

    m_remoteViewList.at(m_currentRemoteView)->repaint(); // be sure there are no artifacts on the remote view

    if (!m_toolBar) {
        actionCollection()->action("switch_fullscreen")->setIcon(KIcon("view-restore"));
        actionCollection()->action("switch_fullscreen")->setText(i18n("Switch to Window Mode"));

        m_toolBar = new FloatingToolBar(m_fullscreenWindow, m_fullscreenWindow);
        m_toolBar->setSide(FloatingToolBar::Top);
        m_toolBar->addAction(actionCollection()->action("switch_fullscreen"));
        m_toolBar->addAction(actionCollection()->action("take_screenshot"));
        m_toolBar->addAction(actionCollection()->action("view_only"));
        m_toolBar->addAction(actionCollection()->action("logout"));
    }

    m_toolBar->showAndAnimate();
}

void MainWindow::updateActionStatus()
{
    kDebug(5010) << "updateActionStatus = " << m_tabWidget->currentIndex() << endl;

    bool enabled;

    if ((m_showStartPage && (m_tabWidget->currentIndex() == 0)) ||
       (!m_showStartPage && (m_tabWidget->currentIndex() < 0)))
        enabled = false;
    else
        enabled = true;

    actionCollection()->action("switch_fullscreen")->setEnabled(enabled);
    actionCollection()->action("take_screenshot")->setEnabled(enabled);
    actionCollection()->action("view_only")->setEnabled(enabled);
    actionCollection()->action("logout")->setEnabled(enabled);
}

void MainWindow::slotPreferences()
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

    if (Settings::showStartPage() && !m_showStartPage)
        createStartPage();
    else if (m_showStartPage) {
        m_tabWidget->removeTab(0);
        m_showStartPage = false;
    }
    updateActionStatus();

    // update the scroll areas background color
    for (int i = m_showStartPage ? 1 : 0; i < m_tabWidget->count(); i++) {
        QPalette palette = m_tabWidget->widget(i)->palette();
        palette.setColor(QPalette::Dark, Settings::backgroundColor());
        m_tabWidget->widget(i)->setPalette(palette);
    }
}

void MainWindow::slotQuit()
{
    close();
}

void MainWindow::slotConfigureNotifications()
{
    KNotifyConfigWidget::configure(this);
}

void MainWindow::slotConfigureKeys()
{
    KShortcutsDialog::configure(actionCollection());
}

void MainWindow::slotConfigureToolbars()
{
    KEditToolBar edit(actionCollection());
    connect(&edit, SIGNAL(newToolbarConfig()), this, SLOT(slotNewToolbarConfig()));
    edit.exec();
}

void MainWindow::slotShowMenubar()
{
    if (m_menubarAction->isChecked())
        menuBar()->show();
    else
        menuBar()->hide();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (KMessageBox::warningYesNoCancel(this,
            i18n("Are you sure you want to close KRDC?"),
            i18n("Confirm Quit"),
            KStandardGuiItem::yes(), KStandardGuiItem::no(), KStandardGuiItem::cancel(),
            "AskBeforeExit") == KMessageBox::Yes) {

        Settings::self()->writeConfig();
        event->accept();
    } else
        event->ignore();
}

void MainWindow::tabChanged(int index)
{
    kDebug(5010) << "tabChanged: " << index << endl;

    updateActionStatus();

    m_currentRemoteView = index - m_showStartPage ? 1 : 0;
}

void MainWindow::createStartPage()
{
    m_showStartPage = true;

    QWidget *startWidget = new QWidget(this);

    QVBoxLayout *startLayout = new QVBoxLayout(startWidget);

    QLabel *headerLabel = new QLabel(this);
    headerLabel->setText(i18n("<h1>KRDC</h1><br />What would you like to do?"));

    QLabel *headerIconLabel = new QLabel(this);
    headerIconLabel->setPixmap(KIcon("krdc").pixmap(48));

    QHBoxLayout *headerLayout = new QHBoxLayout;
    headerLayout->setMargin(20);
    headerLayout->addWidget(headerLabel, 1, Qt::AlignTop);
    headerLayout->addWidget(headerIconLabel);

    KPushButton *vncConnectButton = new KPushButton(this);
    vncConnectButton->setStyleSheet("KPushButton { padding: 12px; margin: 10px; }");
    vncConnectButton->setIcon(KIcon(actionCollection()->action("new_vnc_connection")->icon()));
    vncConnectButton->setText(i18n("Connect to a remote Linux / Unix computer. (Using VNC)"));
    connect(vncConnectButton, SIGNAL(clicked()), SLOT(newVncConnection()));

    KPushButton *rdpConnectButton = new KPushButton(this);
    rdpConnectButton->setStyleSheet("KPushButton { padding: 12px; margin: 10px; }");
    rdpConnectButton->setIcon(KIcon(actionCollection()->action("new_rdp_connection")->icon()));
    rdpConnectButton->setText(i18n("Connect to a remote Windows computer. (Using RDP)"));
    connect(rdpConnectButton, SIGNAL(clicked()), SLOT(newRdpConnection()));

    startLayout->addLayout(headerLayout);
    startLayout->addWidget(vncConnectButton);
    startLayout->addWidget(rdpConnectButton);
    startLayout->addStretch();

    m_tabWidget->insertTab(0, startWidget, KIcon("krdc"), i18n("Start Page"));
}

void MainWindow::newVncConnection()
{
    m_addressNavigator->setUrl(KUrl("vnc://"));
    m_addressNavigator->setFocus();

    QToolTip::showText(m_addressNavigator->pos() + pos() + QPoint(m_addressNavigator->width(),
                                                                  m_addressNavigator->height() + 20),
                       i18n("Enter here the address.<br />"
                            "<i>Example: vncserver:1 (host:port / screen)</i>"), this);
}

void MainWindow::newRdpConnection()
{
    m_addressNavigator->setUrl(KUrl("rdp://"));
    m_addressNavigator->setFocus();

    QToolTip::showText(m_addressNavigator->pos() + pos() + QPoint(m_addressNavigator->width(),
                                                                  m_addressNavigator->height() + 20),
                       i18n("Enter here the address.<br />"
                            "<i>Example: rdpserver (host)</i>"), this);
}

#include "mainwindow.moc"
