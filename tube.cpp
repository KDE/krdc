/****************************************************************************
**
** Copyright (C) 2009 Collabora Ltd <http://www.collabora.co.uk>
** Copyright (C) 2009 Abner Silva <abner.silva@kdemail.net>
**
** This file is part of KDE.
**
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

#include "tube.h"

#include <QDBusConnection>

#include <TelepathyQt4/PendingOperation>
#include <TelepathyQt4/PendingReady>
#include <TelepathyQt4/Debug>

#include <KDebug>

Tube::Tube(Tp::ChannelPtr channel, QObject *parent)
    : QObject(parent),
      m_channel(channel)
{
    kDebug() << "Initializing tube";

    /* Registering struct containing the tube address */
    qDBusRegisterMetaType<StreamTubeAddress>();

    connect(m_channel->becomeReady(),
            SIGNAL(finished(Tp::PendingOperation *)),
            SLOT(onChannelReady(Tp::PendingOperation *)));
}

Tube::~Tube()
{
    kDebug() << "Destroying tube";
}

void Tube::close()
{
    m_channel->requestClose();
}

void Tube::onChannelReady(Tp::PendingOperation *op)
{
    kDebug() << "Channel is ready!";

    if (op->isError()) {
        qWarning() << "Connection cannot become ready";
        return;
    }

    connect(m_channel.data(),
            SIGNAL(invalidated(Tp::DBusProxy*, const QString&, const QString&)),
            SLOT(onChannelInvalidated(Tp::DBusProxy*, const QString&,
                 const QString&)));

    /* Interface used to control the tube state */
    m_tubeInterface = m_channel->tubeInterface();

    /* Interface used to control stream tube */
    m_streamTubeInterface = m_channel->streamTubeInterface();

    if (m_streamTubeInterface && m_tubeInterface) {
        kDebug() << "Accepting tube";

        connect(m_tubeInterface,
                SIGNAL(TubeChannelStateChanged(uint)),
                SLOT(onTubeStateChanged(uint)));

        /* Time to accept the tube stream */
        QDBusVariant ret = m_streamTubeInterface->Accept(
                Tp::SocketAddressTypeIPv4, Tp::SocketAccessControlLocalhost,
                QDBusVariant(""));

        /* Getting back the tube stream address */
        StreamTubeAddress sta = qdbus_cast<StreamTubeAddress>(ret.variant());
        kDebug() << "Tube address:" << sta.address << ":" << sta.port;

        m_url.setHost(sta.address);
        m_url.setPort(sta.port);
        //FIXME - Find a way to remove this hardcoded scheme
        m_url.setScheme("vnc");
    }
}

void Tube::onTubeStateChanged(uint state)
{
    kDebug() << "Tube state changed:" << state;
    if (state == Tp::TubeStateOpen)
        emit statusChanged(Tube::Open);
}

void Tube::onChannelInvalidated(Tp::DBusProxy *proxy,
        const QString &errorName, const QString &errorMessage)
{
    Q_UNUSED(proxy);

    kDebug() << "Tube channel invalidated:" << errorName << errorMessage;
    emit statusChanged(Tube::Close);
}

//Marshall the StreamTubeAddress data into a D-Bus argument
QDBusArgument &operator<<(QDBusArgument &argument,
        const StreamTubeAddress &streamTubeAddress)
{
    argument.beginStructure();
    argument << streamTubeAddress.address << streamTubeAddress.port;
    argument.endStructure();
    return argument;
}

// Retrieve the StreamTubeAddress data from the D-Bus argument
const QDBusArgument &operator>>(const QDBusArgument &argument,
        StreamTubeAddress &streamTubeAddress)
{
    argument.beginStructure();
    argument >> streamTubeAddress.address >> streamTubeAddress.port;
    argument.endStructure();
    return argument;
}
