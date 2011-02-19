/****************************************************************************
**
** Copyright (C) 2009 Collabora Ltd <info@collabora.co.uk>
** Copyright (C) 2009 Abner Silva <abner.silva@kdemail.net>
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

#include "tubesmanager.h"

#include <TelepathyQt4/ChannelClassSpecList>
#include <TelepathyQt4/Debug>

#include <KDebug>

static inline Tp::ChannelClassSpecList channelClassSpecList()
{
    Tp::ChannelClassSpec spec = Tp::ChannelClassSpec();
    spec.setChannelType(TP_QT4_IFACE_CHANNEL_TYPE_STREAM_TUBE);
    spec.setTargetHandleType(Tp::HandleTypeContact);
    spec.setRequested(false);
    spec.setProperty(QLatin1String(TP_QT4_IFACE_CHANNEL_TYPE_STREAM_TUBE ".Service"),
                     QVariant("rfb"));
    return Tp::ChannelClassSpecList() << spec;
}

TubesManager::TubesManager(QObject *parent)
    : QObject(parent),
      AbstractClientHandler(channelClassSpecList())
{
    kDebug() << "Initializing tubes manager";

    Tp::enableDebug(true);
    Tp::enableWarnings(true);

    /* Registering telepathy types */
    Tp::registerTypes();
}

TubesManager::~TubesManager()
{
    kDebug() << "Destroying tube manager";
}

bool TubesManager::bypassApproval() const
{
    kDebug() << "bypassing";
    return false;
}

void TubesManager::handleChannels(const Tp::MethodInvocationContextPtr<> & context,
        const Tp::AccountPtr & account,
        const Tp::ConnectionPtr & connection,
        const QList<Tp::ChannelPtr> & channels,
        const QList<Tp::ChannelRequestPtr> & requestsSatisfied,
        const QDateTime & userActionTime,
        const Tp::AbstractClientHandler::HandlerInfo & handlerInfo)
{
    Q_UNUSED(account);
    Q_UNUSED(connection);
    Q_UNUSED(requestsSatisfied);
    Q_UNUSED(userActionTime);
    Q_UNUSED(handlerInfo);

    foreach(const Tp::ChannelPtr &channel, channels) {
        kDebug() << "Handling new tube";

        QVariantMap properties = channel->immutableProperties();

        if (properties[TELEPATHY_INTERFACE_CHANNEL ".ChannelType"] ==
               TELEPATHY_INTERFACE_CHANNEL_TYPE_STREAM_TUBE) {

            kDebug() << "Handling:" << TELEPATHY_INTERFACE_CHANNEL_TYPE_STREAM_TUBE;

            /* New tube arrived */
            Tube *tube = new Tube(channel, this);

            /* Listening to tube status changed */
            connect(tube,
                    SIGNAL(statusChanged(Tube::Status)),
                    SLOT(onTubeStateChanged(Tube::Status)));

            m_tubes.append(tube);
        }
    }
    context->setFinished();
}

void TubesManager::closeTube(KUrl url)
{
    kDebug() << "Closing tube:" << url;

    foreach (Tube *tube, m_tubes)
        if (tube->url() == url) {
            m_tubes.removeOne(tube);
            tube->close();
        }
}

void TubesManager::onTubeStateChanged(Tube::Status status)
{
    kDebug() << "tube status changed:" << status;
    Tube *tube = qobject_cast<Tube *>(sender());

    if (status == Tube::Open) {
        KUrl url = tube->url();
        kDebug() << "newConnection:" << url;
        emit newConnection(url);
    }
    else if (status == Tube::Close) {
        kDebug() << "Tube:" << tube->url() << "closed";
        m_tubes.removeOne(tube);
        tube->deleteLater();
        kDebug() << m_tubes;
    }
}
