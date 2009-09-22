/****************************************************************************
**
** Copyright (C) 2008 Urs Wolfer <uwolfer @ kde.org>
** Copyright (C) 2009 Tony Murray <murraytony@gmail.com>
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

#ifdef BUILD_ZEROCONF
#include <dnssd/remoteservice.h>
#endif

RemoteDesktopsModel::RemoteDesktopsModel(QObject *parent)
        : QAbstractItemModel(parent)
{
    rootItem = new RemoteDesktopsItem(QList<QVariant>() << "Remote Desktops");
    bookmarkItem = new RemoteDesktopsItem(QList<QVariant>() << "Bookmarks", rootItem);
    rootItem->appendChild(bookmarkItem);

    const QString file = KStandardDirs::locateLocal("data", "krdc/bookmarks.xml");

    m_manager = KBookmarkManager::managerForFile(file, "krdc");
    m_manager->setUpdate(true);
    connect(m_manager, SIGNAL(changed(const QString &, const QString &)), SLOT(bookmarksChanged()));

    buildModelFromBookmarkGroup(m_manager->root(), bookmarkItem);

#ifdef BUILD_ZEROCONF
    // Add RDP and NX if they start announcing via Zeroconf:
    m_protocols["_rfb._tcp"] = "vnc";

    zeroconfItem = new RemoteDesktopsItem(QList<QVariant>() << "Discovered Network Servers", rootItem);
    rootItem->appendChild(zeroconfItem);

    zeroconfBrowser = new DNSSD::ServiceBrowser("_rfb._tcp", true);
    connect(zeroconfBrowser, SIGNAL(finished()), this, SLOT(servicesChanged()));
    zeroconfBrowser->startBrowse();
#endif
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

void RemoteDesktopsModel::bookmarksChanged()
{
    kDebug(5010);

    bookmarkItem->clearChildren();
    buildModelFromBookmarkGroup(m_manager->root(), bookmarkItem);
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
    const QString currentItemTitleString = item->data(0).toString();
    const QString currentItemString = item->data(1).toString();

#ifdef BUILD_ZEROCONF
    if (currentItemTitleString == "Discovered Network Servers" && zeroconfItem->childCount()<=0)
        return QVariant();
#endif

    switch (role) {
    case Qt::DisplayRole:
        return item->data(index.column());
    case Qt::ToolTipRole:
        return currentItemString;  //use the url for the tooltip
    case Qt::DecorationRole:
        if (!currentItemString.isEmpty()) // contains an url
            return KIcon("krdc");
#ifdef BUILD_ZEROCONF
        else if (currentItemTitleString == "Discovered Network Servers")
            return KIcon("network-workgroup");
#endif
#if 0
        else if (currentItemTitleString == "Local Network")
            return KIcon("network-workgroup");
        else if (currentItemTitleString == "...")
            return KIcon("view-history");
#endif
        else
            return KIcon("folder-bookmarks");
    case 10001: //url for dockwidget
        return currentItemString;
    case 10002: //filter
        if (!currentItemString.isEmpty())
            return currentItemString + item->data(0).toString(); // return both uservisible and data
        else
            return "IGNORE";
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

    if (parentItem == 0 || parentItem == rootItem)
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

#ifdef BUILD_ZEROCONF
void RemoteDesktopsModel::servicesChanged() {
    //redo list because it is easier than finding and removing one
    QList<DNSSD::RemoteService::Ptr> services = zeroconfBrowser->services();
    KUrl url;
    zeroconfItem->clearChildren();
    foreach(DNSSD::RemoteService::Ptr service, services) {
        url.setProtocol(m_protocols[service->type()].toLower());
        url.setHost(service->hostName());
        url.setPort(service->port());
//tooltip        service->hostName()+"."+service->domain();
        RemoteDesktopsItem *newItem = new RemoteDesktopsItem(QList<QVariant>() << service->serviceName() << url.url(), zeroconfItem);
        zeroconfItem->appendChild(newItem);
    }
    reset();
}
#endif
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
