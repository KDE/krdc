/*
 * SPDX-FileCopyrightText: 2024 Fabio Bas <ctrlaltca@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "freerdp/graphics.h"
#include <freerdp/version.h>

class RdpGraphics
{
public:
    RdpGraphics(rdpGraphics *graphics);
    ~RdpGraphics();

    static BOOL onPointerNew(rdpContext *context, rdpPointer *pointer);
    static void onPointerFree(rdpContext *context, rdpPointer *pointer);
    static BOOL onPointerSet(rdpContext *context, rdpPointer *pointer);
    static BOOL onPointerSetNull(rdpContext *context);
    static BOOL onPointerSetDefault(rdpContext *context);
    static BOOL onPointerSetPosition(rdpContext *context, UINT32 x, UINT32 y);
};