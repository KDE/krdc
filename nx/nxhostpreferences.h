/****************************************************************************
**
** Copyright (C) 2008 David Gross <gdavid.devel@gmail.com>
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

#ifndef NXHOSTPREFERENCES_H
#define NXHOSTPREFERENCES_H

#include "hostpreferences.h"
#include "ui_nxpreferences.h"

class NxHostPreferences : public HostPreferences
{
    Q_OBJECT

public:
    explicit NxHostPreferences(KConfigGroup configGroup, QObject *parent = 0);
    ~NxHostPreferences();

    void setHeight(int height);
    int height();
    void setWidth(int width);
    int width();
    void setDesktopType(const QString &desktopType);
    QString desktopType() const;
    void setKeyboardLayout(const QString &keyboardLayout);
    QString keyboardLayout() const;
    void setPrivateKey(const QString &privateKey);
    QString privateKey() const;

protected:
    QWidget* createProtocolSpecificConfigPage();
    void acceptConfig();

private:
    QString m_privateKey;
    Ui::NxPreferences nxUi;
    QWidget *nxPage;

private slots:
    void updateWidthHeight(int index);
    void setDefaultPrivateKey(int state);
    void updatePrivateKey();
    void chooseKeyFile();
};

#endif
