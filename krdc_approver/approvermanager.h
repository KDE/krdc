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

#ifndef APPROVERMANAGER_H
#define APPROVERMANAGER_H

#include <TelepathyQt4/AbstractClientApprover>

class Approver;

class ApproverManager : public QObject, public Tp::AbstractClientApprover
{
    Q_OBJECT

public:
    ApproverManager(QObject *parent);
    virtual ~ApproverManager();

    virtual void addDispatchOperation(const Tp::MethodInvocationContextPtr<> &context,
            const QList<Tp::ChannelPtr> &channels,
            const Tp::ChannelDispatchOperationPtr &dispatchOperation);

private Q_SLOTS:
    void onFinished();

private:
    QList<Approver *> m_approvers;
};
#endif
