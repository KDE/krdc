/*
    SPDX-FileCopyrightText: 2007 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef HOSTPREFERENCESLIST_H
#define HOSTPREFERENCESLIST_H

#include "mainwindow.h"

#include <KConfigGroup>

#include <QDomDocument>
#include <QPushButton>
#include <QWidget>

class QListWidget;

class HostPreferencesList : public QWidget
{
    Q_OBJECT

public:
    HostPreferencesList(QWidget *parent, MainWindow *mainWindow, KConfigGroup hostPrefsConfig);
    ~HostPreferencesList() override;

private Q_SLOTS:
    void readConfig();
    void saveSettings();
    void configureHost();
    void removeHost();
    void selectionChanged();

private:
    KConfigGroup m_hostPrefsConfig;

    QPushButton *configureButton;
    QPushButton *removeButton;
    QListWidget *hostList;
    MainWindow *m_mainWindow;
};

#endif // HOSTPREFERENCESLIST_H
