/****************************************************************************
**
** Copyright (C) 2008 Urs Wolfer <uwolfer @ kde.org>
**
** This file is part of KDE.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; see the file COPYING. If not, write to
** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
** Boston, MA 02110-1301, USA.
**
****************************************************************************/

#ifndef TESTVIEW_H
#define TESTVIEW_H

#include "remoteview.h"
#include "hostpreferences.h"

#include <KConfigCore/KConfigGroup>

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
    HostPreferences* hostPreferences() override;

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
        : HostPreferences(configGroup, parent) {}

protected:
    QWidget* createProtocolSpecificConfigPage() override { return nullptr; };
};

#endif // TESTVIEW_H
