/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef VNCVIEWFACTORY_H
#define VNCVIEWFACTORY_H

#include "remoteviewfactory.h"
#include "vncview.h"
#include "vncpreferences.h"

class VncViewFactory : public RemoteViewFactory
{
    Q_OBJECT

public:
    explicit VncViewFactory(QObject *parent, const QVariantList &args);

    ~VncViewFactory() override;

    bool supportsUrl(const QUrl &url) const override;

    RemoteView *createView(QWidget *parent, const QUrl &url, KConfigGroup configGroup) override;

    HostPreferences *createHostPreferences(KConfigGroup configGroup, QWidget *parent) override;

    QString scheme() const override;

    QString connectActionText() const override;

    QString connectButtonText() const override;

    QString connectToolTipText() const override;
};

#endif // VNCVIEWFACTORY_H
