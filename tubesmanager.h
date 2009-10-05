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

#ifndef TUBESMANAGER_H
#define TUBESMANAGER_H

#include "tube.h"

#include <TelepathyQt4/AbstractClientHandler>

#include <KUrl>

class TubesManager : public QObject, public Tp::AbstractClientHandler
{
    Q_OBJECT

public:
    TubesManager(QObject *parent);
    virtual ~TubesManager();

    void closeTube(KUrl url);

    virtual bool bypassApproval() const;
    virtual void handleChannels(const Tp::MethodInvocationContextPtr<> & context,
            const Tp::AccountPtr & account,
            const Tp::ConnectionPtr & connection,
            const QList<Tp::ChannelPtr> & channels,
            const QList<Tp::ChannelRequestPtr> & requestsSatisfied,
            const QDateTime & userActionTime,
            const QVariantMap & handlerInfo);

private Q_SLOTS:
    void onTubeStateChanged(Tube::Status);

Q_SIGNALS:
    void newConnection(KUrl);

private:
    QList<Tube *> m_tubes;
};
#endif
