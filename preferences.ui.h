void PreferencesDialog::init()
{
	KConfig *config = KApplication::kApplication()->config();
	HostPreferences hps(config);
	QValueList<SmartPtr<HostPref> > hplist = hps.getAllHostPrefs();
	QValueList<SmartPtr<HostPref> >::iterator it = hplist.begin();
	while (it != hplist.end()) {
	    HostPref *hp = *it;
	    (void) new KListViewItem(hostListView, hp->host(), hp->type(), hp->prefDescription());
	    it++;
	}
	
	config->setGroup("VncDefaultSettings");
	vncQualityCombo->setCurrentItem(config->readNumEntry("vncQuality", 0));
	showPreferencesCheckbox->setChecked(!config->readBoolEntry("vncShowHostPreferences", true));

	config->setGroup("RdpDefaultSettings");
	rdpWidthSpin->setValue(config->readNumEntry("rdpWidth", 800));
	rdpHeightSpin->setValue(config->readNumEntry("rdpHeight", 600));
	if(rdpWidthSpin->value() == 640 && rdpHeightSpin->value() == 480)
	{
		rdpResolutionCombo->setCurrentItem(0);
	}
	else if(rdpWidthSpin->value() == 800 && rdpHeightSpin->value() == 600)
	{
		rdpResolutionCombo->setCurrentItem(1);
	}
	else if(rdpWidthSpin->value() == 1024 && rdpHeightSpin->value() == 768)
	{
		rdpResolutionCombo->setCurrentItem(2);
	}
	else
	{
		rdpResolutionCombo->setCurrentItem(3);
		rdpWidthSpin->setEnabled(true);
		rdpHeightSpin->setEnabled(true);
	}
	QString keymap = config->readEntry("rdpKeyboardLayout", "en-us");
	int layout;
	for(layout = 0; layout < rdpNumKeymaps; layout++)
	{
		if(keymap == rdpKeymaps[layout])
		{
			break;
		}
	}
	if(layout == rdpNumKeymaps)
	{
		layout = rdpDefaultKeymap;
	}
	rdpKeyboardCombo->setCurrentItem(layout);
	rdpShowPreferencesCheckbox->setChecked(!config->readBoolEntry("rdpShowHostPreferences", true));
}
    
    void PreferencesDialog::okClicked()
    {
	KConfig *config = KApplication::kApplication()->config();
	HostPreferences hps(config);
	QValueList<SmartPtr<HostPref> >::iterator it = deletedHosts.begin();
	while (it != deletedHosts.end()) {
	    hps.removeHostPref(*it);
	    it++;
	}
	deletedHosts.clear();
	
	config->setGroup("VncDefaultSettings");
	config->writeEntry("vncQuality", vncQualityCombo->currentItem());
	config->writeEntry("vncShowHostPreferences", !showPreferencesCheckbox->isChecked());
	
	config->setGroup("RdpDefaultSettings");
	config->writeEntry("rdpWidth", rdpWidthSpin->value());
	config->writeEntry("rdpHeight", rdpHeightSpin->value());
	config->writeEntry("rdpKeyboardLayout", rdpKeymaps[rdpKeyboardCombo->currentItem()]);
	config->writeEntry("rdpShowHostPreferences", !rdpShowPreferencesCheckbox->isChecked());
	
	hps.sync();
    }
    
    
    
    void PreferencesDialog::removeHost()
    {
	KConfig *config = KApplication::kApplication()->config();
	HostPreferences hps(config);
	
	QListViewItemIterator it(hostListView);
	while (it.current()) {
	    QListViewItem *vi = it.current();
	    if (vi->isSelected()) {
		SmartPtr<HostPref> hp = hps.getHostPref(vi->text(0), vi->text(1));
		if (hp)
		    deletedHosts += hp;
		delete vi;
	    }
	    else
		++it;
	}
    }
    
    
    void PreferencesDialog::removeAllHosts()
    {
	KConfig *config = KApplication::kApplication()->config();
	HostPreferences hps(config);
	
	QListViewItemIterator it(hostListView);
	while (it.current()) {
	    QListViewItem *vi = it.current();
	    SmartPtr<HostPref> hp = hps.getHostPref(vi->text(0), vi->text(1));
	    if (hp)
		deletedHosts += hp;
	    ++it;
	}
	hostListView->clear();
    }
    
    
    void PreferencesDialog::selectionChanged()
    {
	removeAllButton->setEnabled(hostListView->childCount() > 0);
	
	QListViewItemIterator it(hostListView);
	while (it.current()) {
	    if (it.current()->isSelected()) {
		removeHostButton->setEnabled(true);
		return;
	    }
	    ++it;
	}
	removeHostButton->setEnabled(false);   
    }
    
void PreferencesDialog::resolutionChanged(int selection)
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
    
