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

#include "nxpreferences.h"
#include "remoteviewfactory.h"
#include "settings.h"

#include "ui_nxpreferences.h"

#include <KDebug>

KRDC_PLUGIN_EXPORT(NxPreferences)

NxPreferences::NxPreferences(QWidget *parent, const QVariantList &args)
        : KCModule(KrdcFactory::componentData(), parent, args)
{
    Ui::NxPreferences nxUi;
    nxUi.setupUi(this);
    nxUi.resolutionComboBox->hide();
    nxUi.checkboxDefaultPrivateKey->hide();
    nxUi.buttonImportPrivateKey->hide();
    nxUi.labelPrivateKey->hide();
    nxUi.groupboxPrivateKey->hide();
    nxUi.kcfg_NxHeight->setEnabled(true);
    nxUi.kcfg_NxWidth->setEnabled(true);
    nxUi.heightLabel->setEnabled(true);
    nxUi.widthLabel->setEnabled(true);
    
    addConfig(Settings::self(), this);
}

NxPreferences::~NxPreferences()
{
}

void NxPreferences::load()
{
    KCModule::load();
}

void NxPreferences::save()
{
    KCModule::save();
}

#include "nxpreferences.moc"
