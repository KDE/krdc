/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef RDPVIEWFACTORY_H
#define RDPVIEWFACTORY_H

#include "remoteviewfactory.h"

#include "rdpview.h"
#include "rdppreferences.h"

class RdpViewFactory : public RemoteViewFactory
{
    Q_OBJECT

public:
    explicit RdpViewFactory(QObject *parent, const QVariantList &args);

    ~RdpViewFactory() override;

    bool supportsUrl(const QUrl &url) const override;

    RemoteView *createView(QWidget *parent, const QUrl &url, KConfigGroup configGroup) override;

    HostPreferences *createHostPreferences(KConfigGroup configGroup, QWidget *parent) override;

    QString scheme() const override;

    QString connectActionText() const override;
    
    QString connectButtonText() const override;

    QString connectToolTipText() const override;

private Q_SLOTS:
    void checkFreerdpAvailability();

private:
    QString m_connectToolTipString;
};

#endif // RDPVIEWFACTORY_H
