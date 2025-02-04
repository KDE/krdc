/*
 * SPDX-FileCopyrightText: 2025 Fabio Bas <ctrlaltca@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "rdpdisp.h"
#include "krdc_debug.h"
#include "rdpsession.h"
#include "rdpview.h"

#include <freerdp/version.h>

UINT RdpDisplay::onDisplayControlCaps(DispClientContext *disp, UINT32 maxNumMonitors, UINT32 maxMonitorAreaFactorA, UINT32 maxMonitorAreaFactorB)
{
    Q_UNUSED(disp);
    qCDebug(KRDC) << "DisplayControlCaps: MaxNumMonitors=" << maxNumMonitors << "MaxMonitorAreaFactorA=" << maxMonitorAreaFactorA
                  << "MaxMonitorAreaFactorB=" << maxMonitorAreaFactorB;

    return CHANNEL_RC_OK;
}

bool RdpDisplay::sendResizeEvent(const QSize newSize)
{
    if (!newSize.isValid() || newSize == m_lastSize) {
        return false;
    }

    m_lastSize = newSize;
    m_updateTimer.start(std::bind(&RdpDisplay::onUpdateTimer, this));

    return true;
}

void RdpDisplay::onUpdateTimer()
{
    auto ctx = reinterpret_cast<rdpContext *>(m_krdp);
    WINPR_ASSERT(ctx);

    auto settings = ctx->settings;
    WINPR_ASSERT(settings);

    qCInfo(KRDC) << "RDP resize event:" << m_lastSize;

    // TODO Multi monitor, orientation and dpi support

    DISPLAY_CONTROL_MONITOR_LAYOUT layout;
    layout.Flags = DISPLAY_CONTROL_MONITOR_PRIMARY;
    layout.Top = 0;
    layout.Left = 0;
    layout.Width = m_lastSize.width();
    layout.Height = m_lastSize.height();
    layout.Orientation = freerdp_settings_get_uint16(settings, FreeRDP_DesktopOrientation);
    layout.DesktopScaleFactor = freerdp_settings_get_uint32(settings, FreeRDP_DesktopScaleFactor);
    layout.DeviceScaleFactor = freerdp_settings_get_uint32(settings, FreeRDP_DeviceScaleFactor);
    layout.PhysicalWidth = m_lastSize.width();
    layout.PhysicalHeight = m_lastSize.height();

    m_dispctx->SendMonitorLayout(m_dispctx, 1, &layout);
}

RdpDisplay::RdpDisplay(RdpContext *krdp, DispClientContext *dispctx)
{
    m_krdp = krdp;

    auto ctx = reinterpret_cast<rdpContext *>(m_krdp);
    WINPR_ASSERT(ctx);

    auto settings = ctx->settings;
    WINPR_ASSERT(settings);

    m_lastSize = QSize(freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth), freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight));

    m_dispctx = dispctx;
    dispctx->custom = reinterpret_cast<void *>(this);
    dispctx->DisplayControlCaps = onDisplayControlCaps;
}

RdpDisplay::~RdpDisplay()
{
    m_updateTimer.stop();
    m_dispctx->custom = nullptr;
    m_dispctx = nullptr;
}