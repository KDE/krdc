/* This file is part of the KDE project
   Copyright (C) 2002-2003 Tim Jansen <tim@tjansen.de>
   Copyright (C) 2003-2004 Nadeem Hasan <nhasan@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (  at your option ) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef MAINDIALOGWIDGET_H
#define MAINDIALOGWIDGET_H

#include "kservicelocator.h"
#include <dnssd/servicebrowser.h>
#include <dnssd/remoteservice.h>
#include "smartptr.h"
#include "ui_maindialogbase.h"

class MainDialogWidget : public QWidget, public Ui::MainDialogBase
{
  Q_OBJECT

  public:
    MainDialogWidget( QWidget *parent );
    ~MainDialogWidget() {}

    void setRemoteHost( const QString & );
    QString remoteHost();
    void save();

  protected:
    void enableBrowsingArea( bool enable );
    bool ensureLocatorOpen();
    void errorScanning();
    void finishScanning();

  signals:
    void hostValid( bool b );
    void accept();

  protected slots:
    void hostChanged( const QString & text);
    void toggleBrowsingArea();
    void itemSelected( Q3ListViewItem * item );
    void itemDoubleClicked( Q3ListViewItem * item );
    void scopeSelected( const QString & scope);
    void rescan();

    void foundService( QString url, int );
    void lastSignalServices( bool success );
    void foundScopes( QStringList scopeList );
    void addedService( DNSSD::RemoteService::Ptr );
    void removedService( DNSSD::RemoteService::Ptr );
    void languageChange();

    void exampleWhatsThis(const QString & link);

  protected:
    QString m_scope;
    bool m_scanning;
    SmartPtr<KServiceLocator> m_locator;
    DNSSD::ServiceBrowser *m_locator_dnssd_rfb;
    DNSSD::ServiceBrowser *m_locator_dnssd_vnc;
};

#endif // MAINDIALOGWIDGET_H
