
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
