/*
    SPDX-FileCopyrightText: 2007 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "vnchostpreferences.h"

#include "settings.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

static const char quality_config_key[] = "quality";
static const char dont_copy_passwords_config_key[] = "dont_copy_passwords";

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

    vncUi.kcfg_Quality->setCurrentIndex(quality() - 1);
    vncUi.kcfg_Scaling->setChecked(windowedScale());
    vncUi.kcfg_ScalingWidth->setValue(width());
    vncUi.kcfg_ScalingHeight->setValue(height());

    connect(vncUi.resolutionComboBox, SIGNAL(currentIndexChanged(int)), SLOT(updateScalingWidthHeight(int)));
    connect(vncUi.kcfg_Scaling, SIGNAL(toggled(bool)), SLOT(updateScaling(bool)));

    const QString resolutionString = QString::number(width()) + QLatin1Char('x') + QString::number(height());
    const int resolutionIndex = vncUi.resolutionComboBox->findText(resolutionString, Qt::MatchContains);
    vncUi.resolutionComboBox->setCurrentIndex((resolutionIndex == -1) ? vncUi.resolutionComboBox->count() - 1 : resolutionIndex);

    updateScaling(windowedScale());

    vncUi.dont_copy_passwords->setChecked(dontCopyPasswords());

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
        QWindow *window = vncUi.kcfg_ScalingWidth->window()->windowHandle();
        QScreen *screen = window ? window->screen() : qGuiApp->primaryScreen();
        const QSize size = screen->size() * screen->devicePixelRatio();

        vncUi.kcfg_ScalingWidth->setValue(size.width());
        vncUi.kcfg_ScalingHeight->setValue(size.height());
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
    if (enabled) {
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
    if (vncUi.kcfg_Scaling->isChecked()) {
        setHeight(vncUi.kcfg_ScalingHeight->value());
        setWidth(vncUi.kcfg_ScalingWidth->value());
    }
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
