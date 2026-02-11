/*
 * SPDX-FileCopyrightText: 2026 Fabio Bas <ctrlaltca@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QCursor>
#include <QPixmap>

inline QCursor scaledRemoteCursor(const QCursor &cursor, qreal scale)
{
    const QPixmap pixmap = cursor.pixmap();
    if (pixmap.isNull() || scale <= 0.0 || qFuzzyCompare(scale, 1.0)) {
        return cursor;
    }

    const QSize size{qMax(1, qRound(pixmap.width() * scale)), qMax(1, qRound(pixmap.height() * scale))};
    const QPoint hotSpot{qRound(cursor.hotSpot().x() * scale), qRound(cursor.hotSpot().y() * scale)};
    return QCursor{pixmap.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation), hotSpot.x(), hotSpot.y()};
}
