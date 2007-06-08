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
** You should have received a copy of the GNU General Public License
** along with this program; see the file COPYING. If not, write to
** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
** Boston, MA 02110-1301, USA.
**
****************************************************************************/

#include "vncclientthread.h"

#include <KDebug>

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
//     kDebug(5011) << "updated client: x: " << x << ", y: " << y << ", w: " << w << ", h: " << h << endl;

    int width = cl->width, height = cl->height;

    QImage img(cl->frameBuffer, width, height, QImage::Format_RGB32);

    if (img.isNull())
        kDebug(5011) << "image not loaded" << endl;


    VncClientThread *t = (VncClientThread*)rfbClientGetClientData(cl, 0);

    t->setImage(img);

    t->emitUpdated(x, y, w, h);
}

extern char *passwd(rfbClient *cl)
{
    Q_UNUSED(cl);
    kDebug(5011) << "password request" << endl;

    VncClientThread *t = (VncClientThread*)rfbClientGetClientData(cl, 0);

    t->emitPasswordRequest();

    return strdup(t->password().toLocal8Bit());
}

extern void log(const char *format, ...)
{
    kDebug(5011) << format << endl;
    Q_UNUSED(format);
}

extern void error(const char *format, ...)
{
    kDebug(5011) << format << endl;
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

const QImage VncClientThread::image(int x, int y, int w, int h)
{
    QMutexLocker locker(&mutex);

    if (w == 0) // full image requested
        return m_image;
    else
        return m_image.copy(x, y, w, h);
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

void VncClientThread::cleanup()
{
    m_cleanup = true;
}

#include "vncclientthread.moc"
