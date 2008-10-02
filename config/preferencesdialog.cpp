/****************************************************************************
**
** Copyright (C) 2007 Urs Wolfer <uwolfer @ kde.org>
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

#include "preferencesdialog.h"

#include "hostpreferenceslist.h"
#include "ui_general.h"
#ifdef BUILD_VNC
#include "ui_vncpreferences.h"
#endif
#ifdef BUILD_RDP
#include "ui_rdppreferences.h"
#endif
#ifdef BUILD_NX
#include "ui_nxpreferences.h"
#endif

#include <KConfigSkeleton>

PreferencesDialog::PreferencesDialog(QWidget *parent, KConfigSkeleton *skeleton)
        : KConfigDialog(parent, "preferences", skeleton)
{
    QWidget *generalPage = new QWidget(this);
    Ui::General generalUi;
    generalUi.setupUi(generalPage);
    addPage(generalPage, i18nc("General Config", "General"), "krdc", i18n("General Config"));

    HostPreferencesList *hostPreferencesList = new HostPreferencesList(this, skeleton->config());
    addPage(hostPreferencesList, i18n("Hosts"), "krdc", i18n("Host Config"));
    setHelp(QString(),"krdc");
#ifdef BUILD_VNC
    QWidget *vncPage = new QWidget(this);
    Ui::VncPreferences vncUi;
    vncUi.setupUi(vncPage);
    addPage(vncPage, i18n("VNC"), "krdc", i18n("VNC Config"));
#endif

#ifdef BUILD_RDP
    QWidget *rdpPage = new QWidget(this);
    Ui::RdpPreferences rdpUi;
    rdpUi.setupUi(rdpPage);
    // would need a lot of code duplication. find a solution, bit it's not
    // that imporant because you will not change this configuration each day...
    // see rdp/rdphostpreferences.cpp
    rdpUi.resolutionComboBox->hide();
    rdpUi.resolutionDummyLabel->hide();
    rdpUi.kcfg_Height->setEnabled(true);
    rdpUi.kcfg_Width->setEnabled(true);
    rdpUi.heightLabel->setEnabled(true);
    rdpUi.widthLabel->setEnabled(true);
    addPage(rdpPage, i18n("RDP"), "krdc", i18n("RDP Config"));
#endif

#ifdef BUILD_NX
    QWidget *nxPage = new QWidget(this);
    Ui::NxPreferences nxUi;
    nxUi.setupUi(nxPage);
    nxUi.resolutionComboBox->hide();
    nxUi.checkboxDefaultPrivateKey->hide();
    nxUi.buttonImportPrivateKey->hide();
    nxUi.labelPrivateKey->hide();
    nxUi.groupboxPrivateKey->hide();
    nxUi.kcfg_NxHeight->setEnabled(true);
    nxUi.kcfg_NxWidth->setEnabled(true);
    nxUi.heightLabel->setEnabled(true);
    nxUi.widthLabel->setEnabled(true);
    addPage(nxPage, i18n("NX"), "krdc", i18n("NX Config"));    
#endif
}
