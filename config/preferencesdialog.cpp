/****************************************************************************
**
** Copyright (C) 2007 - 2010 Urs Wolfer <uwolfer @ kde.org>
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
#include "krdc_debug.h"
#include "hostpreferenceslist.h"
#include "ui_general.h"

#include <KConfigGui/KConfigSkeleton>
#include <KLocalizedString>
#include <KCMUtils/KPluginSelector>
#include <KPluginTrader>
#include <KService/KPluginInfo>

PreferencesDialog::PreferencesDialog(QWidget *parent, KConfigSkeleton *skeleton)
        : KConfigDialog(parent, QLatin1String("preferences"), skeleton)
        , m_settingsChanged(false)
{
    QWidget *generalPage = new QWidget(this);
    Ui::General generalUi;
    generalUi.setupUi(generalPage);
    addPage(generalPage, i18nc("General Config", "General"), QLatin1String("krdc"), i18n("General Configuration"));

    HostPreferencesList *hostPreferencesList = new HostPreferencesList(this,
                                                                       qobject_cast<MainWindow *>(parent),
                                                                       skeleton->config()->group("hostpreferences"));
    addPage(hostPreferencesList, i18n("Hosts"), QLatin1String("computer"), i18n("Host Configuration"));

    m_pluginSelector = new KPluginSelector();
    const KPluginInfo::List  offers = KPluginTrader::self()->query(QLatin1String("krdc"));
    m_pluginSelector->addPlugins(offers, KPluginSelector::ReadConfigFile,
                                    i18n("Plugins"), QLatin1String("Service"), KSharedConfig::openConfig());
    m_pluginSelector->load();
    addPage(m_pluginSelector, i18n("Plugins"), QLatin1String("preferences-plugin"), i18n("Plugin Configuration"));

    connect(this, SIGNAL(accepted()), SLOT(saveState()));
    QPushButton *defaultsButton = buttonBox()->button(QDialogButtonBox::RestoreDefaults);
    connect(defaultsButton, SIGNAL(clicked()), SLOT(loadDefaults()));
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
}

void PreferencesDialog::settingsChanged()
{
    enableButton(QDialogButtonBox::Apply);
    enableButton(QDialogButtonBox::RestoreDefaults);
}

bool PreferencesDialog::isDefault()
{
    return KConfigDialog::isDefault() && m_pluginSelector->isDefault();
}

void PreferencesDialog::enableButton(QDialogButtonBox::StandardButton standardButton)
{
    QPushButton *button =  buttonBox()->button(standardButton);
    if (button) {
        button->setEnabled(true);
    }
}

