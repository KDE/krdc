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

#include "approvermanager.h"
#include "approver.h"

#include <QDBusObjectPath>
#include <QDBusConnection>

#include <KApplication>
#include <KDebug>

#include <TelepathyQt/Account>
#include <TelepathyQt/ChannelClassSpecList>
#include <TelepathyQt/ChannelDispatchOperation>
#include <TelepathyQt/Connection>
#include <TelepathyQt/PendingOperation>
#include <TelepathyQt/PendingReady>
#include <TelepathyQt/Debug>

static inline Tp::ChannelClassSpecList channelClassSpecList()
{
    Tp::ChannelClassSpec spec = Tp::ChannelClassSpec();
    spec.setChannelType(TP_QT_IFACE_CHANNEL_TYPE_STREAM_TUBE);
    spec.setTargetHandleType(Tp::HandleTypeContact);
    spec.setRequested(false);
    spec.setProperty(TP_QT_IFACE_CHANNEL_TYPE_STREAM_TUBE + ".Service",
                     QVariant("rfb"));
    return Tp::ChannelClassSpecList() << spec;
}

ApproverManager::ApproverManager(QObject *parent)
    : QObject(parent),
      AbstractClientApprover(channelClassSpecList())
{
    kDebug() << "Initializing approver manager";
}

ApproverManager::~ApproverManager()
{
    kDebug() << "Destroying approver manager";
}

void ApproverManager::addDispatchOperation(const Tp::MethodInvocationContextPtr<> &context,
        const Tp::ChannelDispatchOperationPtr &dispatchOperation)
{
    kDebug() << "New channel for approving arrived";

    Approver *approver = new Approver(context, dispatchOperation->channels(), dispatchOperation, this);
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
        kDebug() << "Quitting approver manager";
        KApplication::kApplication()->quit();
    }
}
