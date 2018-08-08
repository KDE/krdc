/****************************************************************************
**
**   Copyright (C) 2018    Klarälvdalens Datakonsult AB, a KDAB Group
**                         company, info@kdab.com. Work sponsored by the
**                         LiMux project of the city of Munich
**
** This file is part of KRDC.
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

#ifndef VCNCSSHTUNNELTHREAD_H
#define VCNCSSHTUNNELTHREAD_H

#include <QThread>

#include <QByteArray>
#include <QString>

#include <atomic>

#include <libssh/libssh.h>

class VncSshTunnelThread : public QThread
{
    Q_OBJECT
public:
    VncSshTunnelThread(const QByteArray &host, int vncPort, int tunnelPort, int sshPort, const QByteArray &sshUserName, bool loopback);
    ~VncSshTunnelThread();

    enum PasswordOrigin {
        PasswordFromWallet,
        PasswordFromDialog
    };

    enum PasswordRequestFlags {
        NoFlags,
        IgnoreWallet
    };

    QString password() const;
    void setPassword(const QString &password, PasswordOrigin origin);
    void userCanceledPasswordRequest();

    void run() override;

Q_SIGNALS:
    void passwordRequest(PasswordRequestFlags flags);
    void listenReady();
    void errorMessage(const QString &message);

private:
    QByteArray m_host;
    int m_vncPort;
    int m_tunnelPort;
    int m_sshPort;
    QByteArray m_sshUserName;
    bool m_loopback;
    QString m_password;
    PasswordOrigin m_passwordOrigin;
    bool m_passwordRequestCanceledByUser;

    std::atomic_bool m_stop_thread;
};

#endif
