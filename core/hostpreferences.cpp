/****************************************************************************
**
** Copyright (C) 2007 - 2010 Urs Wolfer <uwolfer @ kde.org>
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

#include "hostpreferences.h"
#include "logging.h"
#include "settings.h"

#include <KI18n/KLocalizedString>
#include <KWidgetsAddons/KPageDialog>

#include <QCheckBox>
#include <QDialog>
#include <QFile>
#include <QIcon>
#include <QLabel>
#include <QVBoxLayout>

HostPreferences::HostPreferences(KConfigGroup configGroup, QObject *parent)
        : QObject(parent),
        m_configGroup(configGroup),
        m_connected(false),
        showAgainCheckBox(0),
        walletSupportCheckBox(0)
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
        const QString noteText  = i18n("Note that settings might only apply when you connect next time to this host.");
        const QString format = QLatin1String("<i>%1</i>");
        QLabel *commentLabel = new QLabel(format.arg(noteText), mainWidget);
        layout->addWidget(commentLabel);
    }

    QWidget* widget = createProtocolSpecificConfigPage();

    if (widget) {
        if (widget->layout())
            widget->layout()->setMargin(0);

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

