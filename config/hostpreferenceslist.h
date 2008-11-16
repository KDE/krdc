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

#ifndef HOSTPREFERENCESLIST_H
#define HOSTPREFERENCESLIST_H

#include <KConfigGroup>

#include <QDomDocument>
#include <QWidget>

class KPushButton;

class QListWidget;

class HostPreferencesList : public QWidget
{
    Q_OBJECT

public:
    HostPreferencesList(QWidget *parent, KConfigGroup hostPrefsConfig);
    ~HostPreferencesList();

private slots:
    void readConfig();
    void saveSettings();
    void configureHost();
    void removeHost();
    void selectionChanged();

private:
    KConfigGroup m_hostPrefsConfig;

    KPushButton *configureButton;
    KPushButton *removeButton;
    QListWidget *hostList;
};

#endif
