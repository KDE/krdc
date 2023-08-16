/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rdppreferences.h"
#include "remoteviewfactory.h"
#include "settings.h"

#include "ui_rdppreferences.h"

K_PLUGIN_CLASS(RdpPreferences)

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
RdpPreferences::RdpPreferences(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
#else
RdpPreferences::RdpPreferences(QObject *parent)
    : KCModule(parent)
#endif
{
    Ui::RdpPreferences rdpUi;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    rdpUi.setupUi(this);
#else
    rdpUi.setupUi(widget());
#endif
    // would need a lot of code duplication. find a solution, but it's not
    // that important because you will not change this configuration each day...
    // see rdp/rdphostpreferences.cpp
    rdpUi.kcfg_Resolution->hide();
    rdpUi.kcfg_Height->setEnabled(true);
    rdpUi.kcfg_Width->setEnabled(true);
    rdpUi.heightLabel->setEnabled(true);
    rdpUi.widthLabel->setEnabled(true);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    addConfig(Settings::self(), this);
#else
    addConfig(Settings::self(), widget());
#endif
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
