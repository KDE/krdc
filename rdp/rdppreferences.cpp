/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rdppreferences.h"
#include "remoteviewfactory.h"
#include "settings.h"

#include "rdpsettingsform.h"

K_PLUGIN_CLASS(RdpPreferences)

static int defaultKeyboardLayout()
{
    return Settings::self()->findItem(QStringLiteral("KeyboardLayout"))->getDefault().toInt();
}

RdpPreferences::RdpPreferences(QObject *parent)
    : KCModule(parent)
{
    migrateRdpDefaults();

    Ui::RdpPreferences rdpUi;
    rdpUi.setupUi(widget());
    // An SSH tunnel is set up per host.
    rdpUi.sshTunnelSection->hide();
    setupRdpSettingsForm(rdpUi);
    alignLabelColumns(widget());

    // The list order differs from the stored values, so the layout is not managed by addConfig().
    m_keyboardLayout = rdpUi.kcfg_KeyboardLayout;
    m_keyboardLayout->setObjectName(QStringLiteral("keyboardLayout"));
    connect(m_keyboardLayout, &QComboBox::currentIndexChanged, this, &RdpPreferences::updateKeyboardLayoutState);

    addConfig(Settings::self(), widget());
}

RdpPreferences::~RdpPreferences()
{
}

void RdpPreferences::load()
{
    KCModule::load();
    m_keyboardLayout->setCurrentIndex(RdpHostPreferences::keyboardLayoutRow(Settings::keyboardLayout()));
    updateKeyboardLayoutState();
}

void RdpPreferences::save()
{
    KCModule::save();
    Settings::setKeyboardLayout(RdpHostPreferences::keyboardLayoutFromRow(m_keyboardLayout->currentIndex()));
    Settings::self()->save();
    updateKeyboardLayoutState();
}

void RdpPreferences::defaults()
{
    KCModule::defaults();
    m_keyboardLayout->setCurrentIndex(RdpHostPreferences::keyboardLayoutRow(defaultKeyboardLayout()));
}

void RdpPreferences::updateKeyboardLayoutState()
{
    const int keyboardLayout = RdpHostPreferences::keyboardLayoutFromRow(m_keyboardLayout->currentIndex());
    unmanagedWidgetChangeState(keyboardLayout != Settings::keyboardLayout());
    unmanagedWidgetDefaultState(keyboardLayout == defaultKeyboardLayout());
}

#include "rdppreferences.moc"
