
/* Disable connect before user typed something like a host name */
void NewConnectionDialog::hostChanged(const QString &text) {
    connectButton->setEnabled((text.length() > 0) && (text!=":"));
}

