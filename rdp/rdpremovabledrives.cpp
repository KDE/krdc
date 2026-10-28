/*
    SPDX-FileCopyrightText: 2026 Tony Murray <murraytony@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rdpremovabledrives.h"

#include <Solid/DeviceNotifier>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>

#include <QDir>

#include <freerdp/channels/rdpdr.h>
#include <freerdp/settings.h>

#include <algorithm>

namespace
{
bool isOnRemovableDrive(const Solid::Device &device)
{
    // Between a volume and its drive there can be a partition table or an encrypted container.
    for (Solid::Device ancestor = device; ancestor.isValid(); ancestor = ancestor.parent()) {
        if (const auto *drive = ancestor.as<Solid::StorageDrive>()) {
            return drive->isRemovable() || drive->isHotpluggable();
        }
    }
    return false;
}
}

RdpRemovableDrives::RdpRemovableDrives(QObject *parent)
    : QObject(parent)
{
    // Other devices, such as input devices, come and go too.
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceAdded, this, [this](const QString &udi) {
        if (Solid::Device(udi).is<Solid::StorageAccess>()) {
            update();
        }
    });
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceRemoved, this, [this](const QString &udi) {
        if (std::any_of(m_devices.cbegin(), m_devices.cend(), [&udi](const Solid::Device &device) {
                return device.udi() == udi;
            })) {
            update();
        }
    });
    update();
}

void RdpRemovableDrives::attach(RdpdrClientContext *context)
{
    context->custom = this;
    context->RdpdrHotplugDevice = hotplug;
}

void RdpRemovableDrives::update()
{
    QStringList mounted;
    m_devices.clear();
    const auto devices = Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess);
    for (const Solid::Device &device : devices) {
        if (!isOnRemovableDrive(device)) {
            continue;
        }
        m_devices.append(device);
        const auto *access = device.as<Solid::StorageAccess>();
        connect(access, &Solid::StorageAccess::accessibilityChanged, this, &RdpRemovableDrives::update, Qt::UniqueConnection);
        if (access->isAccessible() && !access->filePath().isEmpty()) {
            mounted.append(access->filePath());
        }
    }

    QMutexLocker locker(&m_mountedMutex);
    m_mounted = mounted;
}

UINT RdpRemovableDrives::hotplug(RdpdrClientContext *context, RdpdrHotplugEventType type)
{
    return static_cast<RdpRemovableDrives *>(context->custom)->announceChanges(context, type == RDPDR_HOTPLUG_FIRST_CHECK);
}

UINT RdpRemovableDrives::announceChanges(RdpdrClientContext *context, bool firstCheck)
{
    QStringList mounted;
    {
        QMutexLocker locker(&m_mountedMutex);
        mounted = m_mounted;
    }

    QMutexLocker locker(&m_sharedMutex);
    UINT result = CHANNEL_RC_OK;

    // The first check comes with each connection of the channel, which starts without drives.
    if (firstCheck) {
        m_shared.clear();
    }

    // A drive stays known until its removal is announced, so a failed one is tried again.
    QStringList gonePaths;
    QList<uint32_t> goneIds;
    for (auto it = m_shared.cbegin(); it != m_shared.cend(); ++it) {
        if (!mounted.contains(it.key())) {
            gonePaths.append(it.key());
            goneIds.append(it.value());
        }
    }
    if (!goneIds.isEmpty()) {
        result = context->RdpdrUnregisterDevice(context, goneIds.size(), goneIds.constData());
        if (result == CHANNEL_RC_OK) {
            for (const QString &path : std::as_const(gonePaths)) {
                m_shared.remove(path);
            }
        }
    }

    // A drive that cannot be shared is tried again on the next check, as FreeRDP does. Drives
    // are named after their mount folder, which unlike their label is unique.
    for (const QString &mountPoint : std::as_const(mounted)) {
        if (m_shared.contains(mountPoint)) {
            continue;
        }
        const QByteArray name = QDir(mountPoint).dirName().toUtf8();
        const QByteArray path = mountPoint.toUtf8();
        const char *const args[] = {name.constData(), path.constData()};
        RDPDR_DEVICE *device = freerdp_device_new(RDPDR_DTYP_FILESYSTEM, 2, args);
        if (!device) {
            return ERROR_OUTOFMEMORY;
        }
        uint32_t id = 0;
        const UINT registered = context->RdpdrRegisterDevice(context, device, &id);
        freerdp_device_free(device);
        if (registered == CHANNEL_RC_OK) {
            m_shared.insert(mountPoint, id);
        } else {
            result = registered;
        }
    }
    return result;
}
