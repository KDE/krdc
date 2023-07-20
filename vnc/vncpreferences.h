/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef VNCPREFERENCES_H
#define VNCPREFERENCES_H

#include "vnchostpreferences.h"

#include <KCModule>

class VncPreferences : public KCModule
{
    Q_OBJECT

public:
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    explicit VncPreferences(QWidget *parent, const QVariantList &args);
#else
    explicit VncPreferences(QObject *parent);
#endif
    ~VncPreferences() override;

    void save() override;
    void load() override;
};

#endif // VNCPREFERENCES_H
