/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef RDPPREFERENCES_H
#define RDPPREFERENCES_H

#include "rdphostpreferences.h"

#include <KCModule>

class RdpPreferences : public KCModule
{
    Q_OBJECT

public:
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    explicit RdpPreferences(QWidget *parent, const QVariantList &args);
#else
    explicit RdpPreferences(QObject *parent);
#endif
    ~RdpPreferences() override;

    void save() override;
    void load() override;
};

#endif // RDPPREFERENCES_H
