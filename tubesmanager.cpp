/****************************************************************************
**
** Copyright (C) 2009-2011 Collabora Ltd <info@collabora.co.uk>
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
#include "logging.h"

#include <TelepathyQt/IncomingStreamTubeChannel>
#include <TelepathyQt/Debug>
#include <TelepathyQt/Types>

TubesManager::TubesManager(QObject *parent)
    : QObject(parent)
{
    qCDebug(KRDC) << "Initializing tubes manager";

    Tp::enableDebug(true);
    Tp::enableWarnings(true);

    /* Registering telepathy types */
    Tp::registerTypes();

    m_stubeClient = Tp::StreamTubeClient::create(
            QStringList() << QLatin1String("rfb"),
            QStringList(),
            QLatin1String("krdc_rfb_handler"));

    m_stubeClient->setToAcceptAsTcp();

    connect(m_stubeClient.data(),
            SIGNAL(tubeAcceptedAsTcp(QHostAddress,quint16,QHostAddress,quint16,
                                     Tp::AccountPtr,Tp::IncomingStreamTubeChannelPtr)),
            SLOT(onTubeAccepted(QHostAddress,quint16,QHostAddress,quint16,
                                Tp::AccountPtr,Tp::IncomingStreamTubeChannelPtr)));
}

TubesManager::~TubesManager()
{
    qCDebug(KRDC) << "Destroying tubes manager";
}

void TubesManager::closeTube(const QUrl& url)
{
    if (m_tubes.contains(url)) {
        m_tubes.take(url)->requestClose();
    }
}

void TubesManager::onTubeAccepted(
        const QHostAddress & listenAddress, quint16 listenPort,
        const QHostAddress & sourceAddress, quint16 sourcePort,
        const Tp::AccountPtr & account,
        const Tp::IncomingStreamTubeChannelPtr & tube)
{
    Q_UNUSED(sourceAddress);
    Q_UNUSED(sourcePort);
    Q_UNUSED(account);

    QUrl url;
    url.setScheme(QLatin1String("vnc"));
    url.setHost(listenAddress.toString());
    url.setPort(listenPort);

    qCDebug(KRDC) << "newConnection:" << url;
    m_tubes.insert(url, tube);
    emit newConnection(url);
}
