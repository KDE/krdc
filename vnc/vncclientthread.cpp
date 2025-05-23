/*
    SPDX-FileCopyrightText: 2007-2013 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "vncclientthread.h"
#include "krdc_debug.h"

#include <QBitmap>
#include <QCursor>
#include <QMutexLocker>
#include <QPixmap>
#include <QTimer>
#include <cerrno>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

// for detecting intel AMT KVM vnc server
static const QString INTEL_AMT_KVM_STRING = QLatin1String("Intel(r) AMT KVM");

// Dispatch from this static callback context to the member context.
rfbBool VncClientThread::newclientStatic(rfbClient *cl)
{
    VncClientThread *t = (VncClientThread *)rfbClientGetClientData(cl, nullptr);
    Q_ASSERT(t);

    return t->newclient();
}

// Dispatch from this static callback context to the member context.
void VncClientThread::updatefbStaticPartial(rfbClient *cl, int x, int y, int w, int h)
{
    VncClientThread *t = (VncClientThread *)rfbClientGetClientData(cl, nullptr);
    Q_ASSERT(t);

    return t->updatefbPartial(x, y, w, h);
}

// Dispatch from this static callback context to the member context.
void VncClientThread::updateFbStaticFinished(rfbClient *cl)
{
    VncClientThread *t = (VncClientThread *)rfbClientGetClientData(cl, nullptr);
    Q_ASSERT(t);

    return t->updatefbFinished();
}

// Dispatch from this static callback context to the member context.
void VncClientThread::cuttextStatic(rfbClient *cl, const char *text, int textlen)
{
    VncClientThread *t = (VncClientThread *)rfbClientGetClientData(cl, nullptr);
    Q_ASSERT(t);

    t->cuttext(text, textlen, false);
}

// Dispatch from this static callback context to the member context.
void VncClientThread::cuttextUtf8Static(rfbClient *cl, const char *text, int textlen)
{
    VncClientThread *t = (VncClientThread *)rfbClientGetClientData(cl, nullptr);
    Q_ASSERT(t);

    t->cuttext(text, textlen, true);
}

// Dispatch from this static callback context to the member context.
char *VncClientThread::passwdHandlerStatic(rfbClient *cl)
{
    VncClientThread *t = (VncClientThread *)rfbClientGetClientData(cl, nullptr);
    Q_ASSERT(t);

    return t->passwdHandler();
}

// Dispatch from this static callback context to the member context.
rfbCredential *VncClientThread::credentialHandlerStatic(rfbClient *cl, int credentialType)
{
    VncClientThread *t = (VncClientThread *)rfbClientGetClientData(cl, nullptr);
    Q_ASSERT(t);

    return t->credentialHandler(credentialType);
}

// Dispatch from this static callback context to the member context.
void VncClientThread::outputHandlerStatic(const char *format, ...)
{
    VncClientThread *t = qobject_cast<VncClientThread *>(QThread::currentThread());
    Q_ASSERT(t);

    va_list args;
    va_start(args, format);
    t->outputHandler(format, args);
    va_end(args);
}

void VncClientThread::cursorShapeHandlerStatic(rfbClient *cl, int xhot, int yhot, int width, int height, int bpp)
{
    VncClientThread *t = (VncClientThread *)rfbClientGetClientData(cl, nullptr);
    Q_ASSERT(t);

    // get cursor shape from remote cursor field
    // it's important to set stride for images, pictures in VNC are not always 32-bit aligned
    QImage cursorImg;
    switch (bpp) {
    case 4:
        cursorImg = QImage(cl->rcSource, width, height, bpp * width, QImage::Format_RGB32);
        break;
    case 2:
        cursorImg = QImage(cl->rcSource, width, height, bpp * width, QImage::Format_RGB16);
        break;
    case 1:
        cursorImg = QImage(cl->rcSource, width, height, bpp * width, QImage::Format_Indexed8);
        cursorImg.setColorTable(t->m_colorTable);
        break;
    default:
        qCWarning(KRDC) << "Unsupported bpp value for cursor shape:" << bpp;
        return;
    }

    // get alpha channel
    QImage alpha(cl->rcMask, width, height, 1 * width, QImage::Format_Indexed8);
    alpha.setColorTable({qRgb(255, 255, 255), qRgb(0, 0, 0)});

    // apply transparency mask
    QPixmap cursorPixmap(QPixmap::fromImage(cursorImg));
    cursorPixmap.setMask(QBitmap::fromImage(alpha));

    Q_EMIT t->gotCursor(QCursor{cursorPixmap, xhot, yhot});
}

void VncClientThread::setClientColorDepth(rfbClient *cl, VncClientThread::ColorDepth cd)
{
    switch (cd) {
    case bpp8:
        if (m_colorTable.isEmpty()) {
            m_colorTable.resize(256);
            int r, g, b;
            for (int i = 0; i < 256; ++i) {
                // pick out the red (3 bits), green (3 bits) and blue (2 bits) bits and make them maximum significant in 8bits
                // this gives a colortable for 8bit true colors
                r = (i & 0x07) << 5;
                g = (i & 0x38) << 2;
                b = i & 0xc0;
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

rfbBool VncClientThread::newclient()
{
    // 8bit color hack for Intel(r) AMT KVM "classic vnc" = vnc server built in in Intel Vpro chipsets.
    if (INTEL_AMT_KVM_STRING == QLatin1String(cl->desktopName)) {
        qCDebug(KRDC) << "Intel(R) AMT KVM: switching to 8 bit color depth (workaround, recent libvncserver needed)";
        setColorDepth(bpp8);
    }
    setClientColorDepth(cl, colorDepth());

    const int width = cl->width, height = cl->height, depth = cl->format.bitsPerPixel;
    const int size = width * height * (depth / 8);
    if (size <= 0) {
        return false;
    }
    if (frameBuffer)
        delete[] frameBuffer; // do not leak if we get a new framebuffer size
    frameBuffer = new uint8_t[size];
    cl->frameBuffer = frameBuffer;
    memset(cl->frameBuffer, '\0', size);

    switch (quality()) {
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
        // bpp8 and tight encoding is not supported in libvnc
        cl->appData.encodingsString = "copyrect zrle ultra zlib hextile corre rre raw";
        cl->appData.compressLevel = 9;
        cl->appData.qualityLevel = 1;
    }

    SetFormatAndEncodings(cl);
    qCDebug(KRDC) << "Client created";
    return true;
}

void VncClientThread::updatefbPartial(int x, int y, int w, int h)
{
    //    qCDebug(KRDC) << "updated client: x: " << x << ", y: " << y << ", w: " << w << ", h: " << h;

    m_dirtyRect = m_dirtyRect.united(QRect(x, y, w, h));
}

void VncClientThread::updatefbFinished()
{
    const int width = cl->width, height = cl->height;
    QImage img;
    switch (colorDepth()) {
    case bpp8:
        img = QImage(cl->frameBuffer, width, height, width, QImage::Format_Indexed8);
        img.setColorTable(m_colorTable);
        break;
    case bpp16:
        img = QImage(cl->frameBuffer, width, height, 2 * width, QImage::Format_RGB16);
        break;
    case bpp32:
        img = QImage(cl->frameBuffer, width, height, 4 * width, QImage::Format_RGB32);
        break;
    }

    if (img.isNull()) {
        qCDebug(KRDC) << "image not loaded";
    }

    if (isInterruptionRequested()) {
        return; // sending data to a stopped thread is not a good idea
    }

    img.setDevicePixelRatio(m_devicePixelRatio);
    setImage(img);

    // Compute bounding rect.
    QRect updateRect = m_dirtyRect;
    m_dirtyRect = QRect();

    //    qCDebug(KRDC) << Q_FUNC_INFO << updateRect;
    emitUpdated(updateRect.x(), updateRect.y(), updateRect.width(), updateRect.height());
}

void VncClientThread::cuttext(const char *text, int textlen, bool utf8)
{
    QString cutText;
    if (utf8) {
        // The last byte is a null terminator
        cutText = QString::fromUtf8(text, textlen - 1);
    } else {
        cutText = QString::fromLatin1(text, textlen);
    }
    qCDebug(KRDC) << cutText;

    if (!cutText.isEmpty()) {
        emitGotCut(cutText);
    }
}

char *VncClientThread::passwdHandler()
{
    qCDebug(KRDC) << "password request";

    // Never request a password during a reconnect attempt.
    if (!m_keepalive.failed) {
        passwordRequest();
        m_passwordError = true;
    }
    return strdup(m_password.toUtf8().constData());
}

rfbCredential *VncClientThread::credentialHandler(int credentialType)
{
    qCDebug(KRDC) << "credential request" << credentialType;

    rfbCredential *cred = nullptr;

    switch (credentialType) {
    case rfbCredentialTypeUser:
        passwordRequest(true);
        m_passwordError = true;

        cred = new rfbCredential;
        cred->userCredential.username = strdup(username().toUtf8().constData());
        cred->userCredential.password = strdup(password().toUtf8().constData());
        break;
    default:
        qCritical(KRDC) << "credential request failed, unsupported credentialType:" << credentialType;
        outputErrorMessage(i18n("VNC authentication type is not supported."));
        break;
    }
    return cred;
}

void VncClientThread::outputHandler(const char *format, va_list args)
{
    auto message = QString::vasprintf(format, args);

    message = message.trimmed();

    qCDebug(KRDC) << message;

    if ((message.contains(QLatin1String("Couldn't convert "))) || (message.contains(QLatin1String("Unable to connect to VNC server")))) {
        // Don't show a dialog if a reconnection is needed. Never contemplate
        // reconnection if we don't have a password.
        QString tmp = i18n("Server not found.");
        if (m_keepalive.set && !m_password.isNull()) {
            m_keepalive.failed = true;
            if (m_previousDetails != tmp) {
                m_previousDetails = tmp;
                clientStateChange(RemoteView::Disconnected, tmp);
            }
        } else {
            outputErrorMessageString = tmp;
        }
    }

    // Process general authentication failures before more specific authentication
    // failures. All authentication failures cancel any auto-reconnection that
    // may be in progress.
    if (message.contains(QLatin1String("VNC connection failed: Authentication failed"))) {
        m_keepalive.failed = false;
        outputErrorMessageString = i18n("VNC authentication failed.");
    }
    if ((message.contains(QLatin1String("VNC connection failed: Authentication failed, too many tries")))
        || (message.contains(QLatin1String("VNC connection failed: Too many authentication failures")))) {
        m_keepalive.failed = false;
        outputErrorMessageString = i18n("VNC authentication failed because of too many authentication tries.");
    }

    if (message.contains(QLatin1String("VNC server closed connection")))
        outputErrorMessageString = i18n("VNC server closed connection.");

    // If we are not going to attempt a reconnection, at least tell the user
    // the connection went away.
    if (message.contains(QLatin1String("read ("))) {
        // Don't show a dialog if a reconnection is needed. Never contemplate
        // reconnection if we don't have a password.
#ifdef QTONLY
        QString tmp = i18n("Disconnected: %1.", message.toStdString().c_str());
#else
        QString tmp = i18n("Disconnected: %1.", message);
#endif
        if (m_keepalive.set && !m_password.isNull()) {
            m_keepalive.failed = true;
            clientStateChange(RemoteView::Disconnected, tmp);
        } else {
            outputErrorMessageString = tmp;
        }
    }

    // internal messages, not displayed to user
    if (message.contains(QLatin1String("VNC server supports protocol version 3.889"))) // see https://bugs.kde.org/162640
        outputErrorMessageString = QLatin1String("INTERNAL:APPLE_VNC_COMPATIBILTY");
}

VncClientThread::VncClientThread(QObject *parent)
    : QThread(parent)
    , frameBuffer(nullptr)
    , cl(nullptr)
    , m_devicePixelRatio(1.0)
{
    // We choose a small value for interval...after all if the connection is
    // supposed to sustain a VNC session, a reasonably frequent ping should
    // be perfectly supportable.
    m_keepalive.intervalSeconds = 1;
    m_keepalive.failedProbes = 3;
    m_keepalive.set = false;
    m_keepalive.failed = false;
    m_previousDetails = QString();
    outputErrorMessageString.clear(); // don't deliver error messages of old instances...
    QMutexLocker locker(&mutex);

    QTimer *outputErrorMessagesCheckTimer = new QTimer(this);
    outputErrorMessagesCheckTimer->setInterval(500);
    connect(outputErrorMessagesCheckTimer, SIGNAL(timeout()), this, SLOT(checkOutputErrorMessage()));
    outputErrorMessagesCheckTimer->start();
}

VncClientThread::~VncClientThread()
{
    if (isRunning()) {
        stop();
        terminate();
        const bool quitSuccess = wait(1000);
        qCDebug(KRDC) << "Attempting to stop in deconstructor, will crash if this fails:" << quitSuccess;
    }

    clientDestroy();

    delete[] frameBuffer;
}

void VncClientThread::checkOutputErrorMessage()
{
    if (!outputErrorMessageString.isEmpty()) {
        qCDebug(KRDC) << outputErrorMessageString;
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

void VncClientThread::setShowLocalCursor(bool show)
{
    QMutexLocker locker(&mutex);
    m_showLocalCursor = show;

    if (!cl) {
        // no client yet, only store local value
        return;
    }

    // from server point of view, "remote" cursor is the one local to the client
    // so the meaning in AppData struct is inverted
    cl->appData.useRemoteCursor = show;

    // need to communicate this change to server or it won't stop painting cursor
    m_eventQueue.enqueue(new ReconfigureEvent);
}

void VncClientThread::setQuality(RemoteView::Quality quality)
{
    m_quality = quality;
    // set color depth dependent on quality
    switch (quality) {
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

void VncClientThread::setDevicePixelRatio(qreal dpr)
{
    m_devicePixelRatio = dpr;
}

void VncClientThread::setColorDepth(ColorDepth colorDepth)
{
    m_colorDepth = colorDepth;
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
    Q_EMIT imageUpdated(x, y, w, h);
}

void VncClientThread::emitGotCut(const QString &text)
{
    Q_EMIT gotCut(text);
}

void VncClientThread::stop()
{
    requestInterruption();
}

void VncClientThread::run()
{
    while (!isInterruptionRequested()) { // try to connect as long as the server allows
        QMutexLocker locker(&mutex);
        m_passwordError = false;
        locker.unlock();

        if (clientCreate(false)) {
            // The initial connection attempt worked!
            break;
        }

        locker.relock();
        if (m_passwordError) {
            // Try again.
            continue;
        }

        // The initial connection attempt failed, and not because of a
        // password problem. Bail out.
        return;
    }

    qCDebug(KRDC) << "--------------------- Starting main VNC event loop ---------------------";
    while (!isInterruptionRequested()) {
        const int i = WaitForMessage(cl, 500);
        if (isInterruptionRequested() || i < 0) {
            break;
        }
        if (i) {
            if (!HandleRFBServerMessage(cl)) {
                if (m_keepalive.failed) {
                    do {
                        // Reconnect after a short delay. That way, if the
                        // attempt fails very quickly, we don't sit in a very
                        // tight loop.
                        clientDestroy();
                        msleep(1000);
                        clientStateChange(RemoteView::Connecting, i18n("Reconnecting."));
                    } while (!clientCreate(true));
                    continue;
                }
                qCritical(KRDC) << "HandleRFBServerMessage failed";
                break;
            }
        }

        QMutexLocker locker(&mutex);
        while (!m_eventQueue.isEmpty()) {
            ClientEvent *clientEvent = m_eventQueue.dequeue();
            locker.unlock();
            clientEvent->fire(cl);
            delete clientEvent;
            locker.relock();
        }
    }
}

/**
 * Factor out the initialisation of the VNC client library. Notice this has
 * both static parts as in @see rfbClientLog and @see rfbClientErr,
 * as well as instance local data @see rfbGetClient().
 *
 * On return from here, if cl is set, the connection will have been made else
 * cl will not be set.
 */
bool VncClientThread::clientCreate(bool reinitialising)
{
    rfbClientLog = outputHandlerStatic;
    rfbClientErr = outputHandlerStatic;

    // 24bit color dept in 32 bits per pixel = default. Will change colordepth and bpp later if needed
    cl = rfbGetClient(8, 3, 4);
    setClientColorDepth(cl, this->colorDepth());
    cl->MallocFrameBuffer = newclientStatic;
    cl->canHandleNewFBSize = true;
    cl->GetPassword = passwdHandlerStatic;
    cl->GetCredential = credentialHandlerStatic;
    cl->GotFrameBufferUpdate = updatefbStaticPartial;
    cl->FinishedFrameBufferUpdate = updateFbStaticFinished;
    cl->GotXCutText = cuttextStatic;
#if SUPPORT_UTF8_CLIPBOARD
    cl->GotXCutTextUTF8 = cuttextUtf8Static;
#endif
    cl->GotCursorShape = cursorShapeHandlerStatic;
    rfbClientSetClientData(cl, nullptr, this);

    cl->appData.useRemoteCursor = m_showLocalCursor;

    cl->serverHost = strdup(m_host.toUtf8().constData());

    cl->serverPort = m_port;

    qCDebug(KRDC) << "--------------------- trying init ---------------------";

    if (!rfbInitClient(cl, nullptr, nullptr)) {
        if (!reinitialising) {
            // Don't whine on reconnection failure: presumably the network
            // is simply still down.
            qCCritical(KRDC) << "rfbInitClient failed";
        }
        cl = nullptr;
        return false;
    }

    if (reinitialising) {
        clientStateChange(RemoteView::Connected, i18n("Reconnected."));
    } else {
        clientStateChange(RemoteView::Connected, i18n("Connected."));
    }
    clientSetKeepalive();
    return true;
}

/**
 * Undo @see clientCreate().
 */
void VncClientThread::clientDestroy()
{
    if (cl) {
        // Disconnect from vnc server & cleanup allocated resources
        rfbClientCleanup(cl);
        cl = nullptr;
    }
}

/**
 * The VNC client library does not make use of keepalives. We go behind its
 * back to set it up.
 */
void VncClientThread::clientSetKeepalive()
{
    // If keepalive is disabled, do nothing.
    m_keepalive.set = false;
    m_keepalive.failed = false;
    if (!m_keepalive.intervalSeconds) {
        return;
    }
    int optval;
    socklen_t optlen = sizeof(optval);

    // Try to set the option active
    optval = 1;
    if (setsockopt(cl->sock, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
        qCritical(KRDC) << "setsockopt(SO_KEEPALIVE)" << strerror(errno);
        return;
    }

    optval = m_keepalive.intervalSeconds;
    if (setsockopt(cl->sock, IPPROTO_TCP, TCP_KEEPIDLE, &optval, optlen) < 0) {
        qCritical(KRDC) << "setsockopt(TCP_KEEPIDLE)" << strerror(errno);
        return;
    }

    optval = m_keepalive.intervalSeconds;
    if (setsockopt(cl->sock, IPPROTO_TCP, TCP_KEEPINTVL, &optval, optlen) < 0) {
        qCritical(KRDC) << "setsockopt(TCP_KEEPINTVL)" << strerror(errno);
        return;
    }

    optval = m_keepalive.failedProbes;
    if (setsockopt(cl->sock, IPPROTO_TCP, TCP_KEEPCNT, &optval, optlen) < 0) {
        qCritical(KRDC) << "setsockopt(TCP_KEEPCNT)" << strerror(errno);
        return;
    }
    m_keepalive.set = true;
    qCDebug(KRDC) << "TCP keepalive set";
}

/**
 * The VNC client state changed.
 */
void VncClientThread::clientStateChange(RemoteView::RemoteStatus status, const QString &details)
{
    qCDebug(KRDC) << status << details << m_host << ":" << m_port;
    Q_EMIT clientStateChanged(status, details);
}

ClientEvent::~ClientEvent()
{
}

void ReconfigureEvent::fire(rfbClient *cl)
{
    SetFormatAndEncodings(cl);
}

void PointerClientEvent::fire(rfbClient *cl)
{
    SendPointerEvent(cl, m_x, m_y, m_buttonMask);
}

void KeyClientEvent::fire(rfbClient *cl)
{
    SendKeyEvent(cl, m_key, m_pressed);
}

void ClientCutEvent::fire(rfbClient *cl)
{
    QByteArray latin1Bytes = text.toLatin1();
    QByteArray utf8Bytes = text.toUtf8();

#if SUPPORT_UTF8_CLIPBOARD
    rfbBool sendUtf8Result = SendClientCutTextUTF8(cl, utf8Bytes.data(), utf8Bytes.length());
    if (!sendUtf8Result) {
        // Some VNC servers may not support UTF-8 cut text, fallback to Latin1 if needed.
        qCDebug(KRDC) << "SendClientCutTextUTF8 failed, falling back to latin1";
        rfbBool sendLatin1Result = SendClientCutText(cl, latin1Bytes.data(), latin1Bytes.length());
        if (!sendLatin1Result) {
            qCDebug(KRDC) << "SendClientCutText failed";
        }
    }
#else
    rfbBool sendResult = SendClientCutText(cl, latin1Bytes.data(), latin1Bytes.length());
    if (!sendResult) {
        qCDebug(KRDC) << "SendClientCutText failed";
    }
#endif
}

void VncClientThread::mouseEvent(int x, int y, int buttonMask)
{
    if (!isRunning())
        return;

    QMutexLocker lock(&mutex);
    m_eventQueue.enqueue(new PointerClientEvent(x, y, buttonMask));
}

void VncClientThread::keyEvent(int key, bool pressed)
{
    if (!isRunning())
        return;

    QMutexLocker lock(&mutex);
    m_eventQueue.enqueue(new KeyClientEvent(key, pressed));
}

void VncClientThread::clientCut(const QString &text)
{
    if (!isRunning())
        return;

    QMutexLocker lock(&mutex);
    m_eventQueue.enqueue(new ClientCutEvent(text));
}

#include "moc_vncclientthread.cpp"
