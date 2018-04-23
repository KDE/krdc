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

#ifndef CONNECTIONDELEGATE_H
#define CONNECTIONDELEGATE_H

#include <QStyledItemDelegate>

class ConnectionDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ConnectionDelegate(QObject *parent = 0);
    QString displayText(const QVariant &value, const QLocale& locale) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

#endif // CONNECTIONDELEGATE_H
