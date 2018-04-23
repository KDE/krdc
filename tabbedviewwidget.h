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
#include <QTabWidget>
#include <QMouseEvent>

class TabbedViewWidgetModel : public QAbstractItemModel
{
    friend class TabbedViewWidget;
    Q_OBJECT
public:
    explicit TabbedViewWidgetModel(QTabWidget *modelTarget);
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QVariant data(const QModelIndex &index, int role) const override;
protected:
    void emitLayoutAboutToBeChanged();
    void emitLayoutChanged();
    void emitDataChanged(int index);

private:
    QTabWidget *m_tabWidget;
};

class TabbedViewWidget : public QTabWidget
{
    Q_OBJECT
public:
    explicit TabbedViewWidget(QWidget *parent = nullptr);
    ~TabbedViewWidget() override;
    TabbedViewWidgetModel* getModel();
    int addTab(QWidget *page, const QString &label);
    int addTab(QWidget *page, const QIcon &icon, const QString &label);
    int insertTab(int index, QWidget *page, const QString &label);
    int insertTab(int index, QWidget *page, const QIcon &icon, const QString &label);
    void removeTab(int index);
    void removePage(QWidget *page);
    void moveTab(int from, int to);
    void setTabText(int index, const QString &label);

Q_SIGNALS:
    void mouseMiddleClick(int index);

protected:
    void mouseDoubleClickEvent(QMouseEvent * event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *) Q_DECL_OVERRIDE;

private:
    TabbedViewWidgetModel *m_model;
    bool isEmptyTabbarSpace(const QPoint &point) const;
};

#endif // FULLSCREENWINDOW_H

