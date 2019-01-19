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

#ifndef VNCHOSTPREFERENCES_H
#define VNCHOSTPREFERENCES_H

#include "hostpreferences.h"
#include "ui_vncpreferences.h"

class VncHostPreferences : public HostPreferences
{
    Q_OBJECT

public:
    explicit VncHostPreferences(KConfigGroup configGroup, QObject *parent = nullptr);
    ~VncHostPreferences() override;

    void setQuality(RemoteView::Quality quality);
    RemoteView::Quality quality();

    bool useSshTunnel() const;
    bool useSshTunnelLoopback() const;
    int sshTunnelPort() const;
    QString sshTunnelUserName() const;

protected:
    void acceptConfig() override;

    QWidget* createProtocolSpecificConfigPage() override;

private:
    void setUseSshTunnel(bool useSshTunnel);
    void setUseSshTunnelLoopback(bool useSshTunnelLoopback);
    void setSshTunnelPort(int port);
    void setSshTunnelUserName(const QString &userName);

    Ui::VncPreferences vncUi;
    void checkEnableCustomSize(int index);

private Q_SLOTS:
    void updateScalingWidthHeight(int index);
    void updateScaling(bool enabled);
};

#endif
