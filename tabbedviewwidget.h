/*
    SPDX-FileCopyrightText: 2009 Tony Murray <murraytony@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TABBEDVIEWWIDGET_H
#define TABBEDVIEWWIDGET_H

#include <QAbstractItemModel>
#include <QMouseEvent>
#include <QTabWidget>

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
    TabbedViewWidgetModel *getModel();
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
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    TabbedViewWidgetModel *m_model;
    bool isEmptyTabbarSpace(const QPoint &point) const;
};

#endif // FULLSCREENWINDOW_H
