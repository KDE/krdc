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

#include "zeroconfpage.h"

#include <QLabel>
#include <QLayout>
#include <QHeaderView>

#include <KIcon>
#include <KLocale>

#include <dnssd/servicemodel.h>

ZeroconfPage::ZeroconfPage(QWidget* parent):
        QWidget(parent),
        m_browser(0),
        m_servicesView(0),
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

    m_servicesView = new QTableView(this);
    m_servicesView->setShowGrid(false);
    m_servicesView->setSortingEnabled(false);
    m_servicesView->setAlternatingRowColors(true);
    m_servicesView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_servicesView->setEnabled(false);
    m_servicesView->setCornerButtonEnabled(false);
    m_servicesView->verticalHeader()->hide();
    m_servicesView->horizontalHeader()->setStretchLastSection(true);
    DNSSD::ServiceModel *mdl = new DNSSD::ServiceModel(new DNSSD::ServiceBrowser("_rfb._tcp", true), m_servicesView);
    m_servicesView->setModel(mdl);
    m_servicesView->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(m_servicesView->selectionModel(), SIGNAL(currentRowChanged(const QModelIndex&, const QModelIndex&)), SLOT(rowSelected()));
    connect(m_servicesView, SIGNAL(doubleClicked(const QModelIndex&)), SLOT(serviceSelected(const QModelIndex&)));
    contentLayout->addWidget(m_servicesView);

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
        m_servicesView->setEnabled(true);
        connect(m_connectButton, SIGNAL(clicked()), this, SLOT(newConnection()));
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

void ZeroconfPage::serviceSelected(const QModelIndex& idx) 
{
    DNSSD::RemoteService::Ptr srv = idx.data(DNSSD::ServiceModel::ServicePtrRole).value<DNSSD::RemoteService::Ptr>();
    if (srv && srv->resolve()) {
        KUrl url;
        url.setProtocol(m_protocols[srv->type()].toLower());
        url.setHost(srv->hostName());
        url.setPort(srv->port());

        emit newConnection(url, false);
    }
}

void ZeroconfPage::rowSelected()
{
    m_connectButton->setEnabled(true);
}

void ZeroconfPage::newConnection()
{
    serviceSelected(m_servicesView->selectionModel()->selectedRows().at(0));
}
