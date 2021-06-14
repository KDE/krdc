/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SYSTEMTRAYICON_H
#define SYSTEMTRAYICON_H

#include <KStatusNotifierItem>

class MainWindow;

class SystemTrayIcon : public KStatusNotifierItem
{
    Q_OBJECT

public:
    explicit SystemTrayIcon(MainWindow *parent);
    ~SystemTrayIcon() override;

public Q_SLOTS:
    void checkActivatedWindow(bool active);

private:
    MainWindow *m_mainWindow;
};

#endif
