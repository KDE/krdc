/*
    SPDX-FileCopyrightText: 2007 Urs Wolfer <uwolfer@kde.org>
    SPDX-FileCopyrightText: 2021 Rafał Lalik <rafallalik@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef HOSTPREFERENCES_H
#define HOSTPREFERENCES_H

#include "krdccore_export.h"
#include "remoteview.h"

#include <KConfigGroup>

class QCheckBox;
class QWidget;

#ifdef USE_SSH_TUNNEL
class SshTunnelWidget;
#endif

class KRDCCORE_EXPORT HostPreferences : public QObject
{
    Q_OBJECT

public:
    ~HostPreferences() override;

    KConfigGroup configGroup();

    bool walletSupport() const;

    /** Whether scaling is enabled when session is full screen. Note: only windowedScale seems to be used. */
    bool fullscreenScale() const;
    void setFullscreenScale(bool scale);

    /** Whether scaling is enabled when session is not full screen */
    bool windowedScale() const;
    void setWindowedScale(bool scale);

    /** Value from 100->200 (translates to 1x->2x) which scales the view. This only has an effect if scaling is enabled. */
    int scaleFactor() const;
    void setScaleFactor(int factor);

    bool grabAllKeys() const;
    void setGrabAllKeys(bool grab);

    bool showLocalCursor() const;
    void setShowLocalCursor(bool show);

    bool viewOnly() const;
    void setViewOnly(bool view);

    bool clipboardSharing() const;
    void setClipboardSharing(bool share);

    /** Saved height. Generally used for the viewsize. */
    int height() const;
    void setHeight(int height);
    /** Saved width. Generally used for the viewsize. */
    int width() const;
    void setWidth(int width);

    /**
     * Show the configuration dialog if needed, ie. if the user did
     * check "show this dialog again for this host".
     * Returns true if user pressed ok.
     */
    bool showDialogIfNeeded(QWidget *parent);

    /** Show the configuration dialog */
    bool showDialog(QWidget *parent);

    /** If @p connected is true, a message is shown that settings might only apply after a reconnect. */
    void setShownWhileConnected(bool connected);

#ifdef USE_SSH_TUNNEL
    bool useSshTunnel() const;
    bool useSshTunnelLoopback() const;
    int sshTunnelPort() const;
    QString sshTunnelUserName() const;
#endif

protected:
    HostPreferences(KConfigGroup configGroup, QObject *parent);

    virtual QWidget *createProtocolSpecificConfigPage(QWidget *sshTunnelWidget) = 0;

    /** Called when the user validates the config dialog. */
    virtual void acceptConfig();

    bool hostConfigured() const;
    bool showConfigAgain() const;

    KConfigGroup m_configGroup;

private:
    void setShowConfigAgain(bool show);
    void setWalletSupport(bool walletSupport);

#ifdef USE_SSH_TUNNEL
    void setUseSshTunnel(bool useSshTunnel);
    void setUseSshTunnelLoopback(bool useSshTunnelLoopback);
    void setSshTunnelPort(int port);
    void setSshTunnelUserName(const QString &userName);

    SshTunnelWidget *sshTunnelWidget;
#endif

    bool m_hostConfigured;
    bool m_connected;

    QCheckBox *showAgainCheckBox;
    QCheckBox *walletSupportCheckBox;
};

#endif
