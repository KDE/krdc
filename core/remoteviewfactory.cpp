/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "remoteviewfactory.h"

RemoteViewFactory::RemoteViewFactory(QObject *parent)
    : QObject(parent)
{
}

RemoteViewFactory::~RemoteViewFactory()
{
}

RemoteViewFactory::ConnectionFile RemoteViewFactory::loadConnectionFile(const QUrl &url) const
{
    Q_UNUSED(url);
    return {};
}

bool RemoteViewFactory::importFileSettings(const QHash<QString, QString> &settings, KConfigGroup configGroup, QWidget *parent) const
{
    Q_UNUSED(settings);
    Q_UNUSED(configGroup);
    Q_UNUSED(parent);
    return true;
}