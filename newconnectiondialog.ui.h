#include <qregexp.h>
#include <kdebug.h>

/* Disable connect before user typed something like a host name */
void NewConnectionDialog::hostChanged(const QString &text) {
    connectButton->setEnabled(text.contains(QRegExp(":[0-9]+$")) ||
			      text.contains(QRegExp("^vnc://.+")));
}

void NewConnectionDialog::toggleBrowsingArea()
{
    enableBrowsingArea(!browsingPanel->isVisible());
}

void NewConnectionDialog::rescan()
{
}

void NewConnectionDialog::enableBrowsingArea(bool enable)
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
	int h = minimumSize().height()-hOffset;
	setMinimumSize(minimumSize().width(), (h > 0) ? h : 0);
	resize(width(), height()-hOffset);
    }
    
    if (enable)
	rescan();
}

