/*
    SPDX-FileCopyrightText: 2025 Ilya Pominov <ipominov@astralinux.ru>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SSHTUNNELWIDGET_H
#define SSHTUNNELWIDGET_H

#include <QWidget>

namespace Ui
{
class SshTunnelWidget;
}

class SshTunnelWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SshTunnelWidget(QWidget *parent = nullptr);
    ~SshTunnelWidget();

    bool useSshTunnel() const;
    bool useSshTunnelLoopback() const;
    int sshTunnelPort() const;
    QString sshTunnelUserName() const;

    void setUseSshTunnel(bool useSshTunnel);
    void setUseSshTunnelLoopback(bool useSshTunnelLoopback);
    void setSshTunnelPort(int port);
    void setSshTunnelUserName(const QString &userName);

private:
    Ui::SshTunnelWidget *ui;
};

#endif // SSHTUNNELWIDGET_H
