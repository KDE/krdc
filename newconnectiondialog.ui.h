#include <qregexp.h>

/* Disable connect before user typed something like a host name */
void NewConnectionDialog::hostChanged(const QString &text) {
    connectButton->setEnabled(text.contains(QRegExp(":[0-9]+$")) ||
			      text.contains(QRegExp("^vnc://.+")));
}

