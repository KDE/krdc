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

static QStringList keymaps = (QStringList()
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
    int index = keymaps.lastIndexOf(keymap);
    return (index == -1) ? defaultKeymap : index;
}

inline QString int2keymap(int layout)
{
    if (layout >= 0 && layout < keymaps.count())
        return keymaps.at(layout);
    else
        return keymaps.at(defaultKeymap);
}

RdpHostPreferences::RdpHostPreferences(const QString &url, bool forceShow, QObject *parent)
  : HostPreferences(url, parent),
    m_height(600),
    m_width(800),
    m_colorDepth(0),
    m_keyboardLayout("en-us"),
    m_sound(0),
    m_extraOptions(QString())
{
    if (hostConfigured()) {
        if (showConfigAgain() || forceShow) {
            kDebug(5012) << "Show config dialog again";
            showDialog();
        } else
            return; // no changes, no need to save
    } else {
        kDebug(5012) << "No config found, create new";
        if (Settings::showPreferencesForNewConnections())
            showDialog();
    }

    saveConfig();
}

RdpHostPreferences::~RdpHostPreferences()
{
}

void RdpHostPreferences::showDialog()
{
    QWidget *rdpPage = new QWidget();
    rdpUi.setupUi(rdpPage);

    KDialog *dialog = createDialog(rdpPage);

    rdpUi.kcfg_Height->setValue(height());
    rdpUi.kcfg_Width->setValue(width());
    rdpUi.kcfg_ColorDepth->setCurrentIndex(colorDepth());
    rdpUi.kcfg_KeyboardLayout->setCurrentIndex(keymap2int(keyboardLayout()));
    rdpUi.kcfg_Sound->setCurrentIndex(sound());
    rdpUi.kcfg_ExtraOptions->setText(extraOptions());

    connect(rdpUi.resolutionComboBox, SIGNAL(currentIndexChanged(int)), SLOT(updateWidthHeight(int)));

    QString resolutionString = QString::number(width()) + 'x' + QString::number(height());
    int resolutionIndex = rdpUi.resolutionComboBox->findText(resolutionString, Qt::MatchContains);
    rdpUi.resolutionComboBox->setCurrentIndex((resolutionIndex == -1) ? 5 : resolutionIndex);

    if (dialog->exec() == KDialog::Accepted) {
        kDebug(5012) << "RdpHostPreferences config dialog accepted";

        setHeight(rdpUi.kcfg_Height->value());
        setWidth(rdpUi.kcfg_Width->value());
        setColorDepth(rdpUi.kcfg_ColorDepth->currentIndex());
        setKeyboardLayout(int2keymap(rdpUi.kcfg_KeyboardLayout->currentIndex()));
        setSound(rdpUi.kcfg_Sound->currentIndex());
        setExtraOptions(rdpUi.kcfg_ExtraOptions->text());
    }
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

    bool enabled = (index == 6) ? true : false;

    rdpUi.kcfg_Height->setEnabled(enabled);
    rdpUi.kcfg_Width->setEnabled(enabled);
    rdpUi.heightLabel->setEnabled(enabled);
    rdpUi.widthLabel->setEnabled(enabled);
}

void RdpHostPreferences::readProtocolSpecificConfig()
{
    kDebug(5012) << "RdpHostPreferences::readProtocolSpecificConfig";

    if (m_element.firstChildElement("height") != QDomElement()) // not configured yet
        setHeight(m_element.firstChildElement("height").text().toInt());
    else
        setHeight(Settings::height()); // get the proctocol default

    if (m_element.firstChildElement("width") != QDomElement())
        setWidth(m_element.firstChildElement("width").text().toInt());
    else
        setWidth(Settings::width());

    if (m_element.firstChildElement("colorDepth") != QDomElement())
        setColorDepth(m_element.firstChildElement("colorDepth").text().toInt());
    else
        setColorDepth(Settings::colorDepth());

    if (m_element.firstChildElement("keyboardLayout") != QDomElement())
        setKeyboardLayout(int2keymap(m_element.firstChildElement("keyboardLayout").text().toInt()));
    else
        setKeyboardLayout(int2keymap(Settings::keyboardLayout()));

    if (m_element.firstChildElement("sound") != QDomElement())
        setSound(m_element.firstChildElement("sound").text().toInt());
    else
        setSound(Settings::sound());

    if (m_element.firstChildElement("extraOptions") != QDomElement())
        setExtraOptions(m_element.firstChildElement("extraOptions").text());
    else
        setExtraOptions(Settings::extraOptions());
}

void RdpHostPreferences::saveProtocolSpecificConfig()
{
    kDebug(5012) << "RdpHostPreferences::saveProtocolSpecificConfig";

    updateElement("height", QString::number(height()));
    updateElement("width", QString::number(width()));
    updateElement("colorDepth", QString::number(colorDepth()));
    updateElement("keyboardLayout", QString::number(keymap2int(keyboardLayout())));
    updateElement("sound", QString::number(sound()));
    updateElement("extraOptions", extraOptions());
}

void RdpHostPreferences::setHeight(int height)
{
    if (height >= 0)
        m_height = height;
}

int RdpHostPreferences::height()
{
    return m_height;
}

void RdpHostPreferences::setWidth(int width)
{
    if (width >= 0)
        m_width = width;
}

int RdpHostPreferences::width()
{
    return m_width;
}

void RdpHostPreferences::setColorDepth(int colorDepth)
{
    if (colorDepth >= 0)
        m_colorDepth = colorDepth;
}

int RdpHostPreferences::colorDepth()
{
    return m_colorDepth;
}

void RdpHostPreferences::setKeyboardLayout(const QString &keyboardLayout)
{
    if (!keyboardLayout.isNull())
        m_keyboardLayout = keyboardLayout;
}

QString RdpHostPreferences::keyboardLayout() const
{
    return m_keyboardLayout;
}

void RdpHostPreferences::setSound(int sound)
{
    if (sound >= 0)
        m_sound = sound;
}

int RdpHostPreferences::sound() const
{
    return m_sound;
}

void RdpHostPreferences::setExtraOptions(const QString &extraOptions)
{
    if (!extraOptions.isNull())
        m_extraOptions = extraOptions;
}

QString RdpHostPreferences::extraOptions() const
{
    return m_extraOptions;
}

#include "rdphostpreferences.moc"
