/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you wish to add, delete or rename functions or slots use
** Qt Designer which will update this file, preserving your code. Create an
** init() function in place of a constructor, and a destroy() function in
** place of a destructor.
*****************************************************************************/

void VncPrefs::setQuality( int quality )
{
  cmbQuality->setCurrentItem(quality);
}


int VncPrefs::quality()
{
  return cmbQuality->currentItem();
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

