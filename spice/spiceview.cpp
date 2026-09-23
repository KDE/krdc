/*
    SPDX-FileCopyrightText: 2024 KRDC Contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "spiceview.h"
#include "krdc_debug.h"

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QTimer>

#include <KLocalizedString>
#include <KPasswordDialog>

// Default SPICE port
static const int DEFAULT_SPICE_PORT = 5900;

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

SpiceView::SpiceView(QWidget *parent, const QUrl &url, KConfigGroup configGroup)
    : RemoteView(parent)
    , m_spiceSession(nullptr)
    , m_mainChannel(nullptr)
    , m_displayChannel(nullptr)
    , m_cursorChannel(nullptr)
    , m_inputsChannel(nullptr)
    , m_playbackChannel(nullptr)
    , m_resizeTimer(new QTimer(this))
    , m_firstMonitorConfig(true)
    , m_displayMarked(false)
    , m_firstFrame(true)
    , m_quitFlag(false)
    , m_firstPasswordTry(true)
    , m_horizontalFactor(1.0)
    , m_verticalFactor(1.0)
    , m_buttonMask(0)
    , m_serverMouseMode(false)
    , m_hostPreferences(nullptr)
{
    m_url = url;
    m_host = url.host();
    m_port = url.port(DEFAULT_SPICE_PORT);

    m_hostPreferences = new SpiceHostPreferences(configGroup, this);

    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);
    setFocusPolicy(Qt::WheelFocus);

    // Debounce resize: send monitor config 500 ms after the user stops dragging.
    m_resizeTimer->setSingleShot(true);
    m_resizeTimer->setInterval(500);
    connect(m_resizeTimer, &QTimer::timeout, this, &SpiceView::sendMonitorConfig);
}

SpiceView::~SpiceView()
{
    startQuitting();
}

// ---------------------------------------------------------------------------
// RemoteView interface
// ---------------------------------------------------------------------------

bool SpiceView::startConnection()
{
    setStatus(Connecting);

    m_spiceSession = spice_session_new();

    const QByteArray hostBytes = m_host.toUtf8();
    const QByteArray portBytes = QByteArray::number(m_port);

    g_object_set(m_spiceSession, "host", hostBytes.constData(), "port", portBytes.constData(), nullptr);

    // Wire audio to the system backend (GStreamer/PulseAudio via spice-client-glib).
    // spice_audio_get() is a session-scoped singleton; it registers itself on the
    // session and is freed automatically when the session is unreffed.
    if (m_hostPreferences->enableAudio()) {
        if (!spice_audio_get(m_spiceSession, nullptr))
            qCWarning(KRDC) << "SpiceView: could not initialize the audio backend";
    }

    // Apply saved resolution preference.  m_pendingResize is in logical pixels;
    // sendMonitorConfig() will scale by DPR when it fires after the first frame.
    switch (m_hostPreferences->resolution()) {
    case SpiceHostPreferences::Resolution::Small:
        m_pendingResize = QSize(1280, 720);
        break;
    case SpiceHostPreferences::Resolution::Medium:
        m_pendingResize = QSize(1600, 900);
        break;
    case SpiceHostPreferences::Resolution::Large:
        m_pendingResize = QSize(1920, 1080);
        break;
    case SpiceHostPreferences::Resolution::MatchScreen: {
        QScreen *screen = qGuiApp->primaryScreen();
        const QSize physical = screen->size() * screen->devicePixelRatio();
        m_pendingResize = QSize(physical.width(), physical.height());
        break;
    }
    case SpiceHostPreferences::Resolution::Custom:
        m_pendingResize = QSize(m_hostPreferences->width(), m_hostPreferences->height());
        break;
    case SpiceHostPreferences::Resolution::MatchWindow:
        // The scroll-area viewport may not have its final size until Qt finishes
        // the initial layout pass (which happens after startConnection returns).
        // Leave m_pendingResize invalid here; scaleResize() will set it correctly
        // when the scroll area fires its resized() signal during layout.
        break;
    default:
        break;
    }

    connectSession();

    if (!spice_session_connect(m_spiceSession)) {
        qCWarning(KRDC) << "SpiceView: spice_session_connect() failed for" << m_host << m_port;
        setStatus(Disconnected);
        Q_EMIT errorMessage(tr("Connection failed"), tr("Could not connect to %1:%2").arg(m_host).arg(m_port));
        disconnectSession();
        g_object_unref(m_spiceSession);
        m_spiceSession = nullptr;
        return false;
    }

    return true;
}

void SpiceView::startQuittingConnection()
{
    m_quitFlag = true;
    m_firstFrame = true;
    m_firstMonitorConfig = true;
    m_lastSentPhysicalSize = QSize();
    m_serverMouseMode = false;
    m_resizeTimer->stop();

    unpressModifiers();

    if (m_spiceSession) {
        // Disconnect per-channel signals first so that spice_session_disconnect()
        // cannot fire cbChannelEvent (SPICE_CHANNEL_CLOSED) and queue a stale
        // invokeMethod call against this object during teardown.
        if (m_mainChannel)
            g_signal_handlers_disconnect_by_data(SPICE_CHANNEL(m_mainChannel), this);
        if (m_displayChannel)
            g_signal_handlers_disconnect_by_data(SPICE_CHANNEL(m_displayChannel), this);
        if (m_cursorChannel)
            g_signal_handlers_disconnect_by_data(SPICE_CHANNEL(m_cursorChannel), this);

        disconnectSession();
        spice_session_disconnect(m_spiceSession);
        g_object_unref(m_spiceSession);
        m_spiceSession = nullptr;
    }

    m_mainChannel = nullptr;
    m_displayChannel = nullptr;
    m_cursorChannel = nullptr;
    m_inputsChannel = nullptr;
    m_playbackChannel = nullptr;

    setStatus(Disconnected);
}

bool SpiceView::isQuitting()
{
    return m_quitFlag;
}

QSize SpiceView::framebufferSize()
{
    // The frame is stored in physical pixels with no DPR annotation.
    // Return the physical size; callers that need logical size divide by
    // devicePixelRatioF() themselves.
    return m_frame.size();
}

bool SpiceView::supportsScaling() const
{
    return true;
}

bool SpiceView::supportsLocalCursor() const
{
    return true;
}

bool SpiceView::supportsViewOnly() const
{
    return true;
}

bool SpiceView::supportsClipboardSharing() const
{
    return true;
}

HostPreferences *SpiceView::hostPreferences()
{
    return m_hostPreferences;
}

void SpiceView::enableScaling(bool scale)
{
    m_scale = scale;
    setMinimumSize(0, 0);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    if (m_scale) {
        if (!m_frame.isNull() && parentWidget())
            scaleResize(parentWidget()->width(), parentWidget()->height());
    } else {
        if (!m_frame.isNull()) {
            const qreal dpr = devicePixelRatioF();
            m_horizontalFactor = 1.0 / dpr;
            m_verticalFactor = 1.0 / dpr;
            const QSize logicalSize(qRound(m_frame.width() / dpr), qRound(m_frame.height() / dpr));
            resize(logicalSize);
        } else {
            m_horizontalFactor = 1.0;
            m_verticalFactor = 1.0;
        }
    }
    update();
}

void SpiceView::showLocalCursor(LocalCursorState state)
{
    RemoteView::showLocalCursor(state);
    // Apply immediately: if local cursor is on, show the system arrow and suppress
    // any remote cursor shape; if off, the next cursor-set/hide callback will apply.
    if (state == CursorOn)
        setCursor(localDefaultCursor());
    else
        unsetCursor(); // will be overridden by the next cbCursorSet if one is pending
}

void SpiceView::scaleResize(int w, int h)
{
    // w, h = scroll-area viewport size in logical pixels.
    if (!m_frame.isNull()) {
        const qreal dpr = devicePixelRatioF();

        if (m_scale) {
            // Scaling: stretch widget to fill viewport.
            m_horizontalFactor = static_cast<qreal>(w) / m_frame.width();
            m_verticalFactor = static_cast<qreal>(h) / m_frame.height();
            resize(w, h);
        } else {
            // Non-scaling: factor = 1/dpr so srcRect covers the full physical frame.
            m_horizontalFactor = 1.0 / dpr;
            m_verticalFactor = 1.0 / dpr;
        }
        update();

        // Queue monitor config after first frame so server renders at our viewport size.
        if (!m_firstFrame && w > 0 && h > 0 && m_mainChannel && !m_quitFlag) {
            const QSize physicalRequest(qRound(w * dpr), qRound(h * dpr));
            if (physicalRequest != m_lastSentPhysicalSize) {
                m_pendingResize = QSize(w, h);
                if (m_firstMonitorConfig) {
                    // Send immediately on the first config — no debounce — so the
                    // server resizes before the user sees the old framebuffer size.
                    QMetaObject::invokeMethod(this, "sendMonitorConfig", Qt::QueuedConnection);
                } else {
                    m_resizeTimer->start();
                }
            }
        }
    }
}

void SpiceView::handleDevicePixelRatioChange()
{
    if (!m_frame.isNull() && !m_quitFlag) {
        if (m_scale && parentWidget())
            scaleResize(parentWidget()->width(), parentWidget()->height());
        // Re-request at the new physical resolution.
        if (m_mainChannel) {
            m_pendingResize = QSize(width(), height());
            m_resizeTimer->start();
        }
    }
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void SpiceView::paintEvent(QPaintEvent *event)
{
    if (m_frame.isNull() || !m_displayMarked)
        return;

    event->accept();

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // dstRect: logical widget pixels. srcRect: physical frame pixels.
    // factor = logicalViewport / physicalFrame, so srcRect = dstRect / factor.
    const QRectF dstRect = event->rect();
    const QRectF srcRect(dstRect.x() / m_horizontalFactor,
                         dstRect.y() / m_verticalFactor,
                         dstRect.width() / m_horizontalFactor,
                         dstRect.height() / m_verticalFactor);
    painter.drawImage(dstRect, m_frame, srcRect);
}

void SpiceView::resizeEvent(QResizeEvent *event)
{
    RemoteView::resizeEvent(event);
    // scaleResize() is driven by the scroll area's resized() signal, which
    // carries the true viewport dimensions.  Calling it here from resizeEvent
    // would pass the widget's own (post-footprint) size and recurse.
}

// ---------------------------------------------------------------------------
// Input handling
// ---------------------------------------------------------------------------

// Map Qt::MouseButtons to the SPICE button mask bitmask
gint SpiceView::qtButtonsToSpiceMask(Qt::MouseButtons buttons)
{
    gint mask = 0;
    if (buttons & Qt::LeftButton)
        mask |= SPICE_MOUSE_BUTTON_MASK_LEFT;
    if (buttons & Qt::MiddleButton)
        mask |= SPICE_MOUSE_BUTTON_MASK_MIDDLE;
    if (buttons & Qt::RightButton)
        mask |= SPICE_MOUSE_BUTTON_MASK_RIGHT;
    return mask;
}

void SpiceView::handleKeyEvent(QKeyEvent *event)
{
    if (m_viewOnly || !m_inputsChannel)
        return;

    // On Linux/X11, nativeScanCode() is the evdev keycode.
    // SPICE uses XT (set-1) scan codes; evdev = XT + 8.
    const guint scancode = event->nativeScanCode() - 8;
    if (scancode == 0)
        return;

    if (event->type() == QEvent::KeyPress)
        spice_inputs_channel_key_press(m_inputsChannel, scancode);
    else
        spice_inputs_channel_key_release(m_inputsChannel, scancode);
}

void SpiceView::handleWheelEvent(QWheelEvent *event)
{
    if (m_viewOnly || !m_inputsChannel)
        return;

    const QPoint delta = event->angleDelta();

    if (delta.y() > 0) {
        spice_inputs_channel_button_press(m_inputsChannel, SPICE_MOUSE_BUTTON_UP, m_buttonMask);
        spice_inputs_channel_button_release(m_inputsChannel, SPICE_MOUSE_BUTTON_UP, m_buttonMask);
    } else if (delta.y() < 0) {
        spice_inputs_channel_button_press(m_inputsChannel, SPICE_MOUSE_BUTTON_DOWN, m_buttonMask);
        spice_inputs_channel_button_release(m_inputsChannel, SPICE_MOUSE_BUTTON_DOWN, m_buttonMask);
    }
}

void SpiceView::handleMouseEvent(QMouseEvent *event)
{
    if (m_viewOnly || !m_inputsChannel)
        return;

    // event->position() is in logical pixels.
    // factor = logicalW / physicalFrameW, so position in frame = pos / factor.
    int x = static_cast<int>(event->position().x() / m_horizontalFactor);
    int y = static_cast<int>(event->position().y() / m_verticalFactor);

    m_buttonMask = qtButtonsToSpiceMask(event->buttons());

    if (m_serverMouseMode) {
        // Server-side mouse: send relative delta from last position.
        const QPoint cur(x, y);
        if (!m_lastMousePos.isNull()) {
            const int dx = cur.x() - m_lastMousePos.x();
            const int dy = cur.y() - m_lastMousePos.y();
            if (dx != 0 || dy != 0)
                spice_inputs_channel_motion(m_inputsChannel, dx, dy, m_buttonMask);
        }
        m_lastMousePos = cur;
    } else {
        // Client-side mouse: send absolute position.
        spice_inputs_channel_position(m_inputsChannel, x, y, 0 /* display id */, m_buttonMask);
    }

    // Also send explicit press/release for button state changes
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick) {
        gint button = SPICE_MOUSE_BUTTON_INVALID;
        switch (event->button()) {
        case Qt::LeftButton:
            button = SPICE_MOUSE_BUTTON_LEFT;
            break;
        case Qt::MiddleButton:
            button = SPICE_MOUSE_BUTTON_MIDDLE;
            break;
        case Qt::RightButton:
            button = SPICE_MOUSE_BUTTON_RIGHT;
            break;
        default:
            break;
        }
        if (button != SPICE_MOUSE_BUTTON_INVALID)
            spice_inputs_channel_button_press(m_inputsChannel, button, m_buttonMask);
    } else if (event->type() == QEvent::MouseButtonRelease) {
        gint button = SPICE_MOUSE_BUTTON_INVALID;
        switch (event->button()) {
        case Qt::LeftButton:
            button = SPICE_MOUSE_BUTTON_LEFT;
            break;
        case Qt::MiddleButton:
            button = SPICE_MOUSE_BUTTON_MIDDLE;
            break;
        case Qt::RightButton:
            button = SPICE_MOUSE_BUTTON_RIGHT;
            break;
        default:
            break;
        }
        if (button != SPICE_MOUSE_BUTTON_INVALID)
            spice_inputs_channel_button_release(m_inputsChannel, button, m_buttonMask);
    }
}

// ---------------------------------------------------------------------------
// Session signal wiring
// ---------------------------------------------------------------------------

void SpiceView::connectSession()
{
    g_signal_connect(m_spiceSession, "channel-new", G_CALLBACK(cbChannelNew), this);
    g_signal_connect(m_spiceSession, "channel-destroy", G_CALLBACK(cbChannelDestroy), this);
}

void SpiceView::disconnectSession()
{
    g_signal_handlers_disconnect_by_data(m_spiceSession, this);
}

// ---------------------------------------------------------------------------
// Static GLib callbacks – dispatch to Qt slots via QMetaObject::invokeMethod
// ---------------------------------------------------------------------------

void SpiceView::cbChannelNew(SpiceSession * /*session*/, SpiceChannel *channel, gpointer data)
{
    auto *self = static_cast<SpiceView *>(data);

    if (SPICE_IS_MAIN_CHANNEL(channel)) {
        self->m_mainChannel = SPICE_MAIN_CHANNEL(channel);
        g_signal_connect(channel, "channel-event", G_CALLBACK(cbChannelEvent), self);
        // main-agent-update fires when the guest agent connects or disconnects.
        // Use it to send a pending monitor config once the agent is available.
        g_signal_connect(channel, "main-agent-update", G_CALLBACK(cbMainAgentUpdate), self);
        g_signal_connect(channel, "main-mouse-update", G_CALLBACK(cbMainMouseUpdate), self);
        // Clipboard: guest → client
        g_signal_connect(channel, "main-clipboard-selection-grab", G_CALLBACK(cbClipboardGrab), self);
        g_signal_connect(channel, "main-clipboard-selection-request", G_CALLBACK(cbClipboardRequest), self);
        g_signal_connect(channel, "main-clipboard-selection", G_CALLBACK(cbClipboardData), self);
        g_signal_connect(channel, "main-clipboard-selection-release", G_CALLBACK(cbClipboardRelease), self);
        // The main channel is connected by spice_session_connect(); don't call
        // spice_channel_connect() on it — that would start a duplicate connection.

    } else if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
        self->m_displayChannel = SPICE_DISPLAY_CHANNEL(channel);
        g_signal_connect(channel, "channel-event", G_CALLBACK(cbDisplayChannelEvent), self);
        g_signal_connect(channel, "display-primary-create", G_CALLBACK(cbDisplayPrimary), self);
        g_signal_connect(channel, "display-primary-destroy", G_CALLBACK(cbDisplayPrimaryDestroy), self);
        g_signal_connect(channel, "display-invalidate", G_CALLBACK(cbDisplayInvalidate), self);
        g_signal_connect(channel, "display-mark", G_CALLBACK(cbDisplayMark), self);
        // Explicitly connect the channel — spice_session_connect only connects
        // the main channel; all others must be connected manually.
        spice_channel_connect(channel);

    } else if (SPICE_IS_CURSOR_CHANNEL(channel)) {
        self->m_cursorChannel = SPICE_CURSOR_CHANNEL(channel);
        g_signal_connect(channel, "cursor-set", G_CALLBACK(cbCursorSet), self);
        g_signal_connect(channel, "cursor-hide", G_CALLBACK(cbCursorHide), self);
        g_signal_connect(channel, "cursor-reset", G_CALLBACK(cbCursorReset), self);
        spice_channel_connect(channel);

    } else if (SPICE_IS_INPUTS_CHANNEL(channel)) {
        self->m_inputsChannel = SPICE_INPUTS_CHANNEL(channel);
        spice_channel_connect(channel);

    } else if (SPICE_IS_PLAYBACK_CHANNEL(channel)) {
        self->m_playbackChannel = SPICE_PLAYBACK_CHANNEL(channel);
        // SpiceAudio's channel-new handler runs after this callback. It must
        // install its playback handlers before connecting the channel, and
        // skips channels that are already connecting. Let it connect this one.
    }
}

void SpiceView::cbChannelDestroy(SpiceSession * /*session*/, SpiceChannel *channel, gpointer data)
{
    auto *self = static_cast<SpiceView *>(data);

    // Disconnect any per-channel signals we connected in cbChannelNew
    g_signal_handlers_disconnect_by_data(channel, self);

    if (SPICE_IS_MAIN_CHANNEL(channel) && SPICE_MAIN_CHANNEL(channel) == self->m_mainChannel)
        self->m_mainChannel = nullptr;
    else if (SPICE_IS_DISPLAY_CHANNEL(channel) && SPICE_DISPLAY_CHANNEL(channel) == self->m_displayChannel)
        self->m_displayChannel = nullptr;
    else if (SPICE_IS_CURSOR_CHANNEL(channel) && SPICE_CURSOR_CHANNEL(channel) == self->m_cursorChannel)
        self->m_cursorChannel = nullptr;
    else if (SPICE_IS_INPUTS_CHANNEL(channel) && SPICE_INPUTS_CHANNEL(channel) == self->m_inputsChannel)
        self->m_inputsChannel = nullptr;
    else if (SPICE_IS_PLAYBACK_CHANNEL(channel) && SPICE_PLAYBACK_CHANNEL(channel) == self->m_playbackChannel)
        self->m_playbackChannel = nullptr;
}

void SpiceView::cbChannelEvent(SpiceChannel * /*channel*/, SpiceChannelEvent event, gpointer data)
{
    auto *self = static_cast<SpiceView *>(data);

    if (self->m_quitFlag)
        return;

    switch (event) {
    case SPICE_CHANNEL_OPENED:
        QMetaObject::invokeMethod(self, "onChannelConnected", Qt::QueuedConnection);
        break;
    case SPICE_CHANNEL_CLOSED:
        QMetaObject::invokeMethod(self, "onChannelDisconnected", Qt::QueuedConnection);
        break;
    case SPICE_CHANNEL_ERROR_CONNECT:
    case SPICE_CHANNEL_ERROR_TLS:
    case SPICE_CHANNEL_ERROR_LINK:
    case SPICE_CHANNEL_ERROR_IO:
        QMetaObject::invokeMethod(self, "onChannelDisconnected", Qt::QueuedConnection);
        break;
    case SPICE_CHANNEL_ERROR_AUTH:
        // channel-event fires on the GUI thread (Qt uses QEventDispatcherGlib,
        // so the GLib default context IS the Qt event loop). We must not block
        // here — use QueuedConnection to defer the dialog to the next iteration.
        QMetaObject::invokeMethod(self, "requestPassword", Qt::QueuedConnection);
        break;
    default:
        break;
    }
}

void SpiceView::cbDisplayChannelEvent(SpiceChannel *channel, SpiceChannelEvent event, gpointer data)
{
    auto *self = static_cast<SpiceView *>(data);
    if (self->m_quitFlag)
        return;

    if (event != SPICE_CHANNEL_OPENED)
        return;

    // The display channel just finished its handshake. If the server already
    // pushed a primary surface (possible on fast connections) we may have missed
    // the display-primary-create signal. Query it now as a fallback.
    SpiceDisplayPrimary primary;
    if (spice_display_channel_get_primary(channel, 0, &primary)) {
        cbDisplayPrimary(channel, static_cast<gint>(primary.format), primary.width, primary.height, primary.stride, primary.shmid, primary.data, self);
        if (primary.marked)
            cbDisplayMark(channel, TRUE, self);
    }
}

// Helper: map SpiceSurfaceFmt to QImage::Format. Returns Format_Invalid on unknown formats.
static QImage::Format spiceFmtToQt(gint format)
{
    switch (static_cast<SpiceSurfaceFmt>(format)) {
    case SPICE_SURFACE_FMT_32_xRGB:
        return QImage::Format_RGB32;
    case SPICE_SURFACE_FMT_32_ARGB:
        return QImage::Format_ARGB32;
    default:
        return QImage::Format_Invalid;
    }
}

void SpiceView::cbDisplayPrimary(SpiceChannel * /*channel*/, gint format, gint width, gint height, gint stride, gint /*shmid*/, gpointer pixels, gpointer data)
{
    auto *self = static_cast<SpiceView *>(data);
    if (self->m_quitFlag)
        return;

    const QImage::Format qFormat = spiceFmtToQt(format);
    if (qFormat == QImage::Format_Invalid) {
        qCWarning(KRDC) << "SpiceView: unsupported surface format" << format;
        return;
    }

    QImage frame(static_cast<const uchar *>(pixels), width, height, stride, qFormat);
    QMetaObject::invokeMethod(self, "onDisplayPrimary", Qt::QueuedConnection, Q_ARG(QImage, frame.copy()));
}

void SpiceView::cbDisplayPrimaryDestroy(SpiceChannel * /*channel*/, gpointer data)
{
    auto *self = static_cast<SpiceView *>(data);
    if (self->m_quitFlag)
        return;

    QMetaObject::invokeMethod(
        self,
        [self]() {
            self->m_displayMarked = false;
            self->m_frame = QImage();
            self->update();
        },
        Qt::QueuedConnection);
}

void SpiceView::cbDisplayInvalidate(SpiceChannel *channel, gint x, gint y, gint w, gint h, gpointer data)
{
    auto *self = static_cast<SpiceView *>(data);
    if (self->m_quitFlag)
        return;

    SpiceDisplayPrimary primary;
    if (!spice_display_channel_get_primary(channel, 0, &primary))
        return;

    const QImage::Format qFormat = spiceFmtToQt(static_cast<gint>(primary.format));
    if (qFormat == QImage::Format_Invalid)
        return;

    // Wrap the live SPICE buffer in a QImage view and deep-copy only the dirty
    // rect. The callback is on the GUI thread so the buffer is valid right now.
    const QImage src(primary.data, primary.width, primary.height, primary.stride, qFormat);
    QImage patch = src.copy(x, y, w, h);

    QMetaObject::invokeMethod(self, "onDisplayInvalidate", Qt::QueuedConnection, Q_ARG(int, x), Q_ARG(int, y), Q_ARG(QImage, std::move(patch)));
}

void SpiceView::cbDisplayMark(SpiceChannel * /*channel*/, gboolean mark, gpointer data)
{
    auto *self = static_cast<SpiceView *>(data);
    if (self->m_quitFlag)
        return;

    QMetaObject::invokeMethod(self, "onDisplayMark", Qt::QueuedConnection, Q_ARG(bool, static_cast<bool>(mark)));
}

void SpiceView::cbMainAgentUpdate(SpiceChannel *channel, gpointer data)
{
    auto *self = static_cast<SpiceView *>(data);
    if (self->m_quitFlag)
        return;

    // If the agent just became available and we have a pending resize, send it now.
    if (spice_main_channel_agent_test_capability(SPICE_MAIN_CHANNEL(channel), VD_AGENT_CAP_MONITORS_CONFIG)) {
        if (self->m_pendingResize.isValid())
            QMetaObject::invokeMethod(self, "sendMonitorConfig", Qt::QueuedConnection);
    }
}

void SpiceView::cbMainMouseUpdate(SpiceChannel *channel, gpointer data)
{
    auto *self = static_cast<SpiceView *>(data);
    if (self->m_quitFlag)
        return;

    gint mode = 0;
    g_object_get(channel, "mouse-mode", &mode, nullptr);
    const bool serverMode = (mode == SPICE_MOUSE_MODE_SERVER);
    QMetaObject::invokeMethod(self, "onMouseModeUpdate", Qt::QueuedConnection, Q_ARG(bool, serverMode));
}

// ---------------------------------------------------------------------------
// Cursor channel callbacks
// ---------------------------------------------------------------------------

void SpiceView::cbCursorSet(SpiceCursorChannel * /*channel*/, gint width, gint height, gint hot_x, gint hot_y, gpointer rgba, gpointer data)
{
    auto *self = static_cast<SpiceView *>(data);
    if (self->m_quitFlag)
        return;

    // Only ALPHA cursors carry 32-bit ARGB pre-multiplied data.
    // Other types (MONO, COLOR*) are uncommon on modern SPICE servers.
    // We build a QCursor from the ARGB data; for unsupported types fall back
    // to the default arrow cursor.
    if (!rgba || width <= 0 || height <= 0) {
        QMetaObject::invokeMethod(self, "onCursorHide", Qt::QueuedConnection);
        return;
    }

    // The SPICE server delivers ALPHA cursors as ARGB32 pre-multiplied.
    const QImage img(static_cast<const uchar *>(rgba), width, height, width * 4, QImage::Format_ARGB32_Premultiplied);

    QCursor cursor(QPixmap::fromImage(img.copy()), hot_x, hot_y);
    QMetaObject::invokeMethod(self, "onCursorSet", Qt::QueuedConnection, Q_ARG(QCursor, cursor));
}

void SpiceView::cbCursorHide(SpiceCursorChannel * /*channel*/, gpointer data)
{
    auto *self = static_cast<SpiceView *>(data);
    if (self->m_quitFlag)
        return;
    QMetaObject::invokeMethod(self, "onCursorHide", Qt::QueuedConnection);
}

void SpiceView::cbCursorReset(SpiceCursorChannel * /*channel*/, gpointer data)
{
    // reset = hide the custom cursor and restore the default
    auto *self = static_cast<SpiceView *>(data);
    if (self->m_quitFlag)
        return;
    QMetaObject::invokeMethod(self, "onCursorHide", Qt::QueuedConnection);
}

// ---------------------------------------------------------------------------
// Qt slots (GUI thread)
// ---------------------------------------------------------------------------

void SpiceView::requestPassword()
{
    setStatus(Authenticating);
    Q_EMIT showingPasswordDialog(true);

    // First attempt: try wallet / URL credentials silently before showing UI.
    if (m_firstPasswordTry) {
        m_firstPasswordTry = false;

        if (m_hostPreferences->walletSupport()) {
            const QString walletPw = readWalletPassword();
            if (!walletPw.isEmpty()) {
                g_object_set(m_spiceSession, "password", walletPw.toUtf8().constData(), nullptr);
                Q_EMIT showingPasswordDialog(false);
                spice_session_connect(m_spiceSession);
                return;
            }
        }

        const QString urlPw = m_url.password();
        if (!urlPw.isEmpty()) {
            g_object_set(m_spiceSession, "password", urlPw.toUtf8().constData(), nullptr);
            Q_EMIT showingPasswordDialog(false);
            spice_session_connect(m_spiceSession);
            return;
        }

        // Neither wallet nor URL had a password — fall through to show the dialog.
    }

    // Show the password dialog.
    KPasswordDialog dialog(this, KPasswordDialog::ShowKeepPassword);
    dialog.setPrompt(i18n("Authentication is required to connect to <b>%1</b>.", m_host));

    if (dialog.exec() != KPasswordDialog::Accepted) {
        qCDebug(KRDC) << "SpiceView: password dialog cancelled";
        Q_EMIT showingPasswordDialog(false);
        startQuitting();
        return;
    }

    const QString password = dialog.password();

    if (dialog.keepPassword())
        saveWalletPassword(password);

    g_object_set(m_spiceSession, "password", password.toUtf8().constData(), nullptr);

    Q_EMIT showingPasswordDialog(false);
    spice_session_connect(m_spiceSession);
}

void SpiceView::onChannelConnected()
{
    // The main channel opened — we are authenticated and ready.
    if (m_status != Connected) {
        setStatus(Connected);
        Q_EMIT connected();
    }
}

void SpiceView::onChannelDisconnected()
{
    if (!m_quitFlag) {
        setStatus(Disconnected);
        Q_EMIT disconnected();
    }
}

void SpiceView::onDisplayPrimary(QImage frame)
{
    // frame is in physical pixels, no DPR annotation needed.
    const bool sizeChanged = (frame.size() != m_frame.size());
    m_frame = std::move(frame);

    const qreal dpr = devicePixelRatioF();
    // Logical size the widget should occupy (how large the remote desktop
    // appears at 1:1 on this display, accounting for HiDPI).
    const QSize logicalSize(qRound(m_frame.width() / dpr), qRound(m_frame.height() / dpr));

    if (m_firstFrame) {
        m_firstFrame = false;
        setMinimumSize(0, 0);
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        Q_EMIT framebufferSizeChanged(logicalSize.width(), logicalSize.height());
    }

    if (m_scale) {
        // Scaling: widget fills the viewport; factors map viewport→frame.
        const int vpW = width() > 0 ? width() : logicalSize.width();
        const int vpH = height() > 0 ? height() : logicalSize.height();
        m_horizontalFactor = static_cast<qreal>(vpW) / m_frame.width();
        m_verticalFactor = static_cast<qreal>(vpH) / m_frame.height();
        resize(vpW, vpH);
    } else {
        // Non-scaling: widget is logical size; factor = 1/dpr so that
        // srcRect = dstRect * dpr covers the full physical frame.
        m_horizontalFactor = 1.0 / dpr;
        m_verticalFactor = 1.0 / dpr;
        if (sizeChanged)
            resize(logicalSize);
        // Do NOT emit framebufferSizeChanged on subsequent frames — the window
        // must not chase the server framebuffer; the server chases the window
        // (via monitor config).  Chasing in both directions causes oscillation.
    }

    update();
}

void SpiceView::onDisplayInvalidate(int x, int y, QImage patch)
{
    if (m_frame.isNull())
        return;
    QPainter painter(&m_frame);
    painter.drawImage(QPoint(x, y), patch);
    // Convert physical frame coords to logical widget coords for the repaint.
    update(QRectF(x * m_horizontalFactor, y * m_verticalFactor, patch.width() * m_horizontalFactor, patch.height() * m_verticalFactor)
               .toAlignedRect()
               .adjusted(-1, -1, 1, 1));
}

void SpiceView::onDisplayMark(bool mark)
{
    m_displayMarked = mark;
    if (mark)
        update();
}

void SpiceView::onCursorSet(QCursor cursor)
{
    if (m_localCursorState == CursorOn)
        setCursor(localDefaultCursor());
    else
        setCursor(cursor);
}

void SpiceView::onCursorHide()
{
    if (m_localCursorState == CursorOn)
        setCursor(localDefaultCursor());
    else
        unsetCursor();
}

void SpiceView::sendMonitorConfig()
{
    if (!m_mainChannel || !m_pendingResize.isValid() || m_quitFlag)
        return;

    if (!spice_main_channel_agent_test_capability(m_mainChannel, VD_AGENT_CAP_MONITORS_CONFIG)) {
        qCWarning(KRDC) << "SpiceView: guest agent does not support monitor config"
                           " — is spice-vdagent running in the guest?";
        return;
    }

    // m_pendingResize is in logical pixels; multiply by DPR to request physical
    // pixels from the SPICE server.
    const qreal dpr = devicePixelRatioF();
    const int w = qRound(m_pendingResize.width() * dpr);
    const int h = qRound(m_pendingResize.height() * dpr);
    qCDebug(KRDC) << "SpiceView: sending monitor config" << w << "x" << h << "(logical:" << m_pendingResize << " dpr:" << dpr << ")";
    // Ensure the display is marked enabled, then update its resolution.
    // update=TRUE on update_display sends the config immediately; we also
    // call send_monitor_config explicitly for older agent versions.
    spice_main_channel_update_display_enabled(m_mainChannel, 0, TRUE, FALSE);
    spice_main_channel_update_display(m_mainChannel, 0, 0, 0, w, h, FALSE);
    spice_main_channel_send_monitor_config(m_mainChannel);
    m_lastSentPhysicalSize = QSize(w, h);
    m_pendingResize = QSize(); // clear so agent-update won't re-fire this
    m_firstMonitorConfig = false; // subsequent resizes use the debounce timer
}

void SpiceView::onMouseModeUpdate(bool serverMode)
{
    if (m_serverMouseMode == serverMode)
        return;
    m_serverMouseMode = serverMode;
    m_lastMousePos = QPoint(); // reset so next event doesn't send a huge delta
    qCDebug(KRDC) << "SpiceView: mouse mode changed to" << (serverMode ? "server (relative)" : "client (absolute)");
    if (serverMode) {
        // Try to switch back to client mode — most desktops accept this.
        // If the server refuses, m_serverMouseMode stays true and we stay in
        // relative-motion mode.
        spice_main_channel_request_mouse_mode(m_mainChannel, SPICE_MOUSE_MODE_CLIENT);
    }
}

// ---------------------------------------------------------------------------
// Clipboard sharing
// ---------------------------------------------------------------------------

// Guest tells us it has grabbed the clipboard and supports these types.
// Return TRUE to acknowledge that we'll handle it.
gboolean SpiceView::cbClipboardGrab(SpiceMainChannel * /*channel*/, guint selection, guint32 *types, guint ntypes, gpointer data)
{
    auto *self = static_cast<SpiceView *>(data);
    if (self->m_quitFlag || selection != VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD)
        return FALSE;

    QList<guint32> typeList;
    typeList.reserve(static_cast<int>(ntypes));
    for (guint i = 0; i < ntypes; ++i)
        typeList.append(types[i]);

    QMetaObject::invokeMethod(self, "onClipboardGrab", Qt::QueuedConnection, Q_ARG(guint, selection), Q_ARG(QList<guint32>, typeList));
    return TRUE;
}

// Guest is asking us to provide our local clipboard in the given type.
// Return TRUE to indicate we'll call clipboard_selection_notify shortly.
gboolean SpiceView::cbClipboardRequest(SpiceMainChannel *channel, guint selection, guint32 type, gpointer data)
{
    auto *self = static_cast<SpiceView *>(data);
    if (self->m_quitFlag || selection != VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD)
        return FALSE;

    const QClipboard *cb = QApplication::clipboard();
    if (!cb)
        return FALSE;

    if (type == VD_AGENT_CLIPBOARD_UTF8_TEXT) {
        const QByteArray text = cb->text().toUtf8();
        if (text.isEmpty())
            return FALSE;
        spice_main_channel_clipboard_selection_notify(channel,
                                                      selection,
                                                      type,
                                                      reinterpret_cast<const guchar *>(text.constData()),
                                                      static_cast<gsize>(text.size()));
        return TRUE;
    }

    if (type == VD_AGENT_CLIPBOARD_IMAGE_PNG || type == VD_AGENT_CLIPBOARD_IMAGE_BMP) {
        const QImage img = cb->image();
        if (img.isNull())
            return FALSE;
        QByteArray buf;
        QBuffer buffer(&buf);
        buffer.open(QIODevice::WriteOnly);
        const char *fmt = (type == VD_AGENT_CLIPBOARD_IMAGE_PNG) ? "PNG" : "BMP";
        if (!img.save(&buffer, fmt))
            return FALSE;
        spice_main_channel_clipboard_selection_notify(channel,
                                                      selection,
                                                      type,
                                                      reinterpret_cast<const guchar *>(buf.constData()),
                                                      static_cast<gsize>(buf.size()));
        return TRUE;
    }

    return FALSE;
}

// Guest delivers clipboard data that it promised via grab.
void SpiceView::cbClipboardData(SpiceMainChannel * /*channel*/, guint selection, guint32 type, guchar *clipdata, guint size, gpointer data)
{
    auto *self = static_cast<SpiceView *>(data);
    if (self->m_quitFlag || selection != VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD)
        return;
    if (!clipdata || size == 0)
        return;
    if (type != VD_AGENT_CLIPBOARD_UTF8_TEXT && type != VD_AGENT_CLIPBOARD_IMAGE_PNG && type != VD_AGENT_CLIPBOARD_IMAGE_BMP)
        return;

    const QByteArray bytes(reinterpret_cast<const char *>(clipdata), static_cast<int>(size));
    QMetaObject::invokeMethod(self, "onClipboardData", Qt::QueuedConnection, Q_ARG(guint, selection), Q_ARG(guint32, type), Q_ARG(QByteArray, bytes));
}

// Guest has released clipboard ownership.
void SpiceView::cbClipboardRelease(SpiceMainChannel * /*channel*/, guint selection, gpointer data)
{
    auto *self = static_cast<SpiceView *>(data);
    if (self->m_quitFlag || selection != VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD)
        return;
    QMetaObject::invokeMethod(self, "onClipboardRelease", Qt::QueuedConnection, Q_ARG(guint, selection));
}

// ---------------------------------------------------------------------------
// Clipboard Qt slots (GUI thread)
// ---------------------------------------------------------------------------

// Guest has grabbed clipboard with these types — request the best available type.
void SpiceView::onClipboardGrab(guint /*selection*/, QList<guint32> types)
{
    if (!m_mainChannel || m_quitFlag)
        return;

    // Prefer PNG image, then text. BMP is a fallback if PNG isn't offered.
    if (types.contains(VD_AGENT_CLIPBOARD_IMAGE_PNG)) {
        spice_main_channel_clipboard_selection_request(m_mainChannel, VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD, VD_AGENT_CLIPBOARD_IMAGE_PNG);
    } else if (types.contains(VD_AGENT_CLIPBOARD_UTF8_TEXT)) {
        spice_main_channel_clipboard_selection_request(m_mainChannel, VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD, VD_AGENT_CLIPBOARD_UTF8_TEXT);
    } else if (types.contains(VD_AGENT_CLIPBOARD_IMAGE_BMP)) {
        spice_main_channel_clipboard_selection_request(m_mainChannel, VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD, VD_AGENT_CLIPBOARD_IMAGE_BMP);
    }
}

// Guest delivered clipboard data — set it on the Qt clipboard.
void SpiceView::onClipboardData(guint /*selection*/, guint32 type, QByteArray data)
{
    QClipboard *cb = QApplication::clipboard();
    if (!cb)
        return;

    if (type == VD_AGENT_CLIPBOARD_UTF8_TEXT) {
        cb->setText(QString::fromUtf8(data));
    } else if (type == VD_AGENT_CLIPBOARD_IMAGE_PNG || type == VD_AGENT_CLIPBOARD_IMAGE_BMP) {
        QImage img;
        if (img.loadFromData(data))
            cb->setImage(img);
    }
}

// Guest released clipboard — nothing to do on our side (Qt doesn't have a "clear" API
// that distinguishes "remote released" from user action, so we leave our clipboard alone).
void SpiceView::onClipboardRelease(guint /*selection*/)
{
    // nothing required
}

void SpiceView::handleLocalClipboardChanged(const QMimeData *data)
{
    if (!m_mainChannel || m_quitFlag || !data)
        return;

    if (!spice_main_channel_agent_test_capability(m_mainChannel, VD_AGENT_CAP_CLIPBOARD_BY_DEMAND))
        return;

    // Announce all types we can provide.
    QVector<guint32> types;
    if (data->hasImage())
        types.append(VD_AGENT_CLIPBOARD_IMAGE_PNG);
    if (data->hasText())
        types.append(VD_AGENT_CLIPBOARD_UTF8_TEXT);

    if (types.isEmpty())
        return;

    spice_main_channel_clipboard_selection_grab(m_mainChannel, VD_AGENT_CLIPBOARD_SELECTION_CLIPBOARD, types.data(), static_cast<int>(types.size()));
}
