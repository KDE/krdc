/*
    SPDX-FileCopyrightText: 2024 KRDC Contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SPICEHOSTPREFERENCES_H
#define SPICEHOSTPREFERENCES_H

#include "hostpreferences.h"
#include "ui_spicepreferences.h"

class SpiceHostPreferences : public HostPreferences
{
    Q_OBJECT

public:
    enum class Resolution {
        Small,
        Medium,
        Large,
        MatchWindow,
        MatchScreen,
        Custom,
    };

    explicit SpiceHostPreferences(KConfigGroup configGroup, QObject *parent = nullptr);
    ~SpiceHostPreferences() override;

    /** Whether to enable audio playback from the remote host. */
    bool enableAudio() const;
    void setEnableAudio(bool enable);

    Resolution resolution() const;
    void setResolution(Resolution resolution);

protected:
    void acceptConfig() override;

    QWidget *createProtocolSpecificConfigPage(QWidget *sshTunnelWidget) override;

private:
    void updateWidthHeight(Resolution resolution);

    Ui::SpicePreferences spiceUi;
};

#endif // SPICEHOSTPREFERENCES_H
