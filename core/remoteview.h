/*
    SPDX-FileCopyrightText: 2002-2003 Tim Jansen <tim@tjansen.de>
    SPDX-FileCopyrightText: 2007-2008 Urs Wolfer <uwolfer@kde.org>
    SPDX-FileCopyrightText: 2021 Rafał Lalik <rafallalik@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef REMOTEVIEW_H
#define REMOTEVIEW_H

#ifndef QTONLY
#include "krdccore_export.h"
#include <KWallet>
#else
#define KRDCCORE_EXPORT
#endif

#include <QClipboard>
#include <QMap>
#include <QUrl>
#include <QWidget>

#ifdef HAVE_WAYLAND
#include "shortcutinhibition_p.h"
#endif

#if defined(LIBSSH_FOUND) && !defined(QTONLY)
#define USE_SSH_TUNNEL
#include "sshtunnelthread.h"
#endif

struct ModifierKey {
    quint32 nativeScanCode;
    quint32 nativeVirtualKey;
};

class HostPreferences;

/**
 * Generic widget that displays a remote framebuffer.
 * Implement this if you want to add another backend.
 *
 * Things to take care of:
 * @li The RemoteView is responsible for its size. In
 *     non-scaling mode, set the fixed size of the widget
 *     to the remote resolution. In scaling mode, set the
 *     maximum size to the remote size and minimum size to the
 *     smallest resolution that your scaler can handle.
 * @li if you override mouseMoveEvent()
 *     you must ignore the QEvent, because the KRDC widget will
 *     need it for stuff like toolbar auto-hide and bump
 *     scrolling. If you use x11Event(), make sure that
 *     MotionNotify events will be forwarded.
 *
 */
class KRDCCORE_EXPORT RemoteView : public QWidget
{
    Q_OBJECT

public:
    enum Quality {
        Unknown,
        High,
        Medium,
        Low,
    };
    Q_ENUM(Quality)

    /**
     * Describes the state of a local cursor, if there is such a concept in the backend.
     * With local cursors, there are two cursors: the cursor on the local machine (client),
     * and the cursor on the remote machine (server). Because there is usually some lag,
     * some backends show both cursors simultaneously. In the VNC backend the local cursor
     * is a dot and the remote cursor is the 'real' cursor, usually an arrow.
     */

    enum LocalCursorState {
        CursorOn, ///< Always show local cursor based on remote one (or fallback to default).
        CursorOff, ///< Never show local cursor, only the remote one.
        /// Try to measure the lag and enable the local cursor if the latency is too high.
        CursorAuto,
    };
    Q_ENUM(LocalCursorState)

    /**
     * State of the connection. The state of the connection is returned
     * by @ref RemoteView::status().
     *
     * Not every state transition is allowed. You are only allowed to transition
     * a state to the following state, with three exceptions:
     * @li You can move from every state directly to Disconnected
     * @li You can move from every state except Disconnected to
     *     Disconnecting
     * @li You can move from Disconnected to Connecting
     *
     * @ref RemoteView::setStatus() will follow this rules for you.
     * (If you add/remove a state here, you must adapt it)
     */

    enum RemoteStatus {
        Connecting = 0,
        Authenticating = 1,
        Preparing = 2,
        Connected = 3,
        Disconnecting = -1,
        Disconnected = -2,
    };
    Q_ENUM(RemoteStatus)

    enum ErrorCode {
        None = 0,
        Internal,
        Connection,
        Protocol,
        IO,
        Name,
        NoServer,
        ServerBlocked,
        Authentication,
    };
    Q_ENUM(ErrorCode)

    ~RemoteView() override;

    /**
     * Checks whether the backend supports scaling. The
     * default implementation returns false.
     * @return true if scaling is supported
     * @see scaling()
     */
    virtual bool supportsScaling() const;

    /**
     * Checks whether the widget is in scale mode. The
     * default implementation always returns false.
     * @return true if scaling is activated. Must always be
     *         false if @ref supportsScaling() returns false
     * @see supportsScaling()
     */
    virtual bool scaling() const;

    /**
     * Checks whether the backend supports the concept of local cursors. The
     * default implementation returns false.
     * @return true if local cursors are supported/known
     * @see LocalCursorState
     * @see showLocalCursor()
     * @see localCursorState()
     */
    virtual bool supportsLocalCursor() const;

    /**
     * Sets the state of the dot cursor, if supported by the backend.
     * The default implementation does nothing.
     * @param state the new state (CursorOn, CursorOff or
     *        CursorAuto)
     * @see localCursorState()
     * @see supportsLocalCursor()
     */
    virtual void showLocalCursor(LocalCursorState state);

    /**
     * Returns the state of the local cursor. The default implementation returns
     * always CursorOff.
     * @return true if local cursors are supported/known
     * @see showLocalCursor()
     * @see supportsLocalCursor()
     */
    virtual LocalCursorState localCursorState() const;

    /**
     * Checks whether the backend supports the view only mode. The
     * default implementation returns false.
     * @return true if view-only mode is supported
     * @see LocalCursorState
     * @see showLocalCursor()
     * @see localCursorState()
     */
    virtual bool supportsViewOnly() const;

    /**
     * Checks whether the view is in view-only mode. This means
     * that all input is ignored.
     */
    virtual bool viewOnly();

    /**
     * Checks whether the backend supports clipboard sharing. The
     * default implementation returns false.
     * @return true if clipboard sharing is supported
     * @see localClipboardChanged()
     * @see remoteClipboardChanged()
     * @see handleLocalClipboardChanged()
     */
    virtual bool supportsClipboardSharing() const;

    /**
     * Checks whether grabbing all possible keys is enabled.
     */
    virtual bool grabAllKeys();

    /**
     * Returns the resolution of the remote framebuffer.
     * It should return a null @ref QSize when the size
     * is not known.
     * The backend must also emit a @ref framebufferSizeChanged()
     * when the size of the framebuffer becomes available
     * for the first time or the size changed.
     * @return the remote framebuffer size, a null QSize
     *         if unknown
     */
    virtual QSize framebufferSize();

    /**
     * Initiate the disconnection. This doesn't need to happen
     * immediately. The call must not block.
     * @see isQuitting()
     */
    void startQuitting();

    /**
     * Checks whether the view is currently quitting.
     * @return true if it is quitting
     * @see startQuitting()
     * @see setStatus()
     */
    virtual bool isQuitting();

    /**
     * @return the host the view is connected to
     */
    virtual QString host();

    /**
     * @return the port the view is connected to
     */
    virtual int port();

    /**
     * Initialize the view (for example by showing configuration
     * dialogs to the user) and start connecting. Should not block
     * without running the event loop (so displaying a dialog is ok).
     * When the view starts connecting the application must call
     * @ref setStatus() with the status Connecting.
     * @return true if successful (so far), false
     *         otherwise
     * @see connected()
     * @see disconnected()
     * @see disconnectedError()
     * @see statusChanged()
     */
    bool start();

    /**
     * Called when the configuration is changed.
     * The default implementation does nothing.
     */
    virtual void updateConfiguration();

    /**
     * @return screenshot of the view
     */
    virtual QPixmap takeScreenshot();

#ifndef QTONLY
    /**
     * Returns the current host preferences of this view.
     */
    virtual HostPreferences *hostPreferences() = 0;
#endif

    /**
     * Returns the current status of the connection.
     * @return the status of the connection
     * @see setStatus()
     */
    RemoteStatus status();

    /**
     * @return the current url
     */
    QUrl url();

public Q_SLOTS:
    /**
     * Called to enable or disable scaling.
     * Ignored if @ref supportsScaling() is false.
     * The default implementation does nothing.
     * @param scale true to enable, false to disable.
     * @see supportsScaling()
     * @see scaling()
     */
    virtual void enableScaling(bool scale);

    /**
     * Sets scaling factor for the view. If remote view has width R and
     * the window has width W, then scaling factor (float, range 0-1) set the
     * remote view width A to be: A = (R-W)*factor + W. For factor = 0, A=W,
     * so no scalling, for factor=1, A=R.
     *
     * @param factor scaling factor in the range 0-1
     */
    virtual void setScaleFactor(float factor);

    /**
     * Enables/disables the view-only mode.
     * Ignored if @ref supportsScaling() is false.
     * The default implementation does nothing.
     * @param viewOnly true to enable, false to disable.
     * @see supportsScaling()
     * @see viewOnly()
     */
    virtual void setViewOnly(bool viewOnly);

    /**
     * Enables/disables grabbing all possible keys.
     * @param grabAllKeys true to enable, false to disable.
     * Default is false.
     * @see grabAllKeys()
     */
    virtual void setGrabAllKeys(bool grabAllKeys);

    /**
     * Called to let the backend know it when
     * we switch from/to fullscreen.
     * @param on true when switching to fullscreen,
     *           false when switching from fullscreen.
     */
    virtual void switchFullscreen(bool on);

    /**
     * Sends a QKeyEvent to the remote server.
     * @param event the key to send
     */
    virtual void keyEvent(QKeyEvent *event);

    /**
     * Called when the visible place changed so remote
     * view can resize itself.
     *
     * @param w width of the remote view
     * @param h height of the remote view
     */
    virtual void scaleResize(int w, int h);

    /**
     * Internal handling of local clipboard change
     * Do not override in subclasses; implement handleLocalClipboardChanged() instead
     */
    void localClipboardChanged();

    /**
     * Internal handling of remote clipboard change
     * Subclasses must use this method to update local clipboard contents
     * Ownership of the data is transferred to the clipboard.
     */
    void remoteClipboardChanged(QMimeData *data);

Q_SIGNALS:
    /**
     * Emitted when the size of the remote screen changes. Also
     * called when the size is known for the first time.
     * @param w the width of the screen
     * @param h the height of the screen
     */
    void framebufferSizeChanged(int w, int h);

    /**
     * Emitted when the view connected successfully.
     */
    void connected();

    /**
     * Emitted when the view disconnected without error.
     */
    void disconnected();

    /**
     * Emitted when the view disconnected with error.
     */
    void disconnectedError();

    /**
     * Emitted when the view has a specific error.
     */
    void errorMessage(const QString &title, const QString &message);

    /**
     * Emitted when the status of the view changed.
     * @param s the new status
     */
    void statusChanged(RemoteView::RemoteStatus s);

    /**
     * Emitted when the password dialog is shown or hidden.
     * @param b true when the dialog is shown, false when it has been hidden
     */
    void showingPasswordDialog(bool b);

    /**
     * Emitted when the mouse on the remote side has been moved.
     * @param x the new x coordinate
     * @param y the new y coordinate
     * @param buttonMask the mask of mouse buttons (bit 0 for first mouse
     *                   button, 1 for second button etc)a
     */
    void mouseStateChanged(int x, int y, int buttonMask);

protected:
    RemoteView(QWidget *parent = nullptr);

    virtual bool startConnection() = 0;
    virtual void startQuittingConnection() = 0;

    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    bool event(QEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    virtual void handleKeyEvent(QKeyEvent *event) = 0;
    virtual void handleWheelEvent(QWheelEvent *event) = 0;
    virtual void handleMouseEvent(QMouseEvent *event) = 0;
    virtual void handleLocalClipboardChanged(const QMimeData *data) = 0;
    virtual void handleDevicePixelRatioChange() { };
    /**
     * The status of the remote view.
     */
    RemoteStatus m_status;

    /**
     * Set the status of the connection.
     * Emits a statusChanged() signal.
     * Note that the states need to be set in a certain order,
     * see @ref RemoteStatus. setStatus() will try to do this
     * transition automatically, so if you are in Connecting
     * and call setStatus(Preparing), setStatus() will
     * emit a Authenticating and then Preparing.
     * If you transition backwards, it will emit a
     * Disconnected before doing the transition.
     * @param s the new status
     */
    virtual void setStatus(RemoteStatus s);

    QCursor localDefaultCursor() const;

    void unpressModifiers();
    void grabKeyboard();
    void releaseKeyboard();

    QString m_host;
    int m_port;
    bool m_viewOnly;
    bool m_grabAllKeys;
    bool m_scale;
    bool m_keyboardIsGrabbed;
    QUrl m_url;
    qreal m_factor;
    QClipboard *m_clipboard;
    QMap<int, ModifierKey> m_modifiers;
#ifdef HAVE_WAYLAND
    std::unique_ptr<ShortcutInhibition> m_inhibition;
#endif

#ifndef QTONLY
    QString readWalletPassword(bool fromUserNameOnly = false);
    void saveWalletPassword(const QString &password, bool fromUserNameOnly = false);
    void deleteWalletPassword(bool fromUserNameOnly = false);
    QString readWalletPasswordForKey(const QString &key);
    void saveWalletPasswordForKey(const QString &key, const QString &password);
    void deleteWalletPasswordForKey(const QString &key);
    KWallet::Wallet *m_wallet;
#endif

    LocalCursorState m_localCursorState;

#ifdef USE_SSH_TUNNEL
private:
    SshTunnelThread *m_sshTunnelThread;

    QString readWalletSshPassword();
    void saveWalletSshPassword();

private Q_SLOTS:
    void sshRequestPassword(SshTunnelThread::PasswordRequestFlags flags);
    void sshErrorMessage(const QString &message);
#endif
};

#endif
