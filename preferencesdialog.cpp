/* This file is part of the KDE project
   Copyright (C) 2002-2003 Nadeem Hasan <nhasan@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "preferencesdialog.h"

#include "hostprofiles.h"
#include "vnc/vncprefs.h"
#include "rdp/rdpprefs.h"

#include <kvbox.h>

#include <klocale.h>

PreferencesDialog::PreferencesDialog( QWidget *parent, const char *name )
    : KPageDialog( parent )
{
  setObjectName( name );
  setFaceType( KPageDialog::Tabbed );
  setCaption( i18n( "Preferences" ) );
  setButtons( Ok | Cancel );
  setDefaultButton( Ok );
  showButtonSeparator( true );

  KVBox *page;
  page = new KVBox();
  addPage( page, i18n( "&Host Profiles" ) );

  m_hostProfiles = new HostProfiles( page );
  m_hostProfiles->setObjectName( "m_hostProfiles" );

  page = new KVBox();
  addPage( page, i18n( "&VNC Defaults" ) );
  m_vncPrefs = new VncPrefs( page );
  m_vncPrefs->setObjectName( "m_vncPrefs" );

  QWidget *spacer = new QWidget( page );

  page->setStretchFactor( spacer, 10 );

  m_vncPrefs->cbShowPrefs->setText( i18n( "Do not &show the preferences "
                                          "dialog on new connections" ) );

  page = new KVBox();
  addPage( page, i18n( "RD&P Defaults" ) );
  m_rdpPrefs = new RdpPrefs( page );
  m_rdpPrefs->setObjectName( "m_rdpPrefs" );
  spacer = new QWidget( page );
  page->setStretchFactor( spacer, 10 );

  m_rdpPrefs->cbShowPrefs->setText( i18n( "Do not &show the preferences "
                                          "dialog on new connections" ) );

  HostPreferences *hp = HostPreferences::instance();
  m_vncDefaults = SmartPtr<VncHostPref>( hp->vncDefaults() );
  m_rdpDefaults = SmartPtr<RdpHostPref>( hp->rdpDefaults() );

  load();
}

void PreferencesDialog::load()
{
  m_hostProfiles->load();

  m_vncPrefs->setQuality( m_vncDefaults->quality() );
  m_vncPrefs->setShowPrefs( m_vncDefaults->askOnConnect() );
  m_vncPrefs->setUseKWallet( m_vncDefaults->useKWallet() );

  m_rdpPrefs->setRdpWidth( m_rdpDefaults->width() );
  m_rdpPrefs->setRdpHeight( m_rdpDefaults->height() );
  m_rdpPrefs->setShowPrefs( m_rdpDefaults->askOnConnect() );
  m_rdpPrefs->setUseKWallet( m_rdpDefaults->useKWallet() );
  m_rdpPrefs->setColorDepth( m_rdpDefaults->colorDepth() );
  m_rdpPrefs->setKbLayout( keymap2int( m_rdpDefaults->layout() ));
  m_rdpPrefs->setResolution();
}

void PreferencesDialog::save()
{
  m_hostProfiles->save();

  m_vncDefaults->setQuality( m_vncPrefs->quality() );
  m_vncDefaults->setAskOnConnect(  m_vncPrefs->showPrefs() );
  m_vncDefaults->setUseKWallet( m_vncPrefs->useKWallet() );

  m_rdpDefaults->setWidth( m_rdpPrefs->rdpWidth() );
  m_rdpDefaults->setHeight( m_rdpPrefs->rdpHeight() );
  m_rdpDefaults->setLayout( int2keymap( m_rdpPrefs->kbLayout() ));
  m_rdpDefaults->setAskOnConnect( m_rdpPrefs->showPrefs() );
  m_rdpDefaults->setUseKWallet( m_rdpPrefs->useKWallet() );
  m_rdpDefaults->setColorDepth( m_rdpPrefs->colorDepth() );

  HostPreferences *hp = HostPreferences::instance();
  hp->sync();
}

void PreferencesDialog::slotOk()
{
  save();
  accept();
}

#include "preferencesdialog.moc"
