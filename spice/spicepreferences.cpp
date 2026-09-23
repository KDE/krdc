/*
    SPDX-FileCopyrightText: 2024 KRDC Contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "spicepreferences.h"

#include "settings.h"
#include "ui_spicepreferences.h"

#include <KPluginFactory>

K_PLUGIN_CLASS(SpicePreferences)

SpicePreferences::SpicePreferences(QObject *parent)
    : KCModule(parent)
{
    Ui::SpicePreferences spiceUi;
    spiceUi.setupUi(widget());
    addConfig(Settings::self(), widget());
}

SpicePreferences::~SpicePreferences()
{
}

void SpicePreferences::load()
{
    KCModule::load();
}

void SpicePreferences::save()
{
    KCModule::save();
}

#include "spicepreferences.moc"
