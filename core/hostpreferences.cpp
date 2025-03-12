/*
    SPDX-FileCopyrightText: 2007-2010 Urs Wolfer <uwolfer@kde.org>
    SPDX-FileCopyrightText: 2021 Rafał Lalik <rafallalik@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "hostpreferences.h"
#include "krdc_debug.h"
#include "settings.h"

#include <KLocalizedString>
#include <KPageDialog>

#include <QCheckBox>
#include <QDialog>
#include <QFile>
#include <QIcon>
#include <QLabel>
#include <QVBoxLayout>

#ifdef USE_SSH_TUNNEL
#include "sshtunnelwidget.h"

static const char use_ssh_tunnel_config_key[] = "use_ssh_tunnel";
static const char use_ssh_tunnel_loopback_config_key[] = "use_ssh_tunnel_loopback";
static const char ssh_tunnel_port_config_key[] = "ssh_tunnel_port";
static const char ssh_tunnel_user_name_config_key[] = "ssh_tunnel_user_name";
#endif

HostPreferences::HostPreferences(KConfigGroup configGroup, QObject *parent)
    : QObject(parent)
    , m_configGroup(configGroup)
#ifdef USE_SSH_TUNNEL
    , sshTunnelWidget(nullptr)
#endif
    , m_connected(false)
    , showAgainCheckBox(nullptr)
    , walletSupportCheckBox(nullptr)
{
    m_hostConfigured = m_configGroup.hasKey("showConfigAgain");
}

HostPreferences::~HostPreferences()
{
}

KConfigGroup HostPreferences::configGroup()
{
    return m_configGroup;
}

void HostPreferences::acceptConfig()
{
#ifdef USE_SSH_TUNNEL
    if (sshTunnelWidget) {
        setUseSshTunnel(sshTunnelWidget->useSshTunnel());
        setUseSshTunnelLoopback(sshTunnelWidget->useSshTunnelLoopback());
        setSshTunnelPort(sshTunnelWidget->sshTunnelPort());
        setSshTunnelUserName(sshTunnelWidget->sshTunnelUserName());
    }
#endif

    setShowConfigAgain(showAgainCheckBox->isChecked());
    setWalletSupport(walletSupportCheckBox->isChecked());
}

bool HostPreferences::hostConfigured()
{
    return m_hostConfigured;
}

void HostPreferences::setShowConfigAgain(bool show)
{
    m_configGroup.writeEntry("showConfigAgain", show);
}

bool HostPreferences::showConfigAgain()
{
    return m_configGroup.readEntry("showConfigAgain", true);
}

void HostPreferences::setWalletSupport(bool walletSupport)
{
    m_configGroup.writeEntry("walletSupport", walletSupport);
}

bool HostPreferences::walletSupport()
{
    return m_configGroup.readEntry("walletSupport", true);
}

void HostPreferences::setHeight(int height)
{
    if (height >= 0)
        m_configGroup.writeEntry("height", height);
}

int HostPreferences::height()
{
    return m_configGroup.readEntry("height", Settings::height());
}

void HostPreferences::setWidth(int width)
{
    if (width >= 0)
        m_configGroup.writeEntry("width", width);
}

int HostPreferences::width()
{
    return m_configGroup.readEntry("width", Settings::width());
}

bool HostPreferences::fullscreenScale()
{
    return m_configGroup.readEntry("fullscreenScale", false);
}

void HostPreferences::setFullscreenScale(bool scale)
{
    m_configGroup.writeEntry("fullscreenScale", scale);
}

bool HostPreferences::windowedScale()
{
    return m_configGroup.readEntry("windowedScale", false);
}

void HostPreferences::setWindowedScale(bool scale)
{
    m_configGroup.writeEntry("windowedScale", scale);
}

int HostPreferences::scaleFactor()
{
    return m_configGroup.readEntry("scaleFactor", 0);
}

void HostPreferences::setScaleFactor(int factor)
{
    m_configGroup.writeEntry("scaleFactor", factor);
}

bool HostPreferences::grabAllKeys()
{
    return m_configGroup.readEntry("grabAllKeys", false);
}

void HostPreferences::setGrabAllKeys(bool grab)
{
    m_configGroup.writeEntry("grabAllKeys", grab);
}

bool HostPreferences::showLocalCursor()
{
    return m_configGroup.readEntry("showLocalCursor", false);
}

void HostPreferences::setShowLocalCursor(bool show)
{
    m_configGroup.writeEntry("showLocalCursor", show);
}

bool HostPreferences::viewOnly()
{
    return m_configGroup.readEntry("viewOnly", false);
}

void HostPreferences::setViewOnly(bool view)
{
    m_configGroup.writeEntry("viewOnly", view);
}

bool HostPreferences::showDialogIfNeeded(QWidget *parent)
{
    if (hostConfigured()) {
        if (showConfigAgain()) {
            qCDebug(KRDC) << "Show config dialog again";
            return showDialog(parent);
        } else
            return true; // no changes, no need to save
    } else {
        qCDebug(KRDC) << "No config found, create new";
        if (Settings::showPreferencesForNewConnections())
            return showDialog(parent);
        else
            return true;
    }
}

bool HostPreferences::showDialog(QWidget *parent)
{
    // Prepare dialog
    KPageDialog *dialog = new KPageDialog(parent);
    dialog->setWindowTitle(i18n("Host Configuration"));

    QWidget *mainWidget = new QWidget(parent);
    QVBoxLayout *layout = new QVBoxLayout(mainWidget);
    dialog->addPage(mainWidget, i18n("Host Configuration"));

    if (m_connected) {
        const QString noteText = i18n("Note that settings might only apply when you connect next time to this host.");
        const QString format = QLatin1String("<i>%1</i>");
        QLabel *commentLabel = new QLabel(format.arg(noteText), mainWidget);
        layout->addWidget(commentLabel);
    }

#ifdef USE_SSH_TUNNEL
    sshTunnelWidget = new SshTunnelWidget;
    sshTunnelWidget->setUseSshTunnel(useSshTunnel());
    sshTunnelWidget->setUseSshTunnelLoopback(useSshTunnelLoopback());
    sshTunnelWidget->setSshTunnelPort(sshTunnelPort());
    sshTunnelWidget->setSshTunnelUserName(sshTunnelUserName());
    QWidget *widget = createProtocolSpecificConfigPage(sshTunnelWidget);
    if (!widget) {
        delete sshTunnelWidget;
        sshTunnelWidget = nullptr;
    }
#else
    QWidget *widget = createProtocolSpecificConfigPage(nullptr);
#endif

    if (widget) {
        if (widget->layout())
            widget->layout()->setContentsMargins(0, 0, 0, 0);

        layout->addWidget(widget);
    }

    showAgainCheckBox = new QCheckBox(mainWidget);
    showAgainCheckBox->setText(i18n("Show this dialog again for this host"));
    showAgainCheckBox->setChecked(showConfigAgain());

    walletSupportCheckBox = new QCheckBox(mainWidget);
    walletSupportCheckBox->setText(i18n("Remember password (KWallet)"));
    walletSupportCheckBox->setChecked(walletSupport());

    layout->addWidget(showAgainCheckBox);
    layout->addWidget(walletSupportCheckBox);
    layout->addStretch(1);

    // Show dialog
    if (dialog->exec() == QDialog::Accepted) {
        qCDebug(KRDC) << "HostPreferences config dialog accepted";
        acceptConfig();
        return true;
    } else {
        return false;
    }
}

void HostPreferences::setShownWhileConnected(bool connected)
{
    m_connected = connected;
}

#ifdef USE_SSH_TUNNEL
bool HostPreferences::useSshTunnel() const
{
    return m_configGroup.readEntry(use_ssh_tunnel_config_key, false);
}

void HostPreferences::setUseSshTunnel(bool useSshTunnel)
{
    m_configGroup.writeEntry(use_ssh_tunnel_config_key, useSshTunnel);
}

bool HostPreferences::useSshTunnelLoopback() const
{
    return m_configGroup.readEntry(use_ssh_tunnel_loopback_config_key, false);
}

void HostPreferences::setUseSshTunnelLoopback(bool useSshTunnelLoopback)
{
    m_configGroup.writeEntry(use_ssh_tunnel_loopback_config_key, useSshTunnelLoopback);
}

int HostPreferences::sshTunnelPort() const
{
    return m_configGroup.readEntry(ssh_tunnel_port_config_key, 22);
}

void HostPreferences::setSshTunnelPort(int port)
{
    m_configGroup.writeEntry(ssh_tunnel_port_config_key, port);
}

QString HostPreferences::sshTunnelUserName() const
{
    return m_configGroup.readEntry(ssh_tunnel_user_name_config_key, QString());
}

void HostPreferences::setSshTunnelUserName(const QString &userName)
{
    m_configGroup.writeEntry(ssh_tunnel_user_name_config_key, userName);
}
#endif
