/****************************************************************************
**
** Copyright (C) 2007 Urs Wolfer <uwolfer @ kde.org>
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
** You should have received a copy of the GNU Library General Public License
** along with this library; see the file COPYING.LIB. If not, write to
** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
** Boston, MA 02110-1301, USA.
**
****************************************************************************/

#include "vncclientthread.h"

#include <QMutexLocker>

extern rfbBool newclient(rfbClient *cl)
{
    int width = cl->width, height = cl->height, depth = cl->format.bitsPerPixel;
    int size = width * height * (depth / 8);
    uint8_t *buf = new uint8_t[size];
    memset(buf, '\0', size);
    cl->frameBuffer = buf;
    cl->format.bitsPerPixel = 32;
    cl->format.redShift = 16;
    cl->format.greenShift = 8;
    cl->format.blueShift = 0;
    cl->format.redMax = 0xff;
    cl->format.greenMax = 0xff;
    cl->format.blueMax = 0xff;

    //best quality
    cl->appData.useBGR233 = 0;
    cl->appData.encodingsString = "copyrect hextile raw";
    cl->appData.compressLevel = -1;
    cl->appData.qualityLevel = 9;

    SetFormatAndEncodings(cl);
    SendFramebufferUpdateRequest(cl, 0, 0, cl->width, cl->height, false);

    return true;
}

extern void updatefb(rfbClient* cl, int x, int y, int w, int h)
{
//     qDebug("updated client (%d, %d, %d, %d)",x,y,w,h);

    int width = cl->width, height = cl->height;

    QImage img(cl->frameBuffer, width, height, QImage::Format_RGB32);

    if (img.isNull())
        qDebug("image not loaded");


    VncClientThread *t = (VncClientThread*)rfbClientGetClientData(cl, 0);

    t->setImage(img.copy(x, y, w, h));
    t->setFullImage(img);

    t->emitUpdated(x, y, w, h);
}

extern char *passwd(rfbClient *cl)
{
    Q_UNUSED(cl);
//     qDebug("password request");

    VncClientThread *t = (VncClientThread*)rfbClientGetClientData(cl, 0);

    t->emitPasswordRequest();

    return strdup(t->password().toLatin1());
}

extern void log(const char *format, ...)
{
//     qDebug(format);
    Q_UNUSED(format);
}

extern void error(const char *format, ...)
{
//     qDebug(format);
    Q_UNUSED(format);
}

VncClientThread::VncClientThread()
{
    QMutexLocker locker(&mutex);
    m_stopped = false;
    m_cleanup = false;
}

VncClientThread::~VncClientThread()
{
}

void VncClientThread::setHost(const QString &host)
{
    QMutexLocker locker(&mutex);
    m_host = host;
}

void VncClientThread::setPort(int port)
{
    QMutexLocker locker(&mutex);
    m_port = port;
}

void VncClientThread::setPassword(const QString &password)
{
    m_password = password;
}

const QString VncClientThread::password() const
{
    return m_password;
}

void VncClientThread::setImage(const QImage &img)
{
    QMutexLocker locker(&mutex);
    m_image = img;
}

void VncClientThread::setFullImage(const QImage &img)
{
    QMutexLocker locker(&mutex);
    m_fullImage = img;
}

const QImage VncClientThread::image()
{
    QMutexLocker locker(&mutex);
    return m_image;
}

const QImage VncClientThread::fullImage()
{
    QMutexLocker locker(&mutex);
    return m_fullImage;
}

void VncClientThread::emitUpdated(int x, int y, int w, int h)
{
    emit imageUpdated(x, y, w, h);
}

void VncClientThread::emitPasswordRequest()
{
    emit passwordRequest();
}

void VncClientThread::stop()
{
    QMutexLocker locker(&mutex);
    m_stopped = true;
}

void VncClientThread::run()
{
    QMutexLocker locker(&mutex);
//     rfbClientLog = log;
//     rfbClientErr = error;
    cl = rfbGetClient(8, 3, 4);
    cl->MallocFrameBuffer = newclient;
    cl->canHandleNewFBSize = true;
    cl->GetPassword = passwd;
    cl->GotFrameBufferUpdate = updatefb;
    rfbClientSetClientData(cl, 0, this);
    cl->serverHost = m_host.toLatin1().data();
    cl->serverPort = m_port;
    if(!rfbInitClient(cl, 0, 0))
        return;

    locker.unlock();

    while (!m_stopped) {
        if (m_cleanup) {
            rfbClientCleanup(cl);
            return;
        }

        int i = WaitForMessage(cl, 500);
        if (i < 0)
            return;
        if (i)
            if(!HandleRFBServerMessage(cl))
                return;
    }
}

void VncClientThread::mouseEvent(int x, int y, int buttonMask)
{
    SendPointerEvent(cl, x, y, buttonMask);
}

void VncClientThread::keyEvent(int key, bool pressed)
{
    SendKeyEvent(cl, key, pressed);
}

void VncClientThread::requestFullUpdate()
{
    updatefb(cl, 0, 0, cl->width, cl->height);
}

void VncClientThread::cleanup()
{
    m_cleanup = true;
}

#include "vncclientthread.moc"
