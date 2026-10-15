/*
    SPDX-FileCopyrightText: 2007 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "vnchostpreferences.h"
#include "vncsettingsform.h"

#include "settings.h"

static const char quality_config_key[] = "quality";
static const char dont_copy_passwords_config_key[] = "dont_copy_passwords";
static const char scroll_multiplier_config_key[] = "scroll_multiplier";

static const int scroll_multiplier_min = 1;
static const int scroll_multiplier_max = 128;

VncHostPreferences::VncHostPreferences(KConfigGroup configGroup, QObject *parent)
    : HostPreferences(configGroup, parent)
{
}

VncHostPreferences::~VncHostPreferences()
{
}

QWidget *VncHostPreferences::createProtocolSpecificConfigPage(QWidget *sshTunnelWidget)
{
    QWidget *vncPage = new QWidget();
    vncUi.setupUi(vncPage);
    if (sshTunnelWidget) {
        vncUi.sshTunnelLayout->addWidget(sshTunnelWidget);
    }
    addQuickSetting(vncUi.scalingField);
    addQuickSetting(vncUi.kcfg_Quality);

    vncUi.kcfg_Quality->setCurrentIndex(quality() - 1);
    vncUi.kcfg_Scaling->setChecked(windowedScale());
    vncUi.kcfg_ScalingWidth->setValue(width());
    vncUi.kcfg_ScalingHeight->setValue(height());
    vncUi.dont_copy_passwords->setChecked(dontCopyPasswords());
    vncUi.kcfg_ScrollMultiplier->setValue(scrollMultiplier());
    setupVncSettingsForm(vncUi);

    return vncPage;
}

void VncHostPreferences::acceptConfig()
{
    HostPreferences::acceptConfig();
    setQuality((RemoteView::Quality)(vncUi.kcfg_Quality->currentIndex() + 1));
    setWindowedScale(vncUi.kcfg_Scaling->isChecked());
    if (vncUi.kcfg_Scaling->isChecked()) {
        setHeight(vncUi.kcfg_ScalingHeight->value());
        setWidth(vncUi.kcfg_ScalingWidth->value());
    }
    setScrollMultiplier(vncUi.kcfg_ScrollMultiplier->value());
}

void VncHostPreferences::setQuality(RemoteView::Quality quality)
{
    if (quality >= 0 && quality <= 3)
        m_configGroup.writeEntry(quality_config_key, (int)quality);
}

RemoteView::Quality VncHostPreferences::quality()
{
    return (RemoteView::Quality)m_configGroup.readEntry(quality_config_key, (int)Settings::quality() + 1);
}

bool VncHostPreferences::dontCopyPasswords() const
{
    return m_configGroup.readEntry(dont_copy_passwords_config_key, false);
}

void VncHostPreferences::setDontCopyPasswords(bool dontCopyPasswords)
{
    m_configGroup.writeEntry(dont_copy_passwords_config_key, dontCopyPasswords);
}

int VncHostPreferences::scrollMultiplier() const
{
    const int multiplier = m_configGroup.readEntry(scroll_multiplier_config_key, Settings::scrollMultiplier());
    return qBound(scroll_multiplier_min, multiplier, scroll_multiplier_max);
}

void VncHostPreferences::setScrollMultiplier(int multiplier)
{
    m_configGroup.writeEntry(scroll_multiplier_config_key, qBound(scroll_multiplier_min, multiplier, scroll_multiplier_max));
}
