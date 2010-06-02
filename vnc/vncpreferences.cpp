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

#include "vncpreferences.h"
#include "remoteviewfactory.h"
#include "settings.h"

#include "ui_vncpreferences.h"

#include <KDebug>

KRDC_PLUGIN_EXPORT(VncPreferences)

VncPreferences::VncPreferences(QWidget *parent, const QVariantList &args)
        : KCModule(KrdcFactory::componentData(), parent, args)
{
    Ui::VncPreferences vncUi;
    vncUi.setupUi(this);
    
    // copying the RDP preferences... need to create generic code for the plugins.
    vncUi.resolutionDummyLabel->hide();
    vncUi.resolutionComboBox->hide();
    vncUi.kcfg_ScalingHeight->setEnabled(true);
    vncUi.kcfg_ScalingWidth->setEnabled(true);
    vncUi.heightLabel->setEnabled(true);
    vncUi.widthLabel->setEnabled(true);

    addConfig(Settings::self(), this);
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
