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

#ifndef TUBE_H
#define TUBE_H

#include <QObject>
#include <TelepathyQt4/Channel>

#include <KUrl>

namespace Tp
{
    class PendingOperation;
}

struct StreamTubeAddress {
    QString address;
    uint port;
};

Q_DECLARE_METATYPE(StreamTubeAddress)

class Tube : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Tube)
    Q_ENUMS(Status)

public:
    enum Status {
        Open = 0,
        Close = 1
    };

    ~Tube();

    KUrl url() const {return m_url;}
    void close();

private Q_SLOTS:
    void onChannelReady(Tp::PendingOperation *);
    void onTubeStateChanged(uint);
    void onChannelInvalidated(Tp::DBusProxy *proxy, const QString &errorName,
            const QString &errorMessage);

Q_SIGNALS:
    void statusChanged(Tube::Status status);

private:
    Tube(Tp::ChannelPtr channel, QObject *parent);

private:
    KUrl m_url;
    Tp::ChannelPtr m_channel;
    Tp::Client::ChannelTypeStreamTubeInterface *m_streamTubeInterface;
    Tp::Client::ChannelInterfaceTubeInterface *m_tubeInterface;

    friend class TubesManager;
};
#endif
