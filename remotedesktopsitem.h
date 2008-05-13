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

#ifndef REMOTEDESKTOPSITEM_H
#define REMOTEDESKTOPSITEM_H

#include <QList>
#include <QVariant>

class RemoteDesktopsItem
{
public:
    explicit RemoteDesktopsItem(const QList<QVariant> &data, RemoteDesktopsItem *parent = 0);
    ~RemoteDesktopsItem();

    void appendChild(RemoteDesktopsItem *child);

    RemoteDesktopsItem *child(int row);
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    int row() const;
    RemoteDesktopsItem *parent();
    void clearChildren();

private:
    QList<RemoteDesktopsItem*> childItems;
    QList<QVariant> itemData;
    RemoteDesktopsItem *parentItem;
};

#endif
