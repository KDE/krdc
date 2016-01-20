/****************************************************************************
**
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

#include "connectiondelegate.h"
#include "remotedesktopsmodel.h"

#include <KIconThemes/KIconLoader>
#include <KLocalizedString>

#include <QDateTime>
#include <QIcon>

ConnectionDelegate::ConnectionDelegate(QObject *parent) :
        QStyledItemDelegate(parent)
{
}

QString ConnectionDelegate::displayText(const QVariant &value, const QLocale& locale) const
{
    if (value.type() == QVariant::DateTime) {
        QDateTime lastConnected = QDateTime(value.toDateTime());
        QDateTime currentTime = QDateTime::currentDateTimeUtc();

        int daysAgo = lastConnected.daysTo(currentTime);
        if (daysAgo <= 1 && lastConnected.secsTo(currentTime) < 86400) {
            int minutesAgo = lastConnected.secsTo(currentTime) / 60;
            int hoursAgo = minutesAgo / 60;
            if (hoursAgo < 1) {
                if (minutesAgo < 1)
                    return i18n("Less than a minute ago");
                return i18np("A minute ago", "%1 minutes ago", minutesAgo);
            } else {
                return i18np("An hour ago", "%1 hours ago", hoursAgo);
            }
        } else { // 1 day or more
            if (daysAgo < 30)
                return i18np("Yesterday", "%1 days ago", daysAgo);
            if (daysAgo < 365)
                return i18np("Over a month ago", "%1 months ago", daysAgo / 30);
            return i18np("A year ago", "%1 years ago", daysAgo / 365);
        }

    }
    // These aren't the strings you're looking for, move along.
    return QStyledItemDelegate::displayText(value, locale);
}

void ConnectionDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (index.column() == RemoteDesktopsModel::Favorite) {
        QVariant value = index.data(Qt::CheckStateRole);
        if (value.isValid()) {
            Qt::CheckState checkState = static_cast<Qt::CheckState>(value.toInt());
            QIcon favIcon = QIcon::fromTheme(QLatin1String("bookmarks"));
            QIcon::Mode mode = (checkState == Qt::Checked) ? QIcon::Active : QIcon::Disabled;
            favIcon.paint(painter, option.rect, option.decorationAlignment, mode);

        }
    } else {
        QStyledItemDelegate::paint(painter, option, index);
    }
}

QSize ConnectionDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (index.column() == RemoteDesktopsModel::Favorite)
        return QSize(KIconLoader::SizeSmall, KIconLoader::SizeSmall);
    return QStyledItemDelegate::sizeHint(option, index);
}
