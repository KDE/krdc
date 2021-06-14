/*
    SPDX-FileCopyrightText: 2007 Urs Wolfer <uwolfer@kde.org>
    SPDX-FileCopyrightText: 2021 Rafał Lalik <rafallalik@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef HOSTPREFERENCES_H
#define HOSTPREFERENCES_H

#include "remoteview.h"
#include "krdccore_export.h"

#include <KConfigGroup>

class QCheckBox;
class QWidget;

class KRDCCORE_EXPORT HostPreferences : public QObject
{
    Q_OBJECT

public:
    ~HostPreferences() override;

    KConfigGroup configGroup();

    bool walletSupport();

    /** Whether scaling is enabled when session is full screen. Note: only windowedScale seems to be used. */
    bool fullscreenScale();
    void setFullscreenScale(bool scale);

    /** Whether scaling is enabled when session is not full screen */
    bool windowedScale();
    void setWindowedScale(bool scale);

    /** Whether scaling is enabled when session is full screen. Note: only windowedScale seems to be used. */
    int scaleFactor();
    void setScaleFactor(int factor);

    bool grabAllKeys();
    void setGrabAllKeys(bool grab);

    bool showLocalCursor();
    void setShowLocalCursor(bool show);

    bool viewOnly();
    void setViewOnly(bool view);

    /** Saved height. Generally used for the viewsize. */
    int height();
    void setHeight(int height);
    /** Saved width. Generally used for the viewsize. */
    int width();
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

protected:
    HostPreferences(KConfigGroup configGroup, QObject *parent);

    virtual QWidget* createProtocolSpecificConfigPage() = 0;

    /** Called when the user validates the config dialog. */
    virtual void acceptConfig();

    bool hostConfigured();
    bool showConfigAgain();

    KConfigGroup m_configGroup;

private:
    void setShowConfigAgain(bool show);
    void setWalletSupport(bool walletSupport);

    bool m_hostConfigured;
    bool m_connected;

    QCheckBox *showAgainCheckBox;
    QCheckBox *walletSupportCheckBox;
};

#endif
