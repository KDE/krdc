/*
    SPDX-FileCopyrightText: 2007-2012 Urs Wolfer <uwolfer@kde.org>
    SPDX-FileCopyrightText: 2012 AceLan Kao <acelan@acelan.idv.tw>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef RDPHOSTPREFERENCES_H
#define RDPHOSTPREFERENCES_H

#include "hostpreferences.h"
#include "ui_rdppreferences.h"

#include <QFileDialog>

class RdpHostPreferences : public HostPreferences
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

    enum class Sound {
        Local,
        Remote,
        Disabled,
    };

    enum class Acceleration {
        Auto,
        ForceGraphicsPipeline,
        ForceRemoteFx,
        Disabled,
    };

    explicit RdpHostPreferences(KConfigGroup configGroup, QObject *parent = nullptr);
    ~RdpHostPreferences() override;

    Resolution resolution() const;
    void setResolution(Resolution resolution);

    int colorDepth() const;
    void setColorDepth(int colorDepth);

    QString keyboardLayout() const;
    void setKeyboardLayout(const QString &keyboardLayout);

    Sound sound() const;
    void setSound(Sound sound);

    Acceleration acceleration() const;
    void setAcceleration(Acceleration acceleration);

    void setConsole(bool console);
    bool console() const;
    void setPerformance(int performance);
    int performance() const;

    void setShareMedia(const QString &shareMedia);
    QString shareMedia() const;

protected:
    QWidget* createProtocolSpecificConfigPage() override;
    void acceptConfig() override;

private:
    void updateWidthHeight(Resolution resolution);

    Ui::RdpPreferences rdpUi;
};

#endif
