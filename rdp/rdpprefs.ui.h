/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you wish to add, delete or rename functions or slots use
** Qt Designer which will update this file, preserving your code. Create an
** init() function in place of a constructor, and a destroy() function in
** place of a destructor.
*****************************************************************************/


void RdpPrefs::resolutionChanged( int selection )
{
#warning including ui.h
  bool enable = (selection==5);
  spinWidth->setEnabled(enable);
  spinHeight->setEnabled(enable);
  widthLabel->setEnabled(enable);
  heightLabel->setEnabled(enable);

  switch(selection)
  {
    case 0:
      spinWidth->setValue(640);
      spinHeight->setValue(480);
      break;

    case 1:
      spinWidth->setValue(800);
      spinHeight->setValue(600);
      break;

    case 2:
      spinWidth->setValue(1024);
      spinHeight->setValue(768);
      break;

    case 3:
      spinWidth->setValue(1280);
      spinHeight->setValue(1024);
      break;

    case 4:
      spinWidth->setValue(1600);
      spinHeight->setValue(1200);
      break;

    case 5:
    default:
      break;
  }
}


void RdpPrefs::setRdpWidth( int w )
{
  spinWidth->setValue(w);
}


int RdpPrefs::rdpWidth()
{
  return spinWidth->value();
}


void RdpPrefs::setRdpHeight( int h )
{
  spinHeight->setValue(h);
}


int RdpPrefs::rdpHeight()
{
  return spinHeight->value();
}


int RdpPrefs::colorDepth()
{
  qDebug("current depth: %i", cmbColorDepth->currentItem() );
  switch (cmbColorDepth->currentItem())
  {
    case 0:
      return 8;
    case 1:
      return 16;
    case 2:
      return 24;
    default:
      // shouldn't happen, but who knows..
      return 8;
    break;	
  }
}


void RdpPrefs::setColorDepth(int depth)
{
  switch (depth)
  {
    case 8:
      cmbColorDepth->setCurrentItem(0);
      break;   
    case 16:
      cmbColorDepth->setCurrentItem(1);
      break;   
    case 24:
      cmbColorDepth->setCurrentItem(2);
      break;   
    default:
      break;	
  }    
}

void RdpPrefs::setResolution()
{
  if (rdpWidth()==640 && rdpHeight()==480)
  {
    cmbResolution->setCurrentItem(0);
  }
  else if (rdpWidth()==800 && rdpHeight()==600)
  {
    cmbResolution->setCurrentItem(1);
  }
  else if (rdpWidth()==1024 && rdpHeight()==768)
  {
    cmbResolution->setCurrentItem(2);
  }
  else if (rdpWidth()==1280 && rdpHeight()==1024)
  {
    cmbResolution->setCurrentItem(3);
  }
  else if (rdpWidth()==1600 && rdpHeight()==1200)
  {
    cmbResolution->setCurrentItem(4);
  }
  else
  {
    cmbResolution->setCurrentItem(5);
  }
  resolutionChanged( cmbResolution->currentItem() );
}


int RdpPrefs::resolution()
{
  return cmbResolution->currentItem();
}


void RdpPrefs::setKbLayout( int i )
{
  cmbKbLayout->setCurrentItem( i );
}


int RdpPrefs::kbLayout()
{
  return cmbKbLayout->currentItem();
}


void RdpPrefs::setShowPrefs( bool b )
{
  cbShowPrefs->setChecked( b );
}


bool RdpPrefs::showPrefs()
{
  return cbShowPrefs->isChecked();
}

void RdpPrefs::setUseKWallet( bool b )
{
  cbUseKWallet->setChecked(b);
}

bool RdpPrefs::useKWallet()
{
  return cbUseKWallet->isChecked();
}
