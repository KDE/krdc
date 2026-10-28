/*
    SPDX-FileCopyrightText: 2026 Tony Murray <murraytony@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef VNCSETTINGSFORM_H
#define VNCSETTINGSFORM_H

#include "ui_vncpreferences.h"

#include <QGuiApplication>
#include <QScreen>
#include <QSize>
#include <QWindow>

#include <algorithm>
#include <array>

namespace VncScalingSize
{
// The entries of resolutionComboBox: these sizes, then the screen size, then a custom size.
inline constexpr std::array<QSize, 5> presets = {QSize(640, 480), QSize(800, 600), QSize(1024, 768), QSize(1280, 1024), QSize(1600, 1200)};
inline constexpr int screenIndex = presets.size();
inline constexpr int customIndex = screenIndex + 1;

// The size of the screen that shows the form, in device pixels.
inline QSize screenSize(const Ui::VncPreferences &ui)
{
    QWindow *window = ui.kcfg_ScalingWidth->window()->windowHandle();
    QScreen *screen = window ? window->screen() : qGuiApp->primaryScreen();
    return screen->size() * screen->devicePixelRatio();
}

inline void updateFields(const Ui::VncPreferences &ui)
{
    ui.resolutionComboBox->setEnabled(ui.kcfg_Scaling->isChecked());
    ui.customSize->setVisible(ui.kcfg_Scaling->isChecked() && ui.resolutionComboBox->currentIndex() == customIndex);
}
}

/** Selects the entry of resolutionComboBox that matches the scaling width and height. */
inline void selectVncScalingSize(const Ui::VncPreferences &ui)
{
    const QSize size(ui.kcfg_ScalingWidth->value(), ui.kcfg_ScalingHeight->value());
    const auto preset = std::find(VncScalingSize::presets.cbegin(), VncScalingSize::presets.cend(), size);
    int index = VncScalingSize::customIndex;
    if (preset != VncScalingSize::presets.cend()) {
        index = int(preset - VncScalingSize::presets.cbegin());
    } else if (size == VncScalingSize::screenSize(ui)) {
        index = VncScalingSize::screenIndex;
    }
    const QSignalBlocker blocker(ui.resolutionComboBox);
    ui.resolutionComboBox->setCurrentIndex(index);
    VncScalingSize::updateFields(ui);
}

/**
 * Makes the fields of the VNC settings form follow each other, the same way for the
 * settings of a host and for the default settings.
 */
inline void setupVncSettingsForm(const Ui::VncPreferences &ui)
{
    QObject::connect(ui.resolutionComboBox, &QComboBox::currentIndexChanged, ui.resolutionComboBox, [ui](int index) {
        QSize size;
        if (index < VncScalingSize::screenIndex) {
            size = VncScalingSize::presets[index];
        } else if (index == VncScalingSize::screenIndex) {
            size = VncScalingSize::screenSize(ui);
        }
        if (size.isValid()) {
            ui.kcfg_ScalingWidth->setValue(size.width());
            ui.kcfg_ScalingHeight->setValue(size.height());
        }
        VncScalingSize::updateFields(ui);
    });
    QObject::connect(ui.kcfg_Scaling, &QAbstractButton::toggled, ui.kcfg_Scaling, [ui]() {
        VncScalingSize::updateFields(ui);
    });
    selectVncScalingSize(ui);
}

#endif
