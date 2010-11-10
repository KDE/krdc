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

#ifndef APPROVER_H
#define APPROVER_H

#include <QObject>

#include <TelepathyQt4/Channel>

class KNotification;

class Approver : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Approver)

public:
    ~Approver();

private Q_SLOTS:
    void onAccepted();
    void onRejected();
    void onDispatchOperationReady(Tp::PendingOperation*);
    void onChannelReady(Tp::PendingOperation*);
    void onClaimFinished(Tp::PendingOperation*);

Q_SIGNALS:
    void finished();

private:
    Approver(const Tp::MethodInvocationContextPtr<> &context,
            const QList<Tp::ChannelPtr> &channels,
            const Tp::ChannelDispatchOperationPtr &dispatchOperation,
            QObject *parent);

private:
    Tp::MethodInvocationContextPtr<> m_context;
    QList<Tp::ChannelPtr> m_channels;
    Tp::ChannelDispatchOperationPtr m_dispatchOp;
    QPointer<KNotification> m_notification;

    friend class ApproverManager;
};
#endif
