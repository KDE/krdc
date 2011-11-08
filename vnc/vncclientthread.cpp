/****************************************************************************
**
** Copyright (C) 2007-2008 Urs Wolfer <uwolfer @ kde.org>
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

#include <QMutexLocker>
#include <QTimer>

//for detecting intel AMT KVM vnc server
static const QString INTEL_AMT_KVM_STRING= "Intel(r) AMT KVM";
static QString outputErrorMessageString;

QVector<QRgb> VncClientThread::m_colorTable;

void VncClientThread::setClientColorDepth(rfbClient* cl, VncClientThread::ColorDepth cd)
{
    switch(cd) {
    case bpp8:
        if (m_colorTable.isEmpty()) {
            m_colorTable.resize(256);
            int r,g,b;
            for (int i = 0; i < 256; ++i) {
                //pick out the red (3 bits), green (3 bits) and blue (2 bits) bits and make them maximum significant in 8bits
                //this gives a colortable for 8bit true colors
                r= (i & 0x07) << 5;
                g= (i & 0x38) << 2;
                b= i & 0xc0;
                m_colorTable[i] = qRgb(r, g, b);
            }
        }
        cl->format.depth = 8;
        cl->format.bitsPerPixel = 8;
        cl->format.redShift = 0;
        cl->format.greenShift = 3;
        cl->format.blueShift = 6;
        cl->format.redMax = 7;
        cl->format.greenMax = 7;
        cl->format.blueMax = 3;
        break;
    case bpp16:
        cl->format.depth = 16;
        cl->format.bitsPerPixel = 16;
        cl->format.redShift = 11;
        cl->format.greenShift = 5;
        cl->format.blueShift = 0;
        cl->format.redMax = 0x1f;
        cl->format.greenMax = 0x3f;
        cl->format.blueMax = 0x1f;
        break;
    case bpp32:
    default:
        cl->format.depth = 24;
        cl->format.bitsPerPixel = 32;
        cl->format.redShift = 16;
        cl->format.greenShift = 8;
        cl->format.blueShift = 0;
        cl->format.redMax = 0xff;
        cl->format.greenMax = 0xff;
        cl->format.blueMax = 0xff;
    }
}

rfbBool VncClientThread::newclient(rfbClient *cl)
{
    VncClientThread *t = (VncClientThread*)rfbClientGetClientData(cl, 0);
    Q_ASSERT(t);

    //8bit color hack for Intel(r) AMT KVM "classic vnc" = vnc server built in in Intel Vpro chipsets.
    if (INTEL_AMT_KVM_STRING == cl->desktopName) {
        kDebug(5011) << "Intel(R) AMT KVM: switching to 8 bit color depth (workaround, recent libvncserver needed)";
        t->setColorDepth(bpp8);
    }
    setClientColorDepth(cl, t->colorDepth());

    const int width = cl->width, height = cl->height, depth = cl->format.bitsPerPixel;
    const int size = width * height * (depth / 8);
    if (t->frameBuffer)
        delete [] t->frameBuffer; // do not leak if we get a new framebuffer size
    t->frameBuffer = new uint8_t[size];
    cl->frameBuffer = t->frameBuffer;
    memset(cl->frameBuffer, '\0', size);

    switch (t->quality()) {
    case RemoteView::High:
        cl->appData.encodingsString = "copyrect zlib hextile raw";
        cl->appData.compressLevel = 0;
        cl->appData.qualityLevel = 9;
        break;
    case RemoteView::Medium:
        cl->appData.encodingsString = "copyrect tight zrle ultra zlib hextile corre rre raw";
        cl->appData.compressLevel = 5;
        cl->appData.qualityLevel = 7;
        break;
    case RemoteView::Low:
    case RemoteView::Unknown:
    default:
        cl->appData.encodingsString = "copyrect tight zrle ultra zlib hextile corre rre raw";
        cl->appData.compressLevel = 9;
        cl->appData.qualityLevel = 1;
    }

    SetFormatAndEncodings(cl);
    kDebug(5011) << "Client created";
    return true;
}

void VncClientThread::updatefb(rfbClient* cl, int x, int y, int w, int h)
{
//     kDebug(5011) << "updated client: x: " << x << ", y: " << y << ", w: " << w << ", h: " << h;
    VncClientThread *t = (VncClientThread*)rfbClientGetClientData(cl, 0);
    Q_ASSERT(t);

    const int width = cl->width, height = cl->height;
    QImage img;
    switch(t->colorDepth()) {
    case bpp8:
        img = QImage(cl->frameBuffer, width, height, QImage::Format_Indexed8);
        img.setColorTable(m_colorTable);
        break;
    case bpp16:
        img = QImage(cl->frameBuffer, width, height, QImage::Format_RGB16);
        break;
    case bpp32:
        img = QImage(cl->frameBuffer, width, height, QImage::Format_RGB32);
        break;
    }

    if (img.isNull()) {
        kDebug(5011) << "image not loaded";
    }

    t->setImage(img);

    t->emitUpdated(x, y, w, h);
}

void VncClientThread::cuttext(rfbClient* cl, const char *text, int textlen)
{
    const QString cutText = QString::fromUtf8(text, textlen);
    kDebug(5011) << cutText;

    if (!cutText.isEmpty()) {
        VncClientThread *t = (VncClientThread*)rfbClientGetClientData(cl, 0);
        Q_ASSERT(t);

        t->emitGotCut(cutText);
    }
}

char *VncClientThread::passwdHandler(rfbClient *cl)
{
    kDebug(5011) << "password request" << kBacktrace();

    VncClientThread *t = (VncClientThread*)rfbClientGetClientData(cl, 0);
    Q_ASSERT(t);

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

    // internal messages, not displayed to user
    if (message.contains("VNC server supports protocol version 3.889")) // see http://bugs.kde.org/162640
        outputErrorMessageString = "INTERNAL:APPLE_VNC_COMPATIBILTY";
}

VncClientThread::VncClientThread(QObject *parent)
        : QThread(parent)
        , frameBuffer(0)
        , cl(0)
        , m_stopped(false)
{
    QMutexLocker locker(&mutex);

    QTimer *outputErrorMessagesCheckTimer = new QTimer(this);
    outputErrorMessagesCheckTimer->setInterval(500);
    connect(outputErrorMessagesCheckTimer, SIGNAL(timeout()), this, SLOT(checkOutputErrorMessage()));
    outputErrorMessagesCheckTimer->start();
}

VncClientThread::~VncClientThread()
{
    if(isRunning()) {
        stop();
        terminate();
        const bool quitSuccess = wait(1000);
        kDebug(5011) << "Attempting to stop in deconstructor, will crash if this fails:" << quitSuccess;
    }

    if (cl) {
        // Disconnect from vnc server & cleanup allocated resources
        rfbClientCleanup(cl);
    }

    delete [] frameBuffer;
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
    //set color depth dependent on quality
    switch(quality) {
    case RemoteView::Low:
        setColorDepth(bpp8);
        break;
    case RemoteView::High:
        setColorDepth(bpp32);
        break;
    case RemoteView::Medium:
    default:
        setColorDepth(bpp16);
    }
}

void VncClientThread::setColorDepth(ColorDepth colorDepth)
{
    m_colorDepth= colorDepth;
}

RemoteView::Quality VncClientThread::quality() const
{
    return m_quality;
}

VncClientThread::ColorDepth VncClientThread::colorDepth() const
{
    return m_colorDepth;
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

void VncClientThread::emitGotCut(const QString &text)
{
    emit gotCut(text);
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
        locker.relock();
        m_passwordError = false;
        locker.unlock();

        rfbClientLog = outputHandler;
        rfbClientErr = outputHandler;
        //24bit color dept in 32 bits per pixel = default. Will change colordepth and bpp later if needed
        cl = rfbGetClient(8, 3, 4);
        setClientColorDepth(cl, this->colorDepth());
        cl->MallocFrameBuffer = newclient;
        cl->canHandleNewFBSize = true;
        cl->GetPassword = passwdHandler;
        cl->GotFrameBufferUpdate = updatefb;
        cl->GotXCutText = cuttext;
        rfbClientSetClientData(cl, 0, this);

        locker.relock();
        cl->serverHost = strdup(m_host.toUtf8().constData());

        if (m_port < 0 || !m_port) // port is invalid or empty...
            m_port = 5900; // fallback: try an often used VNC port

        if (m_port >= 0 && m_port < 100) // the user most likely used the short form (e.g. :1)
            m_port += 5900;
        cl->serverPort = m_port;
        locker.unlock();

        kDebug(5011) << "--------------------- trying init ---------------------";

        if (rfbInitClient(cl, 0, 0))
            break;
        else
            cl = 0;

        locker.relock();
        if (m_passwordError)
            continue;

        return;
    }

    locker.relock();
    kDebug(5011) << "--------------------- Starting main VNC event loop ---------------------";
    while (!m_stopped) {
        locker.unlock();
        const int i = WaitForMessage(cl, 500);
        if (i < 0) {
            break;
        }
        if (i) {
            if (!HandleRFBServerMessage(cl)) {
                break;
            }
        }

        locker.relock();
        while (!m_eventQueue.isEmpty()) {
            ClientEvent* clientEvent = m_eventQueue.dequeue();
            locker.unlock();
            clientEvent->fire(cl);
            delete clientEvent;
            locker.relock();
        }
    }

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

void ClientCutEvent::fire(rfbClient* cl)
{
    SendClientCutText(cl, text.toUtf8().data(), text.size());
}

void VncClientThread::mouseEvent(int x, int y, int buttonMask)
{
    QMutexLocker lock(&mutex);
    if (m_stopped)
        return;

    m_eventQueue.enqueue(new PointerClientEvent(x, y, buttonMask));
}

void VncClientThread::keyEvent(int key, bool pressed)
{
    QMutexLocker lock(&mutex);
    if (m_stopped)
        return;

    m_eventQueue.enqueue(new KeyClientEvent(key, pressed));
}

void VncClientThread::clientCut(const QString &text)
{
    QMutexLocker lock(&mutex);
    if (m_stopped)
        return;

    m_eventQueue.enqueue(new ClientCutEvent(text));
}

#include "moc_vncclientthread.cpp"
