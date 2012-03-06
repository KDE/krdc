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

#ifndef TUBESMANAGER_H
#define TUBESMANAGER_H

#include <TelepathyQt4/StreamTubeClient>
#include <QtNetwork/QHostAddress>
#include <KUrl>

class TubesManager : public QObject
{
    Q_OBJECT

public:
    TubesManager(QObject *parent);
    virtual ~TubesManager();

    void closeTube(const KUrl & url);

Q_SIGNALS:
    void newConnection(KUrl);

private Q_SLOTS:
    void onTubeAccepted(
            const QHostAddress & listenAddress,
            quint16 listenPort,
            const QHostAddress & sourceAddress,
            quint16 sourcePort,
            const Tp::AccountPtr & account,
            const Tp::IncomingStreamTubeChannelPtr & tube);

private:
    Tp::StreamTubeClientPtr m_stubeClient;
    QHash<KUrl, Tp::IncomingStreamTubeChannelPtr> m_tubes;
};

#endif
