/*
    SPDX-FileCopyrightText: 2024 KRDC Contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SPICEVIEW_H
#define SPICEVIEW_H

#include "remoteview.h"
#include "spicehostpreferences.h"

#include <QBuffer>
#include <QClipboard>
#include <QPoint>

extern "C" {
#include <spice-client-glib-2.0/spice-client.h>
#include <spice/enums.h>
#include <spice/vd_agent.h>
}

class SpiceView : public RemoteView
{
    Q_OBJECT

public:
    explicit SpiceView(QWidget *parent = nullptr, const QUrl &url = QUrl(), KConfigGroup configGroup = KConfigGroup());
    ~SpiceView() override;

    QSize framebufferSize() override;
    bool isQuitting() override;
    bool supportsScaling() const override;
    bool supportsLocalCursor() const override;
    bool supportsViewOnly() const override;
    bool supportsClipboardSharing() const override;

    HostPreferences *hostPreferences() override;

    void enableScaling(bool scale) override;
    void showLocalCursor(LocalCursorState state) override;

    void handleDevicePixelRatioChange() override;

public Q_SLOTS:
    void scaleResize(int w, int h) override;

protected:
    bool startConnection() override;
    void startQuittingConnection() override;

    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void handleKeyEvent(QKeyEvent *event) override;
    void handleWheelEvent(QWheelEvent *event) override;
    void handleMouseEvent(QMouseEvent *event) override;
    void handleLocalClipboardChanged(const QMimeData *data) override;

private:
    // SPICE session and channels
    SpiceSession *m_spiceSession;
    SpiceMainChannel *m_mainChannel;
    SpiceDisplayChannel *m_displayChannel;
    SpiceCursorChannel *m_cursorChannel;
    SpiceInputsChannel *m_inputsChannel;
    SpicePlaybackChannel *m_playbackChannel;

    // Debounce timer for monitor config requests
    QTimer *m_resizeTimer;
    QSize m_pendingResize;
    // Physical size of the last monitor config we sent; used to suppress
    // re-sending when the window adjusts after receiving the new framebuffer.
    QSize m_lastSentPhysicalSize;
    // True until the very first monitor config has been sent.  The first config
    // fires immediately (no debounce) to avoid the visible flash where the KRDC
    // window briefly shows the server's old framebuffer size.
    bool m_firstMonitorConfig;

    // Rendered framebuffer
    QImage m_frame;
    // True once display-mark(true) has been received; suppress painting until then
    bool m_displayMarked;
    // True until the first display-primary frame is fully processed.
    // Used to emit framebufferSizeChanged exactly once for initial window sizing.
    bool m_firstFrame;

    bool m_quitFlag;
    bool m_firstPasswordTry;
    qreal m_horizontalFactor;
    qreal m_verticalFactor;

    // Tracks which Qt mouse buttons are currently held, as a SPICE button mask
    gint m_buttonMask;

    // Mouse mode: true = server-side relative, false = client absolute (default)
    bool m_serverMouseMode;
    // Last logical position sent (for computing relative deltas in server mode)
    QPoint m_lastMousePos;

    SpiceHostPreferences *m_hostPreferences;

    // difference of 8 between x11 wayland.
    // see: https://wayland-devel.freedesktop.narkive.com/6dOtsFGc/gtk-hardware-scancodes-for-wayland-detecting-xwayland
    static constexpr quint32 x11WaylandEvdevOffset = 8;

    // GLib signal connection helpers
    void connectSession();
    void disconnectSession();

    // Translate a Qt mouse button state into a SPICE button mask
    static gint qtButtonsToSpiceMask(Qt::MouseButtons buttons);

    // Static C-style callbacks invoked by libspice-client-glib
    static void cbChannelNew(SpiceSession *session, SpiceChannel *channel, gpointer data);
    static void cbChannelDestroy(SpiceSession *session, SpiceChannel *channel, gpointer data);
    // channel-event on the main channel
    static void cbChannelEvent(SpiceChannel *channel, SpiceChannelEvent event, gpointer data);
    // channel-event on the display channel (fired when it finishes opening)
    static void cbDisplayChannelEvent(SpiceChannel *channel, SpiceChannelEvent event, gpointer data);
    // display channel
    static void cbDisplayPrimary(SpiceChannel *channel, gint format, gint width, gint height, gint stride, gint shmid, gpointer pixels, gpointer data);
    static void cbDisplayPrimaryDestroy(SpiceChannel *channel, gpointer data);
    static void cbDisplayInvalidate(SpiceChannel *channel, gint x, gint y, gint w, gint h, gpointer data);
    static void cbDisplayMark(SpiceChannel *channel, gboolean mark, gpointer data);
    // cursor channel
    static void cbCursorSet(SpiceCursorChannel *channel, gint width, gint height, gint hot_x, gint hot_y, gpointer rgba, gpointer data);
    static void cbCursorHide(SpiceCursorChannel *channel, gpointer data);
    static void cbCursorReset(SpiceCursorChannel *channel, gpointer data);

    static void cbMainAgentUpdate(SpiceChannel *channel, gpointer data);
    static void cbMainMouseUpdate(SpiceChannel *channel, gpointer data);

    // clipboard signals from guest → client
    static gboolean cbClipboardGrab(SpiceMainChannel *channel, guint selection, guint32 *types, guint ntypes, gpointer data);
    static gboolean cbClipboardRequest(SpiceMainChannel *channel, guint selection, guint32 type, gpointer data);
    static void cbClipboardData(SpiceMainChannel *channel, guint selection, guint32 type, guchar *clipdata, guint size, gpointer data);
    static void cbClipboardRelease(SpiceMainChannel *channel, guint selection, gpointer data);

private Q_SLOTS:
    void onChannelConnected();
    void onChannelDisconnected();
    void requestPassword();
    void onDisplayPrimary(QImage frame);
    void onDisplayInvalidate(int x, int y, QImage patch);
    void onDisplayMark(bool mark);
    void onCursorSet(QCursor cursor);
    void onCursorHide();
    void sendMonitorConfig();
    void onMouseModeUpdate(bool serverMode);
    // clipboard
    void onClipboardGrab(guint selection, QList<guint32> types);
    void onClipboardData(guint selection, guint32 type, QByteArray data);
    void onClipboardRelease(guint selection);
};

#endif // SPICEVIEW_H
