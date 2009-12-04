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

#include "tabbedviewwidget.h"

TabbedViewWidgetModel::TabbedViewWidgetModel(KTabWidget *modelTarget)
        : QAbstractItemModel(modelTarget), m_tabWidget(modelTarget)
{
}

QModelIndex TabbedViewWidgetModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }
    return createIndex(row, column, m_tabWidget->widget(row));
}

QModelIndex TabbedViewWidgetModel::parent(const QModelIndex &index) const
{
    return QModelIndex();
}

int TabbedViewWidgetModel::columnCount(const QModelIndex &parent) const
{
    return 1;
}


int TabbedViewWidgetModel::rowCount(const QModelIndex &parent) const
{
    return m_tabWidget->count();
}

Qt::ItemFlags TabbedViewWidgetModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::ItemIsEnabled;
    }
    return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
}

bool TabbedViewWidgetModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.isValid() && role == Qt::EditRole) {
        m_tabWidget->setTabText(index.row(), value.toString());
        emit dataChanged(index, index);
        return true;
    }
    return false;
}

QVariant TabbedViewWidgetModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    switch (role) {
    case Qt::EditRole:
    case Qt::DisplayRole:
        return m_tabWidget->tabText(index.row()).remove(QRegExp("&(?!&)")); //remove accelerator string
    case Qt::ToolTipRole:
        return m_tabWidget->tabToolTip(index.row());
    case Qt::DecorationRole:
        return m_tabWidget->tabIcon(index.row());
    default:
        return QVariant();
    }
}

void TabbedViewWidgetModel::emitLayoutAboutToBeChanged()
{
    emit layoutAboutToBeChanged();
}

void TabbedViewWidgetModel::emitLayoutChanged()
{
    emit layoutChanged();
}

void TabbedViewWidgetModel::emitDataChanged(int index)
{
    QModelIndex modelIndex = createIndex(index,1);
    emit dataChanged(modelIndex, modelIndex);
}

TabbedViewWidget::TabbedViewWidget(QWidget *parent, Qt::WFlags flags)
        : KTabWidget(parent, flags), m_model(new TabbedViewWidgetModel(this))
{
}

TabbedViewWidget::~TabbedViewWidget()
{
}

TabbedViewWidgetModel* TabbedViewWidget::getModel()
{
    return m_model;
}

int TabbedViewWidget::addTab(QWidget *page, const QString &label)
{
    int count = KTabWidget::count();
    m_model->beginInsertRows(QModelIndex(), count, count);
    int ret = KTabWidget::addTab(page, label);
    m_model->endInsertRows();
    return ret;
}

int TabbedViewWidget::addTab(QWidget *page, const QIcon &icon, const QString &label)
{
    int count = KTabWidget::count();
    m_model->beginInsertRows(QModelIndex(), count, count);
    int ret = KTabWidget::addTab(page, icon, label);
    m_model->endInsertRows();
    return ret;
}

int TabbedViewWidget::insertTab(int index, QWidget *page, const QString &label)
{
    m_model->beginInsertRows(QModelIndex(), index, index);
    int ret = KTabWidget::insertTab(index, page, label);
    m_model->endInsertRows();
    return ret;
}

int TabbedViewWidget::insertTab(int index, QWidget *page, const QIcon &icon, const QString &label)
{
    m_model->beginInsertRows(QModelIndex(), index, index);
    int ret = KTabWidget::insertTab(index, page, icon, label);
    m_model->endInsertRows();
    return ret;
}

void TabbedViewWidget::removePage(QWidget *page)
{
    int index = KTabWidget::indexOf(page);
    m_model->beginRemoveRows(QModelIndex(), index, index);
    KTabWidget::removePage(page);
    m_model->endRemoveRows();
}

void TabbedViewWidget::removeTab(int index)
{
    m_model->beginRemoveRows(QModelIndex(), index, index);
    KTabWidget::removeTab(index);
    m_model->endRemoveRows();
}

void TabbedViewWidget::moveTab(int from, int to)
{
    m_model->emitLayoutAboutToBeChanged();
    KTabWidget::moveTab(from, to);
    m_model->emitLayoutChanged();
}

void TabbedViewWidget::setTabText(int index, const QString &label)
{
    KTabWidget::setTabText(index, label);
    m_model->emitDataChanged(index);
}

#include "tabbedviewwidget.moc"
