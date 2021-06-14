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
    explicit RdpHostPreferences(KConfigGroup configGroup, QObject *parent = nullptr);
    ~RdpHostPreferences() override;

    void setResolution(int resolution);
    int resolution() const;
    void setColorDepth(int colorDepth);
    int colorDepth() const;
    void setKeyboardLayout(const QString &keyboardLayout);
    QString keyboardLayout() const;
    void setSound(int sound);
    int sound() const;
    void setSoundSystem(int sound);
    int soundSystem() const;
    void setConsole(bool console);
    bool console() const;
    void setExtraOptions(const QString &extraOptions);
    QString extraOptions() const;
    void setRemoteFX(bool remoteFX);
    bool remoteFX() const;
    void setPerformance(int performance);
    int performance() const;
    void setShareMedia(const QString &shareMedia);
    QString shareMedia() const;

protected:
    QWidget* createProtocolSpecificConfigPage() override;
    void acceptConfig() override;

private:
    Ui::RdpPreferences rdpUi;

private Q_SLOTS:
    void updateWidthHeight(int index);
    void updateSoundSystem(int index);
    void browseMedia();
};

#endif
