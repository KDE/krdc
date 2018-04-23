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

#ifndef REMOTEVIEWFACTORY_H
#define REMOTEVIEWFACTORY_H

#include "remoteview.h"
#include "krdccore_export.h"

#include <KCoreAddons/KPluginFactory>
#include <KConfigCore/KConfigGroup>
#include <KCoreAddons/KPluginLoader>

/**
 * Factory to be implemented by any plugin.
 */
class KRDCCORE_EXPORT RemoteViewFactory : public QObject
{
    Q_OBJECT

public:
    /**
     * Deconstructor.
     */
    ~RemoteViewFactory() override;

    /**
     * Returns true if the provided @p url is supported by the current plugin.
     */
    virtual bool supportsUrl(const QUrl &url) const = 0;

    /**
     * Returns a new RemoteView implementing object.
     */
    virtual RemoteView *createView(QWidget *parent, const QUrl &url, KConfigGroup configGroup) = 0;

    /**
     * Returns a new HostPreferences implementing object or 0 if no settings are available.
     */
    virtual HostPreferences *createHostPreferences(KConfigGroup configGroup, QWidget *parent) = 0;

    /**
     * Returns the supported scheme.
     * @see QUrl::scheme()
     */
    virtual QString scheme() const = 0;

    /**
     * Returns the text of the action in the file menu (by default).
     */
    virtual QString connectActionText() const = 0;

    /**
     * Returns the text of the connect button on the startscreen.
     */
    virtual QString connectButtonText() const = 0;

    /**
     * Returns the tooltip next to the url navigator when the user presses
     * the connect action.
     */
    virtual QString connectToolTipText() const = 0;

protected:
    /**
     * Protected constructor so it cannot be instantiated.
     */
    RemoteViewFactory(QObject *parent = nullptr);

};

#endif // REMOTEVIEWFACTORY_H
