/*
    SPDX-FileCopyrightText: 2024 KRDC Contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "spicehostpreferences.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

static const char enable_audio_config_key[] = "enableAudio";
static const char resolution_config_key[] = "resolution";

SpiceHostPreferences::SpiceHostPreferences(KConfigGroup configGroup, QObject *parent)
    : HostPreferences(configGroup, parent)
{
}

SpiceHostPreferences::~SpiceHostPreferences()
{
}

QWidget *SpiceHostPreferences::createProtocolSpecificConfigPage(QWidget *sshTunnelWidget)
{
    QWidget *spicePage = new QWidget();
    spiceUi.setupUi(spicePage);
    if (sshTunnelWidget) {
        spiceUi.sshTunnelLayout->addWidget(sshTunnelWidget);
    }

    spiceUi.kcfg_EnableAudio->setChecked(enableAudio());
    spiceUi.kcfg_Resolution->setCurrentIndex(int(resolution()));
    spiceUi.kcfg_Width->setValue(width());
    spiceUi.kcfg_Height->setValue(height());
    spiceUi.kcfg_ScaleToWindow->setChecked(windowedScale());

    updateWidthHeight(resolution());

    connect(spiceUi.kcfg_Resolution, &QComboBox::currentIndexChanged, this, [this](int index) {
        updateWidthHeight(Resolution(index));
    });

    return spicePage;
}

void SpiceHostPreferences::updateWidthHeight(Resolution res)
{
    switch (res) {
    case Resolution::Small:
        spiceUi.kcfg_Width->setValue(1280);
        spiceUi.kcfg_Height->setValue(720);
        break;
    case Resolution::Medium:
        spiceUi.kcfg_Width->setValue(1600);
        spiceUi.kcfg_Height->setValue(900);
        break;
    case Resolution::Large:
        spiceUi.kcfg_Width->setValue(1920);
        spiceUi.kcfg_Height->setValue(1080);
        break;
    case Resolution::MatchWindow: {
        auto *window = qApp->activeWindow();
        if (window && window->parentWidget())
            window = window->parentWidget();
        if (window) {
            spiceUi.kcfg_Width->setValue(window->width() * window->devicePixelRatio());
            spiceUi.kcfg_Height->setValue(window->height() * window->devicePixelRatio());
        }
        break;
    }
    case Resolution::MatchScreen: {
        QWindow *win = spiceUi.kcfg_Width->window()->windowHandle();
        QScreen *screen = win ? win->screen() : qGuiApp->primaryScreen();
        const QSize size = screen->size() * screen->devicePixelRatio();
        spiceUi.kcfg_Width->setValue(size.width());
        spiceUi.kcfg_Height->setValue(size.height());
        break;
    }
    case Resolution::Custom:
    default:
        break;
    }

    const bool enabled = (res == Resolution::Custom);
    spiceUi.kcfg_Width->setEnabled(enabled);
    spiceUi.kcfg_Height->setEnabled(enabled);
    spiceUi.widthLabel->setEnabled(enabled);
    spiceUi.heightLabel->setEnabled(enabled);
}

void SpiceHostPreferences::acceptConfig()
{
    HostPreferences::acceptConfig();
    setEnableAudio(spiceUi.kcfg_EnableAudio->isChecked());
    setResolution(Resolution(spiceUi.kcfg_Resolution->currentIndex()));
    setWidth(spiceUi.kcfg_Width->value());
    setHeight(spiceUi.kcfg_Height->value());
    setWindowedScale(spiceUi.kcfg_ScaleToWindow->isChecked());
}

bool SpiceHostPreferences::enableAudio() const
{
    return m_configGroup.readEntry(enable_audio_config_key, true);
}

void SpiceHostPreferences::setEnableAudio(bool enable)
{
    m_configGroup.writeEntry(enable_audio_config_key, enable);
}

SpiceHostPreferences::Resolution SpiceHostPreferences::resolution() const
{
    return Resolution(m_configGroup.readEntry(resolution_config_key, int(Resolution::MatchWindow)));
}

void SpiceHostPreferences::setResolution(Resolution res)
{
    m_configGroup.writeEntry(resolution_config_key, int(res));
}
