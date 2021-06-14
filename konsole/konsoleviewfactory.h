/*
    SPDX-FileCopyrightText: 2009 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KONSOLEVIEWFACTORY_H
#define KONSOLEVIEWFACTORY_H

#include "remoteviewfactory.h"

#include "konsoleview.h"

class KonsoleViewFactory : public RemoteViewFactory
{
    Q_OBJECT

public:
    explicit KonsoleViewFactory(QObject *parent, const QVariantList &args);

    virtual ~KonsoleViewFactory();

    virtual bool supportsUrl(const QUrl &url) const;

    virtual RemoteView *createView(QWidget *parent, const QUrl &url, KConfigGroup configGroup);

    virtual HostPreferences *createHostPreferences(KConfigGroup configGroup, QWidget *parent);

    virtual QString scheme() const;

    virtual QString connectActionText() const;

    virtual QString connectButtonText() const;

    virtual QString connectToolTipText() const;
};

#endif // KONSOLEVIEWFACTORY_H
