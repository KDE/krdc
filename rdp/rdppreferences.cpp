/****************************************************************************
**
** Copyright (C) 2008 Urs Wolfer <uwolfer @ kde.org>
**
** This file is part of KDE.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; see the file COPYING. If not, write to
** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
** Boston, MA 02110-1301, USA.
**
****************************************************************************/

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
