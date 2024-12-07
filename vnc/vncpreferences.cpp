/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "vncpreferences.h"
#include "remoteviewfactory.h"
#include "settings.h"

#include "ui_vncpreferences.h"

K_PLUGIN_CLASS(VncPreferences)

VncPreferences::VncPreferences(QObject *parent)
    : KCModule(parent)
{
    Ui::VncPreferences vncUi;
    vncUi.setupUi(widget());

    // copying the RDP preferences... need to create generic code for the plugins.
    vncUi.resolutionDummyLabel->hide();
    vncUi.resolutionComboBox->hide();
    vncUi.kcfg_ScalingHeight->setEnabled(true);
    vncUi.kcfg_ScalingWidth->setEnabled(true);
    vncUi.heightLabel->setEnabled(true);
    vncUi.widthLabel->setEnabled(true);

    addConfig(Settings::self(), widget());
}

VncPreferences::~VncPreferences()
{
}

void VncPreferences::load()
{
    KCModule::load();
}

void VncPreferences::save()
{
    KCModule::save();
}

#include "vncpreferences.moc"
