/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rdppreferences.h"
#include "remoteviewfactory.h"
#include "settings.h"

#include "rdpsettingsform.h"

K_PLUGIN_CLASS(RdpPreferences)

RdpPreferences::RdpPreferences(QObject *parent)
    : KCModule(parent)
{
    migrateRdpDefaults();

    Ui::RdpPreferences rdpUi;
    rdpUi.setupUi(widget());
    // An SSH tunnel is set up per host.
    rdpUi.sshTunnelSection->hide();
    setupRdpSettingsForm(rdpUi);
    alignLabelColumns(widget());

    addConfig(Settings::self(), widget());
}

RdpPreferences::~RdpPreferences()
{
}

void RdpPreferences::load()
{
    KCModule::load();
}

void RdpPreferences::save()
{
    KCModule::save();
}

#include "rdppreferences.moc"
