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
    explicit RdpPreferences(QObject *parent);
    ~RdpPreferences() override;

    void save() override;
    void load() override;
};

#endif // RDPPREFERENCES_H
