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

QUrl RemoteViewFactory::loadUrlFromFile(const QUrl &url) const
{
    Q_UNUSED(url);
    return {};
}