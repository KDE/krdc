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
    explicit VncPreferences(QObject *parent);
    ~VncPreferences() override;

    void save() override;
    void load() override;
    void defaults() override;

private:
    Ui::VncPreferences m_ui;
};

#endif // VNCPREFERENCES_H
