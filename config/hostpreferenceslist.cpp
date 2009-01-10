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

#include "hostpreferenceslist.h"
#include "hostpreferences.h"

#include <KDebug>
#include <KIcon>
#include <KLocale>
#include <KMessageBox>
#include <KPushButton>
#include <KStandardDirs>

#include <QFile>
#include <QLayout>
#include <QListWidget>

HostPreferencesList::HostPreferencesList(QWidget *parent, MainWindow *mainWindow, KConfigGroup hostPrefsConfig)
        : QWidget(parent)
        , m_hostPrefsConfig(hostPrefsConfig)
        , m_mainWindow(mainWindow)
{
    hostList = new QListWidget(this);
    connect(hostList, SIGNAL(itemSelectionChanged()), SLOT(selectionChanged()));
    connect(hostList, SIGNAL(itemDoubleClicked(QListWidgetItem*)), SLOT(configureHost()));

    configureButton = new KPushButton(this);
    configureButton->setEnabled(false);
    configureButton->setText(i18n("Configure..."));
    configureButton->setIcon(KIcon("configure"));
    connect(configureButton, SIGNAL(clicked()), SLOT(configureHost()));

    removeButton = new KPushButton(this);
    removeButton->setEnabled(false);
    removeButton->setText(i18n("Remove"));
    removeButton->setIcon(KIcon("list-remove"));
    connect(removeButton, SIGNAL(clicked()), SLOT(removeHost()));

    QVBoxLayout *buttonLayout = new QVBoxLayout;
    buttonLayout->addWidget(configureButton);
    buttonLayout->addWidget(removeButton);
    buttonLayout->addStretch();

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->addWidget(hostList);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);

    readConfig();
}

HostPreferencesList::~HostPreferencesList()
{
}

void HostPreferencesList::readConfig()
{
    QStringList urls = m_hostPrefsConfig.groupList();
    
    for (int i = 0; i < urls.size(); ++i) 
        hostList->addItem(new QListWidgetItem(urls.at(i)));
}

void HostPreferencesList::saveSettings()
{
    m_hostPrefsConfig.sync();
}

void HostPreferencesList::configureHost()
{
    QList<QListWidgetItem *> selectedItems = hostList->selectedItems();

    foreach(QListWidgetItem *selectedItem, selectedItems) {
        const QString url = selectedItem->text();

        kDebug(5010) << "Configure host: " << url;

        HostPreferences* prefs = 0;
        
        const QList<RemoteViewFactory *> remoteViewFactories(m_mainWindow->remoteViewFactoriesList());
        foreach(RemoteViewFactory *factory, remoteViewFactories) {
            if (factory->supportsUrl(url)) {
                prefs = factory->createHostPreferences(m_hostPrefsConfig.group(url), this);
                if (prefs) {
                    kDebug(5010) << "Found plugin to handle url (" + url + "): " + prefs->metaObject()->className();
                } else {
                    kDebug(5010) << "Found plugin to handle url (" + url + "), but plugin does not provide preferences";
                }
            }
        }

        if (prefs) {
            prefs->showDialog();
            delete prefs;
        } else {
            KMessageBox::error(this,
                               i18n("The selected host cannot be handled."),
                               i18n("Unusable URL"));
        }
    }
}

void HostPreferencesList::removeHost()
{
    const QList<QListWidgetItem *> selectedItems = hostList->selectedItems();

    foreach(QListWidgetItem *selectedItem, selectedItems) {
        kDebug(5010) << "Remove host: " <<  selectedItem->text();

        m_hostPrefsConfig.deleteGroup(selectedItem->text());
        delete(selectedItem);
    }

    saveSettings();
    hostList->clearSelection();
}

void HostPreferencesList::selectionChanged()
{
    const bool enabled = hostList->selectedItems().isEmpty() ? false : true;

    configureButton->setEnabled(enabled);
    removeButton->setEnabled(enabled);
}

#include "hostpreferenceslist.moc"
