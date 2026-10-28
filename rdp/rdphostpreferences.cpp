/*
    SPDX-FileCopyrightText: 2007-2012 Urs Wolfer <uwolfer@kde.org>
    SPDX-FileCopyrightText: 2012 AceLan Kao <acelan@acelan.idv.tw>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rdphostpreferences.h"
#include "rdpsettingsform.h"

#include "krdc_debug.h"
#include "settings.h"

#include <KConfig>
#include <KConfigGroup>

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

#ifdef HAVE_DBUS
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingReply>
#endif

#include <freerdp/locale/keyboard.h>

#include <iterator>
#include <string_view>

// Detects the layout of this computer when connecting.
static constexpr std::string_view autoKeymap = "auto";

static constexpr std::string_view keymaps[] = {
    "ar",
    "cs",
    "da",
    "de",
    "de-ch",
    "en-dv",
    "en-gb",
    "en-us",
    "es",
    "et",
    "fi",
    "fo",
    "fr",
    "fr-be",
    "fr-ca",
    "fr-ch",
    "he",
    "hr",
    "hu",
    "is",
    "it",
    "ja",
    "ko",
    "lt",
    "lv",
    "mk",
    "nl",
    "nl-be",
    "no",
    "pl",
    "pt",
    "pt-br",
    "ru",
    "sl",
    "sv",
    "th",
    "tr",
    // Last, so that the stored indices of the other layouts stay valid.
    autoKeymap,
};

static_assert(std::size(keymaps) == RdpHostPreferences::AutoKeyboardLayout + 1 && keymaps[RdpHostPreferences::AutoKeyboardLayout] == autoKeymap);
static_assert(keymaps[RdpHostPreferences::FallbackKeyboardLayout] == "en-us");

static QLatin1String latin1(std::string_view keymap)
{
    return QLatin1String(keymap.data(), qsizetype(keymap.size()));
}

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

inline int keymap2int(const QString &keymap)
{
    for (int index = int(std::size(keymaps)) - 1; index >= 0; --index) {
        if (keymap == latin1(keymaps[index])) {
            return index;
        }
    }
    return RdpHostPreferences::FallbackKeyboardLayout;
}

inline QString int2keymap(int layout)
{
    if (layout >= 0 && layout < int(std::size(keymaps)))
        return latin1(keymaps[layout]);
    else
        return latin1(keymaps[RdpHostPreferences::FallbackKeyboardLayout]);
}

// One-shot migration of the old free-text "authPkgList" entry (e.g.
// "!ntlm,kerberos") into the per-package boolean entries. Runs once per
// host group; afterwards the legacy entry is gone.
static void migrateAuthPkgList(KConfigGroup &group)
{
    if (group.hasKey("authPkgKerberos") || group.hasKey("authPkgNtlm") || group.hasKey("authPkgU2u")) {
        return;
    }
    const QString legacy = group.readEntry("authPkgList", QString());
    if (legacy.isEmpty()) {
        return;
    }
    group.deleteEntry("authPkgList");
    if (legacy.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0) {
        return;
    }
    const auto tokens = legacy.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (QString token : tokens) {
        const bool denied = token.trimmed().startsWith(QLatin1Char('!'));
        const QString name = token.trimmed().remove(QLatin1Char('!')).toLower();
        if (name == QLatin1String("kerberos")) {
            group.writeEntry("authPkgKerberos", !denied);
        } else if (name == QLatin1String("ntlm")) {
            group.writeEntry("authPkgNtlm", !denied);
        } else if (name == QLatin1String("u2u")) {
            group.writeEntry("authPkgU2u", !denied);
        }
    }
}

// Before "useGateway" and "useProxy", a route was used whenever its server was set, and the
// shared folder needs migratedShareMedia(). Saving a host writes "useGateway", so a host without
// it was saved by an older version; "settingsMigrated" marks one that is already migrated, as a
// host may have none of the keys that are migrated. Settings a host does not have follow the
// default settings, which migrateRdpDefaults() handles.
static void migrateHostSettings(KConfigGroup &group)
{
    if (!group.exists() || group.hasKey("useGateway") || group.hasKey("settingsMigrated")) {
        return;
    }
    group.writeEntry("settingsMigrated", true);
    if (group.hasKey("gatewayServer")) {
        group.writeEntry("useGateway", !group.readEntry("gatewayServer", QString()).isEmpty());
    }
    if (group.hasKey("proxyHost")) {
        group.writeEntry("useProxy", !group.readEntry("proxyHost", QString()).isEmpty());
    }
    if (group.hasKey("shareMedia")) {
        group.writeEntry("shareMedia", migratedShareMedia(group.readEntry("shareMedia", QString())));
    }
}

// Returns an entry of keymaps, or an empty string if the XKB layout has no RDP equivalent.
static QString keymapForXkbLayout(const QString &layout, const QString &variant)
{
    // clang-format off
    static const QHash<QString, QString> xkbLayouts = {
        {QStringLiteral("ara"), QStringLiteral("ar")},
        {QStringLiteral("be"), QStringLiteral("fr-be")},
        {QStringLiteral("br"), QStringLiteral("pt-br")},
        {QStringLiteral("ca"), QStringLiteral("fr-ca")},
        {QStringLiteral("ch"), QStringLiteral("de-ch")},
        {QStringLiteral("cz"), QStringLiteral("cs")},
        {QStringLiteral("de"), QStringLiteral("de")},
        {QStringLiteral("dk"), QStringLiteral("da")},
        {QStringLiteral("ee"), QStringLiteral("et")},
        {QStringLiteral("es"), QStringLiteral("es")},
        {QStringLiteral("fi"), QStringLiteral("fi")},
        {QStringLiteral("fo"), QStringLiteral("fo")},
        {QStringLiteral("fr"), QStringLiteral("fr")},
        {QStringLiteral("gb"), QStringLiteral("en-gb")},
        {QStringLiteral("hr"), QStringLiteral("hr")},
        {QStringLiteral("hu"), QStringLiteral("hu")},
        {QStringLiteral("il"), QStringLiteral("he")},
        {QStringLiteral("is"), QStringLiteral("is")},
        {QStringLiteral("it"), QStringLiteral("it")},
        {QStringLiteral("jp"), QStringLiteral("ja")},
        {QStringLiteral("kr"), QStringLiteral("ko")},
        {QStringLiteral("lt"), QStringLiteral("lt")},
        {QStringLiteral("lv"), QStringLiteral("lv")},
        {QStringLiteral("mk"), QStringLiteral("mk")},
        {QStringLiteral("nl"), QStringLiteral("nl")},
        {QStringLiteral("no"), QStringLiteral("no")},
        {QStringLiteral("pl"), QStringLiteral("pl")},
        {QStringLiteral("pt"), QStringLiteral("pt")},
        {QStringLiteral("ru"), QStringLiteral("ru")},
        {QStringLiteral("se"), QStringLiteral("sv")},
        {QStringLiteral("si"), QStringLiteral("sl")},
        {QStringLiteral("th"), QStringLiteral("th")},
        {QStringLiteral("tr"), QStringLiteral("tr")},
        {QStringLiteral("us"), QStringLiteral("en-us")},
    };
    // clang-format on

    if (layout == QLatin1String("us") && (variant.contains(QLatin1String("dvorak")) || variant == QLatin1String("dvp"))) {
        return QStringLiteral("en-dv");
    }
    if (layout == QLatin1String("ch") && variant.startsWith(QLatin1String("fr"))) {
        return QStringLiteral("fr-ch");
    }
    if (layout == QLatin1String("ca") && variant == QLatin1String("eng")) {
        return QStringLiteral("en-us");
    }
    if (layout == QLatin1String("tr") && variant == QLatin1String("f")) {
        return {};
    }
    return xkbLayouts.value(layout);
}

struct XkbLayouts {
    QStringList layouts;
    QStringList variants;
};

// Returns the layouts that the session applies, in the order that Plasma switches between them.
static XkbLayouts configuredLayouts()
{
    // Plasma applies the layouts from kxkbrc on X11 and Wayland when the user configured them.
    // Other desktops ignore kxkbrc, which may be left over from an earlier Plasma session.
    if (qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(QLatin1Char(':')).contains(QLatin1String("KDE"))) {
        const KConfig kxkbConfig(QStringLiteral("kxkbrc"), KConfig::NoGlobals);
        const KConfigGroup kxkbLayout = kxkbConfig.group(QStringLiteral("Layout"));
        if (kxkbLayout.readEntry("Use", false)) {
            return {kxkbLayout.readEntry("LayoutList", QStringList()), kxkbLayout.readEntry("VariantList", QStringList())};
        }
    }

    // Otherwise the Wayland session uses the system layout, which Plasma passes on in these variables.
    return {qEnvironmentVariable("XKB_DEFAULT_LAYOUT").split(QLatin1Char(',')), qEnvironmentVariable("XKB_DEFAULT_VARIANT").split(QLatin1Char(','))};
}

#ifdef HAVE_DBUS
// Reads the active layout from Plasma, which the user may have switched away from the first configured one.
static bool activePlasmaLayout(qsizetype &index, QString &layout)
{
    const auto call = [](const QString &method) {
        const QDBusMessage message =
            QDBusMessage::createMethodCall(QStringLiteral("org.kde.keyboard"), QStringLiteral("/Layouts"), QStringLiteral("org.kde.KeyboardLayouts"), method);
        // This runs while connecting, so do not hold up the user for long.
        return QDBusConnection::sessionBus().asyncCall(message, 500);
    };

    QDBusPendingReply<uint> indexReply = call(QStringLiteral("getLayout"));
    QDBusPendingCall listCall = call(QStringLiteral("getLayoutsList"));
    indexReply.waitForFinished();
    listCall.waitForFinished();
    if (indexReply.isError() || listCall.isError()) {
        return false;
    }
    const QDBusMessage listReply = listCall.reply();
    if (listReply.arguments().isEmpty()) {
        return false;
    }

    // Each entry is (short name, display name, long name). The list does not include variants.
    const QDBusArgument list = listReply.arguments().constFirst().value<QDBusArgument>();
    QStringList layouts;
    list.beginArray();
    while (!list.atEnd()) {
        QString shortName;
        QString displayName;
        QString longName;
        list.beginStructure();
        list >> shortName >> displayName >> longName;
        list.endStructure();
        layouts << shortName;
    }
    list.endArray();

    // The layouts may have changed between the two calls.
    if (indexReply.value() >= uint(layouts.size())) {
        return false;
    }
    index = indexReply.value();
    layout = layouts.at(index);
    return true;
}
#endif

// Returns the entry of keymaps for the user's active keyboard layout, or an empty string if it is unknown.
static QString systemKeymap()
{
    const XkbLayouts configured = configuredLayouts();
    qsizetype index = 0;
    QString layout = configured.layouts.value(0).trimmed();

#ifdef HAVE_DBUS
    activePlasmaLayout(index, layout);
#endif

    // Plasma reports only the layout, so take the variant from the configured layouts when they agree.
    const QString variant = configured.layouts.value(index).trimmed() == layout ? configured.variants.value(index).trimmed() : QString();

    const QString keymap = keymapForXkbLayout(layout, variant);
    qCDebug(KRDC) << "Active keyboard layout" << layout << variant << "maps to RDP keymap" << keymap;
    return keymap;
}

RdpHostPreferences::RdpHostPreferences(KConfigGroup configGroup, QObject *parent)
    : HostPreferences(configGroup, parent)
{
    migrateAuthPkgList(m_configGroup);
    migrateRdpDefaults();
    migrateHostSettings(m_configGroup);
}

RdpHostPreferences::~RdpHostPreferences()
{
}

QWidget *RdpHostPreferences::createProtocolSpecificConfigPage(QWidget *sshTunnelWidget)
{
    QWidget *rdpPage = new QWidget();
    rdpUi.setupUi(rdpPage);
    if (sshTunnelWidget) {
        rdpUi.sshTunnelLayout->addWidget(sshTunnelWidget);
    } else {
        rdpUi.sshTunnelSection->hide();
    }
    addQuickSetting(rdpUi.resolutionField);
    addQuickSetting(rdpUi.kcfg_Sound);
    addQuickSetting(rdpUi.kcfg_ShareMedia);

    rdpUi.kcfg_ScaleToSize->setChecked(scaleToSize());
    rdpUi.kcfg_ConsoleMode->setChecked(consoleMode());
    rdpUi.kcfg_PreventSessionLock->setChecked(preventSessionLock());
    rdpUi.kcfg_Height->setValue(height());
    rdpUi.kcfg_Width->setValue(width());
    rdpUi.kcfg_Resolution->setCurrentIndex(int(resolution()));
    rdpUi.kcfg_Acceleration->setCurrentIndex(int(acceleration()));
    rdpUi.kcfg_ColorDepth->setCurrentIndex(int(colorDepth()));
    rdpUi.kcfg_DesktopScaleFactor->setCurrentIndex(int(desktopScaleFactor()));
    rdpUi.kcfg_DesktopScaleFactorCustom->setValue(desktopScaleFactorCustom());
    rdpUi.kcfg_DeviceScaleFactor->setCurrentIndex(int(deviceScaleFactor()));
    rdpUi.kcfg_KeyboardLayout->setCurrentIndex(keyboardLayoutRow(keymap2int(keyboardLayout())));
    rdpUi.kcfg_Sound->setCurrentIndex(int(sound()));
    rdpUi.kcfg_ShareMedia->setFolder(shareMedia());
    rdpUi.kcfg_TlsSecLevel->setCurrentIndex(int(tlsSecLevel()));
    rdpUi.kcfg_UseProxy->setChecked(useProxy());
    rdpUi.kcfg_ProxyProtocol->setCurrentIndex(int(proxyProtocol()));
    rdpUi.kcfg_ProxyUsername->setText(proxyUsername());
    rdpUi.kcfg_ProxyPassword->setPassword(proxyPassword());
    rdpUi.kcfg_ProxyHost->setText(proxyHost());
    rdpUi.kcfg_UseGateway->setChecked(useGateway());
    rdpUi.kcfg_GatewayTransportType->setCurrentIndex(int(gatewayTransportType()));
    rdpUi.kcfg_GatewayUsername->setText(gatewayUsername());
    rdpUi.kcfg_GatewayPassword->setPassword(gatewayPassword());
    rdpUi.kcfg_GatewayServer->setText(gatewayServer());
    rdpUi.kcfg_GatewayDomain->setText(gatewayDomain());
    rdpUi.kcfg_SmartcardName->setText(smartcardName());
    rdpUi.kcfg_SecurityNLA->setChecked(securityNLA());
    rdpUi.kcfg_SecurityTLS->setChecked(securityTLS());
    rdpUi.kcfg_SecurityRDP->setChecked(securityRDP());
    rdpUi.kcfg_SecurityEXT->setChecked(securityEXT());
    rdpUi.kcfg_AuthPkgKerberos->setChecked(authPkgKerberos());
    rdpUi.kcfg_AuthPkgNtlm->setChecked(authPkgNtlm());
    rdpUi.kcfg_AuthPkgU2u->setChecked(authPkgU2u());

    setupRdpSettingsForm(rdpUi);
    updateWidthHeight(resolution());
    connect(rdpUi.kcfg_Resolution, &QComboBox::currentIndexChanged, this, [this](int index) {
        updateWidthHeight(Resolution(index));
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
        if (window && window->parentWidget()) {
            window = window->parentWidget();
        }
        if (window) {
            rdpUi.kcfg_Width->setValue(window->width() * window->devicePixelRatio());
            rdpUi.kcfg_Height->setValue(window->height() * window->devicePixelRatio());
        }
        break;
    }
    case Resolution::MatchScreen:
    case Resolution::AllScreens: {
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
}

void RdpHostPreferences::acceptConfig()
{
    HostPreferences::acceptConfig();

    setScaleToSize(rdpUi.kcfg_ScaleToSize->isChecked());
    setConsoleMode(rdpUi.kcfg_ConsoleMode->isChecked());
    setPreventSessionLock(rdpUi.kcfg_PreventSessionLock->isChecked());
    setWidth(rdpUi.kcfg_Width->value());
    setHeight(rdpUi.kcfg_Height->value());
    setResolution(Resolution(rdpUi.kcfg_Resolution->currentIndex()));
    setAcceleration(Acceleration(rdpUi.kcfg_Acceleration->currentIndex()));
    setColorDepth(ColorDepth(rdpUi.kcfg_ColorDepth->currentIndex()));
    setDesktopScaleFactor(DesktopScaleFactor(rdpUi.kcfg_DesktopScaleFactor->currentIndex()));
    setDesktopScaleFactorCustom(rdpUi.kcfg_DesktopScaleFactorCustom->value());
    setDeviceScaleFactor(DeviceScaleFactor(rdpUi.kcfg_DeviceScaleFactor->currentIndex()));
    setKeyboardLayout(int2keymap(keyboardLayoutFromRow(rdpUi.kcfg_KeyboardLayout->currentIndex())));
    setSound(Sound(rdpUi.kcfg_Sound->currentIndex()));
    setShareMedia(rdpUi.kcfg_ShareMedia->folder());
    setTlsSecLevel(TlsSecLevel(rdpUi.kcfg_TlsSecLevel->currentIndex()));
    setUseProxy(rdpUi.kcfg_UseProxy->isChecked());
    setProxyProtocol(ProxyProtocol(rdpUi.kcfg_ProxyProtocol->currentIndex()));
    setProxyUsername(rdpUi.kcfg_ProxyUsername->text());
    setProxyPassword(rdpUi.kcfg_ProxyPassword->password());
    setProxyHost(rdpUi.kcfg_ProxyHost->text());
    setUseGateway(rdpUi.kcfg_UseGateway->isChecked());
    setGatewayTransportType(GatewayTransportType(rdpUi.kcfg_GatewayTransportType->currentIndex()));
    setGatewayUsername(rdpUi.kcfg_GatewayUsername->text());
    setGatewayPassword(rdpUi.kcfg_GatewayPassword->password());
    setGatewayServer(rdpUi.kcfg_GatewayServer->text());
    setGatewayDomain(rdpUi.kcfg_GatewayDomain->text());
    setSmartcardName(rdpUi.kcfg_SmartcardName->text());
    setSecurityNLA(rdpUi.kcfg_SecurityNLA->isChecked());
    setSecurityTLS(rdpUi.kcfg_SecurityTLS->isChecked());
    setSecurityRDP(rdpUi.kcfg_SecurityRDP->isChecked());
    setSecurityEXT(rdpUi.kcfg_SecurityEXT->isChecked());
    setAuthPkgKerberos(rdpUi.kcfg_AuthPkgKerberos->isChecked());
    setAuthPkgNtlm(rdpUi.kcfg_AuthPkgNtlm->isChecked());
    setAuthPkgU2u(rdpUi.kcfg_AuthPkgU2u->isChecked());
}

bool RdpHostPreferences::scaleToSize() const
{
    return m_configGroup.readEntry("scaleToSize", true);
}

void RdpHostPreferences::setScaleToSize(bool scale)
{
    m_configGroup.writeEntry("scaleToSize", scale);
}

bool RdpHostPreferences::consoleMode() const
{
    return m_configGroup.readEntry("consoleMode", Settings::consoleMode());
}

void RdpHostPreferences::setConsoleMode(bool enabled)
{
    m_configGroup.writeEntry("consoleMode", enabled);
}

bool RdpHostPreferences::preventSessionLock() const
{
    return m_configGroup.readEntry("PreventSessionLock", Settings::preventSessionLock());
}

void RdpHostPreferences::setPreventSessionLock(bool enabled)
{
    m_configGroup.writeEntry("PreventSessionLock", enabled);
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
    QString layout = keyboardLayout();
    if (layout == latin1(autoKeymap)) {
        layout = systemKeymap();
    }
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
    m_configGroup.writeEntry("shareMedia", shareMedia);
}

QString RdpHostPreferences::shareMedia() const
{
    return m_configGroup.readEntry("shareMedia", Settings::shareMedia());
}

void RdpHostPreferences::setTlsSecLevel(TlsSecLevel tlsSecLevel)
{
    m_configGroup.writeEntry("tlsSecLevel", int(tlsSecLevel));
}

RdpHostPreferences::TlsSecLevel RdpHostPreferences::tlsSecLevel() const
{
    return TlsSecLevel(m_configGroup.readEntry("tlsSecLevel", Settings::tlsSecLevel()));
}

// Proxy settings
bool RdpHostPreferences::useProxy() const
{
    return m_configGroup.readEntry("useProxy", Settings::useProxy());
}
void RdpHostPreferences::setUseProxy(bool useProxy)
{
    m_configGroup.writeEntry("useProxy", useProxy);
}

RdpHostPreferences::ProxyProtocol RdpHostPreferences::proxyProtocol() const
{
    return ProxyProtocol(m_configGroup.readEntry("proxyProtocol", Settings::proxyProtocol()));
}

void RdpHostPreferences::setProxyProtocol(RdpHostPreferences::ProxyProtocol proxyProtocol)
{
    m_configGroup.writeEntry("proxyProtocol", int(proxyProtocol));
}

QString RdpHostPreferences::proxyUsername() const
{
    return QString(m_configGroup.readEntry("proxyUsername", Settings::proxyUsername()));
}
void RdpHostPreferences::setProxyUsername(const QString &proxyUsername)
{
    m_configGroup.writeEntry("proxyUsername", proxyUsername);
}

QString RdpHostPreferences::proxyPassword() const
{
    return QString(m_configGroup.readEntry("proxyPassword", Settings::proxyPassword()));
}
void RdpHostPreferences::setProxyPassword(const QString &proxyPassword)
{
    m_configGroup.writeEntry("proxyPassword", proxyPassword);
}

QString RdpHostPreferences::proxyHost() const
{
    return QString(m_configGroup.readEntry("proxyHost", Settings::proxyHost()));
}
void RdpHostPreferences::setProxyHost(const QString &proxyHost)
{
    m_configGroup.writeEntry("proxyHost", proxyHost);
}

// Gateway settings
bool RdpHostPreferences::useGateway() const
{
    return m_configGroup.readEntry("useGateway", Settings::useGateway());
}
void RdpHostPreferences::setUseGateway(bool useGateway)
{
    m_configGroup.writeEntry("useGateway", useGateway);
}

RdpHostPreferences::GatewayTransportType RdpHostPreferences::gatewayTransportType() const
{
    return GatewayTransportType(m_configGroup.readEntry("gatewayTransportType", int(Settings::gatewayTransportType())));
}
void RdpHostPreferences::setGatewayTransportType(RdpHostPreferences::GatewayTransportType gatewayTransportType)
{
    m_configGroup.writeEntry("gatewayTransportType", int(gatewayTransportType));
}

QString RdpHostPreferences::gatewayUsername() const
{
    return QString(m_configGroup.readEntry("gatewayUsername", Settings::gatewayUsername()));
}
void RdpHostPreferences::setGatewayUsername(const QString &gatewayUsername)
{
    m_configGroup.writeEntry("gatewayUsername", gatewayUsername);
}

QString RdpHostPreferences::gatewayPassword() const
{
    return QString(m_configGroup.readEntry("gatewayPassword", Settings::gatewayPassword()));
}
void RdpHostPreferences::setGatewayPassword(const QString &gatewayPassword)
{
    m_configGroup.writeEntry("gatewayPassword", gatewayPassword);
}

QString RdpHostPreferences::gatewayServer() const
{
    return QString(m_configGroup.readEntry("gatewayServer", Settings::gatewayServer()));
}
void RdpHostPreferences::setGatewayServer(const QString &gatewayServer)
{
    m_configGroup.writeEntry("gatewayServer", gatewayServer);
}

QString RdpHostPreferences::gatewayDomain() const
{
    return QString(m_configGroup.readEntry("gatewayDomain", Settings::gatewayDomain()));
}
void RdpHostPreferences::setGatewayDomain(const QString &gatewayDomain)
{
    m_configGroup.writeEntry("gatewayDomain", gatewayDomain);
}

QString RdpHostPreferences::smartcardName() const
{
    return QString(m_configGroup.readEntry("smartcardName", Settings::smartcardName()));
}
void RdpHostPreferences::setSmartcardName(const QString &smartcardName)
{
    m_configGroup.writeEntry("smartcardName", smartcardName);
}

bool RdpHostPreferences::securityNLA() const
{
    return m_configGroup.readEntry("securityNLA", Settings::securityNLA());
}
void RdpHostPreferences::setSecurityNLA(bool enabled)
{
    m_configGroup.writeEntry("securityNLA", enabled);
}
bool RdpHostPreferences::securityTLS() const
{
    return m_configGroup.readEntry("securityTLS", Settings::securityTLS());
}
void RdpHostPreferences::setSecurityTLS(bool enabled)
{
    m_configGroup.writeEntry("securityTLS", enabled);
}
bool RdpHostPreferences::securityRDP() const
{
    return m_configGroup.readEntry("securityRDP", Settings::securityRDP());
}
void RdpHostPreferences::setSecurityRDP(bool enabled)
{
    m_configGroup.writeEntry("securityRDP", enabled);
}
bool RdpHostPreferences::securityEXT() const
{
    return m_configGroup.readEntry("securityEXT", Settings::securityEXT());
}
void RdpHostPreferences::setSecurityEXT(bool enabled)
{
    m_configGroup.writeEntry("securityEXT", enabled);
}

QString RdpHostPreferences::authPkgList() const
{
    // FreeRDP semantics: packages are available unless explicitly denied
    // with a "!" prefix, so only the unchecked ones need to be written out.
    QStringList denied;
    if (!authPkgKerberos()) {
        denied << QStringLiteral("!kerberos");
    }
    if (!authPkgNtlm()) {
        denied << QStringLiteral("!ntlm");
    }
    if (!authPkgU2u()) {
        denied << QStringLiteral("!u2u");
    }
    return denied.join(QLatin1Char(','));
}

bool RdpHostPreferences::authPkgKerberos() const
{
    return m_configGroup.readEntry("authPkgKerberos", Settings::authPkgKerberos());
}
void RdpHostPreferences::setAuthPkgKerberos(bool enabled)
{
    m_configGroup.writeEntry("authPkgKerberos", enabled);
}

bool RdpHostPreferences::authPkgNtlm() const
{
    return m_configGroup.readEntry("authPkgNtlm", Settings::authPkgNtlm());
}
void RdpHostPreferences::setAuthPkgNtlm(bool enabled)
{
    m_configGroup.writeEntry("authPkgNtlm", enabled);
}

bool RdpHostPreferences::authPkgU2u() const
{
    return m_configGroup.readEntry("authPkgU2u", Settings::authPkgU2u());
}
void RdpHostPreferences::setAuthPkgU2u(bool enabled)
{
    m_configGroup.writeEntry("authPkgU2u", enabled);
}

RdpHostPreferences::DesktopScaleFactor RdpHostPreferences::desktopScaleFactor() const
{
    return DesktopScaleFactor(m_configGroup.readEntry("desktopScaleFactor", Settings::desktopScaleFactor()));
}
void RdpHostPreferences::setDesktopScaleFactor(DesktopScaleFactor desktopScaleFactor)
{
    m_configGroup.writeEntry("desktopScaleFactor", int(desktopScaleFactor));
}
int RdpHostPreferences::desktopScaleFactorCustom() const
{
    return m_configGroup.readEntry("desktopScaleFactorCustom", Settings::desktopScaleFactorCustom());
}
void RdpHostPreferences::setDesktopScaleFactorCustom(int desktopScaleFactorCustom)
{
    m_configGroup.writeEntry("desktopScaleFactorCustom", desktopScaleFactorCustom);
}
RdpHostPreferences::DeviceScaleFactor RdpHostPreferences::deviceScaleFactor() const
{
    return DeviceScaleFactor(m_configGroup.readEntry("deviceScaleFactor", Settings::deviceScaleFactor()));
}
void RdpHostPreferences::setDeviceScaleFactor(DeviceScaleFactor deviceScaleFactor)
{
    m_configGroup.writeEntry("deviceScaleFactor", int(deviceScaleFactor));
}
