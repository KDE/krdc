/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you wish to add, delete or rename functions or slots use
** Qt Designer which will update this file, preserving your code. Create an
** init() function in place of a constructor, and a destroy() function in
** place of a destructor.
*****************************************************************************/

void HostProfiles::removeHost()
{
  HostPreferences *hps = HostPreferences::instance();

  Q3ListViewItemIterator it(hostListView);
  while (it.current())
  {
    Q3ListViewItem *vi = it.current();
    if (vi->isSelected())
    {
      HostPrefPtr hp = hps->getHostPref(vi->text(0), vi->text(1));
      if (hp)
        deletedHosts += hp;
      delete vi;
    }
    else
      ++it;
  }
  removeAllButton->setEnabled(hostListView->childCount() > 0);
}

void HostProfiles::removeAllHosts()
{
  HostPreferences *hps = HostPreferences::instance();

  Q3ListViewItemIterator it(hostListView);
  while (it.current())
  {
    Q3ListViewItem *vi = it.current();
    HostPrefPtr hp = hps->getHostPref(vi->text(0), vi->text(1));
    if (hp)
      deletedHosts += hp;
    ++it;
  }
  hostListView->clear();
  removeAllButton->setEnabled(false);
}


void HostProfiles::selectionChanged()
{
  Q3ListViewItemIterator it(hostListView);
  while (it.current())
  {
    if (it.current()->isSelected())
    {
      removeHostButton->setEnabled(true);
      return;
    }
    ++it;
  }
  removeHostButton->setEnabled(false);
}


void HostProfiles::load()
{
  HostPreferences *hps = HostPreferences::instance();

  HostPrefPtrList hplist = hps->getAllHostPrefs();
  HostPrefPtrList::iterator it = hplist.begin();
  while ( it != hplist.end() )
  {
    HostPref *hp = *it;
    new K3ListViewItem( hostListView, hp->host(), hp->type(),
        hp->prefDescription() );
    ++it;
  }
}


void HostProfiles::save()
{
  HostPreferences *hps = HostPreferences::instance();

  HostPrefPtrList::iterator it = deletedHosts.begin();
  while (it != deletedHosts.end())
  {
    hps->removeHostPref(*it);
    it++;
  }

  hps->sync();
}
