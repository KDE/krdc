/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "vncpreferences.h"
#include "remoteviewfactory.h"
#include "settings.h"

#include "vncsettingsform.h"

K_PLUGIN_CLASS(VncPreferences)

VncPreferences::VncPreferences(QObject *parent)
    : KCModule(parent)
{
    m_ui.setupUi(widget());
    setupVncSettingsForm(m_ui);

    addConfig(Settings::self(), widget());
}

VncPreferences::~VncPreferences()
{
}

void VncPreferences::load()
{
    KCModule::load();
    selectVncScalingSize(m_ui);
}

void VncPreferences::defaults()
{
    KCModule::defaults();
    selectVncScalingSize(m_ui);
}

void VncPreferences::save()
{
    KCModule::save();
}

#include "vncpreferences.moc"
