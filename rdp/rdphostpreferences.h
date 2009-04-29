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

#ifndef RDPHOSTPREFERENCES_H
#define RDPHOSTPREFERENCES_H

#include "hostpreferences.h"
#include "ui_rdppreferences.h"

class RdpHostPreferences : public HostPreferences
{
    Q_OBJECT

public:
    explicit RdpHostPreferences(KConfigGroup configGroup, QObject *parent = 0);
    ~RdpHostPreferences();

    void setHeight(int height);
    int height() const;
    void setWidth(int width);
    int width() const;
    void setColorDepth(int colorDepth);
    int colorDepth() const;
    void setKeyboardLayout(const QString &keyboardLayout);
    QString keyboardLayout() const;
    void setSound(int sound);
    int sound() const;
    void setConsole(bool console);
    bool console() const;
    void setExtraOptions(const QString &extraOptions);
    QString extraOptions() const;

protected:
    QWidget* createProtocolSpecificConfigPage();
    void acceptConfig();

private:
    Ui::RdpPreferences rdpUi;

private slots:
    void updateWidthHeight(int index);
};

#endif
