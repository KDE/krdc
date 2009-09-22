/****************************************************************************
**
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

#ifndef TABBEDVIEWWIDGET_H
#define TABBEDVIEWWIDGET_H

#include <QAbstractItemModel>
#include <KTabWidget>

class TabbedViewWidgetModel : public QAbstractItemModel
{
    friend class TabbedViewWidget;
    Q_OBJECT
public:
    TabbedViewWidgetModel(KTabWidget *modelTarget);
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);
    QVariant data(const QModelIndex &index, int role) const;
protected:
    void emitLayoutAboutToBeChanged();
    void emitLayoutChanged();
    void emitDataChanged(int index);
private:
    QTabWidget *m_tabWidget;
};

class TabbedViewWidget : public KTabWidget
{
    Q_OBJECT
public:
    TabbedViewWidget(QWidget *parent = 0, Qt::WFlags flags = 0);
    virtual ~TabbedViewWidget();
    TabbedViewWidgetModel* getModel();
    int addTab(QWidget *page, const QString &label);
    int addTab(QWidget *page, const QIcon &icon, const QString &label);
    int insertTab(int index, QWidget *page, const QString &label);
    int insertTab(int index, QWidget *page, const QIcon &icon, const QString &label);
    void removeTab(int index);
    void removePage(QWidget *page);
    void moveTab(int from, int to);
    void setTabText(int index, const QString &label);
private:
    TabbedViewWidgetModel *m_model;
};

#endif // FULLSCREENWINDOW_H

