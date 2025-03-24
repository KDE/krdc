/*
    SPDX-FileCopyrightText: 2018 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
    Work sponsored by the LiMux project of the city of Munich

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sshtunnelthread.h"
#include "krdc_debug.h"

#include <KLocalizedString>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <QDebug>

SshTunnelThread::SshTunnelThread(const QByteArray &host, int port, int tunnelPort, int sshPort, const QByteArray &sshUserName, bool loopback)
    : m_host(host)
    , m_port(port)
    , m_tunnelPort(tunnelPort)
    , m_sshPort(sshPort)
    , m_sshUserName(sshUserName)
    , m_loopback(loopback)
    , m_stop_thread(false)
{
}

SshTunnelThread::~SshTunnelThread()
{
    m_stop_thread = true;
    wait();
}

int SshTunnelThread::tunnelPort() const
{
    return m_tunnelPort;
}

QString SshTunnelThread::password() const
{
    return m_password;
}

// This is called by the main thread, but from a slot connected to our signal via BlockingQueuedConnection
// so this is safe even without a mutex, the semaphore in BlockingQueuedConnection takes care of the synchronization.
void SshTunnelThread::setPassword(const QString &password, PasswordOrigin origin)
{
    m_password = password;
    m_passwordOrigin = origin;
}

// This is called by the main thread, but from a slot connected to our signal via BlockingQueuedConnection
// so this is safe even without a mutex, the semaphore in BlockingQueuedConnection takes care of the synchronization.
void SshTunnelThread::userCanceledPasswordRequest()
{
    m_passwordRequestCanceledByUser = true;
}

void SshTunnelThread::run()
{
    struct CleanupHelper {
        int server_sock = -1;
        int client_sock = -1;
        ssh_session session = nullptr;
        ssh_channel forwarding_channel = nullptr;

        ~CleanupHelper()
        {
            // the ssh functions just return if the param is null
            ssh_channel_free(forwarding_channel);
            if (client_sock != -1) {
                close(client_sock);
            }
            if (server_sock != -1) {
                close(server_sock);
            }
            ssh_disconnect(session);
            ssh_free(session);
        }
    };

    CleanupHelper cleanup;

    ssh_session session = ssh_new();
    if (session == nullptr)
        return;

    cleanup.session = session;

    ssh_options_set(session, SSH_OPTIONS_HOST, m_host.constData());
    ssh_options_set(session, SSH_OPTIONS_USER, m_sshUserName.constData());
    ssh_options_set(session, SSH_OPTIONS_PORT, &m_sshPort);

    int res = ssh_connect(session);
    if (res != SSH_OK) {
        Q_EMIT errorMessage(i18n("Error connecting to %1: %2", QString::fromUtf8(m_host), QString::fromLocal8Bit(ssh_get_error(session))));
        return;
    }

    // First try authenticating via ssh agent
    res = ssh_userauth_agent(session, nullptr);

    m_passwordRequestCanceledByUser = false;
    if (res != SSH_AUTH_SUCCESS) {
        // If ssh agent didn't work, try with password
        Q_EMIT passwordRequest(NoFlags); // This calls blockingly to the main thread which will call setPassword
        res = ssh_userauth_password(session, nullptr, m_password.toUtf8().constData());

        // If password didn't work but came from the wallet, ask the user for the password
        if (!m_passwordRequestCanceledByUser && res != SSH_AUTH_SUCCESS && m_passwordOrigin == PasswordFromWallet) {
            Q_EMIT passwordRequest(IgnoreWallet); // This calls blockingly to the main thread which will call setPassword
            res = ssh_userauth_password(session, nullptr, m_password.toUtf8().constData());
        }
    }

    if (m_passwordRequestCanceledByUser) {
        return;
    }

    if (res != SSH_AUTH_SUCCESS) {
        Q_EMIT errorMessage(i18n("Error authenticating with password: %1", QString::fromLocal8Bit(ssh_get_error(session))));
        return;
    }

    const int server_sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (server_sock == -1) {
        Q_EMIT errorMessage(i18n("Error creating tunnel socket"));
        return;
    }

    cleanup.server_sock = server_sock;

    // so that we can bind more than once in case more than one tunnel is used
    int sockopt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt));

    {
        // bind the server socket
        struct sockaddr_in sin;
        sin.sin_family = AF_INET;
        sin.sin_port = htons(m_tunnelPort);
        sin.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (bind(server_sock, (struct sockaddr *)&sin, sizeof sin) == -1) {
            Q_EMIT errorMessage(i18n("Error creating tunnel socket"));
            return;
        }

        if (m_tunnelPort == 0) {
            socklen_t sin_len = sizeof sin;
            if (getsockname(server_sock, (struct sockaddr *)&sin, &sin_len) == -1) {
                Q_EMIT errorMessage(i18n("Error creating tunnel socket"));
                return;
            }
            m_tunnelPort = ntohs(sin.sin_port);
        }
    }

    if (listen(server_sock, 1) == -1) {
        Q_EMIT errorMessage(i18n("Error creating tunnel socket"));
        return;
    }

    if (m_stop_thread) {
        return;
    }

    Q_EMIT listenReady();
    // After here we don't need to emit errorMessage anymore on error, qCDebug is enough
    // this is because the actual vnc or rdp thread will start because of this call and thus
    // any socket error here will be detected by the vnc or rdp thread and the usual error mechanisms
    // there will warn the user interface

    int client_sock = -1;
    {
        struct sockaddr_in client_sin;
        socklen_t client_sin_len = sizeof client_sin;
        while (client_sock == -1) {
            if (m_stop_thread) {
                return;
            }

            client_sock = accept(server_sock, (struct sockaddr *)&client_sin, &client_sin_len);
            if (client_sock == -1 && errno != EAGAIN) {
                qCDebug(KRDC) << "Error on tunnel socket accept";
                return;
            }
        }

        cleanup.client_sock = client_sock;

        int sock_flags = fcntl(client_sock, F_GETFL, 0);
        fcntl(client_sock, F_SETFL, sock_flags | O_NONBLOCK);
    }

    ssh_channel forwarding_channel = ssh_channel_new(session);
    {
        const char *forward_remote_host = m_loopback ? "127.0.0.1" : m_host.constData();
        res = ssh_channel_open_forward(forwarding_channel, forward_remote_host, m_port, "127.0.0.1", 0);
        if (res != SSH_OK || !ssh_channel_is_open(forwarding_channel)) {
            qCDebug(KRDC) << "SSH channel open error" << ssh_get_error(session);
            return;
        }
        cleanup.forwarding_channel = forwarding_channel;
    }

    char client_read_buffer[40960];
    char *channel_read_buffer = nullptr;
    char *channel_read_buffer_ptr = nullptr;
    int channel_read_buffer_to_write;
    while (!m_stop_thread && !ssh_channel_is_eof(forwarding_channel)) {
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;

        fd_set set;
        FD_ZERO(&set);
        FD_SET(client_sock, &set);
        ssh_channel channels[2] = {forwarding_channel, nullptr};
        ssh_channel channels_out[2] = {nullptr, nullptr};

        res = ssh_select(channels, channels_out, client_sock + 1, &set, &timeout);
        if (res == SSH_EINTR)
            continue;
        if (res == -1)
            break;

        bool error = false;
        if (FD_ISSET(client_sock, &set)) {
            int bytes_read;
            while (!error && (bytes_read = read(client_sock, client_read_buffer, sizeof client_read_buffer)) > 0) {
                int bytes_written = 0;
                int bytes_to_write = bytes_read;
                for (char *ptr = client_read_buffer; bytes_to_write > 0; bytes_to_write -= bytes_written, ptr += bytes_written) {
                    bytes_written = ssh_channel_write(forwarding_channel, ptr, bytes_to_write);
                    if (bytes_written <= 0) {
                        error = true;
                        qCDebug(KRDC) << "error on ssh_channel_write";
                        break;
                    }
                }
            }
            if (bytes_read == 0) {
                qCDebug(KRDC) << "error on tunnel read";
                error = true;
            }
        }

        // If on the previous iteration we successfully wrote all we read, we need to read again
        if (!error && !channel_read_buffer) {
            const int bytes_available = ssh_channel_poll(forwarding_channel, 0);
            if (bytes_available == SSH_ERROR || bytes_available == SSH_EOF) {
                qCDebug(KRDC) << "error on ssh_channel_poll";
                error = true;
            } else if (bytes_available > 0) {
                channel_read_buffer = new char[bytes_available];
                channel_read_buffer_ptr = channel_read_buffer;
                const int bytes_read = ssh_channel_read_nonblocking(forwarding_channel, channel_read_buffer, bytes_available, 0);
                if (bytes_read <= 0) {
                    qCDebug(KRDC) << "error on ssh_channel_read_nonblocking";
                    error = true;
                } else {
                    channel_read_buffer_to_write = bytes_read;
                }
            }
        }

        if (!error && channel_read_buffer) {
            for (int bytes_written = 0; channel_read_buffer_to_write > 0;
                 channel_read_buffer_to_write -= bytes_written, channel_read_buffer_ptr += bytes_written) {
                bytes_written = write(client_sock, channel_read_buffer_ptr, channel_read_buffer_to_write);
                if (bytes_written == -1 && errno == EAGAIN) {
                    // socket is full, just carry on and we will write on the next iteration
                    // that is why the previous code does 'if (!channel_read_buffer)'
                    break;
                }
                if (bytes_written <= 0) {
                    qCDebug(KRDC) << "error on tunnel write";
                    error = true;
                    break;
                }
            }
            if (channel_read_buffer_to_write <= 0) {
                delete[] channel_read_buffer;
                channel_read_buffer = nullptr;
            }
        }
    }

    delete[] channel_read_buffer;
    channel_read_buffer = nullptr;
}
