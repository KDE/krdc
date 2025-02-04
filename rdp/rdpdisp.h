/*
 * SPDX-FileCopyrightText: 2025 Fabio Bas <ctrlaltca@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QSize>
#include <QTimer>
#include <freerdp/client/disp.h>

struct RdpContext;

/* Simple timer implementation: QTimers can only be used with threads started with QThread */
#include <atomic>
#include <chrono>
#include <thread>

class RdpDisplayTimer
{
    std::atomic_bool m_running = false;

public:
    void start(std::function<void(void)> callback)
    {
        if (m_running) {
            return;
        }
        m_running = true;
        std::thread t([&, callback]() {
            if (!m_running) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (!m_running) {
                return;
            }
            callback();
            m_running = false;
        });
        t.detach();
    }

    void stop()
    {
        m_running = false;
    }
};

/* Timer implementation end */

class RdpDisplay : public QObject
{
    Q_OBJECT

public:
    RdpDisplay(RdpContext *krdp, DispClientContext *dispctx);
    ~RdpDisplay();

    static UINT onDisplayControlCaps(DispClientContext *disp, UINT32 maxNumMonitors, UINT32 maxMonitorAreaFactorA, UINT32 maxMonitorAreaFactorB);

    bool sendResizeEvent(const QSize newSize);

private:
    RdpContext *m_krdp;
    DispClientContext *m_dispctx = nullptr;
    RdpDisplayTimer m_updateTimer;

    QSize m_lastSize;
    UINT16 m_lastSentDesktopOrientation = 0;
    UINT32 m_lastSentDesktopScaleFactor = 0;
    UINT32 m_lastSentDeviceScaleFactor = 0;

public Q_SLOTS:
    void onUpdateTimer();
};