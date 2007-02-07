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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef MAINDIALOGWIDGET_H
#define MAINDIALOGWIDGET_H

#include "kservicelocator.h"
#include "maindialogbase.h"
#include <dnssd/servicebrowser.h>
#include <dnssd/remoteservice.h>
#include "smartptr.h"

class MainDialogWidget : public MainDialogBase
{
  Q_OBJECT

  public:
    MainDialogWidget( QWidget *parent, const char *name );
    ~MainDialogWidget();

    void setRemoteHost( const QString & );
    QString remoteHost();
    void save();

  protected:
    void enableBrowsingArea( bool );
    bool ensureLocatorOpen();
    void errorScanning();
    void finishScanning();

  signals:
    void hostValid( bool b );
    void accept();

  protected slots:
    void hostChanged( const QString & );
    void toggleBrowsingArea();
    void itemSelected( QListViewItem * );
    void itemDoubleClicked( QListViewItem * );
    void scopeSelected( const QString & );
    void rescan();

    void foundService( QString url, int );
    void lastSignalServices( bool success );
    void foundScopes( QStringList scopeList );
    void addedService( DNSSD::RemoteService::Ptr );
    void removedService( DNSSD::RemoteService::Ptr );


  protected:
    QString m_scope;
    bool m_scanning;
    SmartPtr<KServiceLocator> m_locator;
    DNSSD::ServiceBrowser *m_locator_dnssd;
};

#endif // MAINDIALOGWIDGET_H
