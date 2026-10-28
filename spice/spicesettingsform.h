/*
    SPDX-FileCopyrightText: 2026 Tony Murray <murraytony@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SPICESETTINGSFORM_H
#define SPICESETTINGSFORM_H

#include "spicehostpreferences.h"
#include "ui_spicepreferences.h"

/**
 * Makes the fields of the SPICE settings form follow each other, the same way for the
 * settings of a host and for the default settings.
 */
inline void setupSpiceSettingsForm(const Ui::SpicePreferences &ui)
{
    QWidget *customResolution = ui.customResolution;
    const auto updateResolution = [customResolution](int index) {
        customResolution->setVisible(index == int(SpiceHostPreferences::Resolution::Custom));
    };
    updateResolution(ui.kcfg_Resolution->currentIndex());
    QObject::connect(ui.kcfg_Resolution, &QComboBox::currentIndexChanged, customResolution, updateResolution);
}

#endif
