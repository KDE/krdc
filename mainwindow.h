/*
    SPDX-FileCopyrightText: 2007-2013 Urs Wolfer <uwolfer@kde.org>
    SPDX-FileCopyrightText: 2009-2010 Tony Murray <murraytony@gmail.com>
    SPDX-FileCopyrightText: 2021 Rafał Lalik <rafallalik@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "remoteview.h"
#include "remoteviewfactory.h"

#include <KXmlGuiWindow>
#include <KPluginInfo>

class KComboBox;
class KLineEdit;
class KPushButton;
class KToggleAction;
class KTabWidget;

class BookmarkManager;
class FloatingToolBar;
class RemoteDesktopsModel;
class RemoteView;
class SystemTrayIcon;
class TabbedViewWidget;

class QScrollArea;
class QModelIndex;
class QTableView;

#ifdef TELEPATHY_SUPPORT
class TubesManager;
#endif

class MainWindow : public KXmlGuiWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    QMap<QWidget *, RemoteView *> remoteViewList() const;
    QList<RemoteViewFactory *> remoteViewFactoriesList() const;
    RemoteView* currentRemoteView() const;

public Q_SLOTS:
    void newConnection(const QUrl &newUrl = QUrl(), bool switchFullscreenWhenConnected = false, const QString &tabName = QString());
    void setFactor(int scale);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override; // checks for close events on fs window
    void saveProperties(KConfigGroup &group) override;
    void saveHostPrefs();
    void saveHostPrefs(RemoteView *view);

private Q_SLOTS:
    void restoreOpenSessions();
    void quit(bool systemEvent = false);
    void preferences();
    void configureNotifications();
    void showMenubar();
    void resizeTabWidget(int w, int h);
    void statusChanged(RemoteView::RemoteStatus status);
    void showRemoteViewToolbar();
    void takeScreenshot();
    void switchFullscreen();
    void disconnectHost();
    void closeTab(int index);
    void openTabSettings(int index);
    void tabContextMenu(const QPoint &point);
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
    void selectFromRemoteDesktopsModel(const QModelIndex &index);
    void createDockWidget();
    void showConnectionContextMenu(const QPoint &pos);
    void saveConnectionListSort(const int logicalindex, const Qt::SortOrder order);

private:
    void setupActions();
    void loadAllPlugins();
    void showSettingsDialog(const QString &url);
    QScrollArea *createScrollArea(QWidget *parent, RemoteView *remoteView);
    QUrl getInputUrl();

    QWidget *m_fullscreenWindow;
    QByteArray m_mainWindowGeometry;

    KToggleAction *m_menubarAction;
    TabbedViewWidget *m_tabWidget;
    KComboBox *m_protocolInput;
    KLineEdit *m_addressInput;

    FloatingToolBar *m_toolBar;

    BookmarkManager *m_bookmarkManager;

    QMap<QWidget *, RemoteView *> m_remoteViewMap;
    QMap<int, RemoteViewFactory *> m_remoteViewFactories;

    int m_currentRemoteView;
    bool m_switchFullscreenWhenConnected;

    SystemTrayIcon *m_systemTrayIcon;
    QTableView *m_dockWidgetTableView;
    QTableView *m_newConnectionTableView;
    RemoteDesktopsModel *m_remoteDesktopsModel;
    QWidget *m_newConnectionWidget;

Q_SIGNALS:
    void scaleUpdated(bool scale);  // scale state has changed
    void factorUpdated(int factor); // factor havlue has changed
};

#include <QApplication>
#include <QDesktopWidget>
#include <QMouseEvent>

class MinimizePixel : public QWidget
{
    Q_OBJECT
public:
    explicit MinimizePixel(QWidget *parent)
            : QWidget(parent) {
        setFixedSize(1, 1);
        move(QApplication::desktop()->screenGeometry().width() - 1, 0);
    }

Q_SIGNALS:
    void rightClicked();

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::RightButton)
            Q_EMIT rightClicked();
    }
};

#include <QScrollArea>

class RemoteViewScrollArea : public QScrollArea
{
    Q_OBJECT
public:
    explicit RemoteViewScrollArea(QWidget *parent)
            : QScrollArea(parent) {
    }

Q_SIGNALS:
    void resized(int w, int h);

protected:
    void resizeEvent(QResizeEvent *event) override {
        QScrollArea::resizeEvent(event);
        Q_EMIT resized(width() - 2*frameWidth(), height() - 2*frameWidth());
    }
};

#endif
