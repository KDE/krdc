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
    HostPreferences *hostPreferences() override;

protected:
    bool startConnection() override;
    void startQuittingConnection() override;

    void handleKeyEvent(QKeyEvent *event) override;
    void handleWheelEvent(QWheelEvent *event) override;
    void handleMouseEvent(QMouseEvent *event) override;
    void handleLocalClipboardChanged(const QMimeData *data) override;

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
    QWidget *createProtocolSpecificConfigPage(QWidget *) override
    {
        return nullptr;
    };
};

#endif // TESTVIEW_H
