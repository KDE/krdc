/*
    SPDX-FileCopyrightText: 2026 Tony Murray <murraytony@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef RDPREMOVABLEDRIVES_H
#define RDPREMOVABLEDRIVES_H

#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>

#include <Solid/Device>

#include <freerdp/client/rdpdr.h>

/**
 * Shares the removable drives of this computer with an RDP session, including drives that are
 * mounted during the session.
 *
 * FreeRDP checks for new drives every second on a thread of its own, and on its own takes every
 * mount below /media, /mnt and the like, network shares included. This replaces that check:
 * Solid tells which mounts are on removable drives, and FreeRDP's thread announces the ones that
 * came and went.
 */
class RdpRemovableDrives : public QObject
{
    Q_OBJECT

public:
    /** Starts following the removable drives. Must run on the GUI thread, like all of Solid. */
    explicit RdpRemovableDrives(QObject *parent = nullptr);

    /** Makes the drive redirection channel of a new connection ask this object for drives. */
    void attach(RdpdrClientContext *context);

private:
    void update();
    static UINT hotplug(RdpdrClientContext *context, RdpdrHotplugEventType type);
    UINT announceChanges(RdpdrClientContext *context, bool firstCheck);

    // Solid only reports the mounting of a device while a handle to it exists.
    QList<Solid::Device> m_devices;

    // Where the removable drives are mounted.
    QMutex m_mountedMutex;
    QStringList m_mounted;

    // Paths shared with the session, and their device ids. FreeRDP checks for drives once when
    // connecting and then on its own thread, so the checks are kept apart.
    QMutex m_sharedMutex;
    QHash<QString, uint32_t> m_shared;
};

#endif
