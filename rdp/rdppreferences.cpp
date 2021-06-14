/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rdppreferences.h"
#include "remoteviewfactory.h"
#include "settings.h"

#include "ui_rdppreferences.h"

K_PLUGIN_FACTORY_WITH_JSON(KrdcKcmFactory, "krdc_rdp_config.json", registerPlugin<RdpPreferences>();)

RdpPreferences::RdpPreferences(QWidget *parent, const QVariantList &args)
        : KCModule(parent, args)
{
    Ui::RdpPreferences rdpUi;
    rdpUi.setupUi(this);
    // would need a lot of code duplication. find a solution, bit it's not
    // that important because you will not change this configuration each day...
    // see rdp/rdphostpreferences.cpp
    rdpUi.kcfg_Resolution->hide();
    rdpUi.resolutionDummyLabel->hide();
    rdpUi.kcfg_Height->setEnabled(true);
    rdpUi.kcfg_Width->setEnabled(true);
    rdpUi.heightLabel->setEnabled(true);
    rdpUi.widthLabel->setEnabled(true);
    rdpUi.browseMediaButton->hide();

    
    addConfig(Settings::self(), this);
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
