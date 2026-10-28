/*
    SPDX-FileCopyrightText: 2026 Tony Murray <murraytony@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef RDPSETTINGSFORM_H
#define RDPSETTINGSFORM_H

#include "rdphostpreferences.h"
#include "settings.h"
#include "settingsform.h"
#include "ui_rdppreferences.h"

#include <KConfigGroup>

#include <QUrl>

#include <algorithm>

/**
 * The settings page of older versions saved the shared folder as a URL, which FreeRDP does not
 * accept. Returns the folder that @p stored stands for.
 */
inline QString migratedShareMedia(const QString &stored)
{
    const QUrl url(stored);
    return url.isLocalFile() ? url.toLocalFile() : stored;
}

/**
 * Before UseGateway and UseProxy, a route was used whenever its server was set, and the shared
 * folder needs migratedShareMedia(). The default settings drop a value left at its default, so
 * the group is marked once it is migrated. Only the migrated settings are written, so that other
 * settings that are changed but not saved yet stay unsaved.
 */
inline void migrateRdpDefaults()
{
    KConfigSkeleton *settings = Settings::self();
    KConfigGroup group = settings->config()->group(QStringLiteral("RDP"));
    if (group.readEntry("defaultsMigrated", false)) {
        return;
    }
    group.writeEntry("defaultsMigrated", true);
    Settings::setUseGateway(!Settings::gatewayServer().isEmpty());
    Settings::setUseProxy(!Settings::proxyHost().isEmpty());
    Settings::setShareMedia(migratedShareMedia(Settings::shareMedia()));
    for (const QString &name : {QStringLiteral("UseGateway"), QStringLiteral("UseProxy"), QStringLiteral("ShareMedia")}) {
        settings->findItem(name)->writeConfig(settings->config());
    }
    settings->config()->sync();
}

/**
 * Makes the fields of the RDP settings form follow each other, the same way for the
 * settings of a host and for the default settings.
 */
inline void setupRdpSettingsForm(const Ui::RdpPreferences &ui)
{
    QWidget *customResolution = ui.customResolution;
    const auto updateResolution = [customResolution](int index) {
        customResolution->setVisible(index == int(RdpHostPreferences::Resolution::Custom));
    };
    updateResolution(ui.kcfg_Resolution->currentIndex());
    QObject::connect(ui.kcfg_Resolution, &QComboBox::currentIndexChanged, customResolution, updateResolution);

    QWidget *customScaleFactor = ui.kcfg_DesktopScaleFactorCustom;
    const auto updateScaleFactor = [customScaleFactor](int index) {
        customScaleFactor->setVisible(index == int(RdpHostPreferences::DesktopScaleFactor::Custom));
    };
    updateScaleFactor(ui.kcfg_DesktopScaleFactor->currentIndex());
    QObject::connect(ui.kcfg_DesktopScaleFactor, &QComboBox::currentIndexChanged, customScaleFactor, updateScaleFactor);

    // The accelerated graphics modes only work with 32-bit color.
    QComboBox *colorDepth = ui.kcfg_ColorDepth;
    const auto updateColorDepth = [colorDepth](int index) {
        const bool selectable = index == int(RdpHostPreferences::Acceleration::Disabled);
        colorDepth->setEnabled(selectable);
        if (!selectable) {
            colorDepth->setCurrentIndex(0);
        }
    };
    updateColorDepth(ui.kcfg_Acceleration->currentIndex());
    QObject::connect(ui.kcfg_Acceleration, &QComboBox::currentIndexChanged, colorDepth, updateColorDepth);

    showRowsWhileChecked(ui.connectionLayout,
                         ui.kcfg_UseGateway,
                         {ui.kcfg_GatewayServer, ui.kcfg_GatewayUsername, ui.kcfg_GatewayPassword, ui.kcfg_GatewayDomain, ui.kcfg_GatewayTransportType});
    showRowsWhileChecked(ui.connectionLayout, ui.kcfg_UseProxy, {ui.kcfg_ProxyHost, ui.kcfg_ProxyUsername, ui.kcfg_ProxyPassword, ui.kcfg_ProxyProtocol});

    // Without a security protocol, the connection cannot be negotiated, so the last one stays checked.
    const QList<QCheckBox *> protocols = {ui.kcfg_SecurityNLA, ui.kcfg_SecurityTLS, ui.kcfg_SecurityRDP, ui.kcfg_SecurityEXT};
    const auto keepOneProtocol = [protocols]() {
        const auto checked = std::count_if(protocols.cbegin(), protocols.cend(), [](QCheckBox *protocol) {
            return protocol->isChecked();
        });
        for (QCheckBox *protocol : protocols) {
            protocol->setEnabled(checked > 1 || !protocol->isChecked());
        }
    };
    keepOneProtocol();
    for (QCheckBox *protocol : protocols) {
        QObject::connect(protocol, &QCheckBox::toggled, protocol, keepOneProtocol);
    }
}

#endif
