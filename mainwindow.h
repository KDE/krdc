/****************************************************************************
**
** Copyright (C) 2007 - 2008 Urs Wolfer <uwolfer @ kde.org>
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "remoteview.h"
#include "remoteviewfactory.h"
#include "tabbedviewwidget.h"

#include <KService>
#include <KXmlGuiWindow>

#ifdef TELEPATHY_SUPPORT
#include <TelepathyQt4/ClientRegistrar>
#endif

class KComboBox;
class KLineEdit;
class KPushButton;
class KToggleAction;
class KTabWidget;

class BookmarkManager;
class FloatingToolBar;
class RemoteView;
class SystemTrayIcon;
class TabbedViewWidget;

class QScrollArea;
class QModelIndex;
class QSortFilterProxyModel;
class QTableView;

#ifdef TELEPATHY_SUPPORT
class TubesManager;
#endif

class MainWindow : public KXmlGuiWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

    QList<RemoteView *> remoteViewList() const;
    QList<RemoteViewFactory *> remoteViewFactoriesList() const;
    int currentRemoteView() const;

public slots:
    void newConnection(const KUrl &newUrl = KUrl(), bool switchFullscreenWhenConnected = false, const QString &tabName = QString());

protected:
    virtual void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event); // checks for WM fullscreen events
    virtual void saveProperties(KConfigGroup &group);
    void saveHostPrefs(RemoteView *view = 0);

private slots:
    void restoreOpenSessions();
    void quit(bool systemEvent = false);
    void preferences();
    void configureNotifications();
    void configureKeys();
    void configureToolbars();
    void showMenubar();
    void resizeTabWidget(int w, int h);
    void statusChanged(RemoteView::RemoteStatus status);
    void showRemoteViewToolbar();
    void takeScreenshot();
    void switchFullScreen(bool makeFullScreen);
    void fullScreenChanged();
    void disconnectHost();
    void closeTab(QWidget *widget);
    void openTabSettings(QWidget *widget);
    void tabContextMenu(QWidget *widget, const QPoint &point);
    void viewOnly(bool viewOnly);
    void showLocalCursor(bool showLocalCursor);
    void grabAllKeys(bool grabAllKeys);
    void scale(bool scale);
    void updateActionStatus();
    void updateConfiguration();
    void tabChanged(int index);
    QWidget* newConnectionWidget();
    void newConnectionPage(bool clearInput = true);
    void openFromRemoteDesktopsModel(const QModelIndex &index);
    void createDockWidget();
    void showConnectionContextMenu(const QPoint &pos);
    void saveConnectionListSort(const int logicalindex, const Qt::SortOrder order);

private:
    void setupActions();
    void loadAllPlugins();
    RemoteViewFactory *createPluginFromService(const KService::Ptr &service);
    void showSettingsDialog(const QString &url);
    QScrollArea *createScrollArea(QWidget *parent, RemoteView *remoteView);
    KUrl getInputUrl();

    bool m_saveSettingsDisabled;

    KToggleAction *m_menubarAction;
    TabbedViewWidget *m_tabWidget;
    KComboBox *m_protocolInput;
    KLineEdit *m_addressInput;

    FloatingToolBar *m_toolBar;

    BookmarkManager *m_bookmarkManager;

    QList<RemoteView *> m_remoteViewList;
    QMap<int, RemoteViewFactory *> m_remoteViewFactories;

    int m_currentRemoteView;
    bool m_switchFullscreenWhenConnected;

    SystemTrayIcon *m_systemTrayIcon;
    QTableView *m_dockWidgetTableView;
    QTableView *m_newConnectionTableView;
    QSortFilterProxyModel *m_remoteDesktopsModelProxy;
#ifdef TELEPATHY_SUPPORT
    Tp::SharedPtr<TubesManager> m_tubesManager;
    Tp::ClientRegistrarPtr m_registrar;
#endif
    QWidget *m_newConnectionWidget;
};

#include <QApplication>
#include <QDesktopWidget>
#include <QMouseEvent>
#include <KDebug>

class MinimizePixel : public QWidget
{
    Q_OBJECT
public:
    MinimizePixel(QWidget *parent)
            : QWidget(parent) {
        setFixedSize(1, 1);
        move(0, 0);
        kDebug(5010) << parent->width() << parent->x() << parent->y();
//        move(QApplication::desktop()->screenGeometry().width() - 1, 0);
    }

signals:
    void rightClicked();

protected:
    void mousePressEvent(QMouseEvent *event) {
        if (event->button() == Qt::RightButton)
            emit rightClicked();
    }
};

#include <QScrollArea>

class RemoteViewScrollArea : public QScrollArea
{
    Q_OBJECT
public:
    RemoteViewScrollArea(QWidget *parent)
            : QScrollArea(parent) {
    }

signals:
    void resized(int w, int h);

protected:
    void resizeEvent(QResizeEvent *event) {
        QScrollArea::resizeEvent(event);
        emit resized(width() - 2*frameWidth(), height() - 2*frameWidth());
    }
};

#endif
