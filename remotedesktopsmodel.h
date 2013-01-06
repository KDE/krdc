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

#ifndef REMOTEDESKTOPSMODEL_H
#define REMOTEDESKTOPSMODEL_H

#include <QAbstractTableModel>
#include <KDateTime>

#ifdef BUILD_ZEROCONF
#include <dnssd/servicebrowser.h>
#endif

class KBookmarkGroup;
class KBookmarkManager;

struct RemoteDesktop {
public:
    enum Source { None = 0x0, Bookmarks = 0x1, History = 0x2, Zeroconf = 0x4 };
    Q_DECLARE_FLAGS(Sources, Source)
    QString title;
    QString url;
    KDateTime lastConnected;
    KDateTime created;
    int visits;
    RemoteDesktop::Source source;
    bool favorite;
    bool operator<(const RemoteDesktop &rd) const {
        if (lastConnected == rd.lastConnected)
            return url < rd.url;
        return rd.lastConnected < lastConnected;  // seems backward but gets the desired result
    }
    bool operator==(const RemoteDesktop &rd) const {
        return url == rd.url;
    }
};

class RemoteDesktopsModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit RemoteDesktopsModel(QObject *parent);
    ~RemoteDesktopsModel();

    enum DisplayItems { Favorite, Title, LastConnected, VisitCount, Created, Source };
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

private:
    QList<RemoteDesktop> remoteDesktops;
    QString getLastConnectedString(KDateTime lastConnected, bool fuzzy = false) const;
    void removeAllItemsFromSources(RemoteDesktop::Sources sources);
    void buildModelFromBookmarkGroup(const KBookmarkGroup &group);
    KBookmarkManager *m_manager;

#ifdef BUILD_ZEROCONF
    DNSSD::ServiceBrowser *zeroconfBrowser;
    QHash<QString, QString> m_protocols;
#endif

private slots:
    void bookmarksChanged();
#ifdef BUILD_ZEROCONF
    void servicesChanged();
#endif
};

Q_DECLARE_OPERATORS_FOR_FLAGS(RemoteDesktop::Sources)

#endif
