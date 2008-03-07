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

#include "zeroconfpage.h"

#include <QLabel>
#include <QLayout>
#include <QHeaderView>

#include <KIcon>
#include <KLocale>

ZeroconfPage::ZeroconfPage(QWidget* parent):
        QWidget(parent),
        m_browser(0),
        m_numServices(0),
        m_selectedHost(0),
        m_serviceTable(0),
        m_connectButton(0)
{
    // Add RDP and NX if they start announcing via Zeroconf:
    m_protocols["_rfb._tcp"] = "vnc";

    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Base);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QHBoxLayout *headerLayout = new QHBoxLayout;
    headerLayout->setMargin(20);

    // Header
    QLabel *headerLabel = new QLabel(this);
    QString headerString = i18n("<h1>Remote Desktop Services Found on Your Local Network:</h1><br />");
    headerLabel->setWordWrap(true);
    headerLayout->addWidget(headerLabel, 1, Qt::AlignTop);

    QLabel *headerIconLabel = new QLabel(this);
    headerIconLabel->setPixmap(KIcon("edit-find").pixmap(64));
    headerLayout->addWidget(headerIconLabel);

    mainLayout->addLayout(headerLayout);

    // Content
    QHBoxLayout* contentLayout = new QHBoxLayout;
    contentLayout->setMargin(20);

    m_serviceTable = new QTableWidget(0, 4, this);
    m_serviceTable->setShowGrid(false);
    m_serviceTable->setHorizontalHeaderItem(0, new QTableWidgetItem(i18n("Type")));
    m_serviceTable->setHorizontalHeaderItem(1, new QTableWidgetItem(i18n("Host")));
    m_serviceTable->setHorizontalHeaderItem(2, new QTableWidgetItem(i18n("Port")));
    m_serviceTable->setHorizontalHeaderItem(3, new QTableWidgetItem(i18n("Description")));
    m_serviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_serviceTable->setTabKeyNavigation(true);
    m_serviceTable->setAlternatingRowColors(true);
    m_serviceTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_serviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_serviceTable->setSortingEnabled(false);
    m_serviceTable->setEnabled(false);
    m_serviceTable->setCornerButtonEnabled(false);
    m_serviceTable->verticalHeader()->hide();
    m_serviceTable->horizontalHeader()->setStretchLastSection(true);
    contentLayout->addWidget(m_serviceTable);

    // Buttons:
    QVBoxLayout *buttonLayout = new QVBoxLayout;

    m_connectButton = new KPushButton(this);
    m_connectButton->setIcon(KIcon("network-connect"));
    m_connectButton->setText(i18n("Connect"));
    m_connectButton->setEnabled(false);
    buttonLayout->addWidget(m_connectButton);

    KPushButton *closeTabButton = new KPushButton(this);
    closeTabButton->setIcon(KIcon("tab-close"));
    closeTabButton->setText(i18n("Close Tab"));
    buttonLayout->addWidget(closeTabButton);
    buttonLayout->addStretch();
    contentLayout->addLayout(buttonLayout);
    mainLayout->addLayout(contentLayout);

    connect(closeTabButton, SIGNAL(clicked()), this, SIGNAL(closeZeroconfPage()));

    if(DNSSD::ServiceBrowser::isAvailable() == DNSSD::ServiceBrowser::Working)
    {
        headerString.append(i18n("<b>Note:</b> KRDC can only locate Remote Desktop Services that announce themself using Zeroconf (also known as Bonjour)."));
        m_serviceTable->setEnabled(true);
        connect(m_connectButton, SIGNAL(clicked()), this, SLOT(newConnection()));
        connect(m_serviceTable, SIGNAL(itemSelectionChanged()), this, SLOT(rowSelected()));
        connect(m_serviceTable, SIGNAL(itemDoubleClicked(QTableWidgetItem*)), this, SLOT(newConnection()));
        startBrowse();
    }
    else
    {
        headerString.append(i18n("<b>Error:</b> No active Zeroconf daemon could be detected on your computer. Please start Avahi or mDNSd if you want to browse Remote Desktop Services on your Local Network."));
    }
    headerLabel->setText(headerString);
}

ZeroconfPage::~ZeroconfPage()
{
}

void ZeroconfPage::startBrowse()
{
    // Something has to be done here if we are to browse for more than one service (like adding RDP and NX)
    m_browser = new DNSSD::ServiceBrowser("_rfb._tcp", true);
    connect(m_browser, SIGNAL(serviceAdded(DNSSD::RemoteService::Ptr)), this, SLOT(addService(DNSSD::RemoteService::Ptr)));
    m_browser->startBrowse();
}

void ZeroconfPage::addService(DNSSD::RemoteService::Ptr rs)
{
    if (rs->resolve()) {
        m_numServices++;
        m_serviceTable->setRowCount(m_numServices);
        m_serviceTable->setItem(m_numServices - 1, 0, new QTableWidgetItem(m_protocols[rs->type()].toUpper()));
        m_serviceTable->setItem(m_numServices - 1, 1, new QTableWidgetItem(rs->hostName()));
        m_serviceTable->setItem(m_numServices - 1, 2, new QTableWidgetItem(QString().setNum(rs->port())));
        m_serviceTable->setItem(m_numServices - 1, 3, new QTableWidgetItem(rs->serviceName()));
    }
}

void ZeroconfPage::rowSelected()
{
    m_connectButton->setEnabled(true);
}

void ZeroconfPage::newConnection()
{
    int currentRow = m_serviceTable->currentRow();
    KUrl url;
    url.setProtocol(m_serviceTable->item(currentRow, 0)->text().toLower());
    url.setHost(m_serviceTable->item(currentRow, 1)->text());
    url.setPort(m_serviceTable->item(currentRow, 2)->text().toInt());
    emit newConnection(url, false);
}
