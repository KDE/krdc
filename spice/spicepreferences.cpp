/*
    SPDX-FileCopyrightText: 2024 KRDC Contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "spicepreferences.h"

#include "settings.h"
#include "spicesettingsform.h"

#include <KPluginFactory>

K_PLUGIN_CLASS(SpicePreferences)

SpicePreferences::SpicePreferences(QObject *parent)
    : KCModule(parent)
{
    Ui::SpicePreferences spiceUi;
    spiceUi.setupUi(widget());
    // An SSH tunnel is set up per host.
    spiceUi.networkSection->hide();
    setupSpiceSettingsForm(spiceUi);
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
