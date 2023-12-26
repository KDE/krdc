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

#include <freerdp/locale/keyboard.h>

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

static const QHash<QString, int> rdpLayouts = {
    {QStringLiteral("ar"), KBD_ARABIC_101},
    {QStringLiteral("cs"), KBD_CZECH},
    {QStringLiteral("da"), KBD_DANISH},
    {QStringLiteral("de"), KBD_GERMAN},
    {QStringLiteral("de-ch"), KBD_SWISS_GERMAN},
    {QStringLiteral("en-dv"), KBD_UNITED_STATES_DVORAK},
    {QStringLiteral("en-gb"), KBD_UNITED_KINGDOM},
    {QStringLiteral("en-us"), KBD_UNITED_STATES_INTERNATIONAL},
    {QStringLiteral("es"), KBD_SPANISH},
    {QStringLiteral("et"), KBD_ESTONIAN},
    {QStringLiteral("fi"), KBD_FINNISH},
    {QStringLiteral("fo"), KBD_DANISH},
    {QStringLiteral("fr"), KBD_FRENCH},
    {QStringLiteral("fr-be"), KBD_BELGIAN_FRENCH},
    {QStringLiteral("fr-ca"), KBD_CANADIAN_FRENCH},
    {QStringLiteral("fr-ch"), KBD_SWISS_FRENCH},
    {QStringLiteral("he"), KBD_HEBREW},
    {QStringLiteral("hr"), KBD_CROATIAN},
    {QStringLiteral("hu"), KBD_HUNGARIAN},
    {QStringLiteral("is"), KBD_ICELANDIC},
    {QStringLiteral("it"), KBD_ITALIAN},
    {QStringLiteral("ja"), KBD_JAPANESE},
    {QStringLiteral("ko"), KBD_KOREAN},
    {QStringLiteral("lt"), KBD_LITHUANIAN_IBM},
    {QStringLiteral("lv"), KBD_LATVIAN},
    {QStringLiteral("mk"), KBD_FYRO_MACEDONIAN},
    {QStringLiteral("nl"), KBD_DUTCH},
    {QStringLiteral("nl-be"), KBD_BELGIAN_PERIOD},
    {QStringLiteral("no"), KBD_NORWEGIAN},
    {QStringLiteral("pl"), KBD_POLISH_PROGRAMMERS},
    {QStringLiteral("pt"), KBD_PORTUGUESE},
    {QStringLiteral("pt-br"), KBD_PORTUGUESE_BRAZILIAN_ABNT},
    {QStringLiteral("ru"), KBD_RUSSIAN},
    {QStringLiteral("sl"), KBD_SLOVENIAN},
    {QStringLiteral("sv"), KBD_SWEDISH},
    {QStringLiteral("th"), KBD_THAI_KEDMANEE},
    {QStringLiteral("tr"), KBD_TURKISH_Q},
};

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

    rdpUi.kcfg_ScaleToSize->setChecked(scaleToSize());
    rdpUi.kcfg_Height->setValue(height());
    rdpUi.kcfg_Width->setValue(width());
    rdpUi.kcfg_Resolution->setCurrentIndex(int(resolution()));
    rdpUi.kcfg_Acceleration->setCurrentIndex(int(acceleration()));
    rdpUi.kcfg_ColorDepth->setCurrentIndex(int(colorDepth()));
    rdpUi.kcfg_KeyboardLayout->setCurrentIndex(keymap2int(keyboardLayout()));
    rdpUi.kcfg_ShareMedia->setText(shareMedia());
    rdpUi.kcfg_TlsSecLevel->setCurrentIndex(int(tlsSecLevel()));

    // Have to call updateWidthHeight() here
    // We leverage the final part of this function to enable/disable kcfg_Height and kcfg_Width
    updateWidthHeight(resolution());

    connect(rdpUi.kcfg_Resolution, &QComboBox::currentIndexChanged, this, [this](int index) {
        updateWidthHeight(Resolution(index));
    });

    // Color depth depends on acceleration method, with the better ones only working with 32-bit
    // color. So ensure we reflect that in the settings UI.
    updateColorDepth(acceleration());
    connect(rdpUi.kcfg_Acceleration, &QComboBox::currentIndexChanged, this, [this](int index) {
        updateColorDepth(Acceleration(index));
    });

    return rdpPage;
}

void RdpHostPreferences::updateWidthHeight(Resolution resolution)
{
    switch (resolution) {
    case Resolution::Small:
        rdpUi.kcfg_Width->setValue(1280);
        rdpUi.kcfg_Height->setValue(720);
        break;
    case Resolution::Medium:
        rdpUi.kcfg_Width->setValue(1600);
        rdpUi.kcfg_Height->setValue(900);
        break;
    case Resolution::Large:
        rdpUi.kcfg_Width->setValue(1920);
        rdpUi.kcfg_Height->setValue(1080);
        break;
    case Resolution::MatchWindow: {
        auto *window = qApp->activeWindow();
        if (window->parentWidget()) {
            window = window->parentWidget();
        }
        rdpUi.kcfg_Width->setValue(window->width());
        rdpUi.kcfg_Height->setValue(window->height());
        break;
    }
    case Resolution::MatchScreen: {
        QWindow *window = rdpUi.kcfg_Width->window()->windowHandle();
        QScreen *screen = window ? window->screen() : qGuiApp->primaryScreen();
        const QSize size = screen->size() * screen->devicePixelRatio();

        rdpUi.kcfg_Width->setValue(size.width());
        rdpUi.kcfg_Height->setValue(size.height());
        break;
    }
    case Resolution::Custom:
    default:
        break;
    }

    const bool enabled = resolution == Resolution::Custom;

    rdpUi.kcfg_Height->setEnabled(enabled);
    rdpUi.kcfg_Width->setEnabled(enabled);
    rdpUi.heightLabel->setEnabled(enabled);
    rdpUi.widthLabel->setEnabled(enabled);
}

void RdpHostPreferences::acceptConfig()
{
    HostPreferences::acceptConfig();

    setScaleToSize(rdpUi.kcfg_ScaleToSize->isChecked());
    setWidth(rdpUi.kcfg_Width->value());
    setHeight(rdpUi.kcfg_Height->value());
    setResolution(Resolution(rdpUi.kcfg_Resolution->currentIndex()));
    setAcceleration(Acceleration(rdpUi.kcfg_Acceleration->currentIndex()));
    setColorDepth(ColorDepth(rdpUi.kcfg_ColorDepth->currentIndex()));
    setKeyboardLayout(int2keymap(rdpUi.kcfg_KeyboardLayout->currentIndex()));
    setSound(Sound(rdpUi.kcfg_Sound->currentIndex()));
    setShareMedia(rdpUi.kcfg_ShareMedia->text());
    setTlsSecLevel(TlsSecLevel(rdpUi.kcfg_TlsSecLevel->currentIndex()));
}

bool RdpHostPreferences::scaleToSize() const
{
    return m_configGroup.readEntry("scaleToSize", true);
}

void RdpHostPreferences::setScaleToSize(bool scale)
{
    m_configGroup.writeEntry("scaleToSize", scale);
}

RdpHostPreferences::Resolution RdpHostPreferences::resolution() const
{
    return Resolution(m_configGroup.readEntry("resolution", Settings::resolution()));
}

void RdpHostPreferences::setResolution(Resolution resolution)
{
    m_configGroup.writeEntry("resolution", int(resolution));
}

RdpHostPreferences::Acceleration RdpHostPreferences::acceleration() const
{
    return Acceleration(m_configGroup.readEntry("acceleration", Settings::acceleration()));
}

void RdpHostPreferences::setAcceleration(Acceleration acceleration)
{
    m_configGroup.writeEntry("acceleration", int(acceleration));
}

void RdpHostPreferences::setColorDepth(ColorDepth colorDepth)
{
    m_configGroup.writeEntry("colorDepth", int(colorDepth));
}

RdpHostPreferences::ColorDepth RdpHostPreferences::colorDepth() const
{
    return ColorDepth(m_configGroup.readEntry("colorDepth", Settings::colorDepth()));
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

int RdpHostPreferences::rdpKeyboardLayout() const
{
    auto layout = keyboardLayout();
    return rdpLayouts.value(layout, KBD_UNITED_STATES_INTERNATIONAL);
}

void RdpHostPreferences::setSound(Sound sound)
{
    m_configGroup.writeEntry("sound", int(sound));
}

RdpHostPreferences::Sound RdpHostPreferences::sound() const
{
    return Sound(m_configGroup.readEntry("sound", Settings::sound()));
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

void RdpHostPreferences::updateColorDepth(Acceleration acceleration)
{
    switch (acceleration) {
    case Acceleration::ForceGraphicsPipeline:
    case Acceleration::ForceRemoteFx:
        rdpUi.kcfg_ColorDepth->setEnabled(false);
        rdpUi.kcfg_ColorDepth->setCurrentIndex(0);
        break;
    case Acceleration::Disabled:
    case Acceleration::Auto:
        rdpUi.kcfg_ColorDepth->setEnabled(true);
    }
}

void RdpHostPreferences::setTlsSecLevel(TlsSecLevel tlsSecLevel)
{
    m_configGroup.writeEntry("tlsSecLevel", int(tlsSecLevel));
}

RdpHostPreferences::TlsSecLevel RdpHostPreferences::tlsSecLevel() const
{
    return TlsSecLevel(m_configGroup.readEntry("tlsSecLevel", Settings::tlsSecLevel()));
}
