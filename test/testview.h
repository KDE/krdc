/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TESTVIEW_H
#define TESTVIEW_H

#include "hostpreferences.h"
#include "remoteview.h"

#include <KConfigGroup>

class TestHostPreferences;

class TestView : public RemoteView
{
    Q_OBJECT

public:
    explicit TestView(QWidget *parent = nullptr, const QUrl &url = QUrl(), KConfigGroup configGroup = KConfigGroup());

    ~TestView() override;

    QSize framebufferSize() override;
    QSize sizeHint() const override;

    bool isQuitting() override;
    bool start() override;
    HostPreferences *hostPreferences() override;

public Q_SLOTS:
    void switchFullscreen(bool on) override;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    TestHostPreferences *m_hostPreferences;

private Q_SLOTS:
    void asyncConnect();
};

class TestHostPreferences : public HostPreferences
{
    Q_OBJECT
public:
    explicit TestHostPreferences(KConfigGroup configGroup, QObject *parent = nullptr)
        : HostPreferences(configGroup, parent)
    {
    }

protected:
    QWidget *createProtocolSpecificConfigPage() override
    {
        return nullptr;
    };
};

#endif // TESTVIEW_H
