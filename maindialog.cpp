/* This file is part of the KDE project
   Copyright (C) 2003-2004 Nadeem Hasan <nhasan@kde.org>

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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include "maindialog.h"
#include "maindialogwidget.h"
#include "preferencesdialog.h"

#include <kapplication.h>
#include <kguiitem.h>
#include <klocale.h>

MainDialog::MainDialog( QWidget *parent, const char *name )
    : KDialogBase( parent, name, true, i18n( "Remote Desktop Connection" ),
      Ok|Close|Help|User1, Ok, true, KGuiItem( i18n( "&Preferences..." ),
      "configure" ) )
{
  m_dialogWidget = new MainDialogWidget( this, "m_dialogWidget" );
  setMainWidget( m_dialogWidget );

  setButtonText( Ok, i18n( "Connect" ) );
  enableButtonOK( false );

  connect( m_dialogWidget, SIGNAL( hostValid( bool ) ),
                           SLOT( enableButtonOK( bool ) ) );
}

void MainDialog::setRemoteHost( const QString &host )
{
  m_dialogWidget->setRemoteHost( host );
}

QString MainDialog::remoteHost()
{
  return m_dialogWidget->remoteHost();
}

void MainDialog::slotHelp()
{
  kapp->invokeHelp();
}

void MainDialog::slotUser1()
{
  PreferencesDialog p( this );
  p.exec();
}

void MainDialog::slotOk()
{
  m_dialogWidget->save();

  KDialogBase::accept();
}

void MainDialog::slotClose()
{
  m_dialogWidget->save();

  KDialogBase::reject();
}
