/*
    SPDX-FileCopyrightText: 2009 Tony Murray <murraytony@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tabbedviewwidget.h"
#include "krdc_debug.h"

#include <QTabBar>

TabbedViewWidgetModel::TabbedViewWidgetModel(QTabWidget *modelTarget)
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

QModelIndex TabbedViewWidgetModel::parent(const QModelIndex &) const
{
    return QModelIndex();
}

int TabbedViewWidgetModel::columnCount(const QModelIndex &) const
{
    return 1;
}

int TabbedViewWidgetModel::rowCount(const QModelIndex &) const
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
        Q_EMIT dataChanged(index, index);
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
        return m_tabWidget->tabText(index.row()).remove(QRegExp(QLatin1String("&(?!&)"))); //remove accelerator string
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
    Q_EMIT layoutAboutToBeChanged();
}

void TabbedViewWidgetModel::emitLayoutChanged()
{
    Q_EMIT layoutChanged();
}

void TabbedViewWidgetModel::emitDataChanged(int index)
{
    QModelIndex modelIndex = createIndex(index, 1);
    Q_EMIT dataChanged(modelIndex, modelIndex);
}

TabbedViewWidget::TabbedViewWidget(QWidget *parent)
        : QTabWidget(parent), m_model(new TabbedViewWidgetModel(this))
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
    int count = QTabWidget::count();
    m_model->beginInsertRows(QModelIndex(), count, count);
    int ret = QTabWidget::addTab(page, label);
    m_model->endInsertRows();
    return ret;
}

int TabbedViewWidget::addTab(QWidget *page, const QIcon &icon, const QString &label)
{
    int count = QTabWidget::count();
    m_model->beginInsertRows(QModelIndex(), count, count);
    int ret = QTabWidget::addTab(page, icon, label);
    m_model->endInsertRows();
    return ret;
}

int TabbedViewWidget::insertTab(int index, QWidget *page, const QString &label)
{
    m_model->beginInsertRows(QModelIndex(), index, index);
    int ret = QTabWidget::insertTab(index, page, label);
    m_model->endInsertRows();
    return ret;
}

int TabbedViewWidget::insertTab(int index, QWidget *page, const QIcon &icon, const QString &label)
{
    m_model->beginInsertRows(QModelIndex(), index, index);
    int ret = QTabWidget::insertTab(index, page, icon, label);
    m_model->endInsertRows();
    return ret;
}

void TabbedViewWidget::removePage(QWidget *page)
{
    int index = QTabWidget::indexOf(page);
    if (index == -1) {
        return;
    }
    m_model->beginRemoveRows(QModelIndex(), index, index);
    QTabWidget::removeTab(index);
    m_model->endRemoveRows();
}

void TabbedViewWidget::removeTab(int index)
{
    m_model->beginRemoveRows(QModelIndex(), index, index);
    QTabWidget::removeTab(index);
    m_model->endRemoveRows();
}

void TabbedViewWidget::moveTab(int from, int to)
{
    m_model->emitLayoutAboutToBeChanged();
    tabBar()->moveTab(from, to);
    m_model->emitLayoutChanged();
}

void TabbedViewWidget::setTabText(int index, const QString &label)
{
    QTabWidget::setTabText(index, label);
    m_model->emitDataChanged(index);
}

//This functionality is taken from  KTabWidget for compatibility.
//KTabWidget has been moved to KdeLibs4Support and QTabWidget::tabBarDoubleClicked does not
//work on empty space after tabs,
bool TabbedViewWidget::isEmptyTabbarSpace(const QPoint &point) const
{
    if (count() == 0) {
        return true;
    }
    if (tabBar()->isHidden()) {
        return false;
    }
    QSize size(tabBar()->sizeHint());
    if ((tabPosition() == QTabWidget::North && point.y() < size.height()) ||
            (tabPosition() == QTabWidget::South && point.y() > (height() - size.height()))) {

        QWidget *rightcorner = cornerWidget(Qt::TopRightCorner);
        if (rightcorner && rightcorner->isVisible()) {
            if (point.x() >= width() - rightcorner->width()) {
                return false;
            }
        }

        QWidget *leftcorner = cornerWidget(Qt::TopLeftCorner);
        if (leftcorner && leftcorner->isVisible()) {
            if (point.x() <= leftcorner->width()) {
                return false;
            }
        }

        for (int i = 0; i < count(); ++i)
            if (tabBar()->tabRect(i).contains(tabBar()->mapFromParent(point))) {
                return false;
            }

        return true;
    }

    return false;
}

void TabbedViewWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }

    if (isEmptyTabbarSpace(event->pos())) {
        Q_EMIT tabBarDoubleClicked(-1);
        return;
    }

    QTabWidget::mouseDoubleClickEvent(event);
}

void TabbedViewWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        if (isEmptyTabbarSpace(event->pos())) {
            Q_EMIT mouseMiddleClick(-1);
            return;
        }
        int pos = tabBar()->tabAt(event->pos());
        if(pos != -1){
            Q_EMIT mouseMiddleClick(pos);
            return;
        }
    }

    QTabWidget::mouseReleaseEvent(event);
}

