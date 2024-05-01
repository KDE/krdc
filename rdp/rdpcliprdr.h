/*
 * SPDX-FileCopyrightText: 2024 Fabio Bas <ctrlaltca@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <freerdp/client/cliprdr.h>

struct RdpContext;
class QMimeData;

class RdpClipboard
{
public:
    RdpClipboard(RdpContext *rdpC, CliprdrClientContext *cliprdr);
    ~RdpClipboard();

    bool sendClipboard(const QMimeData *data);

private:
    RdpContext *m_rdpC;
};