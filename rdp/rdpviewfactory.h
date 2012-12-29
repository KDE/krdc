/****************************************************************************
**
** Copyright (C) 2008 Urs Wolfer <uwolfer @ kde.org>
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

    virtual ~RdpViewFactory();

    virtual bool supportsUrl(const KUrl &url) const;

    virtual RemoteView *createView(QWidget *parent, const KUrl &url, KConfigGroup configGroup);

    virtual HostPreferences *createHostPreferences(KConfigGroup configGroup, QWidget *parent);

    virtual QString scheme() const;

    virtual QString connectActionText() const;
    
    virtual QString connectButtonText() const;

    virtual QString connectToolTipText() const;

private Q_SLOTS:
    void checkFreerdpAvailability();

private:
    QString m_connectToolTipString;
};

#endif // RDPVIEWFACTORY_H
