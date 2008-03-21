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

#include <QStringList>

#include "remotedesktopsitem.h"

RemoteDesktopsItem::RemoteDesktopsItem(const QList<QVariant> &data, RemoteDesktopsItem *parent)
{
    parentItem = parent;
    itemData = data;
}

RemoteDesktopsItem::~RemoteDesktopsItem()
{
    qDeleteAll(childItems);
}

void RemoteDesktopsItem::appendChild(RemoteDesktopsItem *item)
{
    childItems.append(item);
}

RemoteDesktopsItem *RemoteDesktopsItem::child(int row)
{
    return childItems.value(row);
}

int RemoteDesktopsItem::childCount() const
{
    return childItems.count();
}

int RemoteDesktopsItem::columnCount() const
{
    return itemData.count();
}

QVariant RemoteDesktopsItem::data(int column) const
{
    return itemData.value(column);
}

RemoteDesktopsItem *RemoteDesktopsItem::parent()
{
    return parentItem;
}

int RemoteDesktopsItem::row() const
{
    if (parentItem)
        return parentItem->childItems.indexOf(const_cast<RemoteDesktopsItem*>(this));

    return 0;
}
