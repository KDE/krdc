/*
    SPDX-FileCopyrightText: 2025 Ilya Pominov <ipominov@astralinux.ru>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sshtunnelwidget.h"
#include "ui_sshtunnelwidget.h"

SshTunnelWidget::SshTunnelWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SshTunnelWidget)
{
    ui->setupUi(this);

    if (!ui->use_ssh_tunnel->isChecked()) {
        ui->ssh_groupBox->hide();
    }
    connect(ui->use_ssh_tunnel, &QCheckBox::toggled, ui->ssh_groupBox, &QWidget::setVisible);
}

SshTunnelWidget::~SshTunnelWidget()
{
    delete ui;
}

bool SshTunnelWidget::useSshTunnel() const
{
    return ui->use_ssh_tunnel->isChecked();
}

bool SshTunnelWidget::useSshTunnelLoopback() const
{
    return ui->use_loopback->isChecked();
}

int SshTunnelWidget::sshTunnelPort() const
{
    return ui->ssh_tunnel_port->value();
}

QString SshTunnelWidget::sshTunnelUserName() const
{
    return ui->ssh_tunnel_user_name->text();
}

void SshTunnelWidget::setUseSshTunnel(bool useSshTunnel)
{
    ui->use_ssh_tunnel->setChecked(useSshTunnel);
}

void SshTunnelWidget::setUseSshTunnelLoopback(bool useSshTunnelLoopback)
{
    ui->use_loopback->setChecked(useSshTunnelLoopback);
}

void SshTunnelWidget::setSshTunnelPort(int port)
{
    ui->ssh_tunnel_port->setValue(port);
}

void SshTunnelWidget::setSshTunnelUserName(const QString &userName)
{
    ui->ssh_tunnel_user_name->setText(userName);
}
