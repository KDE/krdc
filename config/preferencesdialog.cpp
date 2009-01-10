/****************************************************************************
**
** Copyright (C) 2007 - 2008 Urs Wolfer <uwolfer @ kde.org>
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

#include <KConfigSkeleton>
#include <KDebug>
#include <KPluginSelector>
#include <KService>
#include <KServiceTypeTrader>
#include <KPluginInfo>

PreferencesDialog::PreferencesDialog(QWidget *parent, KConfigSkeleton *skeleton)
        : KConfigDialog(parent, "preferences", skeleton)
        , m_settingsChanged(false)
{
    QWidget *generalPage = new QWidget(this);
    Ui::General generalUi;
    generalUi.setupUi(generalPage);
    addPage(generalPage, i18nc("General Config", "General"), "krdc", i18n("General Configuration"));

    HostPreferencesList *hostPreferencesList = new HostPreferencesList(this,
                                                                       qobject_cast<MainWindow *>(parent),
                                                                       skeleton->config()->group("hostpreferences"));
    addPage(hostPreferencesList, i18n("Hosts"), "krdc", i18n("Host Configuration"));
    
    m_pluginSelector = new KPluginSelector();
    KService::List offers = KServiceTypeTrader::self()->query("KRDC/Plugin");
    m_pluginSelector->addPlugins(KPluginInfo::fromServices(offers), KPluginSelector::ReadConfigFile,
                               i18n("Plugins"), "Service", KGlobal::config());
    m_pluginSelector->load();
    addPage(m_pluginSelector, i18n("Plugins"), "preferences-plugin", i18n("Plugin Configuration"));

    connect(this, SIGNAL(accepted()), SLOT(saveState()));
    connect(this, SIGNAL(defaultClicked()), SLOT(loadDefaults()));
    connect(m_pluginSelector, SIGNAL(changed(bool)), SLOT(settingsChanged()));
}

void PreferencesDialog::saveState()
{
    //TODO: relaod plugins at runtime?
    m_pluginSelector->save();
}

void PreferencesDialog::loadDefaults()
{
    m_pluginSelector->defaults();
    enableButton(Default, false);
}

void PreferencesDialog::settingsChanged()
{
    enableButton(Apply, true);
    enableButton(Default, true);
}

bool PreferencesDialog::isDefault()
{
    return KConfigDialog::isDefault()
#if KDE_IS_VERSION(4, 2, 61)
        && m_pluginSelector->isDefault()
#endif
        ;
}
