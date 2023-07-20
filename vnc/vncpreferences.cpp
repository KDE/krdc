/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "vncpreferences.h"
#include "remoteviewfactory.h"
#include "settings.h"

#include "ui_vncpreferences.h"

K_PLUGIN_CLASS(VncPreferences)

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
VncPreferences::VncPreferences(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
#else
VncPreferences::VncPreferences(QObject *parent)
    : KCModule(parent)
#endif
{
    Ui::VncPreferences vncUi;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    vncUi.setupUi(this);
#else
    vncUi.setupUi(widget());
#endif

    // copying the RDP preferences... need to create generic code for the plugins.
    vncUi.resolutionDummyLabel->hide();
    vncUi.resolutionComboBox->hide();
    vncUi.kcfg_ScalingHeight->setEnabled(true);
    vncUi.kcfg_ScalingWidth->setEnabled(true);
    vncUi.heightLabel->setEnabled(true);
    vncUi.widthLabel->setEnabled(true);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    addConfig(Settings::self(), this);
#else
    addConfig(Settings::self(), widget());
#endif
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
