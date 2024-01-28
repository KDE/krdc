/*
    SPDX-FileCopyrightText: 2007 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "hostpreferenceslist.h"
#include "hostpreferences.h"
#include "krdc_debug.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <QIcon>

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
    connect(hostList, SIGNAL(itemDoubleClicked(QListWidgetItem *)), SLOT(configureHost()));

    configureButton = new QPushButton(this);
    configureButton->setEnabled(false);
    configureButton->setText(i18n("Configure..."));
    configureButton->setIcon(QIcon::fromTheme(QLatin1String("configure")));
    connect(configureButton, SIGNAL(clicked()), SLOT(configureHost()));

    removeButton = new QPushButton(this);
    removeButton->setEnabled(false);
    removeButton->setText(i18n("Remove"));
    removeButton->setIcon(QIcon::fromTheme(QLatin1String("list-remove")));
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
    const QList<QListWidgetItem *> selectedItems = hostList->selectedItems();

    for (QListWidgetItem *selectedItem : selectedItems) {
        const QString urlString = selectedItem->text();
        const QUrl url = QUrl(urlString);

        qCDebug(KRDC) << "Configure host: " << urlString;

        HostPreferences *prefs = nullptr;

        const QList<RemoteViewFactory *> remoteViewFactories(m_mainWindow->remoteViewFactoriesList());
        for (RemoteViewFactory *factory : remoteViewFactories) {
            if (factory->supportsUrl(url)) {
                prefs = factory->createHostPreferences(m_hostPrefsConfig.group(urlString), this);
                if (prefs) {
                    qCDebug(KRDC) << "Found plugin to handle url (" << urlString << "): " << prefs->metaObject()->className();
                } else {
                    qCDebug(KRDC) << "Found plugin to handle url (" << urlString << "), but plugin does not provide preferences";
                }
            }
        }

        if (prefs) {
            prefs->showDialog(this);
            delete prefs;
        } else {
            KMessageBox::error(this, i18n("The selected host cannot be handled."), i18n("Unusable URL"));
        }
    }
}

void HostPreferencesList::removeHost()
{
    const QList<QListWidgetItem *> selectedItems = hostList->selectedItems();

    for (QListWidgetItem *selectedItem : selectedItems) {
        qCDebug(KRDC) << "Remove host: " << selectedItem->text();

        m_hostPrefsConfig.deleteGroup(selectedItem->text());
        delete (selectedItem);
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
