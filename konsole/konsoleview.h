/*
    SPDX-FileCopyrightText: 2009 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
