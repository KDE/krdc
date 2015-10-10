/****************************************************************************
**
** Copyright (C) 2009 Urs Wolfer <uwolfer @ kde.org>
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

#ifndef KONSOLEVIEW_H
#define KONSOLEVIEW_H

#include "konsoleview.h"

#include "remoteview.h"
#include "hostpreferences.h"

#include <KConfigGroup>

class KonsoleHostPreferences;
class TerminalInterface;

class KonsoleView : public RemoteView
{
    Q_OBJECT

public:
    explicit KonsoleView(QWidget *parent = 0, const QUrl &url = QUrl(), KConfigGroup configGroup = KConfigGroup());

    virtual ~KonsoleView();

    virtual QSize framebufferSize();
    QSize sizeHint() const;

    virtual bool isQuitting();
    virtual bool start();
    HostPreferences* hostPreferences();

public Q_SLOTS:
    virtual void switchFullscreen(bool on);

protected:
    bool eventFilter(QObject *obj, QEvent *event);

private:
    KonsoleHostPreferences *m_hostPreferences;
    TerminalInterface* m_terminal;
    QWidget *m_terminalWidget;
};


class KonsoleHostPreferences : public HostPreferences
{
    Q_OBJECT
public:
    explicit KonsoleHostPreferences(KConfigGroup configGroup, QObject *parent = 0)
        : HostPreferences(configGroup, parent) {}

protected:
    virtual QWidget* createProtocolSpecificConfigPage() { return 0; };
};

#endif // KONSOLEVIEW_H
