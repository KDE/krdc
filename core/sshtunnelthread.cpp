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
#include <poll.h>
#include <unistd.h>

#include <cerrno>
#include <memory>
#include <sys/socket.h>
#include <vector>

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
        ssh_session session = nullptr;
        ssh_event event = nullptr;

        ~CleanupHelper()
        {
            // the ssh functions just return if the param is null
            if (event) {
                ssh_event_remove_session(event, session);
                ssh_event_free(event);
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

    if (listen(server_sock, SOMAXCONN) == -1) {
        Q_EMIT errorMessage(i18n("Error creating tunnel socket"));
        return;
    }

    if (m_stop_thread) {
        return;
    }

    // All channels share one SSH session, owned exclusively by this thread.
    // Nonblocking operations prevent one slow connection from stalling the others.
    ssh_set_blocking(session, 0);
    cleanup.event = ssh_event_new();
    if (!cleanup.event || ssh_event_add_session(cleanup.event, session) != SSH_OK) {
        Q_EMIT errorMessage(i18n("Error creating tunnel socket"));
        return;
    }

    struct ForwardedConnection {
        int socket = -1;
        ssh_channel channel = nullptr;
        bool opened = false;
        bool localEof = false;
        bool eofSent = false;
        bool remoteEof = false;
        QByteArray toRemote;
        QByteArray toLocal;

        ~ForwardedConnection()
        {
            ssh_channel_free(channel);
            if (socket != -1)
                close(socket);
        }
    };
    // Destroy channels before the session in CleanupHelper.
    std::vector<std::unique_ptr<ForwardedConnection>> connections;
    const char *remoteHost = m_loopback ? "127.0.0.1" : m_host.constData();
    bool madeProgress = false;
    Q_EMIT listenReady();

    while (!m_stop_thread && ssh_is_connected(session)) {
        std::vector<pollfd> sockets;
        sockets.push_back({server_sock, POLLIN, 0});
        const short sshEvents = POLLIN | ((ssh_get_poll_flags(session) & SSH_WRITE_PENDING) ? POLLOUT : 0);
        sockets.push_back({ssh_get_fd(session), sshEvents, 0});
        for (const auto &connection : connections) {
            short events = 0;
            if (connection->opened && !connection->localEof && connection->toRemote.isEmpty())
                events |= POLLIN;
            if (!connection->toLocal.isEmpty())
                events |= POLLOUT;
            // Ignore sockets with no work until SSH makes progress (including
            // sockets with a persistent POLLHUP while buffered data is drained).
            sockets.push_back({events ? connection->socket : -1, events, 0});
        }

        res = poll(sockets.data(), sockets.size(), madeProgress ? 0 : 200);
        madeProgress = false;
        if (res < 0) {
            if (errno == EINTR)
                continue;
            qCDebug(KRDC) << "Error polling tunnel sockets";
            break;
        }
        if (m_stop_thread)
            break;
        if (ssh_event_dopoll(cleanup.event, 0) == SSH_ERROR)
            break;

        // Accept one connection per iteration so incoming clients cannot starve
        // existing channels. SPICE opens separate TCP connections on this port.
        if (sockets[0].revents & POLLIN) {
            const int client = accept(server_sock, nullptr, nullptr);
            if (client >= 0) {
                auto connection = std::make_unique<ForwardedConnection>();
                connection->socket = client;
                const int flags = fcntl(client, F_GETFL, 0);
                if (flags != -1 && fcntl(client, F_SETFL, flags | O_NONBLOCK) != -1) {
                    connection->channel = ssh_channel_new(session);
                    if (connection->channel)
                        connections.push_back(std::move(connection));
                }
                madeProgress = true;
            } else if (errno != EAGAIN && errno != EINTR) {
                qCDebug(KRDC) << "Error on tunnel socket accept";
                break;
            }
        }

        for (auto it = connections.begin(); it != connections.end();) {
            auto &connection = **it;
            bool error = false;
            if (!connection.opened) {
                res = ssh_channel_open_forward(connection.channel, remoteHost, m_port, "127.0.0.1", 0);
                if (res == SSH_AGAIN) {
                    ++it;
                    continue;
                }
                if (res != SSH_OK) {
                    qCDebug(KRDC) << "SSH channel open error" << ssh_get_error(session);
                    it = connections.erase(it);
                    continue;
                }
                connection.opened = true;
                madeProgress = true;
            }

            // Bound each direction's pending data and perform at most one read
            // and write per channel per iteration for fairness and backpressure.
            char buffer[40960];
            if (!connection.localEof && connection.toRemote.isEmpty()) {
                const int count = read(connection.socket, buffer, sizeof buffer);
                if (count > 0) {
                    connection.toRemote.append(buffer, count);
                    madeProgress = true;
                } else if (count == 0) {
                    connection.localEof = true;
                } else if (errno != EAGAIN && errno != EINTR) {
                    error = true;
                }
            }
            if (!error && !connection.toRemote.isEmpty()) {
                const int count = ssh_channel_write(connection.channel, connection.toRemote.constData(), connection.toRemote.size());
                if (count > 0) {
                    connection.toRemote.remove(0, count);
                    madeProgress = true;
                } else if (count != 0 && count != SSH_AGAIN) {
                    error = true;
                }
            }
            if (!error && connection.localEof && connection.toRemote.isEmpty() && !connection.eofSent) {
                res = ssh_channel_send_eof(connection.channel);
                if (res == SSH_OK) {
                    connection.eofSent = true;
                    madeProgress = true;
                } else if (res != SSH_AGAIN) {
                    error = true;
                }
            }
            if (!error && !connection.remoteEof && connection.toLocal.isEmpty()) {
                const int count = ssh_channel_read_nonblocking(connection.channel, buffer, sizeof buffer, 0);
                if (count > 0) {
                    connection.toLocal.append(buffer, count);
                    madeProgress = true;
                } else if (count == SSH_EOF || (count == 0 && ssh_channel_is_eof(connection.channel))) {
                    connection.remoteEof = true;
                    shutdown(connection.socket, SHUT_WR);
                    madeProgress = true;
                } else if (count == SSH_ERROR) {
                    error = true;
                }
            }
            if (!error && !connection.toLocal.isEmpty()) {
                const int count = send(connection.socket, connection.toLocal.constData(), connection.toLocal.size(), MSG_NOSIGNAL);
                if (count > 0) {
                    connection.toLocal.remove(0, count);
                    madeProgress = true;
                } else if (count == 0 || (errno != EAGAIN && errno != EINTR)) {
                    error = true;
                }
            }
            if (error || (connection.remoteEof && connection.toLocal.isEmpty() && (connection.eofSent || ssh_channel_is_closed(connection.channel)))) {
                if (error)
                    qCDebug(KRDC) << "Error forwarding tunnel connection" << ssh_get_error(session);
                it = connections.erase(it);
            } else {
                ++it;
            }
        }
    }
}
