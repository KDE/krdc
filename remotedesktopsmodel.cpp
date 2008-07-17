/****************************************************************************
**
** Copyright (C) 2008 Urs Wolfer <uwolfer @ kde.org>
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

#include "remotedesktopsmodel.h"

#include "remotedesktopsitem.h"

#include <KBookmarkManager>
#include <KIcon>
#include <KStandardDirs>
#include <KDebug>
#include <KLocale>
#include <KProcess>

RemoteDesktopsModel::RemoteDesktopsModel(QObject *parent)
        : QAbstractItemModel(parent)
{
    rootItem = new RemoteDesktopsItem(QList<QVariant>() << "Remote desktops");

    QString file = KStandardDirs::locateLocal("data", "krdc/bookmarks.xml");

    m_manager = KBookmarkManager::managerForFile(file, "krdc");
    m_manager->setUpdate(true);
    connect(m_manager, SIGNAL(changed(const QString &, const QString &)), SLOT(changed()));

    buildModelFromBookmarkGroup(m_manager->root(), rootItem);

#if 0
    localNetworkItem = new RemoteDesktopsItem(QList<QVariant>() << "Local Network", rootItem);
    rootItem->appendChild(localNetworkItem);

    scanLocalNetwork();

    localNetworkItem->appendChild(new RemoteDesktopsItem(QList<QVariant>() << "...", localNetworkItem));
#endif
}

RemoteDesktopsModel::~RemoteDesktopsModel()
{
}

void RemoteDesktopsModel::changed()
{
    kDebug(5010);
    rootItem->clearChildren();
    buildModelFromBookmarkGroup(m_manager->root(), rootItem);
    reset();
}

int RemoteDesktopsModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return static_cast<RemoteDesktopsItem*>(parent.internalPointer())->columnCount();
    else
        return rootItem->columnCount();
}

QVariant RemoteDesktopsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    RemoteDesktopsItem *item = static_cast<RemoteDesktopsItem*>(index.internalPointer());

    switch (role) {
    case Qt::DisplayRole:
        return item->data(index.column());
    case Qt::DecorationRole:
        if (item->data(index.column()).toString().contains("://")) //TODO: clean impl
            return KIcon("krdc");
#if 0
        else if (item->data(index.column()).toString() == "Local Network") //TODO: clean impl
            return KIcon("network-workgroup");
#endif
        else if (item->data(index.column()).toString() == "...") //TODO: clean impl
            return KIcon("view-history");
        else
            return KIcon("folder-bookmarks");
    case Qt::UserRole:
        if (item->data(index.column() + 1).toString().contains("://"))
            return QVariant(item->data(index.column() + 1).toString());
        else
            return QVariant();
    default:
        return QVariant();
    }
}

Qt::ItemFlags RemoteDesktopsModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant RemoteDesktopsModel::headerData(int section, Qt::Orientation orientation,
                                         int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return rootItem->data(section);

    return QVariant();
}

QModelIndex RemoteDesktopsModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    RemoteDesktopsItem *parentItem;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<RemoteDesktopsItem*>(parent.internalPointer());

    RemoteDesktopsItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QModelIndex RemoteDesktopsModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    RemoteDesktopsItem *childItem = static_cast<RemoteDesktopsItem*>(index.internalPointer());
    RemoteDesktopsItem *parentItem = childItem->parent();

    if (parentItem == rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int RemoteDesktopsModel::rowCount(const QModelIndex &parent) const
{
    RemoteDesktopsItem *parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<RemoteDesktopsItem*>(parent.internalPointer());

    return parentItem->childCount();
}

void RemoteDesktopsModel::buildModelFromBookmarkGroup(const KBookmarkGroup &group, RemoteDesktopsItem *item)
{
    KBookmark bm = group.first();
    while (!bm.isNull()) {
        RemoteDesktopsItem *newItem = new RemoteDesktopsItem(QList<QVariant>() << bm.text() << bm.url().url(), item);
        item->appendChild(newItem);
        if (bm.isGroup())
            buildModelFromBookmarkGroup(bm.toGroup(), newItem); //recursive
        bm = group.next(bm);
    }
}

#if 0
void RemoteDesktopsModel::scanLocalNetwork()
{
    m_scanProcess = new KProcess(this);
    m_scanProcess->setOutputChannelMode(KProcess::SeparateChannels);
    QStringList args(QStringList() << "-vv" << "-PN" << "-p5901" << "192.168.1.0-255");
    connect(m_scanProcess, SIGNAL(readyReadStandardOutput()),
                           SLOT(readInput()));
    m_scanProcess->setProgram("nmap", args);
    m_scanProcess->start();
}

void RemoteDesktopsModel::readInput()
{
    // we do not know if the output array ends in the middle of an utf-8 sequence
    m_output += m_scanProcess->readAllStandardOutput();

    int pos;
    while ((pos = m_output.indexOf('\n')) != -1) {
        QString line = QString::fromLocal8Bit(m_output, pos + 1);
        m_output.remove(0, pos + 1);

        if (line.contains("open port")) {
            kDebug(5010) << line;

            QString ip(line.mid(line.lastIndexOf(' ') + 1));
            ip = ip.left(ip.length() - 1);

            QString port(line.left(line.indexOf('/')));
            port = port.mid(port.lastIndexOf(' ') + 1);
            RemoteDesktopsItem *item = new RemoteDesktopsItem(QList<QVariant>() << "vnc://" + ip + ':' + port, localNetworkItem);
            localNetworkItem->appendChild(item);
            emit dataChanged(QModelIndex(), QModelIndex());
        }
    }
}
#endif

#include "remotedesktopsmodel.moc"
