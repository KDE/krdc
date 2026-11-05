/*
    SPDX-FileCopyrightText: 2024 KRDC Contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SPICEVIEWFACTORY_H
#define SPICEVIEWFACTORY_H

#include "remoteviewfactory.h"
#include "spicepreferences.h"
#include "spiceview.h"

#include <QHash>

#include <optional>

class SpiceViewFactory : public RemoteViewFactory
{
    Q_OBJECT

public:
    explicit SpiceViewFactory(QObject *parent, const QVariantList &args);

    ~SpiceViewFactory() override;

    bool supportsUrl(const QUrl &url) const override;

    ConnectionFile loadConnectionFile(const QUrl &url) const override;

    bool importFileSettings(const QHash<QString, QString> &entries, KConfigGroup configGroup, QWidget *parent) const override;

    RemoteView *createView(QWidget *parent, const QUrl &url, KConfigGroup configGroup) override;

    HostPreferences *createHostPreferences(KConfigGroup configGroup, QWidget *parent) override;

    QString scheme() const override;

    QString connectActionText() const override;

    QString connectButtonText() const override;

    QString connectToolTipText() const override;

private:
    // The connection that importFileSettings() accepted, for the createView() that follows it.
    mutable std::optional<SpiceFileConnection> m_acceptedConnection;
};

#endif // SPICEVIEWFACTORY_H
