#include <qregexp.h>
#include <kdebug.h>
#include <kapplication.h>
#include <config.h>
#include "preferences.h"

void MainDialog::hostChanged( const QString & text )
{
    connectButton->setEnabled(text.contains(QRegExp(":[0-9]+$")) ||
			      text.contains(QRegExp("^vnc:/.+")) ||
			      text.contains(QRegExp("^rdp:/.+")));
}


void MainDialog::toggleBrowsingArea()
{
    enableBrowsingArea(!browsingPanel->isVisible());
}


void MainDialog::rescan()
{
}


void MainDialog::enableBrowsingArea( bool enable )
{
    int hOffset = 0;
    if (enable) {
	browsingPanel->show();
	browsingPanel->setMaximumSize(1000, 1000);
	browsingPanel->setEnabled(true);
	browseButton->setText(browseButton->text().replace(QRegExp(">>"), "<<"));
    } else {
        hOffset = browsingPanel->height();
	browsingPanel->hide();
	browsingPanel->setMaximumSize(0, 0);
	browsingPanel->setEnabled(false);
	browseButton->setText(browseButton->text().replace(QRegExp("<<"), ">>"));
#ifndef HAVE_SLP
	browseButton->hide();
#endif
	int h = minimumSize().height()-hOffset;
	setMinimumSize(minimumSize().width(), (h > 0) ? h : 0);
	resize(width(), height()-hOffset);
    }
    
    if (enable)
	rescan();
}

void MainDialog::preferencesClicked()
{
    PreferencesDialog p(0, "PreferencesDialog", true);
    p.exec();
}

void MainDialog::helpClicked()
{
	KApplication::kApplication()->invokeHelp();
}
