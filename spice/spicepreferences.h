/*
    SPDX-FileCopyrightText: 2024 KRDC Contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SPICEPREFERENCES_H
#define SPICEPREFERENCES_H

#include <KCModule>

class SpicePreferences : public KCModule
{
    Q_OBJECT

public:
    explicit SpicePreferences(QObject *parent);
    ~SpicePreferences() override;

    void save() override;
    void load() override;
};

#endif // SPICEPREFERENCES_H
