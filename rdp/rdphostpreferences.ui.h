/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you wish to add, delete or rename functions or slots use
** Qt Designer which will update this file, preserving your code. Create an
** init() function in place of a constructor, and a destroy() function in
** place of a destructor.
*****************************************************************************/

void RdpHostPreferences::resolutionChanged(int selection)
{
	switch(selection)
	{
		case 0:
			rdpWidthSpin->setEnabled(false);
			rdpHeightSpin->setEnabled(false);
			rdpWidthSpin->setValue(640);
			rdpHeightSpin->setValue(480);
			break;

		case 1:
			rdpWidthSpin->setEnabled(false);
			rdpHeightSpin->setEnabled(false);
			rdpWidthSpin->setValue(800);
			rdpHeightSpin->setValue(600);
			break;
	    
		case 2:
			rdpWidthSpin->setEnabled(false);
			rdpHeightSpin->setEnabled(false);
			rdpWidthSpin->setValue(1024);
			rdpHeightSpin->setValue(768);
			break;

		case 3:
			rdpWidthSpin->setEnabled(true);
			rdpHeightSpin->setEnabled(true);
	}
}
