/****************************************************************************
**
** Copyright (C) 2009 Collabora Ltd <info@collabora.co.uk>
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

#include "approver.h"
#include "logging.h"

#include <KNotification>
#include <KLocalizedString>

#include <TelepathyQt/Contact>
#include <TelepathyQt/Connection>
#include <TelepathyQt/ReferencedHandles>
#include <TelepathyQt/PendingReady>
#include <TelepathyQt/ChannelDispatchOperation>

Approver::Approver(const Tp::MethodInvocationContextPtr<> &context,
            const QList<Tp::ChannelPtr> &channels,
            const Tp::ChannelDispatchOperationPtr &dispatchOperation,
            QObject *parent)
    : QObject(parent),
      m_context(context),
      m_channels(channels),
      m_dispatchOp(dispatchOperation),
      m_notification(0)
{
    qCDebug(KRDC) << "Initializing approver";

    connect(m_dispatchOp->becomeReady(),
            SIGNAL(finished(Tp::PendingOperation*)),
            SLOT(onDispatchOperationReady(Tp::PendingOperation*)));
}

void Approver::onDispatchOperationReady(Tp::PendingOperation *op)
{
    if (op->isError()) {
        qCritical(KRDC) << "Dispatch operation failed to become ready"
                << op->errorName() << op->errorMessage();
        m_context->setFinishedWithError(op->errorName(), op->errorMessage());
        emit finished();
        return;
    }

    qCDebug(KRDC) << "DispatchOp ready!";

    Tp::ChannelPtr channel = m_dispatchOp->channels()[0];
    connect(channel->becomeReady(),
            SIGNAL(finished(Tp::PendingOperation*)),
            SLOT(onChannelReady(Tp::PendingOperation*)));
}

void Approver::onChannelReady(Tp::PendingOperation *op)
{
    if (op->isError()) {
        qCritical(KRDC) << "Channel failed to become ready"
                << op->errorName() << op->errorMessage();
        m_context->setFinishedWithError(op->errorName(), op->errorMessage());
        emit finished();
        return;
    }

    qCDebug(KRDC) << "Channel ready!";

    Tp::ContactPtr contact = m_dispatchOp->channels()[0]->initiatorContact();

    KNotification *notification = new KNotification(QLatin1String("newrfb"), NULL, KNotification::Persistent);
    notification->setTitle(i18n("Invitation to view remote desktop"));
    notification->setText(i18n("%1 wants to share his/her desktop with you", contact->alias()));
    notification->setActions(QStringList() << i18n("Accept") << i18n("Reject"));

    Tp::Client::ConnectionInterfaceAvatarsInterface *avatarIface =
        m_dispatchOp->channels()[0]->connection()->optionalInterface<Tp::Client::ConnectionInterfaceAvatarsInterface>();

    if (avatarIface) {
        QDBusPendingReply<QByteArray, QString> reply = avatarIface->RequestAvatar(
                contact->handle().takeFirst());

        reply.waitForFinished();

        if (!reply.isError()) {
            QPixmap avatar;
            avatar.loadFromData(reply.value());
            notification->setPixmap(avatar);
        }
    }

    connect(notification, SIGNAL(action1Activated()), SLOT(onAccepted()));
    connect(notification, SIGNAL(action2Activated()), SLOT(onRejected()));
    connect(notification, SIGNAL(ignored()), SLOT(onRejected()));

    notification->sendEvent();

    m_notification = notification;

    m_context->setFinished();
}

Approver::~Approver()
{
    qCDebug(KRDC) << "Destroying approver";
}

void Approver::onAccepted()
{
    qCDebug(KRDC) << "Channel approved";
    m_dispatchOp->handleWith(TP_QT_IFACE_CLIENT + QLatin1String(".krdc_rfb_handler"));

    emit finished();
}

void Approver::onRejected()
{
    qCDebug(KRDC) << "Channel rejected";
    connect(m_dispatchOp->claim(), SIGNAL(finished(Tp::PendingOperation*)),
            SLOT(onClaimFinished(Tp::PendingOperation*)));
}

void Approver::onClaimFinished(Tp::PendingOperation *op)
{
    if (op->isError()) {
        qCritical(KRDC) << "Claim operation failed"
                << op->errorName() << op->errorMessage();
        m_context->setFinishedWithError(op->errorName(), op->errorMessage());
    } else {
        foreach(const Tp::ChannelPtr &channel, m_channels)
            channel->requestClose();
    }

    emit finished();
}
