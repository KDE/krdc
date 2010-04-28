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

#include "vnchostpreferences.h"

#include "settings.h"

#include <KDebug>

#include <QDesktopWidget>

VncHostPreferences::VncHostPreferences(KConfigGroup configGroup, QObject *parent)
        : HostPreferences(configGroup, parent)
{
}

VncHostPreferences::~VncHostPreferences()
{
}

QWidget* VncHostPreferences::createProtocolSpecificConfigPage()
{
    QWidget *vncPage = new QWidget();
    vncUi.setupUi(vncPage);

    vncUi.kcfg_Quality->setCurrentIndex(quality() - 1);
    vncUi.kcfg_Scaling->setChecked(windowedScale());
    vncUi.kcfg_ScalingWidth->setValue(width());
    vncUi.kcfg_ScalingHeight->setValue(height());

    connect(vncUi.resolutionComboBox, SIGNAL(currentIndexChanged(int)), SLOT(updateScalingWidthHeight(int)));
    connect(vncUi.kcfg_Scaling, SIGNAL(toggled(bool)), SLOT(updateScaling(bool)));

    const QString resolutionString = QString::number(width()) + 'x' + QString::number(height());
    const int resolutionIndex = vncUi.resolutionComboBox->findText(resolutionString, Qt::MatchContains);
    vncUi.resolutionComboBox->setCurrentIndex((resolutionIndex == -1) ? vncUi.resolutionComboBox->count() - 1 : resolutionIndex);

    updateScaling(windowedScale());

    return vncPage;
}

void VncHostPreferences::updateScalingWidthHeight(int index)
{
    switch (index) {
    case 0:
        vncUi.kcfg_ScalingHeight->setValue(480);
        vncUi.kcfg_ScalingWidth->setValue(640);
        break;
    case 1:
        vncUi.kcfg_ScalingHeight->setValue(600);
        vncUi.kcfg_ScalingWidth->setValue(800);
        break;
    case 2:
        vncUi.kcfg_ScalingHeight->setValue(768);
        vncUi.kcfg_ScalingWidth->setValue(1024);
        break;
    case 3:
        vncUi.kcfg_ScalingHeight->setValue(1024);
        vncUi.kcfg_ScalingWidth->setValue(1280);
        break;
    case 4:
        vncUi.kcfg_ScalingHeight->setValue(1200);
        vncUi.kcfg_ScalingWidth->setValue(1600);
        break;
    case 5: {
        QDesktopWidget *desktop = QApplication::desktop();
        int currentScreen = desktop->screenNumber(vncUi.kcfg_ScalingHeight);
        vncUi.kcfg_ScalingHeight->setValue(desktop->screenGeometry(currentScreen).height());
        vncUi.kcfg_ScalingWidth->setValue(desktop->screenGeometry(currentScreen).width());
        break;
    }
    case 6:
    default:
        break;
    }

    checkEnableCustomSize(index);
}

void VncHostPreferences::updateScaling(bool enabled)
{
    vncUi.resolutionComboBox->setEnabled(enabled);
    if(enabled) {
        checkEnableCustomSize(vncUi.resolutionComboBox->currentIndex());
    } else {
        checkEnableCustomSize(-1);
    }
}

void VncHostPreferences::checkEnableCustomSize(int index)
{
    const bool enabled = (index == 6);

    vncUi.kcfg_ScalingHeight->setEnabled(enabled);
    vncUi.kcfg_ScalingWidth->setEnabled(enabled);
    vncUi.heightLabel->setEnabled(enabled);
    vncUi.widthLabel->setEnabled(enabled);
}

void VncHostPreferences::acceptConfig()
{
    HostPreferences::acceptConfig();
    setQuality((RemoteView::Quality)(vncUi.kcfg_Quality->currentIndex() + 1));
    setWindowedScale(vncUi.kcfg_Scaling->isChecked());
    if(vncUi.kcfg_Scaling->isChecked()) {
        setHeight(vncUi.kcfg_ScalingHeight->value());
        setWidth(vncUi.kcfg_ScalingWidth->value());
    }
}

void VncHostPreferences::setQuality(RemoteView::Quality quality)
{
    if (quality >= 0 && quality <= 3)
        m_configGroup.writeEntry("quality", (int) quality);
}

RemoteView::Quality VncHostPreferences::quality()
{
    return (RemoteView::Quality) m_configGroup.readEntry("quality", (int) Settings::quality() + 1);
}

#include "vnchostpreferences.moc"
