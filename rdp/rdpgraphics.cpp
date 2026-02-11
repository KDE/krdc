/*
 * SPDX-FileCopyrightText: 2024 Fabio Bas <ctrlaltca@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QCursor>
#include <QImage>
#include <QPixmap>

#include "rdpgraphics.h"
#include "rdpsession.h"
#include <freerdp/gdi/gfx.h>

struct krdcPointer {
    rdpPointer pointer;
    QPixmap *pixmap = nullptr;
};

BOOL RdpGraphics::onPointerNew(rdpContext *context, rdpPointer *pointer)
{
    auto ptx = reinterpret_cast<krdcPointer *>(pointer);
    WINPR_ASSERT(ptx);

    auto buffer = QImage(pointer->width, pointer->height, QImage::Format_RGBA8888);
    if (!freerdp_image_copy_from_pointer_data(buffer.bits(),
                                              PIXEL_FORMAT_RGBA32,
                                              0,
                                              0,
                                              0,
                                              pointer->width,
                                              pointer->height,
                                              pointer->xorMaskData,
                                              pointer->lengthXorMask,
                                              pointer->andMaskData,
                                              pointer->lengthAndMask,
                                              pointer->xorBpp,
                                              &context->gdi->palette)) {
        return false;
    }

    ptx->pixmap = new QPixmap(QPixmap::fromImage(buffer));
    return true;
}

void RdpGraphics::onPointerFree(rdpContext *context, rdpPointer *pointer)
{
    Q_UNUSED(context);

    auto ptx = reinterpret_cast<krdcPointer *>(pointer);
    WINPR_ASSERT(ptx);

    if (ptx->pixmap) {
        delete ptx->pixmap;
    }
}

BOOL RdpGraphics::onPointerSet(rdpContext *context, rdpPointer *pointer)
{
    auto rctx = reinterpret_cast<RdpContext *>(context);
    WINPR_ASSERT(rctx);

    auto session = rctx->session;
    WINPR_ASSERT(session);

    auto ptx = reinterpret_cast<krdcPointer *>(pointer);
    WINPR_ASSERT(ptx);

    if (ptx->pixmap) {
        // Keep the cursor in remote pixel coordinates. The widget displaying
        // it knows the scale of its local monitor and applies it there.
        session->setRemoteCursor(QCursor{*ptx->pixmap, static_cast<int>(pointer->xPos), static_cast<int>(pointer->yPos)});
        return true;
    }
    return false;
}

BOOL RdpGraphics::onPointerSetNull(rdpContext *context)
{
    auto rctx = reinterpret_cast<RdpContext *>(context);
    WINPR_ASSERT(rctx);

    auto session = rctx->session;
    WINPR_ASSERT(session);

    session->setRemoteCursor(Qt::BlankCursor);
    return true;
}

BOOL RdpGraphics::onPointerSetDefault(rdpContext *context)
{
    auto rctx = reinterpret_cast<RdpContext *>(context);
    WINPR_ASSERT(rctx);

    auto session = rctx->session;
    WINPR_ASSERT(session);

    session->setRemoteCursor(Qt::ArrowCursor);
    return true;
}

BOOL RdpGraphics::onPointerSetPosition(rdpContext *context, UINT32 x, UINT32 y)
{
    // Not implemented
    Q_UNUSED(context);
    Q_UNUSED(x);
    Q_UNUSED(y);
    return true;
}

RdpGraphics::RdpGraphics(rdpGraphics *graphics)
{
    rdpPointer pointer = {};
    pointer.size = sizeof(krdcPointer);
    pointer.New = onPointerNew;
    pointer.Free = onPointerFree;
    pointer.Set = onPointerSet;
    pointer.SetNull = onPointerSetNull;
    pointer.SetDefault = onPointerSetDefault;
    pointer.SetPosition = onPointerSetPosition;
    graphics_register_pointer(graphics, &pointer);
}

RdpGraphics::~RdpGraphics()
{
}
