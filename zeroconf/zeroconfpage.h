/****************************************************************************
**
** Copyright (C) 2008 Magnus Romnes <romnes @ stud.ntnu.no>
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

#ifndef ZEROCONFPAGE_H
#define ZEROCONFPAGE_H

#include <QTableView>

#include <KUrl>
#include <KPushButton>

#include <dnssd/servicebrowser.h>

class ZeroconfPage : public QWidget
{
    Q_OBJECT

public:
    ZeroconfPage(QWidget *parent = 0);
    ~ZeroconfPage();

public slots:
    void rowSelected();
    void newConnection();
    void serviceSelected(const QModelIndex& idx);

private:
    DNSSD::ServiceBrowser* m_browser;
    QTableView* m_servicesView;
    KPushButton* m_connectButton;
    QHash<QString, QString> m_protocols;

signals:
    void newConnection(const KUrl, bool);
    void closeZeroconfPage();
};
#endif
