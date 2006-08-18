/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "vncprefs.h"

VncPrefs::VncPrefs(QWidget *parent) : QWidget( parent )
{
  setupUi( this );
}

void VncPrefs::setQuality( int quality )
{
  cmbQuality->setCurrentIndex(quality);
}


int VncPrefs::quality()
{
  return cmbQuality->currentIndex();
}


void VncPrefs::setShowPrefs( bool b )
{
  cbShowPrefs->setChecked(b);
}


int VncPrefs::showPrefs()
{
  return cbShowPrefs->isChecked();
}


void VncPrefs::setUseEncryption( bool b )
{
  cbUseEncryption->setChecked(b);
}

bool VncPrefs::useEncryption()
{
  return cbUseEncryption->isChecked();
}

void VncPrefs::setUseKWallet( bool b )
{
  cbUseKWallet->setChecked(b);
}

bool VncPrefs::useKWallet()
{
  return cbUseKWallet->isChecked();
}

#include "vncprefs.moc"

