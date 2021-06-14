/*
    SPDX-FileCopyrightText: 2007-2012 Urs Wolfer <uwolfer@kde.org>
    SPDX-FileCopyrightText: 2012 AceLan Kao <acelan@acelan.idv.tw>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rdphostpreferences.h"

#include "settings.h"

#include <QScreen>
#include <QWindow>
#include <QGuiApplication>

static const QStringList keymaps = (QStringList()
    << QStringLiteral("ar")
    << QStringLiteral("cs")
    << QStringLiteral("da")
    << QStringLiteral("de")
    << QStringLiteral("de-ch")
    << QStringLiteral("en-dv")
    << QStringLiteral("en-gb")
    << QStringLiteral("en-us")
    << QStringLiteral("es")
    << QStringLiteral("et")
    << QStringLiteral("fi")
    << QStringLiteral("fo")
    << QStringLiteral("fr")
    << QStringLiteral("fr-be")
    << QStringLiteral("fr-ca")
    << QStringLiteral("fr-ch")
    << QStringLiteral("he")
    << QStringLiteral("hr")
    << QStringLiteral("hu")
    << QStringLiteral("is")
    << QStringLiteral("it")
    << QStringLiteral("ja")
    << QStringLiteral("ko")
    << QStringLiteral("lt")
    << QStringLiteral("lv")
    << QStringLiteral("mk")
    << QStringLiteral("nl")
    << QStringLiteral("nl-be")
    << QStringLiteral("no")
    << QStringLiteral("pl")
    << QStringLiteral("pt")
    << QStringLiteral("pt-br")
    << QStringLiteral("ru")
    << QStringLiteral("sl")
    << QStringLiteral("sv")
    << QStringLiteral("th")
    << QStringLiteral("tr")
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

    connect(rdpUi.kcfg_Sound, SIGNAL(currentIndexChanged(int)), SLOT(updateSoundSystem(int)));
    connect(rdpUi.browseMediaButton, SIGNAL(released()), SLOT(browseMedia()));

    rdpUi.loginGroupBox->setVisible(false);

    rdpUi.kcfg_Height->setValue(height());
    rdpUi.kcfg_Width->setValue(width());
    rdpUi.kcfg_Resolution->setCurrentIndex(resolution());
    rdpUi.kcfg_ColorDepth->setCurrentIndex(colorDepth());
    rdpUi.kcfg_KeyboardLayout->setCurrentIndex(keymap2int(keyboardLayout()));
    rdpUi.kcfg_Sound->setCurrentIndex(sound());
    rdpUi.kcfg_SoundSystem->setCurrentIndex(soundSystem());
    rdpUi.kcfg_Console->setChecked(console());
    rdpUi.kcfg_ExtraOptions->setText(extraOptions());
    rdpUi.kcfg_RemoteFX->setChecked(remoteFX());
    rdpUi.kcfg_Performance->setCurrentIndex(performance());
    rdpUi.kcfg_ShareMedia->setText(shareMedia());

    // Have to call updateWidthHeight() here
    // We leverage the final part of this function to enable/disable kcfg_Height and kcfg_Width
    updateWidthHeight(resolution());

    connect(rdpUi.kcfg_Resolution, SIGNAL(currentIndexChanged(int)), SLOT(updateWidthHeight(int)));

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
        QWindow *window = rdpUi.kcfg_Width->window()->windowHandle();
        QScreen *screen = window ? window->screen() : qGuiApp->primaryScreen();
        const QSize size = screen->size() * screen->devicePixelRatio();

        rdpUi.kcfg_Width->setValue(size.width());
        rdpUi.kcfg_Height->setValue(size.height());
        break;
    }
    case 7:
        rdpUi.kcfg_Height->setValue(0);
        rdpUi.kcfg_Width->setValue(0);
        break;
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

void RdpHostPreferences::updateSoundSystem(int index)
{
    switch (index) {
    case 0: /* On This Computer */
        rdpUi.kcfg_SoundSystem->setCurrentIndex(soundSystem());
        rdpUi.kcfg_SoundSystem->setEnabled(true);
        break;
    case 1: /* On Remote Computer */
    case 2: /* Disable Sound */
        rdpUi.kcfg_SoundSystem->setCurrentIndex(2);
        rdpUi.kcfg_SoundSystem->setEnabled(false);
        break;
    default:
        break;
    }
}

void RdpHostPreferences::browseMedia()
{
    QString shareDir = QFileDialog::getExistingDirectory(rdpUi.browseMediaButton, i18n("Browse to media share path"), rdpUi.kcfg_ShareMedia->text());
    if (!shareDir.isNull()) {
        rdpUi.kcfg_ShareMedia->setText(shareDir);
    }
}

void RdpHostPreferences::acceptConfig()
{
    HostPreferences::acceptConfig();

    setHeight(rdpUi.kcfg_Height->value());
    setWidth(rdpUi.kcfg_Width->value());
    setResolution(rdpUi.kcfg_Resolution->currentIndex());
    setColorDepth(rdpUi.kcfg_ColorDepth->currentIndex());
    setKeyboardLayout(int2keymap(rdpUi.kcfg_KeyboardLayout->currentIndex()));
    setSound(rdpUi.kcfg_Sound->currentIndex());
    setSoundSystem(rdpUi.kcfg_SoundSystem->currentIndex());
    setConsole(rdpUi.kcfg_Console->isChecked());
    setExtraOptions(rdpUi.kcfg_ExtraOptions->text());
    setRemoteFX(rdpUi.kcfg_RemoteFX->isChecked());
    setPerformance(rdpUi.kcfg_Performance->currentIndex());
    setShareMedia(rdpUi.kcfg_ShareMedia->text());
}

void RdpHostPreferences::setResolution(int resolution)
{
    if (resolution >= 0)
        m_configGroup.writeEntry("resolution", resolution);
}

int RdpHostPreferences::resolution() const
{
    return m_configGroup.readEntry("resolution", Settings::resolution());
}

void RdpHostPreferences::setColorDepth(int colorDepth)
{
    if (colorDepth >= 0)
        m_configGroup.writeEntry("colorDepth", colorDepth);
}

int RdpHostPreferences::colorDepth() const
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

void RdpHostPreferences::setSoundSystem(int sound)
{
    if (sound >= 0)
        m_configGroup.writeEntry("soundSystem", sound);
}

int RdpHostPreferences::soundSystem() const
{
    return m_configGroup.readEntry("soundSystem", Settings::soundSystem());
}

void RdpHostPreferences::setConsole(bool console)
{
    m_configGroup.writeEntry("console", console);
}

bool RdpHostPreferences::console() const
{
    return m_configGroup.readEntry("console", Settings::console());
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

void RdpHostPreferences::setRemoteFX(bool remoteFX)
{
    m_configGroup.writeEntry("remoteFX", remoteFX);
}

bool RdpHostPreferences::remoteFX() const
{
    return m_configGroup.readEntry("remoteFX", Settings::remoteFX());
}

void RdpHostPreferences::setPerformance(int performance)
{
    if (performance >= 0)
        m_configGroup.writeEntry("performance", performance);
}

int RdpHostPreferences::performance() const
{
    return m_configGroup.readEntry("performance", Settings::performance());
}

void RdpHostPreferences::setShareMedia(const QString &shareMedia)
{
    if (!shareMedia.isNull())
        m_configGroup.writeEntry("shareMedia", shareMedia);
}

QString RdpHostPreferences::shareMedia() const
{
    return m_configGroup.readEntry("shareMedia", Settings::shareMedia());
}

