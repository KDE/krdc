/****************************************************************************
**
** Copyright (C) 2008 Magnus Romnes <romnes @ stud.ntnu.no>
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

#ifndef ZEROCONFPAGE_H
#define ZEROCONFPAGE_H

#include <QWidget>
#include <QTableWidget>
#include <QHash>

#include <KUrl>
#include <KPushButton>

#include <dnssd/servicebrowser.h>
#include <dnssd/remoteservice.h>

class ZeroconfPage : public QWidget
{
    Q_OBJECT

public:
    ZeroconfPage(QWidget *parent = 0);
    ~ZeroconfPage();
    void startBrowse();

public slots:
    void addService(DNSSD::RemoteService::Ptr);
    void rowSelected();
    void newConnection();

private:
    DNSSD::ServiceBrowser* m_browser;
    int m_numServices;
    QString m_selectedHost;
    QTableWidget* m_serviceTable;
    KPushButton* m_connectButton;
    QHash<QString, QString> m_protocols;

signals:
    void newConnection(const KUrl, bool);
    void closeZeroconfPage();
};
#endif
