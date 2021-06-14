/*
    SPDX-FileCopyrightText: 2009 Tony Murray <murraytony@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CONNECTIONDELEGATE_H
#define CONNECTIONDELEGATE_H

#include <QStyledItemDelegate>

class ConnectionDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ConnectionDelegate(QObject *parent = nullptr);
    QString displayText(const QVariant &value, const QLocale& locale) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

#endif // CONNECTIONDELEGATE_H
