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

#include "ui_vncpreferences.h"
#include "settings.h"

#include <KDebug>

VncHostPreferences::VncHostPreferences(const QString &url, bool forceShow, QObject *parent)
        : HostPreferences(url, parent),
        m_quality(RemoteView::Low)
{
    if (hostConfigured()) {
        if (showConfigAgain() || forceShow) {
            kDebug(5011) << "Show config dialog again";
            showDialog();
        } else
            return; // no changes, no need to save
    } else {
        kDebug(5011) << "No config found, create new";
        if (Settings::showPreferencesForNewConnections())
            showDialog();
    }

    saveConfig();
}

VncHostPreferences::~VncHostPreferences()
{
}

void VncHostPreferences::showDialog()
{
    QWidget *vncPage = new QWidget();
    Ui::VncPreferences vncUi;
    vncUi.setupUi(vncPage);

    KDialog *dialog = createDialog(vncPage);

    vncUi.kcfg_Quality->setCurrentIndex(quality() - 1);

    if (dialog->exec() == KDialog::Accepted) {
        kDebug(5011) << "VncHostPreferences config dialog accepted";

        setQuality((RemoteView::Quality)(vncUi.kcfg_Quality->currentIndex() + 1));
    }
}

void VncHostPreferences::readProtocolSpecificConfig()
{
    kDebug(5011) << "VncHostPreferences::readProtocolSpecificConfig";

    if (m_element.firstChildElement("quality") != QDomElement()) // not configured yet
        setQuality((RemoteView::Quality)(m_element.firstChildElement("quality").text().toInt()));
    else
        setQuality((RemoteView::Quality)(Settings::quality() + 1)); // get the proctocol default
}

void VncHostPreferences::saveProtocolSpecificConfig()
{
    kDebug(5011) << "VncHostPreferences::saveProtocolSpecificConfig";

    updateElement("quality", QString::number(quality()));
}

void VncHostPreferences::setQuality(RemoteView::Quality quality)
{
    if (quality >= 0 && quality <= 3)
        m_quality = quality;
}

RemoteView::Quality VncHostPreferences::quality()
{
    return m_quality;
}

#include "vnchostpreferences.moc"
