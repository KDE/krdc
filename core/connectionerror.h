/*
    SPDX-FileCopyrightText: 2026 Tony Murray <murraytony@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CONNECTIONERROR_H
#define CONNECTIONERROR_H

#include "krdccore_export.h"

#include <QFlags>
#include <QMetaType>
#include <QString>
#include <QStringList>

/**
 * Why a connection failed or ended, and what the user can do about it.
 * Create with create(), which fills in defaults for the stage.
 */
class KRDCCORE_EXPORT ConnectionError
{
public:
    enum class Stage {
        Unknown,
        SshTunnel, ///< Establishing the SSH tunnel to the host
        NameResolution, ///< Looking up the host name
        Network, ///< Opening a network connection to the remote desktop service
        Security, ///< Negotiating encryption or verifying the server certificate
        Authentication, ///< Logging in
        Session, ///< Setting up the remote session after the server accepted the connection
        ConnectionLost, ///< An established connection was interrupted
        SessionEnded, ///< The remote side ended an established session on purpose; not a failure
    };

    enum Action {
        Retry = 1, ///< Connect again with the same address and settings
        EditSettings = 2, ///< Change the host settings, then connect again
        EditAddress = 4, ///< Change the address, then connect again
    };
    Q_DECLARE_FLAGS(Actions, Action)

    /**
     * @param message what the title does not already say, in one sentence, or empty. The page
     *                shows the host and address, so the message does not repeat them.
     * @param details technical details, such as a backend error name or code
     */
    static ConnectionError create(Stage stage, const QString &message = QString(), const QString &details = QString());

    /** The server accepted the connection, then closed it before the session started. */
    static ConnectionError serverClosedDuringSetup(const QString &details = QString());

    static QString stageName(Stage stage);

    /** Why the server did not take a network connection, as the operating system reported it. */
    enum class NetworkFailure {
        Unknown,
        Refused,
        NoResponse,
        Unreachable,
    };

    /** A Network error with the title and steps that fit @p failure. */
    static ConnectionError networkFailure(NetworkFailure failure, const QString &details = QString());

    Stage stage = Stage::Unknown;
    /// the kind of failure, such as "Server not found"
    QString title;
    QString message;
    /// what to check or try, most likely first; each names the cause it rules out
    QStringList steps;
    QString details;
    Actions actions = Actions(Retry | EditSettings);
    /// false when the backend may continue after reporting this, e.g. an informational logon message
    bool fatal = true;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ConnectionError::Actions)
Q_DECLARE_METATYPE(ConnectionError)

#endif
