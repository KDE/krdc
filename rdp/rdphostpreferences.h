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
    explicit RdpHostPreferences(const QString &url, bool forceShow = false, QObject *parent = 0);
    ~RdpHostPreferences();

    void setHeight(int height);
    int height();
    void setWidth(int width);
    int width();
    void setColorDepth(int colorDepth);
    int colorDepth();
    void setKeyboardLayout(const QString &keyboardLayout);
    QString keyboardLayout() const;

protected:
    void showDialog();
    void readProtocolSpecificConfig();
    void saveProtocolSpecificConfig();

private:
    int m_height;
    int m_width;
    int m_colorDepth;
    QString m_keyboardLayout;
    Ui::RdpPreferences rdpUi;

private slots:
    void updateWidthHeight(int index);
};

#endif
