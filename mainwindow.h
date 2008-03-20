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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "remoteview.h"

#include <KXmlGuiWindow>

class KToggleAction;
class KTabWidget;
class KUrlNavigator;

class BookmarkManager;
class FloatingToolBar;
class RemoteView;
class SystemTrayIcon;
class QScrollArea;

class MainWindow : public KXmlGuiWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

    QList<RemoteView *> remoteViewList() const;
    int currentRemoteView() const;

public slots:
    void newConnection(const KUrl &newUrl = KUrl(), bool switchFullscreenWhenConnected = false);

protected:
    virtual void closeEvent(QCloseEvent *event);

private slots:
    void restoreOpenSessions();
    void quit();
    void preferences();
    void configureNotifications();
    void configureKeys();
    void configureToolbars();
    void showMenubar();
    void resizeTabWidget(int w, int h);
    void statusChanged(RemoteView::RemoteStatus status);
    void showRemoteViewToolbar();
    void takeScreenshot();
    void switchFullscreen();
    void logout();
    void viewOnly(bool viewOnly);
    void showLocalCursor(bool showLocalCursor);
    void grabAllKeys(bool grabAllKeys);
    void updateActionStatus();
    void updateConfiguration();
    void tabChanged(int index);
    void createStartPage();
    void newVncConnection();
    void newNxConnection();
    void newRdpConnection();
    void createZeroconfPage();
    void closeZeroconfPage();

private:
    void setupActions();
    QScrollArea *createScrollArea(QWidget *parent, RemoteView *remoteView);

    QWidget *m_fullscreenWindow;
    QByteArray m_mainWindowGeometry;

    KToggleAction *m_menubarAction;
    KTabWidget *m_tabWidget;
    KUrlNavigator *m_addressNavigator;

    FloatingToolBar *m_toolBar;

    BookmarkManager *m_bookmarkManager;

    QList<RemoteView *> m_remoteViewList;

    int m_topBottomBorder; // tabwidget borders
    int m_leftRightBorder;

    int m_currentRemoteView;
    bool m_showStartPage;
    bool m_showZeroconfPage;
    int m_zeroconfTabIndex;
    bool m_switchFullscreenWhenConnected;

    SystemTrayIcon *m_systemTrayIcon;
};

#include <QApplication>
#include <QDesktopWidget>
#include <QMouseEvent>

class MinimizePixel : public QWidget
{
    Q_OBJECT
public:
    MinimizePixel(QWidget *parent)
            : QWidget(parent) {
        setFixedSize(1, 1);
        move(QApplication::desktop()->screenGeometry().width() - 1, 0);
    }

signals:
    void rightClicked();

protected:
    void mousePressEvent(QMouseEvent *event) {
        if (event->button() == Qt::RightButton)
            emit rightClicked();
    }
};

#endif
