/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rdppreferences.h"
#include "remoteviewfactory.h"
#include "settings.h"

#include "ui_rdppreferences.h"

K_PLUGIN_CLASS(RdpPreferences)

RdpPreferences::RdpPreferences(QObject *parent)
    : KCModule(parent)
{
    Ui::RdpPreferences rdpUi;
    rdpUi.setupUi(widget());

    // would need a lot of code duplication. find a solution, but it's not
    // that important because you will not change this configuration each day...
    // see rdp/rdphostpreferences.cpp
    rdpUi.kcfg_Resolution->hide();
    rdpUi.kcfg_Height->setEnabled(true);
    rdpUi.kcfg_Width->setEnabled(true);
    rdpUi.heightLabel->setEnabled(true);
    rdpUi.widthLabel->setEnabled(true);

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
