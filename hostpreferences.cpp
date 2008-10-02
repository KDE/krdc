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

#include "hostpreferences.h"

#include "settings.h"

#include <KDebug>
#include <KLocale>
#include <KStandardDirs>
#include <KTitleWidget>

#include <QCheckBox>
#include <QFile>
#include <QVBoxLayout>

HostPreferences::HostPreferences(KConfigGroup configGroup, QObject *parent)
        : QObject(parent),
        m_configGroup(configGroup),
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
    
bool HostPreferences::showDialogIfNeeded()
{
    if (hostConfigured()) {
        if (showConfigAgain()) {
            kDebug(5010) << "Show config dialog again";
            return showDialog();
        } else
            return true; // no changes, no need to save
    } else {
        kDebug(5010) << "No config found, create new";
        if (Settings::showPreferencesForNewConnections())
            return showDialog();
        else
            return true;
    }
}


bool HostPreferences::showDialog()
{
    // Prepare dialog
    KDialog *dialog = new KDialog;
    dialog->setCaption(i18n("Host Configuration"));

    QWidget *mainWidget = new QWidget(dialog);
    QVBoxLayout *layout = new QVBoxLayout(mainWidget);

    KTitleWidget *titleWidget = new KTitleWidget(dialog);
    titleWidget->setText(i18n("Host Configuration"));
    titleWidget->setPixmap(KIcon("krdc"));
    layout->addWidget(titleWidget);

    QWidget* widget = createProtocolSpecificConfigPage();
    
    if (widget->layout())
        widget->layout()->setMargin(0);

    layout->addWidget(widget);

    showAgainCheckBox = new QCheckBox(mainWidget);
    showAgainCheckBox->setText(i18n("Show this dialog again for this host"));
    showAgainCheckBox->setChecked(showConfigAgain());

    walletSupportCheckBox = new QCheckBox(mainWidget);
    walletSupportCheckBox->setText(i18n("Remember password (KWallet)"));
    walletSupportCheckBox->setChecked(walletSupport());

    layout->addWidget(showAgainCheckBox);
    layout->addWidget(walletSupportCheckBox);
    layout->addStretch(1);
    mainWidget->setLayout(layout);

    dialog->setMainWidget(mainWidget);
    
    // Show dialog
    if (dialog->exec() == KDialog::Accepted) {
        kDebug(5010) << "HostPreferences config dialog accepted";
        acceptConfig();
        return true;
    } else {
        return false;
    }
}

#include "hostpreferences.moc"
