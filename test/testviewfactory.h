/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TESTVIEWFACTORY_H
#define TESTVIEWFACTORY_H

#include "remoteviewfactory.h"

#include "testview.h"

class TestViewFactory : public RemoteViewFactory
{
    Q_OBJECT

public:
    explicit TestViewFactory(QObject *parent, const QVariantList &args);

    ~TestViewFactory() override;

    bool supportsUrl(const QUrl &url) const override;

    RemoteView *createView(QWidget *parent, const QUrl &url, KConfigGroup configGroup) override;

    HostPreferences *createHostPreferences(KConfigGroup configGroup, QWidget *parent) override;

    QString scheme() const override;

    QString connectActionText() const override;

    QString connectButtonText() const override;

    QString connectToolTipText() const override;
};

#endif // TESTVIEWFACTORY_H
