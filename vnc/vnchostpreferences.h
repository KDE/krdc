/*
    SPDX-FileCopyrightText: 2007 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
    bool dontCopyPasswords() const;

protected:
    void acceptConfig() override;

    QWidget *createProtocolSpecificConfigPage() override;

private:
    void setUseSshTunnel(bool useSshTunnel);
    void setUseSshTunnelLoopback(bool useSshTunnelLoopback);
    void setSshTunnelPort(int port);
    void setSshTunnelUserName(const QString &userName);
    void setDontCopyPasswords(bool dontCopyPasswords);

    Ui::VncPreferences vncUi;
    void checkEnableCustomSize(int index);

private Q_SLOTS:
    void updateScalingWidthHeight(int index);
    void updateScaling(bool enabled);
};

#endif
