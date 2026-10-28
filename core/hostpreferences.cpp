/*
    SPDX-FileCopyrightText: 2007-2010 Urs Wolfer <uwolfer@kde.org>
    SPDX-FileCopyrightText: 2021 Rafał Lalik <rafallalik@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "hostpreferences.h"
#include "krdc_debug.h"
#include "settings.h"
#include "settingsform.h"

#include <KGuiItem>
#include <KLocalizedString>
#include <KMessageWidget>

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#ifdef USE_SSH_TUNNEL
#include "sshtunnelwidget.h"

static const char use_ssh_tunnel_config_key[] = "use_ssh_tunnel";
static const char use_ssh_tunnel_loopback_config_key[] = "use_ssh_tunnel_loopback";
static const char ssh_tunnel_port_config_key[] = "ssh_tunnel_port";
static const char ssh_tunnel_user_name_config_key[] = "ssh_tunnel_user_name";
#endif

namespace
{
// A row of a form on the settings page that is shown on its own before connecting.
struct QuickRow {
    QFormLayout *form = nullptr;
    int row = -1;
    QFormLayout::ItemRole fieldRole = QFormLayout::FieldRole;
    QWidget *label = nullptr;
    QWidget *field = nullptr;
};

QuickRow quickRowOf(QWidget *field)
{
    QuickRow quickRow;
    const auto forms = field->parentWidget()->findChildren<QFormLayout *>();
    for (QFormLayout *form : forms) {
        form->getWidgetPosition(field, &quickRow.row, &quickRow.fieldRole);
        if (quickRow.row >= 0) {
            quickRow.form = form;
            quickRow.field = field;
            if (QLayoutItem *label = form->itemAt(quickRow.row, QFormLayout::LabelRole)) {
                quickRow.label = label->widget();
            }
            break;
        }
    }
    return quickRow;
}

/**
 * Shows only @p rows of @p page, in a form of their own in @p layout, until @p allSettingsButton
 * is clicked. The rows keep their widgets, so nothing changes when they move back to the page.
 */
void showQuickRows(QVBoxLayout *layout, QWidget *page, const QList<QuickRow> &rows, QPushButton *allSettingsButton)
{
    auto *quickForm = new QFormLayout;
    for (const QuickRow &row : rows) {
        if (row.label) {
            row.form->removeWidget(row.label);
        }
        row.form->removeWidget(row.field);
        quickForm->addRow(row.label, row.field);
    }
    layout->insertLayout(layout->indexOf(page), quickForm);
    page->hide();
    alignLabelColumns(layout->parentWidget(), page);

    QObject::connect(allSettingsButton, &QPushButton::clicked, page, [layout, page, rows, quickForm, allSettingsButton]() {
        for (const QuickRow &row : rows) {
            if (row.label) {
                quickForm->removeWidget(row.label);
                row.form->setWidget(row.row, QFormLayout::LabelRole, row.label);
            }
            quickForm->removeWidget(row.field);
            row.form->setWidget(row.row, row.fieldRole, row.field);
        }
        page->show();
        allSettingsButton->hide();
        alignLabelColumns(layout->parentWidget());
    });
}

QString hostDisplayName(const QString &urlString)
{
    const QUrl url(urlString);
    if (url.host().isEmpty()) {
        return urlString;
    }
    // The authority brackets an IPv6 address, so its port stays apart from it.
    return url.adjusted(QUrl::RemoveUserInfo).authority();
}
}

HostPreferences::HostPreferences(KConfigGroup configGroup, QObject *parent)
    : QObject(parent)
    , m_configGroup(configGroup)
#ifdef USE_SSH_TUNNEL
    , sshTunnelWidget(nullptr)
#endif
    , m_connected(false)
    , m_editConnectionDetails(false)
    , showAgainCheckBox(nullptr)
    , walletSupportCheckBox(nullptr)
    , connectionNameLineEdit(nullptr)
    , connectionUrlLineEdit(nullptr)
{
    m_hostConfigured = m_configGroup.hasKey("showConfigAgain");

    // Previous versions of KRDC had an invalid default value for "scaleFactor" of 0.
    // This doesn't make any sense, see MainWindow::setFactor where it divides it by 100.
    // See BUG: 485094 which is caused by this being set to 0.
    if (m_configGroup.readEntry("scaleFactor").toInt() < 100) {
        m_configGroup.deleteEntry("scaleFactor");
    }
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
    return m_configGroup.readEntry("scaleFactor", 100);
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

bool HostPreferences::clipboardSharing()
{
    return m_configGroup.readEntry("clipboardSharing", true);
}

void HostPreferences::setClipboardSharing(bool share)
{
    m_configGroup.writeEntry("clipboardSharing", share);
}

bool HostPreferences::showDialogIfNeeded(QWidget *parent)
{
    if (hostConfigured()) {
        if (showConfigAgain()) {
            qCDebug(KRDC) << "Show config dialog again";
            return execDialog(parent, true);
        } else
            return true; // no changes, no need to save
    } else {
        qCDebug(KRDC) << "No config found, create new";
        if (Settings::showPreferencesForNewConnections())
            return execDialog(parent, true);
        else
            return true;
    }
}

bool HostPreferences::showDialog(QWidget *parent)
{
    return execDialog(parent, false);
}

void HostPreferences::addQuickSetting(QWidget *field)
{
    m_quickSettings.append(field);
}

bool HostPreferences::execDialog(QWidget *parent, bool beforeConnecting)
{
    auto *dialog = new QDialog(parent);
    if (beforeConnecting) {
        dialog->setWindowTitle(i18nc("@title:window %1 is a host name", "Connect to %1", hostDisplayName(m_configGroup.name())));
    } else {
        dialog->setWindowTitle(i18n("Host Configuration"));
    }
    auto *dialogLayout = new QVBoxLayout(dialog);

    if (m_connected) {
        auto *note = new KMessageWidget(i18n("Note that settings might only apply when you connect next time to this host."), dialog);
        note->setMessageType(KMessageWidget::Information);
        note->setIcon(QIcon::fromTheme(QStringLiteral("dialog-information")));
        note->setCloseButtonVisible(false);
        // Keyboard focus belongs to the first setting, not to this note.
        const auto noteChildren = note->findChildren<QWidget *>();
        for (QWidget *child : noteChildren) {
            child->setFocusPolicy(Qt::NoFocus);
        }
        dialogLayout->addWidget(note);
    }

    auto *body = new QWidget(dialog);
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    dialogLayout->addWidget(body, 1);

    auto *dialogButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    connect(dialogButtonBox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(dialogButtonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    QPushButton *acceptButton = dialogButtonBox->button(QDialogButtonBox::Ok);
    acceptButton->setDefault(true);
    if (beforeConnecting) {
        KGuiItem::assign(acceptButton, KGuiItem(i18nc("@action:button", "Connect"), QStringLiteral("network-connect")));
    }

    // Everything but the host options; before connecting, the quick rows stand in for it.
    auto *allSettings = new QWidget(body);
    auto *allSettingsLayout = new QVBoxLayout(allSettings);
    allSettingsLayout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(allSettings);

    connectionNameLineEdit = nullptr;
    connectionUrlLineEdit = nullptr;
    if (m_editConnectionDetails || beforeConnecting) {
        auto *connectionLayout = new QFormLayout;
        connectionNameLineEdit = new QLineEdit(m_connectionName, allSettings);
        connectionLayout->addRow(i18nc("@label:textbox Name of a remote connection", "Name:"), connectionNameLineEdit);
        allSettingsLayout->addLayout(connectionLayout);
        allSettingsLayout->addSpacing(allSettings->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing));

        if (m_editConnectionDetails) {
            connectionUrlLineEdit = new QLineEdit(m_connectionUrl.toDisplayString(QUrl::StripTrailingSlash), allSettings);
            connectionLayout->addRow(i18nc("@label:textbox URL of a remote connection", "URL:"), connectionUrlLineEdit);

            const auto updateAcceptButton = [acceptButton, this]() {
                const QUrl url = QUrl::fromUserInput(connectionUrlLineEdit->text().trimmed());
                const bool validUrl = url.isValid() && !url.scheme().isEmpty() && (!url.host().isEmpty() || url.port() >= 0)
                    && (url.path().isEmpty() || url.path() == QStringLiteral("/"));
                acceptButton->setEnabled(!connectionNameLineEdit->text().trimmed().isEmpty() && validUrl);
            };
            connect(connectionNameLineEdit, &QLineEdit::textChanged, dialog, updateAcceptButton);
            connect(connectionUrlLineEdit, &QLineEdit::textChanged, dialog, updateAcceptButton);
            updateAcceptButton();
        } else {
            // Without a name, the connection is listed by its address.
            connectionNameLineEdit->setPlaceholderText(m_configGroup.name());
        }
    }

    m_quickSettings.clear();

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

        allSettingsLayout->addWidget(widget);
    }

    walletSupportCheckBox = new QCheckBox(body);
    walletSupportCheckBox->setText(i18n("Remember password"));
    walletSupportCheckBox->setChecked(walletSupport());

    showAgainCheckBox = new QCheckBox(body);
    showAgainCheckBox->setText(i18n("Show this dialog again for this host"));
    showAgainCheckBox->setChecked(showConfigAgain());

    // The empty label keeps the options in the field column.
    auto *optionsLayout = new QFormLayout;
    optionsLayout->addRow(new QWidget(body), walletSupportCheckBox);
    optionsLayout->addRow(nullptr, showAgainCheckBox);
    layout->addSpacing(body->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing));
    layout->addLayout(optionsLayout);
    layout->addStretch(1);

    QList<QuickRow> quickRows;
    if (beforeConnecting && widget) {
        for (QWidget *field : std::as_const(m_quickSettings)) {
            if (const QuickRow row = quickRowOf(field); row.form) {
                quickRows.append(row);
            }
        }
    }

    auto *buttonLayout = new QHBoxLayout;
    if (quickRows.isEmpty()) {
        alignLabelColumns(body);
    } else {
        auto *allSettingsButton = new QPushButton(QIcon::fromTheme(QStringLiteral("configure")), i18nc("@action:button", "All Settings…"), dialog);
        allSettingsButton->setAutoDefault(false);
        buttonLayout->addWidget(allSettingsButton);
        showQuickRows(layout, allSettings, quickRows, allSettingsButton);
    }
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(dialogButtonBox);
    dialogLayout->addSpacing(dialog->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing));
    dialogLayout->addLayout(buttonLayout);

    // Show dialog
    const QPointer<QDialog> guard(dialog);
    const bool accepted = dialog->exec() == QDialog::Accepted && guard;
    if (accepted) {
        qCDebug(KRDC) << "HostPreferences config dialog accepted";
        if (connectionNameLineEdit) {
            m_connectionName = connectionNameLineEdit->text().trimmed();
        }
        if (m_editConnectionDetails) {
            m_connectionUrl = QUrl::fromUserInput(connectionUrlLineEdit->text().trimmed());
        }
        acceptConfig();
    }

    // The widgets go with the dialog, so nothing may refer to them afterwards.
    delete guard.data();
#ifdef USE_SSH_TUNNEL
    sshTunnelWidget = nullptr;
#endif
    showAgainCheckBox = nullptr;
    walletSupportCheckBox = nullptr;
    connectionNameLineEdit = nullptr;
    connectionUrlLineEdit = nullptr;
    return accepted;
}

void HostPreferences::setShownWhileConnected(bool connected)
{
    m_connected = connected;
}

void HostPreferences::setConnectionDetails(const QString &name, const QUrl &url)
{
    m_editConnectionDetails = true;
    m_connectionName = name;
    m_connectionUrl = url;
}

void HostPreferences::setConnectionName(const QString &name)
{
    m_connectionName = name;
}

QString HostPreferences::connectionName() const
{
    return m_connectionName;
}

QUrl HostPreferences::connectionUrl() const
{
    return m_connectionUrl;
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
