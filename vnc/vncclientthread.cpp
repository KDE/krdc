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
#include <KLocale>

#include <QMutexLocker>
#include <QTimer>

static QString outputErrorMessageString;

static rfbBool newclient(rfbClient *cl)
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

    VncClientThread *t = (VncClientThread*)rfbClientGetClientData(cl, 0);

    switch (t->quality()) {
    case RemoteView::High:
        cl->appData.useBGR233 = 0;
        cl->appData.encodingsString = "copyrect hextile raw";
        cl->appData.compressLevel = 0;
        cl->appData.qualityLevel = 9;
        break;
    case RemoteView::Medium:
        cl->appData.useBGR233 = 0;
        cl->appData.encodingsString = "tight zrle ultra copyrect hextile zlib corre rre raw";
        cl->appData.compressLevel = 5;
        cl->appData.qualityLevel = 7;
        break;
    case RemoteView::Low:
    case RemoteView::Unknown:
    default:
        cl->appData.useBGR233 = 1;
        cl->appData.encodingsString = "tight zrle ultra copyrect hextile zlib corre rre raw";
        cl->appData.compressLevel = 9;
        cl->appData.qualityLevel = 1;
    }

    SetFormatAndEncodings(cl);

    return true;
}

extern void updatefb(rfbClient* cl, int x, int y, int w, int h)
{
//     kDebug(5011) << "updated client: x: " << x << ", y: " << y << ", w: " << w << ", h: " << h;

    int width = cl->width, height = cl->height;

    QImage img(cl->frameBuffer, width, height, QImage::Format_RGB32);

    if (img.isNull())
        kDebug(5011) << "image not loaded";


    VncClientThread *t = (VncClientThread*)rfbClientGetClientData(cl, 0);

    t->setImage(img);

    t->emitUpdated(x, y, w, h);
}

char *VncClientThread::passwdHandler(rfbClient *cl)
{
    kDebug(5011) << "password request" << kBacktrace();

    VncClientThread *t = (VncClientThread*)rfbClientGetClientData(cl, 0);

    t->passwordRequest();
    t->m_passwordError = true;

    return strdup(t->password().toLocal8Bit());
}

void VncClientThread::outputHandler(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    QString message;
    message.vsprintf(format, args);

    va_end(args);

    message = message.trimmed();

    kDebug(5011) << message;

    if ((message.contains("Couldn't convert ")) ||
        (message.contains("Unable to connect to VNC server")))
        outputErrorMessageString = i18n("Server not found.");

    if ((message.contains("VNC connection failed: Authentication failed, too many tries")) ||
        (message.contains("VNC connection failed: Too many authentication failures")))
        outputErrorMessageString = i18n("VNC authentication failed because of too many authentication tries.");

    if (message.contains("VNC connection failed: Authentication failed"))
        outputErrorMessageString = i18n("VNC authentication failed.");

    if (message.contains("VNC server closed connection"))
        outputErrorMessageString = i18n("VNC server closed connection.");
}

VncClientThread::VncClientThread()
{
    QMutexLocker locker(&mutex);
    m_stopped = false;
    m_event = 0;

    QTimer *outputErrorMessagesCheckTimer = new QTimer(this);
    outputErrorMessagesCheckTimer->setInterval(500);
    connect(outputErrorMessagesCheckTimer, SIGNAL(timeout()), this, SLOT(checkOutputErrorMessage()));
    outputErrorMessagesCheckTimer->start();
}

VncClientThread::~VncClientThread()
{
    stop();
    wait(500);
}

void VncClientThread::checkOutputErrorMessage()
{
    if (!outputErrorMessageString.isEmpty()) {
        kDebug(5011) << outputErrorMessageString;
        QString errorMessage = outputErrorMessageString;
        outputErrorMessageString.clear();
        // show authentication failure error only after the 3rd unsuccessful try
        if ((errorMessage != i18n("VNC authentication failed.")) || m_passwordError)
            outputErrorMessage(errorMessage);
    }
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

void VncClientThread::setQuality(RemoteView::Quality quality)
{
    m_quality = quality;
}

RemoteView::Quality VncClientThread::quality() const
{
    return m_quality;
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

void VncClientThread::stop()
{
    QMutexLocker locker(&mutex);
    m_stopped = true;
}

void VncClientThread::run()
{
    QMutexLocker locker(&mutex);

    while (!m_stopped) { // try to connect as long as the server allows
        m_passwordError = false;

        rfbClientLog = outputHandler;
        rfbClientErr = outputHandler;
        cl = rfbGetClient(8, 3, 4);
        cl->MallocFrameBuffer = newclient;
        cl->canHandleNewFBSize = true;
        cl->GetPassword = passwdHandler;
        cl->GotFrameBufferUpdate = updatefb;
        rfbClientSetClientData(cl, 0, this);

        cl->serverHost = strdup(m_host.toUtf8().constData());

        if (m_port < 0 || !m_port) // port is invalid or empty...
            m_port = 5900; // fallback: try an often used VNC port

        if (m_port >= 0 && m_port < 100) // the user most likely used the short form (e.g. :1)
            m_port += 5900;
        cl->serverPort = m_port;

        kDebug(5011) << "--------------------- trying init ---------------------";

        if (rfbInitClient(cl, 0, 0))
            break;

        if (m_passwordError)
            continue;

        return;
    }

    locker.unlock();

    // Main VNC event loop
    while (!m_stopped) {
        int i = WaitForMessage(cl, 500);
        if (i < 0)
            break;
        if (i)
            if (!HandleRFBServerMessage(cl))
                break;

        locker.relock();

        if (m_event) {
            m_event->fire(cl);
            delete m_event;
            m_event = 0;
        }
        locker.unlock();
    }

    // Cleanup allocated resources
    locker.relock();
    delete [] cl->frameBuffer;
    cl->frameBuffer = 0;
    rfbClientCleanup(cl);
    m_stopped = true;
}

ClientEvent::~ClientEvent()
{
}

void PointerClientEvent::fire(rfbClient* cl)
{
    SendPointerEvent(cl, m_x, m_y, m_buttonMask);
}

void KeyClientEvent::fire(rfbClient* cl)
{
    SendKeyEvent(cl, m_key, m_pressed);
}

void VncClientThread::mouseEvent(int x, int y, int buttonMask)
{
    QMutexLocker lock (&mutex);

    delete m_event;
    m_event = new PointerClientEvent(x, y, buttonMask);
}

void VncClientThread::keyEvent(int key, bool pressed)
{
    QMutexLocker lock(&mutex);

    delete m_event;
    m_event = new KeyClientEvent(key, pressed);
}

#include "vncclientthread.moc"
