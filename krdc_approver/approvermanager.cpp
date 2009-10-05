/****************************************************************************
**
** Copyright (C) 2009 Collabora Ltd <http://www.collabora.co.uk>
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

#include "approvermanager.h"
#include "approver.h"

#include <QDBusObjectPath>
#include <QDBusConnection>

#include <TelepathyQt4/Connection>
#include <TelepathyQt4/PendingOperation>
#include <TelepathyQt4/PendingReady>
#include <TelepathyQt4/Debug>

#include <KDebug>

static inline Tp::ChannelClassList channelClassList()
{
    QMap<QString, QDBusVariant> filter0;
    filter0[TELEPATHY_INTERFACE_CHANNEL ".ChannelType"] =
            QDBusVariant(TELEPATHY_INTERFACE_CHANNEL_TYPE_STREAM_TUBE);
    filter0[TELEPATHY_INTERFACE_CHANNEL_TYPE_STREAM_TUBE ".Service"] = QDBusVariant("rfb");
    filter0[TELEPATHY_INTERFACE_CHANNEL ".Requested"] = QDBusVariant(false);

    return Tp::ChannelClassList() << Tp::ChannelClass(filter0);
}

ApproverManager::ApproverManager(QObject *parent)
    : QObject(parent),
      AbstractClientApprover(channelClassList())
{
    kDebug() << "Initializing approver manager";
}

ApproverManager::~ApproverManager()
{
    kDebug() << "Destroying approver manager";
}

void ApproverManager::addDispatchOperation(const Tp::MethodInvocationContextPtr<> &context,
        const QList<Tp::ChannelPtr> &channels,
        const Tp::ChannelDispatchOperationPtr &dispatchOperation)
{
    kDebug() << "New channel for approving arrived";

    Approver *approver = new Approver(context, channels, dispatchOperation, this);
    connect(approver, SIGNAL(finished()), SLOT(onFinished()));

    m_approvers << approver;
}

void ApproverManager::onFinished()
{
    kDebug() << "Approver finished";

    Approver *approver = qobject_cast<Approver*>(sender());
    m_approvers.removeOne(approver);
    approver->deleteLater();

    if (m_approvers.empty()) {
        kDebug() << "Quiting approver manager";
        QCoreApplication::quit();
    }
}
