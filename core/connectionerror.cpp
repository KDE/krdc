/*
    SPDX-FileCopyrightText: 2026 Tony Murray <murraytony@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "connectionerror.h"

#include <KLocalizedString>

QString ConnectionError::stageName(Stage stage)
{
    switch (stage) {
    case Stage::SshTunnel:
        return i18nc("@info connection stage", "Setting up the SSH tunnel");
    case Stage::NameResolution:
        return i18nc("@info connection stage", "Looking up the host name");
    case Stage::Network:
        return i18nc("@info connection stage", "Connecting to the server");
    case Stage::Security:
        return i18nc("@info connection stage", "Securing the connection");
    case Stage::Authentication:
        return i18nc("@info connection stage", "Logging in");
    case Stage::Session:
        return i18nc("@info connection stage", "Starting the remote session");
    case Stage::ConnectionLost:
        return i18nc("@info connection stage", "Connection interrupted");
    case Stage::SessionEnded:
        return i18nc("@info connection stage", "Session ended remotely");
    case Stage::Unknown:
        break;
    }
    return i18nc("@info connection stage", "Unknown");
}

ConnectionError ConnectionError::create(Stage stage, const QString &message, const QString &details)
{
    ConnectionError error;
    error.stage = stage;
    error.message = message;
    error.details = details;

    switch (stage) {
    case Stage::SshTunnel:
        error.title = i18nc("@title connection error", "SSH tunnel failed");
        error.steps = {
            i18nc("@info", "Check the SSH tunnel user name and port in the host settings."),
            i18nc("@info", "Log in with an SSH client in a terminal to confirm that the server is reachable and your credentials work."),
            i18nc("@info", "If you use key authentication, add the key to your SSH agent with ssh-add."),
            i18nc("@info", "Make sure the SSH server allows port forwarding."),
        };
        break;
    case Stage::NameResolution:
        error.title = i18nc("@title connection error", "Server not found");
        error.steps = {
            i18nc("@info", "Check the spelling of the address."),
            i18nc("@info", "Connect to the network or VPN the computer is on."),
            i18nc("@info", "Try the IP address of the computer instead of its name."),
        };
        error.actions = Actions(Retry | EditAddress);
        break;
    case Stage::Network:
        error.title = i18nc("@title connection error", "Server not reachable");
        error.steps = {
            i18nc("@info", "Make sure the remote computer is turned on, awake and connected to the network."),
            i18nc("@info", "Check that remote desktop sharing is enabled on the remote computer."),
            i18nc("@info", "Check the port in the address. The defaults are 3389 for RDP and 5900 for VNC and SPICE."),
            i18nc("@info", "Check that no firewall blocks the connection."),
        };
        error.actions = Actions(Retry | EditAddress);
        break;
    case Stage::Security:
        error.title = i18nc("@title connection error", "Secure connection failed");
        error.steps = {
            i18nc("@info", "Review the security settings for this host, or reset them to their defaults."),
            i18nc("@info", "If the server certificate changed unexpectedly, confirm the change with the server administrator before accepting it."),
        };
        break;
    case Stage::Authentication:
        error.title = i18nc("@title connection error", "Authentication failed");
        error.steps = {
            i18nc("@info", "Check the user name, password and domain, and that Caps Lock is off."),
            i18nc("@info", "If you use a saved password, enter it again. It may be out of date."),
            i18nc("@info", "If the account is locked or expired, contact the administrator of the remote computer."),
        };
        break;
    case Stage::Session:
        error.title = i18nc("@title connection error", "Session setup failed");
        error.steps = {
            i18nc("@info", "Reset advanced options in the host settings, such as graphics acceleration or color depth, to their defaults."),
            i18nc("@info", "Ask the administrator of the remote computer whether new sessions are allowed. The number of users may be limited."),
        };
        break;
    case Stage::ConnectionLost:
        // The page heading already says the connection was lost.
        error.title = i18nc("@title connection error", "Network connection interrupted");
        error.steps = {
            i18nc("@info", "Check your network connection, then reconnect."),
            i18nc("@info", "If the remote computer restarted or went to sleep, wait until it is back, then reconnect."),
        };
        error.actions = Retry;
        break;
    case Stage::SessionEnded:
        error.title = i18nc("@title connection error", "Closed by the server");
        error.actions = Retry;
        break;
    case Stage::Unknown:
        // Not "Connection failed": the heading already says that.
        error.title = i18nc("@title connection error", "Unexpected error");
        break;
    }

    return error;
}

ConnectionError ConnectionError::serverClosedDuringSetup(const QString &details)
{
    ConnectionError error = create(Stage::Session, i18nc("@label", "It accepted the connection, then closed it before the session started."), details);
    error.title = i18nc("@title connection error", "Server closed the connection");
    error.steps = {
        i18nc("@info", "If another viewer is connected, close it. Some servers allow only one at a time."),
        i18nc("@info", "Check that this port belongs to the remote desktop service you expect."),
        i18nc("@info", "Check the access and security settings of the server, or its log, for why it closed the connection."),
    };
    return error;
}

ConnectionError ConnectionError::networkFailure(NetworkFailure failure, const QString &details)
{
    switch (failure) {
    case NetworkFailure::Refused: {
        ConnectionError error = create(Stage::Network, QString(), details);
        error.title = i18nc("@title connection error", "Connection refused");
        error.steps = {
            i18nc("@info", "Check that remote desktop sharing is enabled on the remote computer."),
            i18nc("@info", "Check the port in the address. The defaults are 3389 for RDP and 5900 for VNC and SPICE."),
            i18nc("@info", "Check that no firewall on the remote computer rejects the connection."),
        };
        return error;
    }
    case NetworkFailure::NoResponse: {
        ConnectionError error = create(Stage::Network, QString(), details);
        error.title = i18nc("@title connection error", "Server not responding");
        error.steps = {
            i18nc("@info", "Make sure the remote computer is turned on, awake and connected to the network."),
            i18nc("@info", "Check that no firewall blocks the connection."),
            i18nc("@info", "Check that the address is right."),
        };
        return error;
    }
    case NetworkFailure::Unreachable: {
        ConnectionError error = create(Stage::Network, QString(), details);
        error.title = i18nc("@title connection error", "No route to the server");
        error.steps = {
            i18nc("@info", "Connect to the network or VPN the remote computer is on."),
            i18nc("@info", "Check that the address is right."),
        };
        return error;
    }
    case NetworkFailure::Unknown:
        break;
    }
    return create(Stage::Network, QString(), details);
}
