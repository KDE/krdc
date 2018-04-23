/****************************************************************************
**
** Copyright (C) 2007 - 2012 Urs Wolfer <uwolfer @ kde.org>
** Copyright (C) 2012 AceLan Kao <acelan @ acelan.idv.tw>
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
    explicit RdpHostPreferences(KConfigGroup configGroup, QObject *parent = nullptr);
    ~RdpHostPreferences() override;

    void setResolution(int resolution);
    int resolution() const;
    void setColorDepth(int colorDepth);
    int colorDepth() const;
    void setKeyboardLayout(const QString &keyboardLayout);
    QString keyboardLayout() const;
    void setSound(int sound);
    int sound() const;
    void setSoundSystem(int sound);
    int soundSystem() const;
    void setConsole(bool console);
    bool console() const;
    void setExtraOptions(const QString &extraOptions);
    QString extraOptions() const;
    void setRemoteFX(bool remoteFX);
    bool remoteFX() const;
    void setPerformance(int performance);
    int performance() const;
    void setShareMedia(const QString &shareMedia);
    QString shareMedia() const;

protected:
    QWidget* createProtocolSpecificConfigPage() override;
    void acceptConfig() override;

private:
    Ui::RdpPreferences rdpUi;

private Q_SLOTS:
    void updateWidthHeight(int index);
    void updateSoundSystem(int index);
};

#endif
