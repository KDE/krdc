/****************************************************************************
**
** Copyright (C) 2007 Urs Wolfer <uwolfer @ kde.org>
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

#include "rdphostpreferences.h"

#include "settings.h"

#include <KDebug>

#include <QDesktopWidget>

static const QStringList keymaps = (QStringList()
    << "ar"
    << "cs"
    << "da"
    << "de"
    << "de-ch"
    << "en-dv"
    << "en-gb"
    << "en-us"
    << "es"
    << "et"
    << "fi"
    << "fo"
    << "fr"
    << "fr-be"
    << "fr-ca"
    << "fr-ch"
    << "he"
    << "hr"
    << "hu"
    << "is"
    << "it"
    << "ja"
    << "ko"
    << "lt"
    << "lv"
    << "mk"
    << "nl"
    << "nl-be"
    << "no"
    << "pl"
    << "pt"
    << "pt-br"
    << "ru"
    << "sl"
    << "sv"
    << "th"
    << "tr"
);

static const int defaultKeymap = 7; // en-us

inline int keymap2int(const QString &keymap)
{
    const int index = keymaps.lastIndexOf(keymap);
    return (index == -1) ? defaultKeymap : index;
}

inline QString int2keymap(int layout)
{
    if (layout >= 0 && layout < keymaps.count())
        return keymaps.at(layout);
    else
        return keymaps.at(defaultKeymap);
}

RdpHostPreferences::RdpHostPreferences(KConfigGroup configGroup, QObject *parent)
  : HostPreferences(configGroup, parent)
{
}

RdpHostPreferences::~RdpHostPreferences()
{
}

QWidget* RdpHostPreferences::createProtocolSpecificConfigPage()
{
    QWidget *rdpPage = new QWidget();
    rdpUi.setupUi(rdpPage);

    rdpUi.loginGroupBox->setVisible(false);

    rdpUi.kcfg_Height->setValue(height());
    rdpUi.kcfg_Width->setValue(width());
    rdpUi.kcfg_ColorDepth->setCurrentIndex(colorDepth());
    rdpUi.kcfg_KeyboardLayout->setCurrentIndex(keymap2int(keyboardLayout()));
    rdpUi.kcfg_Sound->setCurrentIndex(sound());
    rdpUi.kcfg_ExtraOptions->setText(extraOptions());

    connect(rdpUi.resolutionComboBox, SIGNAL(currentIndexChanged(int)), SLOT(updateWidthHeight(int)));

    const QString resolutionString = QString::number(width()) + 'x' + QString::number(height());
    const int resolutionIndex = rdpUi.resolutionComboBox->findText(resolutionString, Qt::MatchContains);
    rdpUi.resolutionComboBox->setCurrentIndex((resolutionIndex == -1) ? rdpUi.resolutionComboBox->count() - 1 : resolutionIndex);

    return rdpPage;
}

void RdpHostPreferences::updateWidthHeight(int index)
{
    switch (index) {
    case 0:
        rdpUi.kcfg_Height->setValue(480);
        rdpUi.kcfg_Width->setValue(640);
        break;
    case 1:
        rdpUi.kcfg_Height->setValue(600);
        rdpUi.kcfg_Width->setValue(800);
        break;
    case 2:
        rdpUi.kcfg_Height->setValue(768);
        rdpUi.kcfg_Width->setValue(1024);
        break;
    case 3:
        rdpUi.kcfg_Height->setValue(1024);
        rdpUi.kcfg_Width->setValue(1280);
        break;
    case 4:
        rdpUi.kcfg_Height->setValue(1200);
        rdpUi.kcfg_Width->setValue(1600);
        break;
    case 5: {
        QDesktopWidget *desktop = QApplication::desktop();
        int currentScreen = desktop->screenNumber(rdpUi.kcfg_Height);
        rdpUi.kcfg_Height->setValue(desktop->screenGeometry(currentScreen).height());
        rdpUi.kcfg_Width->setValue(desktop->screenGeometry(currentScreen).width());
        break;
    }
    case 6:
    default:
        break;
    }

    const bool enabled = (index == 6) ? true : false;

    rdpUi.kcfg_Height->setEnabled(enabled);
    rdpUi.kcfg_Width->setEnabled(enabled);
    rdpUi.heightLabel->setEnabled(enabled);
    rdpUi.widthLabel->setEnabled(enabled);
}

void RdpHostPreferences::acceptConfig()
{
    HostPreferences::acceptConfig();

    setHeight(rdpUi.kcfg_Height->value());
    setWidth(rdpUi.kcfg_Width->value());
    setColorDepth(rdpUi.kcfg_ColorDepth->currentIndex());
    setKeyboardLayout(int2keymap(rdpUi.kcfg_KeyboardLayout->currentIndex()));
    setSound(rdpUi.kcfg_Sound->currentIndex());
    setExtraOptions(rdpUi.kcfg_ExtraOptions->text());
}

void RdpHostPreferences::setHeight(int height)
{
    if (height >= 0)
        m_configGroup.writeEntry("height", height);
}

int RdpHostPreferences::height()
{
    return m_configGroup.readEntry("height", Settings::height());
}

void RdpHostPreferences::setWidth(int width)
{
    if (width >= 0)
        m_configGroup.writeEntry("width", width);
}

int RdpHostPreferences::width()
{
    return m_configGroup.readEntry("width", Settings::width());
}

void RdpHostPreferences::setColorDepth(int colorDepth)
{
    if (colorDepth >= 0)
        m_configGroup.writeEntry("colorDepth", colorDepth);
}

int RdpHostPreferences::colorDepth()
{
    return m_configGroup.readEntry("colorDepth", Settings::colorDepth());
}

void RdpHostPreferences::setKeyboardLayout(const QString &keyboardLayout)
{
    if (!keyboardLayout.isNull())
        m_configGroup.writeEntry("keyboardLayout", keymap2int(keyboardLayout));
}

QString RdpHostPreferences::keyboardLayout() const
{
    return int2keymap(m_configGroup.readEntry("keyboardLayout", Settings::keyboardLayout()));
}

void RdpHostPreferences::setSound(int sound)
{
    if (sound >= 0)
        m_configGroup.writeEntry("sound", sound);
}

int RdpHostPreferences::sound() const
{
    return m_configGroup.readEntry("sound", Settings::sound());
}

void RdpHostPreferences::setExtraOptions(const QString &extraOptions)
{
    if (!extraOptions.isNull())
        m_configGroup.writeEntry("extraOptions", extraOptions);
}

QString RdpHostPreferences::extraOptions() const
{
    return m_configGroup.readEntry("extraOptions", Settings::extraOptions());
}

#include "rdphostpreferences.moc"
