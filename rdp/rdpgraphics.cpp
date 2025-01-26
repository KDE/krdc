/*
 * SPDX-FileCopyrightText: 2024 Fabio Bas <ctrlaltca@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QCursor>
#include <QImage>
#include <QPixmap>

#include "rdpgraphics.h"
#include "rdpsession.h"
#include "rdpview.h"
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

#if FREERDP_VERSION_MAJOR == 3
BOOL RdpGraphics::onPointerSet(rdpContext *context, rdpPointer *pointer)
#else
BOOL RdpGraphics::onPointerSet(rdpContext *context, const rdpPointer *pointer)
#endif
{
    auto rctx = reinterpret_cast<RdpContext *>(context);
    WINPR_ASSERT(rctx);

    auto session = rctx->session;
    WINPR_ASSERT(session);

#if FREERDP_VERSION_MAJOR == 3
    auto ptx = reinterpret_cast<krdcPointer *>(pointer);
#else
    auto ptx = reinterpret_cast<const krdcPointer *>(pointer);
#endif
    WINPR_ASSERT(ptx);

    auto view = session->rdpView();
    if (view && ptx->pixmap) {
        auto srcSize = QSizeF{session->size()};
        auto destSize = QSizeF{view->size()};
        auto scale = destSize.width() / srcSize.width();
        auto cursor = QCursor(ptx->pixmap->scaledToWidth(pointer->width * scale, Qt::SmoothTransformation), pointer->xPos * scale, pointer->yPos * scale);

        view->setRemoteCursor(cursor);
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

    auto view = session->rdpView();
    if (view) {
        view->setRemoteCursor(Qt::BlankCursor);
        return true;
    }
    return false;
}

BOOL RdpGraphics::onPointerSetDefault(rdpContext *context)
{
    auto rctx = reinterpret_cast<RdpContext *>(context);
    WINPR_ASSERT(rctx);

    auto session = rctx->session;
    WINPR_ASSERT(session);

    auto view = session->rdpView();
    if (view) {
        view->setRemoteCursor(Qt::ArrowCursor);
        return true;
    }
    return false;
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
