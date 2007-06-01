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
** You should have received a copy of the GNU Library General Public License
** along with this library; see the file COPYING.LIB. If not, write to
** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
** Boston, MA 02110-1301, USA.
**
****************************************************************************/

#include "mainwindow.h"

#include "rdpview.h"
#include "vncview.h"

#include <KAction>
#include <KActionCollection>
#include <KApplication>
#include <KEditToolBar>
#include <KHistoryComboBox>
#include <KIcon>
#include <KLocale>
#include <KNotifyConfigWidget>
#include <KMenuBar>
#include <KShortcut>
#include <KShortcutsDialog>
#include <KStatusBar>
#include <KTabWidget>
#include <KToggleAction>

#include <QLabel>
#include <QLayout>
#include <QX11EmbedContainer>
#include <QToolButton>
#include <QScrollArea>

MainWindow::MainWindow(QWidget *parent)
  : KXmlGuiWindow(parent)
{
    setupActions();

    createGUI("krdcui.rc");

    setStandardToolBarMenuEnabled(true);

    m_tabWidget = new KTabWidget(this);
    m_tabWidget->setMinimumSize(400, 300);
    setCentralWidget(m_tabWidget);

    statusBar()->showMessage(i18n("KRDC started"));
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupActions()
{
    QAction *newDownloadAction = actionCollection()->addAction("new_connection");
    newDownloadAction->setText(i18n("&New Connection..."));
    newDownloadAction->setIcon(KIcon("document-new"));
    newDownloadAction->setShortcuts(KShortcut("Ctrl+N"));
    connect(newDownloadAction, SIGNAL(triggered()), SLOT(slotNewConnection()));

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


    m_addressComboBox = new KHistoryComboBox(this);

    connect(m_addressComboBox, SIGNAL(returnPressed()), this, SLOT(slotNewConnection()));

    QLabel *addressLabel = new QLabel(i18n("Remote desktop:"), this);

    QWidget *addressWidget = new QWidget(this);
    QHBoxLayout *addressLayout = new QHBoxLayout(addressWidget);
    addressLayout->setMargin(0);
    addressLayout->addWidget(addressLabel);
    addressLayout->addWidget(m_addressComboBox, 1);

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
    QUrl url = m_addressComboBox->currentText();
    m_addressComboBox->clear();

    if (url.isEmpty())
        return;

    QScrollArea *scrollArea = new QScrollArea(m_tabWidget);
    scrollArea->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    scrollArea->setBackgroundRole(QPalette::Dark);

    RemoteView *view;

#ifdef BUILDVNC
    if (url.scheme().toLower() == "vnc")
        view = new VncView(scrollArea, url.host(), url.port());
    else
#endif

    if (url.scheme().toLower() == "rdp")
        view = new RdpView(scrollArea, url.host(), url.port());
    else
        return;

    connect(view, SIGNAL(changeSize(int, int)), this, SLOT(resizeTabWidget(int, int)));

    view->resize(0, 0);
    view->start();

    scrollArea->setWidget(view);

    m_tabWidget->addTab(scrollArea, KIcon("krdc"), url.toString());

    statusBar()->showMessage(i18n("Connected to %1 via %2", url.host(), url.scheme().toUpper()));
}

void MainWindow::resizeTabWidget(int w, int h)
{
    kDebug(5010) << "tabwidget resize: w: " << w << ", h: " << h << endl;

    //WORKAROUND: QTabWidget resize problem. Let's see if there is a clean solution for this issue.
    m_tabWidget->setMinimumSize(w + 8, h + 38); // FIXME: do not use hardcoded values
    m_tabWidget->adjustSize();
    QCoreApplication::processEvents();
    m_tabWidget->setMinimumSize(400, 300);
}

void MainWindow::slotPreferences()
{
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

#include "mainwindow.moc"
